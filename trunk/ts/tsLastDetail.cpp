//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "ts/tsLastDetail.h"

#include "renderInstance/renderPassManager.h"
#include "ts/tsShapeInstance.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "lighting/lightInfo.h"

#include "renderInstance/renderImposterMgr.h"
#include "gfx/gfxTransformSaver.h"
#include "math/mRandom.h"
#include "core/stream/fileStream.h"
#include "util/imposterCapture.h"
#include "core/resourceManager.h"

#include "gfx/bitmap/ddsFile.h"
#include "gfx/bitmap/ddsUtils.h"


Vector<TSLastDetail*> TSLastDetail::smLastDetails;


GFX_ImplementTextureProfile(TSImposterDiffuseTexProfile, 
                            GFXTextureProfile::DiffuseMap, 
                            GFXTextureProfile::Static, 
                            GFXTextureProfile::None);

GFX_ImplementTextureProfile(TSImposterNormalMapTexProfile, 
                            GFXTextureProfile::NormalMap, 
                            GFXTextureProfile::Static, 
                            GFXTextureProfile::None);


TSLastDetail::TSLastDetail(   TSShape *shape,
                              const String &cachePath,
                              U32 numEquatorSteps,
                              U32 numPolarSteps,
                              F32 polarAngle,
                              bool includePoles,
                              S32 dl, S32 dim )
{
   mNumEquatorSteps = numEquatorSteps;
   mNumPolarSteps = numPolarSteps;
   mPolarAngle = polarAngle;
   mIncludePoles = includePoles;
   mShape = shape;
   mDl = dl;
   mDim = dim;

   mCachePath = cachePath;

   // Store this in the static list.
   smLastDetails.push_back( this );

   mRadius = mShape->radius;
}

TSLastDetail::~TSLastDetail()
{
   mTexture.free();
   mNormalMap.free();

   // Remove ourselves from the list.
   Vector<TSLastDetail*>::iterator iter = find( smLastDetails.begin(), smLastDetails.end(), this );
   smLastDetails.erase( iter );
}

void TSLastDetail::render( const TSRenderState &rdata, F32 alpha )
{
   // If the texture isn't setup... we have nothing to render.
   if ( mTexture.isNull() )
      return;

   const MatrixF &mat = GFX->getWorldMatrix();

   // Post a render instance for this imposter... the special
   // imposter render manager will do the magic!
   RenderPassManager *renderPass = rdata.getSceneState()->getRenderPass();

   ImposterRenderInst *ri = renderPass->allocInst<ImposterRenderInst>();
   ri->alpha = alpha;
   ri->scale = mat.getScale().x;
   ri->halfSize = mRadius * ri->scale;
   ri->detail = this;

   // We use the center of the object bounds for
   // the center of the billboard quad.
   mShape->bounds.getCenter( &ri->center );
   mat.mulP( ri->center );

   // Since we support billboards at any angle we
   // need the full rotation stored in a quat.
   ri->rotQuat.set( mat );

   // We sort by TSLastDetail since we render in TSLastDetail batches.
   ri->defaultKey = (U32)this;

   // TODO: Does this work... or even gain us anything?
   //#define HIGH_NUM ((U32(-1)/2) - 1)
   //F32 camDist = ( rdata.getCamTransform().getPosition() - ri->pos ).lenSquared();
   //ri->defaultKey2 = HIGH_NUM - camDist;

   renderPass->addInst( ri );   
}

