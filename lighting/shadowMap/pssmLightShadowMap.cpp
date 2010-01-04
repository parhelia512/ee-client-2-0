//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/pssmLightShadowMap.h"
#include "lighting/common/lightMapParams.h"
#include "console/console.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "lighting/lightManager.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "renderInstance/renderPassManager.h"
#include "gui/controls/guiBitmapCtrl.h"
#include "lighting/shadowMap/blurTexture.h"
#include "lighting/shadowMap/shadowMapManager.h"
#include "materials/shaderData.h"
#include "ts/tsShapeInstance.h"

PSSMLightShadowMap::PSSMLightShadowMap( LightInfo *light )
   :  LightShadowMap( light ),
      mNumSplits( 0 )
{  
   mIsViewDependent = true;
}

void PSSMLightShadowMap::_setNumSplits( U32 numSplits, U32 texSize )
{
   AssertFatal( numSplits > 0 && numSplits <= MAX_SPLITS, 
      "PSSMLightShadowMap::_setNumSplits() - Splits must be between 1 and 4!" );

   releaseTextures();
  
   mNumSplits = numSplits;
   mTexSize = texSize;
   F32 texWidth, texHeight;

   // If the split count is less than 4 then do a
   // 1xN layout of shadow maps...
   if ( mNumSplits < 4 )
   {
      texHeight = texSize;
      texWidth = texSize * mNumSplits;

      for ( U32 i = 0; i < 4; i++ )
      {
         mViewports[i].extent.set(texSize, texSize);
         mViewports[i].point.set(texSize*i, 0);
      }
   } 
   else 
   {
      // ... with 4 splits do a 2x2.
      texWidth = texHeight = texSize * 2;

      for ( U32 i = 0; i < 4; i++ )
      {
         F32 xOff = (i == 1 || i == 3) ? 0.5f : 0.0f;
         F32 yOff = (i > 1) ? 0.5f : 0.0f;
         mViewports[i].extent.set( texSize, texSize );
         mViewports[i].point.set( xOff * texWidth, yOff * texHeight );
      }
   }

   mShadowMapTex.set(   texWidth, texHeight, 
                        ShadowMapFormat, &ShadowMapProfile, 
                        "PSSMLightShadowMap" );
}

void PSSMLightShadowMap::_calcSplitPos(const Frustum& currFrustum)
{
   const F32 nearDist = 0.01f; //currFrustum.getNearDist();
   const F32 farDist = currFrustum.getFarDist();
   
   // TODO: I like this split scheme alot... but it
   // only looks good for 4 splits.
   /*
   mSplitDist[0] = near;
   mSplitDist[mNumSplits] = far;

   F32 factor = 1.0f / ( mNumSplits - 1 );
   for ( S32 i=mNumSplits-1; i > 0; i-- )
      mSplitDist[i] = mSplitDist[i+1] * factor;
   */

   for ( U32 i = 1; i < mNumSplits; i++ )
   {
      F32 step = (F32) i / (F32) mNumSplits;
      F32 logSplit = nearDist * mPow(farDist / nearDist, step);
      F32 linearSplit = nearDist + (farDist - nearDist) * step;
      mSplitDist[i] = mLerp( linearSplit, logSplit, mClampF( mLogWeight, 0.0f, 1.0f ) );
   }

   mSplitDist[0] = nearDist;
   mSplitDist[mNumSplits] = farDist;
}

Box3F PSSMLightShadowMap::_calcClipSpaceAABB(const Frustum& f, const MatrixF& transform, F32 farDist)
{
   Box3F result;
   for (U32 i = 0; i < 8; i++)
   {
      const Point3F& pt = f.getPoints()[i];
      // Lets just translate to lightspace

      // We need the Point4F so that we can project the w
      Point4F xformed(pt.x, pt.y, pt.z, 1.0f);
      transform.mul(xformed);
      F32 absW = mFabs(xformed.w);
      xformed.x /= absW;
      xformed.y /= absW;
      xformed.z /= absW;
      Point3F xformed3(xformed.x, xformed.y, xformed.z);
      if (i != 0)
      {
         result.minExtents.setMin(xformed3);
         result.maxExtents.setMax(xformed3);
      } else {
         result.minExtents = xformed3;
         result.maxExtents = xformed3;
      }
   }

   result.minExtents.x = mClampF(result.minExtents.x, -1.0f, 1.0f);
   result.minExtents.y = mClampF(result.minExtents.y, -1.0f, 1.0f);
   result.maxExtents.x = mClampF(result.maxExtents.x, -1.0f, 1.0f);
   result.maxExtents.y = mClampF(result.maxExtents.y, -1.0f, 1.0f);

   return result;
}

