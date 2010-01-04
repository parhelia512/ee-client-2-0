//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/common/projectedShadow.h"

#include "gfx/primBuilder.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/bitmap/gBitmap.h"
#include "gfx/gfxDebugEvent.h"
#include "math/mathUtils.h"
#include "lighting/lightInfo.h"
#include "lighting/lightingInterfaces.h"
#include "T3D/shapeBase.h"
#include "sceneGraph/sceneGraph.h"
#include "lighting/lightManager.h"
//#include "lighting/shadowMap/lightShadowMap.h"
#include "ts/tsMesh.h"
#include "T3D/decal/decalManager.h"
#include "T3D/decal/decalInstance.h"
#include "renderInstance/renderPassManager.h"
#include "renderInstance/renderMeshMgr.h"
#include "gfx/gfxTransformSaver.h"
#include "materials/customMaterialDefinition.h"
#include "materials/materialFeatureTypes.h"
#include "console/console.h"
#include "postFx/postEffect.h"
#include "lighting/basic/basicLightManager.h"
#include "materials/materialManager.h"

SimObjectPtr<RenderPassManager> ProjectedShadow::smRenderPass = NULL;
F32 ProjectedShadow::smDepthAdjust = 10.0f;


GFX_ImplementTextureProfile( BLProjectedShadowProfile,
                              GFXTextureProfile::DiffuseMap,
                              GFXTextureProfile::PreserveSize | 
                              GFXTextureProfile::RenderTarget |
                              GFXTextureProfile::Pooled,
                              GFXTextureProfile::None );

GFX_ImplementTextureProfile( BLProjectedShadowZProfile,
                              GFXTextureProfile::DiffuseMap,
                              GFXTextureProfile::PreserveSize | 
                              GFXTextureProfile::ZTarget |
                              GFXTextureProfile::Pooled,
                              GFXTextureProfile::None );

//--------------------------------------------------------------

ProjectedShadow::ProjectedShadow( SceneObject *object )
{
   mParentObject = object;
   mShapeBase = dynamic_cast<ShapeBase*>( object );

   mRadius = 0;
   mLastRenderTime = 0;
   mUpdateTexture = false;

   mShadowLength = 10.0f;

   mDecalData = new DecalData;
   mDecalData->startPixRadius = 200.0f;
   mDecalData->endPixRadius = 35.0f;

   mDecalInstance = NULL;
   
   mLastLightDir.set( 0, 0, 0 );
   mLastObjectPosition.set( object->getRenderPosition() );
   mLastObjectScale.set( object->getScale() );

   CustomMaterial *customMat = NULL;
   Sim::findObject( "BL_ProjectedShadowMaterial", customMat );
   if ( customMat )
   {
      mDecalData->material = customMat;
      mDecalData->matInst = customMat->createMatInstance();
   }
   else
      mDecalData->matInst = MATMGR->createMatInstance( "WarningMaterial" );

   mDecalData->matInst->init( MATMGR->getDefaultFeatures(), getGFXVertexFormat<GFXVertexPNTT>() );

   mCasterPositionSC = NULL;
   mShadowLengthSC = NULL;
}

ProjectedShadow::~ProjectedShadow()
{
   if ( mDecalInstance )
      gDecalManager->removeDecal( mDecalInstance );

   delete mDecalData;
   
   mShadowTexture = NULL;
   mRenderTarget = NULL;
}

bool ProjectedShadow::shouldRender( const SceneState *state )
{
   // Don't render if our object has been removed from the
   // scene graph.
      
   if( !mParentObject->getSceneGraph() )
      return false;

   // Don't render if the ShapeBase 
   // object's fade value is greater 
   // than the visibility epsilon.
   bool shapeFade = mShapeBase && mShapeBase->getFadeVal() < TSMesh::VISIBILITY_EPSILON;

   // Get the shapebase datablock if we have one.
   ShapeBaseData *data = NULL;
   if ( mShapeBase )
      data = static_cast<ShapeBaseData*>( mShapeBase->getDataBlock() );
   
   // Also don't render if 
   // the camera distance is greater
   // than the shadow length.
   if (  shapeFade || !mDecalData ||
         (  mDecalInstance && 
            mDecalInstance->calcPixelRadius( state ) < mDecalInstance->calcEndPixRadius( state->getViewportExtent() ) ) )
   {
      // Release our shadow texture
      // so that others can grab it out
      // of the pool.
      mShadowTexture = NULL;

      return false;
   }

   return true;
}