void TSLastDetail::update( bool forceUpdate )
{
   // This should never be called on a dedicated server or
   // anywhere else where we don't have a GFX device!
   AssertFatal( GFXDevice::devicePresent(), "TSLastDetail::update() - Cannot update without a GFX device!" );

   // Clear the texture first.
   mTexture.free();
   mNormalMap.free();
   mTextureUVs.clear();

   String imposterDDSPath = mCachePath + ".imposter.dds";
   String normalsDDSPath = mCachePath + ".imposter_normals.dds";

   // Get the date/time for the dts, 
   // and the imposter DDS and imposter
   // normals DDS.
   FileTime dtsTime = {0}, imposterTime = {0}, normalsTime = {0};
   Platform::getFileTimes( mCachePath.c_str(), NULL, &dtsTime );
   Platform::getFileTimes( imposterDDSPath.c_str(), NULL, &imposterTime );
   Platform::getFileTimes( normalsDDSPath.c_str(), NULL, &normalsTime );

   // If the binary is newer... load that.
   if ( !forceUpdate && Platform::compareFileTimes( imposterTime, dtsTime ) >= 0 && Platform::compareFileTimes( normalsTime, dtsTime ) >= 0 )
   {
      Torque::Path imposterTPath( imposterDDSPath );
      Torque::Path normalTPath( normalsDDSPath );

      bool imposterRet = mTexture.set( imposterDDSPath, &TSImposterDiffuseTexProfile, avar( "TSImposterDiffuseTexProfile %s() - (line %d)", __FUNCTION__, __LINE__ ) );
      bool normalsRet = mNormalMap.set( normalsDDSPath, &TSImposterNormalMapTexProfile, avar( "TSImposterNormalMapTexProfile %s() - (line %d)", __FUNCTION__, __LINE__ ) );

      if ( imposterRet && normalsRet )
      {
         Resource<DDSFile> imposterDDS = ResourceManager::get().load( imposterTPath );
      
         Point2I texSize( imposterDDS->getWidth(), imposterDDS->getHeight() );

         // TODO: This doesn't account for the mDim being reduced 
         // to fix the maximum imposter texture size.  We should 
         // probably calculate that first.

         // Ok... pack in bitmaps till we run out.
         for ( S32 y=0; y < texSize.y; )
         {
            for ( S32 x=0; x < texSize.x; )
            {
               // Store the uv for later lookup.
               RectF info;
               info.point.set( (F32)x / (F32)texSize.x, (F32)y / (F32)texSize.y );
               info.extent.set( (F32)mDim / (F32)texSize.x, (F32)mDim / (F32)texSize.y );
               mTextureUVs.push_back( info );
               
               x += mDim;
            }

            y += mDim;
         }
         return;
      }
   }

   // Give ourselves plenty of room for high quality billboards.
   const S32 maxTexSize = 2048;

   // Loop till they fit.
   S32 newDim = mDim;
   while ( true )
   {
      S32 maxImposters = ( maxTexSize / newDim ) * ( maxTexSize / newDim );
      S32 imposterCount = ( ((2*mNumPolarSteps) + 1 ) * mNumEquatorSteps ) + ( mIncludePoles ? 2 : 0 );
      if ( imposterCount <= maxImposters )
         break;

      // There are too many imposters to fit a single 
      // texture, so we fail.  These imposters are for
      // rendering small distant objects.  If you need
      // a really high resolution imposter or many images
      // around the equator and poles, maybe you need a
      // custom solution.

      newDim /= 2;
   }

   if ( newDim != mDim )
   {
      Con::printf( "TSLastDetail::update( '%s' ) - Detail dimensions too big! Reduced from %d to %d.", 
         mCachePath.c_str(),
         mDim, newDim );

      mDim = newDim;
   }

   const F32 equatorStepSize = M_2PI_F / (F32) mNumEquatorSteps;
   const F32 polarStepSize = mNumPolarSteps>0 ? (0.5f * M_PI_F - mPolarAngle) / (F32)mNumPolarSteps : 0.0f;

   Vector<GBitmap*> bitmaps;
   Vector<GBitmap*> normalmaps;

   PROFILE_START(TSLastDetail_snapshots);

   // We need to create our own instance to render with.
   TSShapeInstance *shape = new TSShapeInstance( mShape, true );

   // Animate the shape once.
   shape->animate( mDl );

   PROFILE_START(TSShapeInstance_snapshot_sb_setup);

      ImposterCapture *imposterCap = new ImposterCapture();

      // We render these objects unlit at full ambient color.
      LightManager* lm = gClientSceneGraph->getLightManager();
      lm->unregisterAllLights();
      LightInfo *light = LightManager::createLightInfo();
      light->setType( LightInfo::Vector );
	   light->setDirection( VectorF( 1.0, 0.0, 0.0 ) );
      light->setColor( ColorF( 0, 0, 0, 1 ) );
      light->setAmbient( ColorF( 1, 1, 1, 1 ) );
      light->setCastShadows( false );
      lm->setSpecialLight( LightManager::slSunLightType, light );

   PROFILE_END(); // TSShapeInstance_snapshot_sb_setup

   // We capture the images in a particular order which must
   // match the order expected by the imposter renderer.  The
   // order is...
   //
   //
   //
   //
   GBitmap *imposter = NULL;
   GBitmap *normalmap = NULL;

   imposterCap->begin( shape, mDl, mDim, mRadius, mShape->center );

   F32 rotX = mNumPolarSteps > 0 ? mPolarAngle - 0.5f * M_PI_F : 0.0f;

   for ( U32 j=0; j < (2 * mNumPolarSteps + 1); j++ )
   {
      F32 rotZ = 0;

      for ( U32 i=0; i < mNumEquatorSteps; i++ )
      {
         MatrixF angMat;
         angMat.mul(  MatrixF( EulerF( 0, rotX, 0 ) ),
                     MatrixF( EulerF( 0, 0, rotZ ) ) );

         imposterCap->capture( angMat, &imposter, &normalmap );

         bitmaps.push_back( imposter );
         normalmaps.push_back( normalmap );

         rotZ += equatorStepSize;
      }

      rotX += polarStepSize;
   }

   if (mIncludePoles)
   {
      MatrixF topXfm( EulerF( 0, -M_PI_F / 2.0f, 0 ) );
      MatrixF bottomXfm( EulerF( 0, M_PI_F / 2.0f, 0 ) );

      imposterCap->capture( topXfm, &imposter, &normalmap );

      bitmaps.push_back(imposter);
      normalmaps.push_back( normalmap );

      imposterCap->capture( bottomXfm, &imposter, &normalmap );

      bitmaps.push_back( imposter );
      normalmaps.push_back( normalmap );
   }

   imposterCap->end();

   delete shape;

   PROFILE_END(); // TSLastDetail_snapshots

   // done rendering, reset render states
   PROFILE_START(TSShapeInstance_snapshot_sb_unsetup);

      lm->unregisterAllLights();
      delete light;

   PROFILE_END();

   // NOTE: If imposters start rendering wrong, look at the 
   // D3DTexture lock for swapped xy coords!

   // Combine the impostors into a single texture 
   // to allow for batch rendering.

   // Figure out the optimal texture size.
   Point2I texSize( maxTexSize, maxTexSize );
   while ( true )
   {
      Point2I halfSize( texSize.x / 2, texSize.y / 2 );
      U32 count = ( halfSize.x / mDim ) * ( halfSize.y / mDim );
      if ( count < bitmaps.size() )
      {
         // Try half of the height.
         count = ( texSize.x / mDim ) * ( halfSize.y / mDim );
         if ( count >= bitmaps.size() )
            texSize.y = halfSize.y;
         break;
      }

      texSize = halfSize;
   }

   // Prepare a new texture for compositing.
   GFXFormat format = bitmaps.first()->getFormat();
   GBitmap destBmp( texSize.x, texSize.y, false, format );
   dMemset( destBmp.getWritableBits(), 0, texSize.x * texSize.y * (S32)GFXDevice::formatByteSize( format ) );

   format = normalmaps.first()->getFormat();
   GBitmap destNormal( texSize.x, texSize.y, false, format );
   dMemset( destNormal.getWritableBits(), 0, texSize.x * texSize.y * (S32)GFXDevice::formatByteSize( format ) );

   // Ok... pack in bitmaps till we run out.
   for ( S32 y=0; y < texSize.y; )
   {
      for ( S32 x=0; x < texSize.x; )
      {
         // Copy the next bitmap to the dest texture.
         GBitmap* bmp = bitmaps.first();
         bitmaps.pop_front();
         destBmp.copyRect( bmp, RectI( 0, 0, mDim, mDim ), Point2I( x, y ) );
         delete bmp;

         // Copy the next normal to the dest texture.
         GBitmap* normalmap = normalmaps.first();
         normalmaps.pop_front();
         destNormal.copyRect( normalmap, RectI( 0, 0, mDim, mDim ), Point2I( x, y ) );
         delete normalmap;

         // Store the uv for later lookup.
         RectF info;
         info.point.set( (F32)x / (F32)texSize.x, (F32)y / (F32)texSize.y );
         info.extent.set( (F32)mDim / (F32)texSize.x, (F32)mDim / (F32)texSize.y );
         mTextureUVs.push_back( info );

         // Did we finish?
         if ( bitmaps.empty() )
            break;
         
         x += mDim;
      }

      // Did we finish?
      if ( bitmaps.empty() )
         break;

      y += mDim;
   }


   /*
   // Should we dump the images?
   if ( Con::getBoolVariable( "$TSLastDetail::dumpImposters", false ) )
   {
      FileStream stream;
      if ( stream.open( imposterPath, Torque::FS::File::Write  ) )
         destBmp.writeBitmap( "png", stream );
      stream.close();

      if ( stream.open( normalsPath, Torque::FS::File::Write ) )
         destNormal.writeBitmap( "png", stream );
      stream.close();
   }
   */

   // DEBUG: Some code to force usage of a test image.
   //GBitmap* tempMap = GBitmap::load( "./forest/data/test1234.png" );
   //tempMap->extrudeMipLevels();
   //mTexture.set( tempMap, &GFXDefaultStaticDiffuseProfile, false );
   //delete tempMap;

   // TODO: Full mips with the current generator makes
   // everything look a bit too soft.  The Nvidia DDS 
   // plugin with point sampled mips looks pretty good 
   // as it preserves alot more detail.
   //
   // Look into adding some sort of sharpen filter to mip
   // generation in the future.
   //
   destBmp.extrudeMipLevels();
   destNormal.extrudeMipLevels();

   DDSFile *ddsDest = DDSFile::createDDSFileFromGBitmap( &destBmp );
   DDSUtil::squishDDS( ddsDest, GFXFormatDXT5 );

   DDSFile *ddsNormals = DDSFile::createDDSFileFromGBitmap( &destNormal );
   DDSUtil::squishDDS( ddsNormals, GFXFormatDXT1 );

   if ( 1 )
   {
      FileStream fs;
      if ( fs.open( imposterDDSPath, Torque::FS::File::Write ) )
         ddsDest->write( fs );

      fs.close();

      if ( fs.open( normalsDDSPath, Torque::FS::File::Write ) )
         ddsNormals->write( fs );

      fs.close();
   }

   // TODO: Move the "fizzle" generation in the alpha layer to
   // after mip extrusion and do it for each mip.  Currently
   // the mip extrusion will muddy the fizzle values with the
   // lower layers becoming uniform grey.


   // Prep the mips and copy the dest bmp to the texture.
   
   mTexture.set( imposterDDSPath, &TSImposterDiffuseTexProfile, avar( "TSImposterDiffuseTexProfile %s() - (line %d)", __FUNCTION__, __LINE__ ) );
   mNormalMap.set( normalsDDSPath, &TSImposterNormalMapTexProfile, avar( "TSImposterNormalMapTexProfile %s() - (line %d)", __FUNCTION__, __LINE__ ) );

   delete ddsDest;
   delete ddsNormals;

   delete imposterCap;
}

void TSLastDetail::updateImposterImages( bool forceUpdate )
{
   // Can't do it without GFX!
   if ( !GFX )
      return;

   //D3DPERF_SetMarker( D3DCOLOR_RGBA( 0, 255, 0, 255 ), L"TSLastDetail::makeImposter" );

   bool sceneBegun = GFX->canCurrentlyRender();
   if ( !sceneBegun )
      GFX->beginScene();

   Vector<TSLastDetail*>::iterator iter = smLastDetails.begin();
   for ( ; iter != smLastDetails.end(); iter++ )
      (*iter)->update( forceUpdate );

   if ( !sceneBegun )
      GFX->endScene();
}

ConsoleFunction(tsUpdateImposterImages, void, 1, 2, "tsUpdateImposterImages( bool forceupdate )")
{
   if ( argc > 1 )
      TSLastDetail::updateImposterImages( dAtob( argv[1] ) );
   else
      TSLastDetail::updateImposterImages();
}