// This "rounds" the projection matrix to remove subtexel movement during shadow map
// rasterization.  This is here to reduce shadow shimmering.
void PSSMLightShadowMap::_roundProjection(const MatrixF& lightMat, const MatrixF& cropMatrix, Point3F &offset, U32 splitNum)
{
   // Round to the nearest shadowmap texel, this helps reduce shimmering
   MatrixF currentProj = GFX->getProjectionMatrix();
   currentProj = cropMatrix * currentProj * lightMat;

   // Project origin to screen.
   Point4F originShadow4F(0,0,0,1);
   currentProj.mul(originShadow4F);
   Point2F originShadow(originShadow4F.x / originShadow4F.w, originShadow4F.y / originShadow4F.w);   

   // Convert to texture space (0..shadowMapSize)
   F32 t = mShadowMapTex->getWidth() / mNumSplits;
   Point2F texelsToTexture(t / 2.0f, mShadowMapTex->getHeight() / 2.0f);
   originShadow.convolve(texelsToTexture);   

   // Clamp to texel boundary
   Point2F originRounded;
   originRounded.x = mFloor(originShadow.x + 0.5f);
   originRounded.y = mFloor(originShadow.y + 0.5f);

   // Subtract origin to get an offset to recenter everything on texel boundaries
   originRounded -= originShadow;

   // Convert back to texels (0..1) and offset
   originRounded.convolveInverse(texelsToTexture);
   offset.x += originRounded.x;
   offset.y += originRounded.y;
}