bool ProjectedShadow::_updateDecal( const SceneState *state )
{
   PROFILE_SCOPE( ProjectedShadow_UpdateDecal );

   // Get the sunlight for the shadow projection.
   // We want the LightManager to return NULL if it can't
   // get the "real" sun, so we specify false for the useDefault parameter.
   LightManager *lm = state->getLightManager();

   LightInfo *lights[4] = {0};
   if ( !lm )
      return false;

   lm->setupLights( NULL, mParentObject->getWorldSphere(), 4 );
   lm->getBestLights( lights, 4 );
   lm->resetLights();

   // Pull out the render transform
   // for use in a couple of places later.
   const MatrixF &renderTransform = mParentObject->getRenderTransform();
   Point3F pos = renderTransform.getPosition();

   Point3F lightDir( 0, 0, 0 );
   Point3F tmp( 0, 0, 0 );
   F32 weight = 0;
   F32 range = 0;
   U32 lightCount = 0;
   F32 dist = 0;
   F32 fade = 0;
   for ( U32 i = 0; i < 4; i++ )
   {
      // If we got a NULL light, 
      // we're at the end of the list.
      if ( !lights[i] )
         break;

      if ( !lights[i]->getCastShadows() )
         continue;

      if ( lights[i]->getType() != LightInfo::Point )
         tmp = lights[i]->getDirection();
      else
         tmp = pos - lights[i]->getPosition();

      range = lights[i]->getRange().x;
      dist = ( (tmp.lenSquared()) / ((range * range) * 0.5f));
      weight = mClampF( 1.0f - ( tmp.lenSquared() / (range * range)), 0.00001f, 1.0f );

      if ( lights[i]->getType() == LightInfo::Vector )
         fade = getMax( fade, 1.0f );
      else
         fade = getMax( fade, mClampF( 1.0f - dist, 0.00001f, 1.0f ) );

      lightDir += tmp * weight;
      lightCount++;
   }

   lightDir.normalize();

   LightInfo *light = lights ? lights[0] : NULL;

   // No light... no shadow.
   if ( !light )
      return false;
     
   const Box3F &objBox = mParentObject->getObjBox();
   Point3F boxCenter = objBox.getCenter();

   // Has the light direction
   // changed since last update?
   bool lightDirChanged = !mLastLightDir.equal( lightDir );

   // Has the parent object moved
   // or scaled since the last update?
   bool hasMoved = !mLastObjectPosition.equal( mParentObject->getRenderPosition() );
   bool hasScaled = !mLastObjectScale.equal( mParentObject->getScale() );

   // Set the last light direction
   // to the current light direction.
   mLastLightDir = lightDir;
   mLastObjectPosition = mParentObject->getRenderPosition();
   mLastObjectScale = mParentObject->getScale();


   // Temps used to generate
   // tangent vector for DecalInstance below.
   VectorF right( 0, 0, 0 );
   VectorF fwd( 0, 0, 0 );
   VectorF tmpFwd( 0, 0, 0 );
   
   U32 idx = lightDir.getLeastComponentIndex();

   tmpFwd[idx] = 1.0f;

   right = mCross( tmpFwd, lightDir );
   fwd = mCross( lightDir, right );
   right = mCross( fwd, lightDir );

   right.normalize();

   // Set up the world to light space
   // matrix, along with proper position
   // and rotation to be used as the world
   // matrix for the render to texture later on.
   static MatrixF sRotMat(EulerF( 0.0f, -(M_PI_F/2.0f), 0.0f));
   //mWorldToLight = light->getTransform();
   mWorldToLight.identity();
   MathUtils::getMatrixFromForwardVector( lightDir, &mWorldToLight );
   mWorldToLight.setPosition( ( pos + boxCenter ) - ( ( (mRadius * smDepthAdjust) + 0.001f ) * lightDir ) );
   mWorldToLight.mul( sRotMat );
   mWorldToLight.inverse();
   
   // Set up the decal position.
   // We use the object space box center
   // multiplied by the render transform
   // of the object to ensure we benefit
   // from interpolation.
   renderTransform.mulP( boxCenter );

   // Get the shapebase datablock if we have one.
   ShapeBaseData *data = NULL;
   if ( mShapeBase )
      data = static_cast<ShapeBaseData*>( mShapeBase->getDataBlock() );

   // We use the object box's extents multiplied
   // by the object's scale divided by 2 for the radius
   // because the object's worldsphere radius is not
   // rotationally invariant.
   mRadius = (objBox.getExtents() * mParentObject->getScale()).len() * 0.5f;

   if ( data )
      mRadius *= data->shadowSphereAdjust;

   // Create the decal if we don't have one yet.
   if ( !mDecalInstance )
      mDecalInstance = gDecalManager->addDecal( boxCenter, 
                                                lightDir, 
                                                right, 
                                                mDecalData, 
                                                1.0f, 
                                                0, 
                                                PermanentDecal | ClipDecal | CustomDecal );

   if ( !mDecalInstance )
      return false;

   mDecalInstance->mVisibility = fade;

   // Setup decal parameters.
   mDecalInstance->mSize = mRadius * 2.0f;
   mDecalInstance->mNormal = -lightDir;
   mDecalInstance->mTangent = -right;
   mDecalInstance->mRotAroundNormal = 0;
   mDecalInstance->mPosition = boxCenter;
   mDecalInstance->mDataBlock = mDecalData;
   
   // If the position of the world 
   // space box center is the same
   // as the decal's position, and
   // the light direction has not 
   // changed, we don't need to clip.
   bool shouldClip = !mDecalInstance->mPosition.equal( boxCenter ) || lightDirChanged || hasMoved || hasScaled;

   // Now, check and see if the object is visible.
   const Frustum &frust = state->getFrustum();
   if ( !frust.sphereInFrustum( mDecalInstance->mPosition, mDecalInstance->mSize * mDecalInstance->mSize ) && !shouldClip )
      return false;

   F32 shadowLen = 10.0f;
   if ( data )
      shadowLen = data->shadowProjectionDistance;

   const Point3F &boxExtents = objBox.getExtents();
   
   
   mShadowLength = shadowLen * mParentObject->getScale().z;

   // Set up clip depth, and box half 
   // offset for decal clipping.
   Point2F clipParams(  mShadowLength, (boxExtents.x + boxExtents.y) * 0.25f );

   bool render = false;
   bool clipSucceeded = true;

   // Clip!
   if ( shouldClip )
   {
      clipSucceeded = gDecalManager->clipDecal( mDecalInstance, 
                                                NULL, 
                                                &clipParams );
   }

   // If the clip failed,
   // we'll return false in
   // order to keep from
   // unnecessarily rendering
   // into the texture.  If
   // there was no reason to clip
   // on this update, we'll assume we
   // should update the texture.
   render = clipSucceeded;

   // Tell the DecalManager we've changed this decal.
   gDecalManager->notifyDecalModified( mDecalInstance );

   return render;
}

