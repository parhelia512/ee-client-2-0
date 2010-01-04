//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/stream/bitStream.h"

#include "cubemapData.h"
#include "console/consoleTypes.h"
#include "gfx/gfxCubemap.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDebugEvent.h"
#include "sceneGraph/sceneGraph.h"

IMPLEMENT_CONOBJECT( CubemapData );

//****************************************************************************
// Cubemap Data
//****************************************************************************


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------
CubemapData::CubemapData()
{
   mCubemap = NULL;
   mDynamic = false;
   mDynamicSize = 512;
   mDynamicNearDist = 0.1f;
   mDynamicFarDist = 100.0f;
   mDynamicObjectTypeMask = 0;
#ifdef INIT_HACK
   mInit = false;
#endif
}

CubemapData::~CubemapData()
{
   mCubemap = NULL;
}

//--------------------------------------------------------------------------
// Init fields
//--------------------------------------------------------------------------
void CubemapData::initPersistFields()
{
   addField("cubeFace",                TypeStringFilename,  Offset(mCubeFaceFile, CubemapData), 6);
   addField("dynamic",                 TypeBool,            Offset(mDynamic, CubemapData));
   addField("dynamicSize",             TypeS32,             Offset(mDynamicSize, CubemapData));
   addField("dynamicNearDist",         TypeF32,             Offset(mDynamicNearDist, CubemapData));
   addField("dynamicFarDist",          TypeF32,             Offset(mDynamicFarDist, CubemapData));
   addField("dynamicObjectTypeMask",   TypeS32,             Offset(mDynamicObjectTypeMask, CubemapData));

   Parent::initPersistFields();
}

//--------------------------------------------------------------------------
// onAdd
//--------------------------------------------------------------------------
bool CubemapData::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   // Do NOT call this here as it forces every single cubemap defined to load its images, immediately, without mercy
   // createMap();

   return true;
}

//--------------------------------------------------------------------------
// Create map - this is public so cubemaps can be used with materials
// in the show tool
//--------------------------------------------------------------------------
void CubemapData::createMap()
{
   if( !mCubemap )
   {
      if( mDynamic )
      {
         mCubemap = GFX->createCubemap();
         mCubemap->initDynamic( mDynamicSize );
         mDepthBuff = GFXTexHandle( mDynamicSize, mDynamicSize, GFXFormatD24S8, 
            &GFXDefaultZTargetProfile, avar("%s() - mDepthBuff (line %d)", __FUNCTION__, __LINE__));
         mRenderTarget = GFX->allocRenderToTextureTarget();
      }
      else
      {
         bool initSuccess = true;

         for( U32 i=0; i<6; i++ )
         {
            if( !mCubeFaceFile[i].isEmpty() )
            {
               if(!mCubeFace[i].set(mCubeFaceFile[i], &GFXDefaultStaticDiffuseProfile, avar("%s() - mCubeFace[%d] (line %d)", __FUNCTION__, i, __LINE__) ))
               {
                  Con::errorf("CubemapData::createMap - Failed to load texture '%s'", mCubeFaceFile[i].c_str());
                  initSuccess = false;
               }
            }
         }

         if( initSuccess )
         {
            mCubemap = GFX->createCubemap();
            mCubemap->initStatic( mCubeFace );
         }
      }
   }
}