void PSSMLightShadowMap::_render(   SceneGraph* sceneManager, 
                                    const SceneState *diffuseState )
{
   PROFILE_SCOPE(PSSMLightShadowMap_render);

   const ShadowMapParams *params = mLight->getExtended<ShadowMapParams>();
   const LightMapParams *lmParams = mLight->getExtended<LightMapParams>();
   const bool bUseLightmappedGeometry = lmParams ? !lmParams->representedInLightmap || lmParams->includeLightmappedGeometryInShadow : true;

   if (  mShadowMapTex.isNull() || 
         mNumSplits != params->numSplits || 
         mTexSize != params->texSize )
      _setNumSplits( params->numSplits, params->texSize );
   mLogWeight = params->logWeight;

   Frustum fullFrustum(_getFrustum());
   fullFrustum.cropNearFar(fullFrustum.getNearDist(), params->shadowDistance);

   GFXTransformSaver saver;
   F32 left, right, bottom, top, nearPlane, farPlane;
   bool isOrtho;
   GFX->getFrustum(&left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho);

   // Set our render target
   GFX->pushActiveRenderTarget();
   mTarget->attachTexture( GFXTextureTarget::Color0, mShadowMapTex );
   mTarget->attachTexture( GFXTextureTarget::DepthStencil, 
      _getDepthTarget( mShadowMapTex->getWidth(), mShadowMapTex->getHeight() ) );
   GFX->setActiveRenderTarget( mTarget );
   GFX->clear( GFXClearStencil | GFXClearZBuffer | GFXClearTarget, ColorI(255,255,255), 1.0f, 0 );

   // Calculate our standard light matrices
   MatrixF lightMatrix;
   calcLightMatrices(lightMatrix);
   lightMatrix.inverse();
   MatrixF lightViewProj = GFX->getProjectionMatrix() * lightMatrix;

   F32 pleft, pright, pbottom, ptop, pnear, pfar;
   bool pisOrtho;
   GFX->getFrustum(&pleft, &pright, &pbottom, &ptop, &pnear, &pfar,&pisOrtho);

   // Set our view up
   GFX->setWorldMatrix(lightMatrix);
   MatrixF toLightSpace = lightMatrix; // * invCurrentView;

   _calcSplitPos(fullFrustum);
   
   mWorldToLightProj = GFX->getProjectionMatrix() * toLightSpace;

   //TSShapeInstance::smSmallestVisiblePixelSize = 25.0f;
   //F32 savedDetailAdjust = TSShapeInstance::smDetailAdjust;

   for (U32 i = 0; i < mNumSplits; i++)
   {
      GFXTransformSaver saver;

      // Calculate a sub-frustum
      Frustum subFrustum(fullFrustum);
      subFrustum.cropNearFar(mSplitDist[i], mSplitDist[i+1]);

      // Calculate our AABB in the light's clip space.
      Box3F clipAABB = _calcClipSpaceAABB(subFrustum, lightViewProj, fullFrustum.getFarDist());
 
      // Calculate our crop matrix
      Point3F scale(2.0f / (clipAABB.maxExtents.x - clipAABB.minExtents.x),
         2.0f / (clipAABB.maxExtents.y - clipAABB.minExtents.y),
         1.0f);

      // TODO: This seems to produce less "pops" of the
      // shadow resolution as the camera spins around and
      // it should produce pixels that are closer to being
      // square.
      //
      // Still is it the right thing to do?
      //
      scale.y = scale.x = ( getMin( scale.x, scale.y ) ); 
      //scale.x = mFloor(scale.x); 
      //scale.y = mFloor(scale.y); 

      Point3F offset(-0.5f * (clipAABB.maxExtents.x + clipAABB.minExtents.x) * scale.x,
         -0.5f * (clipAABB.maxExtents.y + clipAABB.minExtents.y) * scale.y,
         0.0f);

      MatrixF cropMatrix(true);
      cropMatrix.scale(scale);
      cropMatrix.setPosition(offset);

      _roundProjection(lightMatrix, cropMatrix, offset, i);

      cropMatrix.setPosition(offset);      

      // Save scale/offset for shader computations
      mScaleProj[i].set(scale);
      mOffsetProj[i].set(offset);

      // Adjust the far plane to the max z we got (maybe add a little to deal with split overlap)
      {
         F32 left, right, bottom, top, nearDist, farDist;
         bool isOrtho;
         GFX->getFrustum(&left, &right, &bottom, &top, &nearDist, &farDist,&isOrtho);
         // BTRTODO: Fix me!
         farDist = clipAABB.maxExtents.z;
         if (!isOrtho)
            GFX->setFrustum(left, right, bottom, top, nearDist, farDist);

         else
         {
            // Calculate a new far plane, add a fudge factor to avoid bringing
            // the far plane in too close.
            F32 newFar = pfar * clipAABB.maxExtents.z + 1.0f;
            mFarPlaneScalePSSM[i] = (pfar - pnear) / (newFar - pnear);
            GFX->setOrtho(left, right, bottom, top, pnear, newFar, true);
         }
      }

      // Crop matrix multiply needs to be post-projection.
      MatrixF alightProj = GFX->getProjectionMatrix();
      alightProj = cropMatrix * alightProj;

      // Set our new projection
      GFX->setProjectionMatrix(alightProj);

      // Render into the quad of the shadow map we are using.
      GFX->setViewport(mViewports[i]);

      // Setup the scene state and use the diffuse state
      // camera position and screen metrics values so that
      // lod is done the same as in the diffuse pass.
      SceneState *baseState = sceneManager->createBaseState( SPT_Shadow );
      baseState->mRenderNonLightmappedMeshes = true;
      baseState->mRenderLightmappedMeshes = bUseLightmappedGeometry;

      /*
      if ( i != 0 )
      {
         // The idea here is to set a frustum optimal for culling
         // out anything that isn't in this split and stuff behind
         // the camera.
         //
         // So far it seems to not work well... things get cut out
         // from a split when they shouldn't.

         const Frustum &diffuseFrust = diffuseState->getFrustum();
         MatrixF camXfm( diffuseFrust.getTransform() );
         camXfm.setPosition( camXfm.getPosition() - camXfm.getForwardVector() * 5.0f );

         Frustum cullFrustum;
         cullFrustum.set(  diffuseFrust.isOrtho(),
                           diffuseFrust.getNearLeft(),
                           diffuseFrust.getNearRight(),
                           diffuseFrust.getNearTop(),
                           diffuseFrust.getNearBottom(),
                           diffuseFrust.getNearDist(),
                           mSplitDist[i+1] + 5.0f,
                           camXfm );

         baseState->setFrustum( cullFrustum );      
      }
      */

      baseState->setDiffuseCameraTransform( diffuseState->getCameraTransform() );
      baseState->setViewportExtent( diffuseState->getViewportExtent() );
      baseState->setWorldToScreenScale( diffuseState->getWorldToScreenScale() );

      U32 objectMask;
      if ( i == mNumSplits-1 && params->lastSplitTerrainOnly )
         objectMask = TerrainObjectType;
      else
         objectMask = ShadowCasterObjectType | StaticRenderedObjectType | ShapeBaseObjectType;

      sceneManager->renderScene( baseState, objectMask );
      //TSShapeInstance::smDetailAdjust *= 0.75f;
   
      delete baseState;
   }

   //TSShapeInstance::smSmallestVisiblePixelSize = -1;
   //TSShapeInstance::smDetailAdjust = savedDetailAdjust;

   // Release our render target
   mTarget->resolve();
   GFX->popActiveRenderTarget();
   
   // Cleanup (restore frustum, clear render instance, kill scene state)
   if (!isOrtho)
      GFX->setFrustum(left, right, bottom, top, nearPlane, farPlane);
   else
      GFX->setOrtho(left, right, bottom, top, nearPlane, farPlane);
}