void ProjectedShadow::_calcScore( const SceneState *state )
{
   if ( !mDecalInstance )
      return;

   F32 pixRadius = mDecalInstance->calcPixelRadius( state ); 
 
   F32 pct = pixRadius / mDecalInstance->mDataBlock->startPixRadius;

   U32 msSinceLastRender = Platform::getVirtualMilliseconds() - getLastRenderTime();

   ShapeBaseData *data = NULL;
   if ( mShapeBase )
      data = static_cast<ShapeBaseData*>( mShapeBase->getDataBlock() );

   // For every 1s this shadow hasn't been
   // updated we'll add 10 to the score.
   F32 secs = mFloor( (F32)msSinceLastRender / 1000.0f );
   
   mScore = pct + secs;
   mClampF( mScore, 0.0f, 2000.0f );
}

void ProjectedShadow::update( const SceneState *state )
{
   mUpdateTexture = true;

   // Update our decal before
   // we render to texture.
   // If it fails, something bad happened
   // (no light to grab/failed clip) and we should return.
   if ( !_updateDecal( state ) )
   {
      // Release our shadow texture
      // so that others can grab it out
      // of the pool.
      mShadowTexture = NULL;
      
      mUpdateTexture = false;

      return;
   }
   else
      _calcScore( state );

   if ( !mCasterPositionSC || !mCasterPositionSC->isValid() )
      mCasterPositionSC = mDecalData->matInst->getMaterialParameterHandle( "$shadowCasterPosition" );

   if ( !mShadowLengthSC || !mShadowLengthSC->isValid() )
      mShadowLengthSC = mDecalData->matInst->getMaterialParameterHandle( "$shadowLength" );

   MaterialParameters *matParams = mDecalData->matInst->getMaterialParameters();

   matParams->set( mCasterPositionSC, mParentObject->getRenderPosition() );   
   matParams->set( mShadowLengthSC, mShadowLength / 4.0f );
}

void ProjectedShadow::render( F32 camDist, const TSRenderState &rdata )
{
   if ( !mUpdateTexture )
      return;
            
   // Do the render to texture,
   // DecalManager handles rendering
   // the shadow onto the world.
   _renderToTexture( camDist, rdata );
}

