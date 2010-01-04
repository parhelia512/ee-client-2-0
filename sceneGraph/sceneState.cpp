//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "sceneGraph/sceneState.h"

#include "sceneGraph/sgUtil.h"
#include "sceneGraph/sceneObject.h"
#include "platform/profiler.h"
#include "gfx/gfxDevice.h"
#include "gfx/primBuilder.h"
#include "interior/interiorInstance.h"
#include "T3D/gameConnection.h"


// MM/JF: Added for mirrorSubObject fix.
void SceneState::setupClipPlanes( ZoneState *rState )
{
   rState->frustum.set( rState->frustum.isOrtho(),
                        rState->frustum.getNearLeft(),
                        rState->frustum.getNearRight(),
                        rState->frustum.getNearTop(),
                        rState->frustum.getNearBottom(),
                        mFrustum.getNearDist(),
                        mFrustum.getFarDist(),
                        mFrustum.getTransform() );

   // clip-planes through mirror portal are inverted
   if ( rState->frustum.isInverted() )
      rState->frustum.invert();

   rState->clipPlanesValid = true;
}


SceneState::SceneState( SceneState *parent,
                        SceneGraph *mgr,
                        ScenePassType passType,
                        U32 numZones,
                        const Frustum &frustum,
                        const RectI &viewport,
                        bool usePostEffects,
                        bool invert )
{	
   VECTOR_SET_ASSOCIATION( mZoneStates );
   VECTOR_SET_ASSOCIATION( mSubsidiaries );
   VECTOR_SET_ASSOCIATION( mTransformPortals );
   VECTOR_SET_ASSOCIATION( mInteriorList );
   	
   mFrustum.set( frustum );

   const F32 left = mFrustum.getNearLeft();
   const F32 right = mFrustum.getNearRight();
   const F32 top = mFrustum.getNearTop();
   const F32 bottom = mFrustum.getNearBottom();
   const F32 nearPlane = mFrustum.getNearDist();

   mSceneManager = mgr;
   mLightManager = mgr->getLightManager();

   mParent   = parent;

   mScenePassType = passType;
   
   if ( passType == SPT_Reflect || invert )
   {
      mFlipCull = true;
      mFrustum.invert();      
   }
   else
      mFlipCull = false;

   mRenderNonLightmappedMeshes = true;
   mRenderLightmappedMeshes = true;

   mBaseZoneState.render          = false;
   mBaseZoneState.clipPlanesValid = false;
   mBaseZoneState.frustum = mFrustum;
   mBaseZoneState.viewport = viewport;

   // Setup the default parameters for the screen metrics methods.
   mDiffuseCameraTransform = mFrustum.getTransform();
   mViewportExtent = viewport.extent;
   
   // TODO: What about ortho modes?  Is near plane ok
   // or do i need to remove it... maybe ortho has a near
   // plane of 1 and it just works out?
   mWorldToScreenScale.set(   ( nearPlane * mViewportExtent.x ) / ( right - left ),
                              ( nearPlane * mViewportExtent.y ) / ( top - bottom ) );

   mZoneStates.setSize(numZones);
   for (U32 i = 0; i < numZones; i++)
   {
      mZoneStates[i].render          = false;
      mZoneStates[i].clipPlanesValid = false;
   }

   mPortalOwner = NULL;
   mPortalIndex = 0xFFFFFFFF;

   mTerrainOverride = false;

   mUsePostEffects = usePostEffects;

   mAlwaysRender = false;
}

SceneState::~SceneState()
{
   U32 i;
   for (i = 0; i < mSubsidiaries.size(); i++)
      delete mSubsidiaries[i];
}

void SceneState::setPortal(SceneObject* owner, const U32 index)
{
   mPortalOwner = owner;
   mPortalIndex = index;
}


void SceneState::insertTransformPortal(SceneObject* owner, U32 portalIndex,
                                       U32 globalZone,     const Point3F& traversalStartPoint,
                                       const bool flipCull)
{
   mTransformPortals.increment();
   mTransformPortals.last().owner         = owner;
   mTransformPortals.last().portalIndex   = portalIndex;
   mTransformPortals.last().globalZone    = globalZone;
   mTransformPortals.last().traverseStart = traversalStartPoint;
   mTransformPortals.last().flipCull      = flipCull;
}

void SceneState::renderCurrentImages()
{
   GFX->pushWorldMatrix();

   // need to do this AFTER scene traversal, otherwise zones/portals will not
   // work correctly

   PROFILE_START(InteriorPrepBatchRender);
   for( U32 i=0; i<mInteriorList.size(); i++ )
   {
      InteriorListElem &elem = mInteriorList[i];

      Interior* pInterior = elem.obj->getResource()->getDetailLevel( elem.detailLevel );
      pInterior->prepBatchRender( elem.obj, this, elem.worldXform );
   }
   PROFILE_END();

   GFX->popWorldMatrix();

   RenderPassManager *renderPass = mSceneManager->getRenderPass();
   AssertFatal( renderPass, "SceneState::renderCurrentImages() - Got null RenderPassManager!" );

   renderPass->sort();
   renderPass->render(this);
   renderPass->clear();

   mInteriorList.clear();

}

bool SceneState::isObjectRendered(const SceneObject* obj)
{
   if ( mAlwaysRender )
      return true;

   const SceneObjectRef* pWalk = obj->mZoneRefHead;

   /*
   static F32 darkToOGLCoord[16] = { 1, 0,  0, 0,
                                     0, 0, -1, 0,
                                     0, 1,  0, 0,
                                     0, 0,  0, 1 };
   static MatrixF darkToOGLMatrix;
   static bool matrixInitialized = false;
   if (matrixInitialized == false)
   {
      F32* m = darkToOGLMatrix;
      for (U32 i = 0; i < 16; i++)
         m[i] = darkToOGLCoord[i];
      darkToOGLMatrix.transpose();
      matrixInitialized = true;
   }
   */

   while (pWalk != NULL) 
   {
      if (getZoneState(pWalk->zone).render == true)
      {
         ZoneState &rState = getZoneStateNC( pWalk->zone );
         
         if ( rState.clipPlanesValid == false )
            setupClipPlanes( &rState );

         // The object's world box intersects or 
         // overlaps the frustum, so we need to render.
         if ( rState.frustum.intersects( obj->getWorldBox() ) )
            return true;
      }

      pWalk = pWalk->nextInObj;
   }

   // Special-case the control object here.  If the game connection
   // is in first-person, don't allow it to be culled.

   GameConnection* serverConnection = GameConnection::getConnectionToServer();
   if( serverConnection->isFirstPerson()
       && serverConnection->getControlObject() == obj )
      return true;

   return false;
}