void PSSMLightShadowMap::setShaderParameters(GFXShaderConstBuffer* params, LightingShaderConstants* lsc)
{
   if ( lsc->mTapRotationTexSC->isValid() )
      GFX->setTexture( lsc->mTapRotationTexSC->getSamplerRegister(), 
                        SHADOWMGR->getTapRotationTex() );

   const ShadowMapParams *p = mLight->getExtended<ShadowMapParams>();

   Point2F shadowMapAtlas;
   if (mNumSplits < 4)
   {
      shadowMapAtlas.x = 1.0f / (F32) mNumSplits;
      shadowMapAtlas.y = 1.0f;
   }
   else
      shadowMapAtlas.set(0.5f, 0.5f);

   // Split start, split end
   Point4F sStart(Point4F::Zero), sEnd(Point4F::Zero), sx(Point4F::Zero), sy(Point4F::Zero);
   Point4F ox(Point4F::Zero), oy(Point4F::Zero), aXOff(Point4F::Zero), aYOff(Point4F::Zero);

   for (U32 i = 0; i < mNumSplits; i++)
   {
      sStart[i] = mSplitDist[i];
      sEnd[i] = mSplitDist[i+1];
      sx[i] = mScaleProj[i].x;
      sy[i] = mScaleProj[i].y;
      ox[i] = mOffsetProj[i].x;
      oy[i] = mOffsetProj[i].y;
   }

   if (mNumSplits < 4)
   {
      // 1xmNumSplits
      for (U32 i = 0; i < mNumSplits; i++)
         aXOff[i] = (F32)i * shadowMapAtlas.x;
   } else {
      // 2x2
      for (U32 i = 0; i < mNumSplits; i++)
      {
         if (i == 1 || i == 3)
            aXOff[i] = 0.5f;
         if (i > 1)
            aYOff[i] = 0.5f;
      }
   }

   params->set(lsc->mSplitStartSC, sStart);
   params->set(lsc->mSplitEndSC, sEnd);
   params->set(lsc->mScaleXSC, sx);
   params->set(lsc->mScaleYSC, sy);
   params->set(lsc->mOffsetXSC, ox);
   params->set(lsc->mOffsetYSC, oy);
   params->set(lsc->mAtlasXOffsetSC, aXOff);
   params->set(lsc->mAtlasYOffsetSC, aYOff);
   params->set(lsc->mAtlasScaleSC, shadowMapAtlas);

   Point4F lightParams( mLight->getRange().x, p->overDarkFactor.x, 0.0f, 0.0f );
   params->set( lsc->mLightParamsSC, lightParams );
      
   params->set( lsc->mFarPlaneScalePSSM, mFarPlaneScalePSSM);

   Point2F fadeStartLength(p->fadeStartDist, 0.0f);
   if (fadeStartLength.x == 0.0f)
   {
      // By default, lets fade the last half of the last split.
      fadeStartLength.x = (mSplitDist[mNumSplits-1] + mSplitDist[mNumSplits]) / 2.0f;
   }
   fadeStartLength.y = 1.0f / (mSplitDist[mNumSplits] - fadeStartLength.x);
   params->set( lsc->mFadeStartLength, fadeStartLength);
   
   params->set( lsc->mOverDarkFactorPSSM, p->overDarkFactor);
   params->set( lsc->mSplitFade, p->splitFadeDistances);

  // The softness is a factor of the texel size.
   if ( lsc->mShadowSoftnessConst )
      params->set( lsc->mShadowSoftnessConst, p->shadowSoftness * ( 1.0f / mTexSize ) );
}
