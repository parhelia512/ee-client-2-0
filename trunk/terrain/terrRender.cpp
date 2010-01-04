//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "terrain/terrRender.h"

#include "terrain/terrData.h"
#include "terrain/terrCell.h"
#include "terrain/terrMaterial.h"
#include "terrain/terrCellMaterial.h"
#include "materials/shaderData.h"

#include "platform/profiler.h"
#include "sceneGraph/sceneState.h"
#include "math/util/frustum.h"
#include "renderInstance/renderPassManager.h"
#include "renderInstance/renderTerrainMgr.h"

#include "lighting/lightInfo.h"
#include "lighting/lightManager.h"

#include "materials/matInstance.h"
#include "materials/materialManager.h"
#include "materials/matTextureTarget.h"
#include "shaderGen/conditionerFeature.h"

#include "gfx/gfxDrawUtil.h"

#include "gfx/gfxTransformSaver.h"
#include "gfx/bitmap/gBitmap.h"
#include "gfx/bitmap/ddsFile.h"
#include "gfx/bitmap/ddsUtils.h"
#include "terrain/terrMaterial.h"
#include "gfx/gfxDebugEvent.h"
#include "gfx/gfxCardProfile.h"
#include "core/stream/fileStream.h"


bool TerrainBlock::smDebugRender = false;


GFX_ImplementTextureProfile( TerrainLayerTexProfile,
                            GFXTextureProfile::DiffuseMap, 
                            GFXTextureProfile::PreserveSize | 
                            GFXTextureProfile::Static,
                            GFXTextureProfile::None );


void TerrainBlock::_onLMActivate( const char *lm, bool activate )
{
   if ( activate )
   {
      if ( mCell )
         mCell->deleteMaterials();

      SAFE_DELETE( mBaseMaterial );
   }
}

void TerrainBlock::_updateMaterials()
{   
   mBaseTextures.setSize( mFile->mMaterials.size() );

   mMaxDetailDistance = 0.0f;

   for ( U32 i=0; i < mFile->mMaterials.size(); i++ )
   {
      TerrainMaterial *mat = mFile->mMaterials[i];

      mBaseTextures[i].set( mat->getDiffuseMap(),  
         &GFXDefaultStaticDiffuseProfile, 
         "TerrainBlock::_updateMaterials() - DiffuseMap" );

      // Find the maximum detail distance.
      if (  mat->getDetailMap().isNotEmpty() &&
            mat->getDetailDistance() > mMaxDetailDistance )
         mMaxDetailDistance = mat->getDetailDistance();
   }

   if ( mCell )
      mCell->deleteMaterials();
}

void TerrainBlock::_updateLayerTexture()
{  
   const U32 layerSize = mFile->mSize;
   const Vector<U8> &layerMap = mFile->mLayerMap;
   const U32 pixelCount = layerMap.size();

   if (  mLayerTex.isNull() ||
         mLayerTex.getWidth() != layerSize ||
         mLayerTex.getHeight() != layerSize )
      mLayerTex.set( layerSize, layerSize, GFXFormatR8G8B8A8, &TerrainLayerTexProfile, "" );

   AssertFatal(   mLayerTex.getWidth() == layerSize &&
                  mLayerTex.getHeight() == layerSize,
      "TerrainBlock::_updateLayerTexture - The texture size doesn't match the requested size!" );

   // Update the layer texture.
   GFXLockedRect *lock = mLayerTex.lock();

   for ( U32 i=0; i < pixelCount; i++ )
   {  
      lock->bits[0] = layerMap[i];

      if ( i + 1 >= pixelCount )
         lock->bits[1] = lock->bits[0];
      else
         lock->bits[1] = layerMap[i+1];

      if ( i + layerSize >= pixelCount )
         lock->bits[2] = lock->bits[0];
      else
         lock->bits[2] = layerMap[i + layerSize];

      if ( i + layerSize + 1 >= pixelCount )
         lock->bits[3] = lock->bits[0];
      else
         lock->bits[3] = layerMap[i + layerSize + 1];

      lock->bits += 4;
   }

   mLayerTex.unlock();
   //mLayerTex->dumpToDisk( "png", "./layerTex.png" );
}

