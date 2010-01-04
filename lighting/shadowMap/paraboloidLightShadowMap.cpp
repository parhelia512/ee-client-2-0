//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/paraboloidLightShadowMap.h"
#include "lighting/common/lightMapParams.h"
#include "lighting/shadowMap/shadowMapManager.h"
#include "math/mathUtils.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
//#include "scene/sceneReflectPass.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "renderInstance/renderPassManager.h"
#include "materials/materialDefinition.h"
#include "gui/controls/guiBitmapCtrl.h"

ParaboloidLightShadowMap::ParaboloidLightShadowMap( LightInfo *light )
   :  Parent( light ),
      mShadowMapScale( 1, 1 ),
      mShadowMapOffset( 0, 0 )
{   
}

ParaboloidLightShadowMap::~ParaboloidLightShadowMap()
{
   releaseTextures();
}

ShadowType ParaboloidLightShadowMap::getShadowType() const
{
   const ShadowMapParams *params = mLight->getExtended<ShadowMapParams>();
   return params->shadowType;
}

void ParaboloidLightShadowMap::setShaderParameters(GFXShaderConstBuffer* params, LightingShaderConstants* lsc)
{
   if ( lsc->mTapRotationTexSC->isValid() )
      GFX->setTexture( lsc->mTapRotationTexSC->getSamplerRegister(), 
                        SHADOWMGR->getTapRotationTex() );

   ShadowMapParams *p = mLight->getExtended<ShadowMapParams>();
   if ( lsc->mLightParamsSC->isValid() )
   {
      Point4F lightParams( mLight->getRange().x, p->overDarkFactor.x, 0.0f, 0.0f);
      params->set( lsc->mLightParamsSC, lightParams );
   }

   // Atlasing parameters (only used in the dual case, set here to use same shaders)
   if ( lsc->mAtlasScaleSC->isValid() )
      params->set( lsc->mAtlasScaleSC, mShadowMapScale );

   if( lsc->mAtlasXOffsetSC->isValid() )
      params->set( lsc->mAtlasXOffsetSC, mShadowMapOffset );

   // The softness is a factor of the texel size.
   if ( lsc->mShadowSoftnessConst->isValid() )
      params->set( lsc->mShadowSoftnessConst, p->shadowSoftness * ( 1.0f / mTexSize ) );
}

void ParaboloidLightShadowMap::_render(   SceneGraph *sceneManager, 
                                          const SceneState *diffuseState )
{
   PROFILE_SCOPE(ParaboloidLightShadowMap_render);

   const ShadowMapParams *p = mLight->getExtended<ShadowMapParams>();
   const LightMapParams *lmParams = mLight->getExtended<LightMapParams>();
   const bool bUseLightmappedGeometry = lmParams ? !lmParams->representedInLightmap || lmParams->includeLightmappedGeometryInShadow : true;

   if (  mShadowMapTex.isNull() || 
         mTexSize != p->texSize )
   {
      mTexSize = p->texSize;

      mShadowMapTex.set(   mTexSize, mTexSize, 
                           ShadowMapFormat, &ShadowMapProfile, 
                           "ParaboloidLightShadowMap" );
   }

   GFXTransformSaver saver;
   F32 left, right, bottom, top, nearPlane, farPlane;
   bool isOrtho;
   GFX->getFrustum(&left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho);

   // Render the shadowmap!
   GFX->pushActiveRenderTarget();

   // Calc matrix and set up visible distance
   mWorldToLightProj = mLight->getTransform();
   mWorldToLightProj.inverse();
   GFX->setWorldMatrix(mWorldToLightProj);

   const F32 &lightRadius = mLight->getRange().x;
   GFX->setOrtho(-lightRadius, lightRadius, -lightRadius, lightRadius, 1.0f, lightRadius, true);

   // Set up target
   mTarget->attachTexture( GFXTextureTarget::Color0, mShadowMapTex );
   mTarget->attachTexture( GFXTextureTarget::DepthStencil, 
      _getDepthTarget( mShadowMapTex->getWidth(), mShadowMapTex->getHeight() ) );
   GFX->setActiveRenderTarget(mTarget);
   GFX->clear(GFXClearTarget | GFXClearStencil | GFXClearZBuffer, ColorI(255,255,255,255), 1.0f, 0);

   // Create scene state, prep it
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
}