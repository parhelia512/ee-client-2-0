//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/dualParaboloidLightShadowMap.h"
#include "lighting/common/lightMapParams.h"
#include "lighting/shadowMap/shadowMapManager.h"
#include "math/mathUtils.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxDebugEvent.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "renderInstance/renderPassManager.h"
#include "materials/materialDefinition.h"
#include "math/util/matrixSet.h"

DualParaboloidLightShadowMap::DualParaboloidLightShadowMap( LightInfo *light )
   : Parent( light )
{
}

bool DualParaboloidLightShadowMap::setTextureStage(   const SceneGraphData &sgData, 
                                                      const U32 currTexFlag,
                                                      const U32 textureSlot, 
                                                      GFXShaderConstBuffer *shaderConsts,
                                                      GFXShaderConstHandle *shadowMapSC )
{
   switch (currTexFlag)
   {
      case Material::DynamicLight:
      {
         GFX->setTexture(textureSlot, mShadowMapTex);
         return true;
      }
      break;
   }

   return false;
}

void DualParaboloidLightShadowMap::_render(  SceneGraph *sceneManager, 
                                             const SceneState *diffuseState )
{
   PROFILE_SCOPE(DualParaboloidLightShadowMap_render);

   const ShadowMapParams *p = mLight->getExtended<ShadowMapParams>();
   const LightMapParams *lmParams = mLight->getExtended<LightMapParams>();
   const bool bUseLightmappedGeometry = lmParams ? !lmParams->representedInLightmap || lmParams->includeLightmappedGeometryInShadow : true;

   if (  mShadowMapTex.isNull() || 
         mTexSize != p->texSize )
   {
      mTexSize = p->texSize;

      mShadowMapTex.set(   mTexSize * 2, mTexSize, 
                           ShadowMapFormat, &ShadowMapProfile, 
                           "DualParaboloidLightShadowMap" );
   }

   GFXTransformSaver saver;
   F32 left, right, bottom, top, nearPlane, farPlane;
   bool isOrtho;
   GFX->getFrustum(&left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho);

   // Set and Clear target
   GFX->pushActiveRenderTarget();

   mTarget->attachTexture(GFXTextureTarget::Color0, mShadowMapTex);
   mTarget->attachTexture( GFXTextureTarget::DepthStencil, 
      _getDepthTarget( mShadowMapTex->getWidth(), mShadowMapTex->getHeight() ) );
   GFX->setActiveRenderTarget(mTarget);
   GFX->clear(GFXClearTarget | GFXClearStencil | GFXClearZBuffer, ColorI::WHITE, 1.0f, 0);

   const bool bUseSinglePassDPM = (p->shadowType == ShadowType_DualParaboloidSinglePass);

   // Set up matrix and visible distance
   mWorldToLightProj = mLight->getTransform();
   mWorldToLightProj.inverse();

   const F32 &lightRadius = mLight->getRange().x;
   const F32 paraboloidNearPlane = 0.01f;
   const F32 renderPosOffset = 0.01f;
   
   // Alter for creation of scene state if this is a single pass map
   if(bUseSinglePassDPM)
   {
      VectorF camDir;
      MatrixF temp = mLight->getTransform();
      temp.getColumn(1, &camDir);
      temp.setPosition(mLight->getPosition() - camDir * (lightRadius + renderPosOffset));
      temp.inverse();
      GFX->setWorldMatrix(temp);
      GFX->setOrtho(-lightRadius, lightRadius, -lightRadius, lightRadius, paraboloidNearPlane, 2.0f * lightRadius, true);
   }
   else
   {
      VectorF camDir;
      MatrixF temp = mLight->getTransform();
      temp.getColumn(1, &camDir);
      temp.setPosition(mLight->getPosition() - camDir * renderPosOffset);
      temp.inverse();
      GFX->setWorldMatrix(temp);

      GFX->setOrtho(-lightRadius, lightRadius, -lightRadius, lightRadius, paraboloidNearPlane, lightRadius, true);
   }

   // Set up a scene state
   SceneState *baseState = sceneManager->createBaseState( SPT_Shadow );
   baseState->mRenderNonLightmappedMeshes = true;
   baseState->mRenderLightmappedMeshes = bUseLightmappedGeometry;
   baseState->setDiffuseCameraTransform( diffuseState->getCameraTransform() );
   baseState->setViewportExtent( diffuseState->getViewportExtent() );
   baseState->setWorldToScreenScale( diffuseState->getWorldToScreenScale() );

   if(bUseSinglePassDPM)
   {
      GFX->setWorldMatrix(mWorldToLightProj);
      baseState->getRenderPass()->getMatrixSet().setSceneView(mWorldToLightProj);
      GFX->setOrtho(-lightRadius, lightRadius, -lightRadius, lightRadius, paraboloidNearPlane, lightRadius, true);
   }

   // Front map render
   {
      GFXDEBUGEVENT_SCOPE( DualParaboloidLightShadowMap_Render_FrontFacingParaboloid, ColorI::RED );
      mShadowMapScale.set(0.5f, 1.0f);
      mShadowMapOffset.set(-0.5f, 0.0f);
      sceneManager->renderScene(baseState);
   }
   
   // Back map render 
   if(!bUseSinglePassDPM)
   {
      GFXDEBUGEVENT_SCOPE( DualParaboloidLightShadowMap_Render_BackFacingParaboloid, ColorI::RED );

      mShadowMapScale.set(0.5f, 1.0f);
      mShadowMapOffset.set(0.5f, 0.0f);

      // Invert direction on camera matrix
      VectorF right, forward;
      MatrixF temp = mLight->getTransform();
      temp.getColumn( 1, &forward );
      temp.getColumn( 0, &right );
      forward *= -1.0f;      
      right *= -1.0f;
      temp.setColumn( 1, forward );
      temp.setColumn( 0, right );
      temp.setPosition(mLight->getPosition() - forward * -renderPosOffset);
      temp.inverse();
      GFX->setWorldMatrix(temp);

      // Create an inverted scene state for the back-map
      delete baseState;
      baseState = sceneManager->createBaseState( SPT_Shadow );
      baseState->mRenderNonLightmappedMeshes = true;
      baseState->mRenderLightmappedMeshes = bUseLightmappedGeometry;
      baseState->setDiffuseCameraTransform( diffuseState->getCameraTransform() );
      baseState->setViewportExtent( diffuseState->getViewportExtent() );
      baseState->setWorldToScreenScale( diffuseState->getWorldToScreenScale() );

      baseState->getRenderPass()->getMatrixSet().setSceneView(temp);

      // Draw scene
      sceneManager->renderScene(baseState);
   }

   // Clean up
   delete baseState;

   mTarget->resolve();
   GFX->popActiveRenderTarget();

   // Restore frustum
   if (!isOrtho)
      GFX->setFrustum(left, right, bottom, top, nearPlane, farPlane);
   else
      GFX->setOrtho(left, right, bottom, top, nearPlane, farPlane);
}