bool TerrainBlock::_initBaseShader()
{
   ShaderData *shaderData = NULL;
   if ( !Sim::findObject( "TerrainBlendShader", shaderData ) || !shaderData )
      return false;

   mBaseShader = shaderData->getShader();

   mBaseShaderConsts = mBaseShader->allocConstBuffer();
   mBaseTexScaleConst = mBaseShader->getShaderConstHandle( "$texScale" );
   mBaseTexIdConst = mBaseShader->getShaderConstHandle( "$texId" );
   mBaseLayerSizeConst = mBaseShader->getShaderConstHandle( "$layerSize" );

   mBaseTarget = GFX->allocRenderToTextureTarget();

   GFXStateBlockDesc desc;
   desc.samplersDefined = true;
   desc.samplers[0] = GFXSamplerStateDesc::getClampPoint();   
   desc.samplers[1] = GFXSamplerStateDesc::getWrapLinear();   
   desc.zDefined = true;
   desc.zWriteEnable = false;
   desc.zEnable = false;
   desc.setBlend( true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha );
   desc.cullDefined = true;
   desc.cullMode = GFXCullNone;
   mBaseShaderSB = GFX->createStateBlock( desc );

   return true;
}

void TerrainBlock::_updateBaseTexture( bool writeToCache )
{
   if ( !mBaseShader && !_initBaseShader() )
      return;

   // This can sometimes occur outside a begin/end scene.
   const bool sceneBegun = GFX->canCurrentlyRender();
   if ( !sceneBegun )
      GFX->beginScene();

   GFXDEBUGEVENT_SCOPE( TerrainBlock_UpdateBaseTexture, ColorI::GREEN );

   PROFILE_SCOPE( TerrainBlock_UpdateBaseTexture );

   GFXTransformSaver saver;

   const U32 maxTextureSize = GFX->getCardProfiler()->queryProfile( "maxTextureSize", 1024 );

   U32 baseTexSize = getNextPow2( mBaseTexSize );
   baseTexSize = getMin( maxTextureSize, baseTexSize );
   Point2I destSize( baseTexSize, baseTexSize );

   // Setup geometry
   GFXVertexBufferHandle<GFXVertexPT> vb;
   {
      F32 copyOffsetX = 2.0f * GFX->getFillConventionOffset() / (F32)destSize.x;
      F32 copyOffsetY = 2.0f * GFX->getFillConventionOffset() / (F32)destSize.y;

      const bool needsYFlip = GFX->getAdapterType() == OpenGL;

      GFXVertexPT points[4];
      points[0].point      = Point3F( -1.0 - copyOffsetX, -1.0 + copyOffsetY, 0.0 );
      points[0].texCoord   = Point2F(  0.0, needsYFlip ? 0.0f : 1.0f );
      points[1].point      = Point3F( -1.0 - copyOffsetX,  1.0 + copyOffsetY, 0.0 );
      points[1].texCoord   = Point2F(  0.0, needsYFlip ? 1.0f : 0.0f );
      points[2].point      = Point3F(  1.0 - copyOffsetX,  1.0 + copyOffsetY, 0.0 );
      points[2].texCoord   = Point2F(  1.0, needsYFlip ? 1.0f : 0.0f );
      points[3].point      = Point3F(  1.0 - copyOffsetX, -1.0 + copyOffsetY, 0.0 );
      points[3].texCoord   = Point2F(  1.0, needsYFlip ? 0.0f : 1.0f );

      vb.set( GFX, 4, GFXBufferTypeVolatile );
      dMemcpy( vb.lock(), points, sizeof(GFXVertexPT) * 4 );
      vb.unlock();
   }

   GFXTexHandle blendTex;

   // If the base texture is already a valid render target then 
   // use it to render to else we create one.
   if (  mBaseTex.isValid() && 
         mBaseTex->isRenderTarget() &&
         mBaseTex->getFormat() == GFXFormatR8G8B8A8 &&
         mBaseTex->getWidth() == destSize.x &&
         mBaseTex->getHeight() == destSize.y )
      blendTex = mBaseTex;
   else
      blendTex.set( destSize.x, destSize.y, GFXFormatR8G8B8A8, &GFXDefaultRenderTargetProfile, "" );

   GFX->pushActiveRenderTarget();   

   // Set our shader stuff
   GFX->setShader( mBaseShader );
   GFX->setShaderConstBuffer( mBaseShaderConsts );
   GFX->setStateBlock( mBaseShaderSB );
   GFX->setVertexBuffer( vb );

   mBaseTarget->attachTexture( GFXTextureTarget::Color0, blendTex );
   GFX->setActiveRenderTarget( mBaseTarget );

   GFX->setTexture( 0, mLayerTex );
   mBaseShaderConsts->set( mBaseLayerSizeConst, (F32)mLayerTex->getWidth() );      

   for ( U32 i=0; i < mBaseTextures.size(); i++ )
   {
      GFXTextureObject *tex = mBaseTextures[i];
      if ( !tex )
         continue;

      GFX->setTexture( 1, tex );

      F32 baseSize = mFile->mMaterials[i]->getDiffuseSize();
      F32 scale = 1.0f;
      if ( !mIsZero( baseSize ) )
         scale = getWorldBlockSize() / baseSize;
      
      // A mistake early in development means that texture
      // coords are not flipped correctly.  To compensate
      // we flip the y scale here.
      mBaseShaderConsts->set( mBaseTexScaleConst, Point2F( scale, -scale ) );
      mBaseShaderConsts->set( mBaseTexIdConst, (F32)i );

      GFX->drawPrimitive( GFXTriangleFan, 0, 2 );
   }

   mBaseTarget->resolve();
   
   GFX->setShader( NULL );
   //GFX->setStateBlock( NULL ); // WHY NOT?
   GFX->setShaderConstBuffer( NULL );
   GFX->setVertexBuffer( NULL );

   GFX->popActiveRenderTarget();

   // End it if we begun it... Yeehaw!
   if ( !sceneBegun )
      GFX->endScene();

   /// Do we cache this sucker?
   if ( writeToCache )
   {
      String cachePath = _getBaseTexCacheFileName();

      FileStream fs;
      if ( fs.open( _getBaseTexCacheFileName(), Torque::FS::File::Write ) )
      {
         // Read back the render target, dxt compress it, and write it to disk.
         GBitmap blendBmp( destSize.x, destSize.y, false, GFXFormatR8G8B8A8 );
         blendTex.copyToBmp( &blendBmp );

         /*
         // Test code for dumping uncompressed bitmap to disk.
         {
         FileStream fs;
         if ( fs.open( "./basetex.png", Torque::FS::File::Write ) )
         {
         blendBmp.writeBitmap( "png", fs );
         fs.close();
         }         
         }
         */

         blendBmp.extrudeMipLevels();

         DDSFile *blendDDS = DDSFile::createDDSFileFromGBitmap( &blendBmp );
         DDSUtil::squishDDS( blendDDS, GFXFormatDXT1 );

         // Write result to file stream
         blendDDS->write( fs );
      }
      fs.close();
   }
   else
   {
      // We didn't cache the result, so set the base texture
      // to the render target we updated.  This should be good
      // for realtime painting cases.
      mBaseTex = blendTex;
   }
}

