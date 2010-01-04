//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/cubeLightShadowMap.h"

#include "lighting/shadowMap/shadowMapManager.h"
#include "lighting/common/lightMapParams.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDebugEvent.h"
#include "renderInstance/renderPassManager.h"
#include "materials/materialDefinition.h"


CubeLightShadowMap::CubeLightShadowMap( LightInfo *light )
   : Parent( light )
{
}

bool CubeLightShadowMap::setTextureStage( const SceneGraphData &sgData, 
                                          const U32 currTexFlag,
                                          const U32 textureSlot, 
                                          GFXShaderConstBuffer *shaderConsts,
                                          GFXShaderConstHandle *shadowMapSC )
{
   switch (currTexFlag)
   {
   case Material::DynamicLight:
      {
         if(shaderConsts && shadowMapSC->isValid())
            shaderConsts->set(shadowMapSC, (S32)textureSlot);
         GFX->setCubeTexture( textureSlot, mCubemap );
         return true;
      }
      break;
   }
   return false;
}

void CubeLightShadowMap::setShaderParameters(   GFXShaderConstBuffer *params, 
                                                LightingShaderConstants *lsc )
{
   if ( lsc->mTapRotationTexSC->isValid() )
      GFX->setTexture( lsc->mTapRotationTexSC->getSamplerRegister(), 
                        SHADOWMGR->getTapRotationTex() );

   ShadowMapParams *p = mLight->getExtended<ShadowMapParams>();

   if ( lsc->mLightParamsSC->isValid() )
   {
      Point4F lightParams( mLight->getRange().x, 
                           p->overDarkFactor.x, 
                           0.0f, 
                           0.0f );
      params->set(lsc->mLightParamsSC, lightParams);
   }

   // The softness is a factor of the texel size.
   if ( lsc->mShadowSoftnessConst->isValid() )
      params->set( lsc->mShadowSoftnessConst, p->shadowSoftness * ( 1.0f / mTexSize ) );
}

void CubeLightShadowMap::releaseTextures()
{
   Parent::releaseTextures();
   mCubemap = NULL;
}

void CubeLightShadowMap::_render(   SceneGraph *sceneManager, 
                                    const SceneState *diffuseState )
{
   PROFILE_SCOPE( CubeLightShadowMap_Render );

   const ShadowMapParams *p = mLight->getExtended<ShadowMapParams>();
   const LightMapParams *lmParams = mLight->getExtended<LightMapParams>();
   const bool bUseLightmappedGeometry = lmParams ? !lmParams->representedInLightmap || lmParams->includeLightmappedGeometryInShadow : true;

   if (  mCubemap.isNull() || 
         mTexSize != p->texSize )
   {
      mTexSize = p->texSize;
      mCubemap = GFX->createCubemap();
      mCubemap->initDynamic( mTexSize, LightShadowMap::ShadowMapFormat );
   }

   GFXTransformSaver saver;
   F32 left, right, bottom, top, nearPlane, farPlane;
   bool isOrtho;
   GFX->getFrustum(&left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho);

   // Set up frustum and visible distance
   GFX->setFrustum( 90.0f, 1.0f, 0.1f, mLight->getRange().x );

   // Render the shadowmap!
   GFX->pushActiveRenderTarget();

   for( U32 i = 0; i < 6; i++ )
   {
      // Standard view that will be overridden below.
      VectorF vLookatPt(0.0f, 0.0f, 0.0f), vUpVec(0.0f, 0.0f, 0.0f), vRight(0.0f, 0.0f, 0.0f);

      switch( i )
      {
      case 0 : // D3DCUBEMAP_FACE_POSITIVE_X:
         vLookatPt = VectorF(1.0f, 0.0f, 0.0f);
         vUpVec    = VectorF(0.0f, 1.0f, 0.0f);
         break;
      case 1 : // D3DCUBEMAP_FACE_NEGATIVE_X:
         vLookatPt = VectorF(-1.0f, 0.0f, 0.0f);
         vUpVec    = VectorF(0.0f, 1.0f, 0.0f);
         break;
      case 2 : // D3DCUBEMAP_FACE_POSITIVE_Y:
         vLookatPt = VectorF(0.0f, 1.0f, 0.0f);
         vUpVec    = VectorF(0.0f, 0.0f,-1.0f);
         break;
      case 3 : // D3DCUBEMAP_FACE_NEGATIVE_Y:
         vLookatPt = VectorF(0.0f, -1.0f, 0.0f);
         vUpVec    = VectorF(0.0f, 0.0f, 1.0f);
         break;
      case 4 : // D3DCUBEMAP_FACE_POSITIVE_Z:
         vLookatPt = VectorF(0.0f, 0.0f, 1.0f);
         vUpVec    = VectorF(0.0f, 1.0f, 0.0f);
         break;
      case 5: // D3DCUBEMAP_FACE_NEGATIVE_Z:
         vLookatPt = VectorF(0.0f, 0.0f, -1.0f);
         vUpVec    = VectorF(0.0f, 1.0f, 0.0f);
         break;
      }

      GFXDEBUGEVENT_START( CubeLightShadowMap_Render_Face, ColorI::RED );

      // create camera matrix
      VectorF cross = mCross(vUpVec, vLookatPt);
      cross.normalizeSafe();

      MatrixF lightMatrix(true);
      lightMatrix.setColumn(0, cross);
      lightMatrix.setColumn(1, vLookatPt);
      lightMatrix.setColumn(2, vUpVec);
      lightMatrix.setPosition( mLight->getPosition() );
      lightMatrix.inverse();

      GFX->setWorldMatrix( lightMatrix );

      mTarget->attachTexture(GFXTextureTarget::Color0, mCubemap, i);
      mTarget->attachTexture(GFXTextureTarget::DepthStencil, _getDepthTarget( mTexSize, mTexSize ));
      GFX->setActiveRenderTarget(mTarget);
      GFX->clear( GFXClearTarget | GFXClearStencil | GFXClearZBuffer, ColorI(255,255,255,255), 1.0f, 0 );

      // Create scene state, prep it
      SceneState* baseState = sceneManager->createBaseState( SPT_Shadow );
      baseState->mRenderNonLightmappedMeshes = true;
      baseState->mRenderLightmappedMeshes = bUseLightmappedGeometry;
      baseState->setDiffuseCameraTransform( diffuseState->getCameraTransform() );
      baseState->setViewportExtent( diffuseState->getViewportExtent() );
      baseState->setWorldToScreenScale( diffuseState->getWorldToScreenScale() );

      sceneManager->renderScene(baseState);
      delete baseState;

      // Resolve this face
      mTarget->resolve();

      GFXDEBUGEVENT_END();
   }
   GFX->popActiveRenderTarget();

   // Restore frustum
   if ( !isOrtho )
      GFX->setFrustum(left, right, bottom, top, nearPlane, farPlane);
   else
      GFX->setOrtho(left, right, bottom, top, nearPlane, farPlane);
}
