//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/singleLightShadowMap.h"
#include "lighting/shadowMap/shadowMapManager.h"
#include "lighting/common/lightMapParams.h"
#include "console/console.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
//#include "scene/sceneReflectPass.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "renderInstance/renderPassManager.h"

SingleLightShadowMap::SingleLightShadowMap(  LightInfo *light )
   :  LightShadowMap( light )
{
}

SingleLightShadowMap::~SingleLightShadowMap()
{
   releaseTextures();
}

void SingleLightShadowMap::_render( SceneGraph *sceneManager, 
                                    const SceneState *diffuseState )
{
   PROFILE_SCOPE(SingleLightShadowMap_render);

   const ShadowMapParams *params = mLight->getExtended<ShadowMapParams>();
   const LightMapParams *lmParams = mLight->getExtended<LightMapParams>();
   const bool bUseLightmappedGeometry = lmParams ? !lmParams->representedInLightmap || lmParams->includeLightmappedGeometryInShadow : true;

   if (  mShadowMapTex.isNull() ||
         mTexSize != params->texSize )
   {
      mTexSize = params->texSize;

      mShadowMapTex.set(   mTexSize, mTexSize, 
                           ShadowMapFormat, &ShadowMapProfile, 
                           "SingleLightShadowMap" );
   }

   GFXTransformSaver saver;
   F32 left, right, bottom, top, nearPlane, farPlane;
   bool isOrtho;
   GFX->getFrustum(&left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho);

   MatrixF lightMatrix;
   calcLightMatrices(lightMatrix);
   lightMatrix.inverse();
   GFX->setWorldMatrix(lightMatrix);

   const MatrixF& lightProj = GFX->getProjectionMatrix();
   mWorldToLightProj = lightProj * lightMatrix;

   // Render the shadowmap!
   GFX->pushActiveRenderTarget();
   mTarget->attachTexture( GFXTextureTarget::Color0, mShadowMapTex );
   mTarget->attachTexture( GFXTextureTarget::DepthStencil, 
      _getDepthTarget( mShadowMapTex->getWidth(), mShadowMapTex->getHeight() ) );
   GFX->setActiveRenderTarget(mTarget);
   GFX->clear(GFXClearStencil | GFXClearZBuffer | GFXClearTarget, ColorI(255,255,255), 1.0f, 0);

   SceneState* baseState = sceneManager->createBaseState( SPT_Shadow );
   baseState->mRenderNonLightmappedMeshes = true;
   baseState->mRenderLightmappedMeshes = bUseLightmappedGeometry;
   baseState->setDiffuseCameraTransform( diffuseState->getCameraTransform() );
   baseState->setViewportExtent( diffuseState->getViewportExtent() );
   baseState->setWorldToScreenScale( diffuseState->getWorldToScreenScale() );

   sceneManager->renderScene(baseState);

   delete baseState;

   mTarget->resolve();
   GFX->popActiveRenderTarget();

   // Restore frustum
   if (!isOrtho)
      GFX->setFrustum(left, right, bottom, top, nearPlane, farPlane);
   else
      GFX->setOrtho(left, right, bottom, top, nearPlane, farPlane);

   //   showmap(mShadowMapTex);
}

void SingleLightShadowMap::setShaderParameters(GFXShaderConstBuffer* params, LightingShaderConstants* lsc)
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