void TerrainBlock::_renderBlock( SceneState *state )
{
   PROFILE_SCOPE( TerrainBlock_RenderBlock );

   MatrixF worldViewXfm = GFX->getWorldMatrix();
   worldViewXfm.mul( getRenderTransform() );

   MatrixF worldViewProjXfm = GFX->getProjectionMatrix();
   worldViewProjXfm.mul( worldViewXfm );

   const MatrixF &objectXfm = getRenderWorldTransform();

   Point3F objCamPos = state->getDiffuseCameraPosition();
   objectXfm.mulP( objCamPos );

   // Get the shadow material.
   if ( !mDefaultMatInst )
      mDefaultMatInst = MATMGR->createMatInstance( "AL_DefaultShadowMaterial", getGFXVertexFormat<TerrVertex>() );

   // Make sure we have a base material.
   if ( !mBaseMaterial )
   {
      mBaseMaterial = new TerrainCellMaterial();
      mBaseMaterial->init( this, 0, false, true );
   }

   // The cells are in object space... we must
   // transform frustum so we can cull them.
   Frustum frustum( state->getFrustum() );
   frustum.mulL( objectXfm );

   // If this is a reflection pass we must invert
   // the frustum else culling will fail!
   if ( state->isReflectPass() )
      frustum.invert();  

   // Did the detail layers change?
   if ( mDetailsDirty )
   {   
      _updateMaterials();
      mDetailsDirty = false;
   }

   // Do we need to update the textures?
   if ( mLayerTexDirty || mBaseTex.isNull() )
   {
      _updateLayerTexture();
      _updateBaseTexture( false );
      mLayerTexDirty = false;
   }

   static Vector<TerrCell*> renderCells;
   renderCells.clear();

   mCell->cullCells( &frustum, 
                     state,
                     objCamPos,
                     &renderCells );

   RenderPassManager *renderPass = state->getRenderPass();

   MatrixF *riObjectToWorldXfm = renderPass->allocUniqueXform( getRenderTransform() );

   const bool isColorDrawPass = state->isDiffusePass() || state->isReflectPass();

   // Only pass and use the light manager if this is not a shadow pass.
   LightManager *lm = NULL;
   if ( isColorDrawPass )
      lm = state->getLightManager();

   for ( U32 i=0; i < renderCells.size(); i++ )
   {
      TerrCell *cell = renderCells[i];

      // Ok this cell is fit to render.
      TerrainRenderInst *inst = renderPass->allocInst<TerrainRenderInst>();

      // Setup lights for this cell.
      if ( lm )
      {
         SphereF bounds = cell->getSphereBounds();
         getRenderTransform().mulP( bounds.center );
         lm->setupLights( NULL, bounds );
         lm->getBestLights( inst->lights, 4 );
         lm->resetLights();
      }

      GFXVertexBufferHandleBase vertBuff;

      cell->getRenderPrimitive( &inst->prim, &vertBuff );

      // This is only here so that we can get the
      // hook for the shadow material.
      inst->mat = mDefaultMatInst;

      inst->vertBuff = vertBuff.getPointer();
      inst->primBuff = mPrimBuffer.getPointer();

      inst->objectToWorldXfm = riObjectToWorldXfm;

      // If we're not drawing to the shadow map then we need
      // to include the normal rendering materials. 
      if ( isColorDrawPass )
      {         
         const SphereF &bounds = cell->getSphereBounds();         

         F32 sqDist = ( bounds.center - objCamPos ).lenSquared();         

         F32 radiusSq = ( mMaxDetailDistance + bounds.radius ) * smDetailScale;
         radiusSq *= radiusSq;

         // If this cell is near enough to get detail textures then
         // use the full detail mapping material.  Else we use the
         // simple base only material.
         if ( !state->isReflectPass() && sqDist < radiusSq )
            inst->cellMat = cell->getMaterial();
         else
            inst->cellMat = mBaseMaterial;
      }

      inst->defaultKey = (U32)cell->getMaterials();

      // Submit it for rendering.
      renderPass->addInst( inst );
   }

   // Trigger the debug rendering.
   if (  state->isDiffusePass() && 
         !renderCells.empty() && 
         smDebugRender )
   {      
      // Store the render cells for later.
      mDebugCells = renderCells;

      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &TerrainBlock::_renderDebug );
      ri->type = RenderPassManager::RIT_ObjectTranslucent;
      state->getRenderPass()->addInst( ri );
   }
}

void TerrainBlock::_renderDebug( ObjectRenderInst *ri, 
                                 SceneState *state, 
                                 BaseMatInstance *overrideMat )
{
   GFXTransformSaver saver;
   GFX->multWorld( getRenderTransform() );

   for ( U32 i=0; i < mDebugCells.size(); i++ )
      mDebugCells[i]->renderBounds();

   mDebugCells.clear();
}