// Should we just pass the world into here?
void CubemapData::updateDynamic(SceneGraph* sm, const Point3F& pos)
{
   AssertFatal(mDynamic, "This is not a dynamic cubemap!");

   GFXDEBUGEVENT_SCOPE( CubemapData_updateDynamic, ColorI::WHITE );

#ifdef INIT_HACK
   if( mInit ) return;
   mInit = true;
#endif

   GFX->pushActiveRenderTarget();
   mRenderTarget->attachTexture(GFXTextureTarget::DepthStencil, mDepthBuff );

   // store current matrices
   GFXTransformSaver saver;
   F32 oldVisibleDist = sm->getVisibleDistance();

   F32 l, r, b, t, n, f;
   bool ortho;
   GFX->getFrustum( &l, &r, &b, &t, &n, &f, &ortho );

   // set projection to 90 degrees vertical and horizontal
   GFX->setFrustum(90.0f, 1.0f, mDynamicNearDist, mDynamicFarDist);
   sm->setVisibleDistance(mDynamicFarDist);

   // We don't use a special clipping projection, but still need to initialize 
   // this for objects like SkyBox which will use it during a reflect pass.
   gClientSceneGraph->setNonClipProjection( (MatrixF&) GFX->getProjectionMatrix() );

   // Loop through the six faces of the cube map.
   for(U32 i=0; i<6; i++)
   {      
      // Standard view that will be overridden below.
      VectorF vLookatPt(0.0f, 0.0f, 0.0f), vUpVec(0.0f, 0.0f, 0.0f), vRight(0.0f, 0.0f, 0.0f);

      switch( i )
      {
      case 0 : // D3DCUBEMAP_FACE_POSITIVE_X:
         vLookatPt = VectorF( 1.0f, 0.0f, 0.0f );
         vUpVec    = VectorF( 0.0f, 1.0f, 0.0f );
         break;
      case 1 : // D3DCUBEMAP_FACE_NEGATIVE_X:
         vLookatPt = VectorF( -1.0f, 0.0f, 0.0f );
         vUpVec    = VectorF( 0.0f, 1.0f, 0.0f );
         break;
      case 2 : // D3DCUBEMAP_FACE_POSITIVE_Y:
         vLookatPt = VectorF( 0.0f, 1.0f, 0.0f );
         vUpVec    = VectorF( 0.0f, 0.0f,-1.0f );
         break;
      case 3 : // D3DCUBEMAP_FACE_NEGATIVE_Y:
         vLookatPt = VectorF( 0.0f, -1.0f, 0.0f );
         vUpVec    = VectorF( 0.0f, 0.0f, 1.0f );
         break;
      case 4 : // D3DCUBEMAP_FACE_POSITIVE_Z:
         vLookatPt = VectorF( 0.0f, 0.0f, 1.0f );
         vUpVec    = VectorF( 0.0f, 1.0f, 0.0f );
         break;
      case 5: // D3DCUBEMAP_FACE_NEGATIVE_Z:
         vLookatPt = VectorF( 0.0f, 0.0f, -1.0f );
         vUpVec    = VectorF( 0.0f, 1.0f, 0.0f );
         break;
      }

      // create camera matrix
      VectorF cross = mCross( vUpVec, vLookatPt );
      cross.normalizeSafe();

      MatrixF matView(true);
      matView.setColumn( 0, cross );
      matView.setColumn( 1, vLookatPt );
      matView.setColumn( 2, vUpVec );
      matView.setPosition( pos );
      matView.inverse();

      GFX->pushWorldMatrix();
      GFX->setWorldMatrix(matView);

      mRenderTarget->attachTexture( GFXTextureTarget::Color0, mCubemap, i );
      GFX->setActiveRenderTarget( mRenderTarget );
      GFX->clear( GFXClearStencil | GFXClearTarget | GFXClearZBuffer, ColorI( 64, 64, 64 ), 1.f, 0 );

      // render scene
      sm->renderScene( SPT_Reflect, mDynamicObjectTypeMask );

      // Resolve render target for each face
      mRenderTarget->resolve();
      GFX->popWorldMatrix();
   }

   // restore render surface and depth buffer
   GFX->popActiveRenderTarget();

   mRenderTarget->attachTexture(GFXTextureTarget::Color0, NULL);
   sm->setVisibleDistance(oldVisibleDist);

   GFX->setFrustum( l, r, b, t, n, f );
}

void CubemapData::updateFaces()
{
	bool initSuccess = true;

	for( U32 i=0; i<6; i++ )
   {
      if( !mCubeFaceFile[i].isEmpty() )
      {
         if(!mCubeFace[i].set(mCubeFaceFile[i], &GFXDefaultStaticDiffuseProfile, avar("%s() - mCubeFace[%d] (line %d)", __FUNCTION__, i, __LINE__) ))
         {
				initSuccess = false;
            Con::errorf("CubemapData::createMap - Failed to load texture '%s'", mCubeFaceFile[i].c_str());
         }
      }
   }

	if( initSuccess )
	{
		mCubemap = NULL;
		mCubemap = GFX->createCubemap();

		mCubemap->initStatic( mCubeFace );
	}
}

ConsoleMethod( CubemapData, updateFaces, void, 2, 2, 
              "updateFaces(): Update the newly assigned cubemaps faces." )
{
	object->updateFaces();
}

ConsoleMethod(CubemapData, getFilename, const char*, 2, 2, "Get filename of CubemapData")
{
	SimObject *cubemap = static_cast<SimObject *>(object);
   return cubemap->getFilename();
}