void ProjectedShadow::_renderToTexture( F32 camDist, const TSRenderState &rdata )
{
   PROFILE_SCOPE( ProjectedShadow_RenderToTexture );

   GFXDEBUGEVENT_SCOPE( ProjectedShadow_RenderToTexture, ColorI( 255, 0, 0 ) );

   RenderPassManager *renderPass = _getRenderPass();
   if ( !renderPass )
      return;

   GFXTransformSaver saver;
   
   // NOTE: GFXTransformSaver does not save/restore the frustum
   // so we must save it here before we modify it.
   F32 l, r, b, t, n, f;
   bool ortho;
   GFX->getFrustum( &l, &r, &b, &t, &n, &f, &ortho );  

   // Set the orthographic projection
   // matrix up, to be based on the radius
   // generated based on our shape.
   GFX->setOrtho( -mRadius, mRadius, -mRadius, mRadius, 0.001f, (mRadius * 2) * smDepthAdjust, true );

   // Set the world to light space
   // matrix set up in shouldRender().
   GFX->setWorldMatrix( mWorldToLight );

   // Get the shapebase datablock if we have one.
   ShapeBaseData *data = NULL;
   if ( mShapeBase )
      data = static_cast<ShapeBaseData*>( mShapeBase->getDataBlock() );

   // Init or update the shadow texture size.
   if ( mShadowTexture.isNull() || ( data && data->shadowSize != mShadowTexture.getWidth() ) )
   {
      U32 texSize = data ? data->shadowSize : 256;
      mShadowTexture.set( texSize, texSize, GFXFormatR8G8B8A8, &PostFxTargetProfile, "BLShadow" );
   }

   GFX->pushActiveRenderTarget();

   if ( !mRenderTarget )
      mRenderTarget = GFX->allocRenderToTextureTarget();

   mRenderTarget->attachTexture( GFXTextureTarget::DepthStencil, _getDepthTarget( mShadowTexture->getWidth(), mShadowTexture->getHeight() ) );
   mRenderTarget->attachTexture( GFXTextureTarget::Color0, mShadowTexture );
   GFX->setActiveRenderTarget( mRenderTarget );

   GFX->clear( GFXClearZBuffer | GFXClearStencil | GFXClearTarget, ColorI( 0, 0, 0, 0 ), 1.0f, 0 );

   gClientSceneGraph->pushRenderPass( renderPass );

   SceneState *diffuseState = rdata.getSceneState();
   SceneGraph *sceneManager = diffuseState->getSceneManager();

   SceneState *baseState = sceneManager->createBaseState( SPT_Shadow );
   baseState->setDiffuseCameraTransform( diffuseState->getCameraTransform() );
   baseState->setViewportExtent( diffuseState->getViewportExtent() );
   baseState->setWorldToScreenScale( diffuseState->getWorldToScreenScale() );
   
   // This is a tricky hack,
   // in order to get ShapeBase mounted
   // objects to render properly into
   // our render target.
   baseState->objectAlwaysRender( true );
   
   mParentObject->prepRenderImage( baseState, gClientSceneGraph->getStateKey(), 0xFFFFFFFF );
   renderPass->renderPass( baseState );

   baseState->objectAlwaysRender( false );

   // Grab the ShadowFilterPFX object
   // and call process on it with our target.
   PostEffect *shadowFilterPfx = NULL;
   if (  camDist < BasicLightManager::getShadowFilterDistance() && 
         Sim::findObject( "BL_ShadowFilterPostFx", shadowFilterPfx ) )
         shadowFilterPfx->process( baseState, mShadowTexture );

   // Delete the SceneState we allocated.
   delete baseState;

   gClientSceneGraph->popRenderPass();

   mRenderTarget->resolve();

   GFX->popActiveRenderTarget();
 
   // Restore frustum
   if (!ortho)
      GFX->setFrustum(l, r, b, t, n, f);
   else
      GFX->setOrtho(l, r, b, t, n, f);

   // Set the last render time.
   mLastRenderTime = Platform::getVirtualMilliseconds();

   // HACK: Will remove in future release!
   mDecalInstance->mCustomTex = &mShadowTexture;
}

RenderPassManager* ProjectedShadow::_getRenderPass()
{
   if ( smRenderPass.isNull() )
   {
      SimObject* renderPass = NULL;

      if ( !Sim::findObject( "BL_ProjectedShadowRPM", renderPass ) )
         Con::errorf( "ProjectedShadow::init() - 'BL_ProjectedShadowRPM' not initialized" );
      else
         smRenderPass = dynamic_cast<RenderPassManager*>(renderPass);
   }

   return smRenderPass;
}

GFXTextureObject* ProjectedShadow::_getDepthTarget( U32 width, U32 height )
{
   // Get a depth texture target from the pooled profile
   // which is returned as a temporary.
   GFXTexHandle depthTex( width, height, GFXFormatD24S8, &BLProjectedShadowZProfile, 
      "ProjectedShadow::_getDepthTarget()" );

   return depthTex;
}
