//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sgUtil.h"
#include "sceneGraph/sceneObject.h"
#include "sceneGraph/sceneState.h"
#include "math/mMatrix.h"
#include "terrain/terrData.h"
#include "gfx/gfxDevice.h"
#include "T3D/gameConnection.h"
#include "interior/interiorInstance.h"

namespace {

class PotentialRenderList
{

public:

   Box3F mBox;
   Frustum mFrustum;

   SceneState *mState;
   Vector<SceneObject*> mList;

   void insertObject(SceneObject* obj);
   void setupClipPlanes(SceneState*);
};

// MM/JF: Added for mirrorSubObject fix.
void PotentialRenderList::setupClipPlanes(SceneState* state)
{
   mState = state;

   const F32 nearPlane = state->getNearPlane();
   const F32 farPlane = state->getFarPlane();

   const SceneState::ZoneState &zoneState = state->getBaseZoneState();

   mFrustum.set(  zoneState.frustum.isOrtho(),
                  zoneState.frustum.getNearLeft(),
                  zoneState.frustum.getNearRight(),
                  zoneState.frustum.getNearTop(),
                  zoneState.frustum.getNearBottom(),
                  nearPlane,
                  farPlane,
                  state->getCameraTransform() );

   // clip-planes through mirror portal are inverted
   if ( mFrustum.isInverted() )
      mFrustum.invert();

   mBox = mFrustum.getBounds();
}

void PotentialRenderList::insertObject(SceneObject* obj)
{
   // Check to see if we need to render always.
   if (  obj->isGlobalBounds() || 
         mFrustum.intersects( obj->getZoneBox() ) )
      mList.push_back(obj);
}

void prlInsertionCallback(SceneObject* obj, void *key)
{
   if ( !obj->isRenderEnabled() )
      return;

   PotentialRenderList* prList = (PotentialRenderList*)key;
   prList->insertObject(obj);
}

} // namespace {}

void SceneGraph::_buildSceneTree(   SceneState *state,
                                    U32 objectMask,
                                    SceneObject *baseObject,
                                    U32 baseZone,
                                    U32 currDepth )
{
   AssertFatal(this == gClientSceneGraph, "Error, only the client scenegraph can support this call!");

   // Find the start zone if we haven't already.
   if ( !baseObject )
   {
      findZone( state->getCameraPosition(), baseObject, baseZone );
      currDepth = 1;
   }

   // Search proceeds from the baseObject, and starts in the baseZone.
   // General Outline:
   //    - Traverse up the tree, stopping at either the root, or the last interior
   //       that prevents traversal outside
   //    - Query the container database for all objects intersecting the viewcone,
   //       which is clipped to the bounding box returned at the last stage of the
   //       above traversal.
   //    - Topo sort the returned objects.
   //    - Traverse through the list, calling setupZones on zone managers,
   //       and retreiving render images from all applicable objects (including
   //       ZM's)
   //    - This process may return "Transform portals", i.e., mirrors, rendered
   //       teleporters, etc.  For each of these, create a new SceneState object
   //       subsidiary to state, and restart the traversal, with the new parameters,
   //       and the correct baseObject and baseZone.

   // Objects (in particular, those managers that are part of the initial up
   //  traversal) keep track of whether or not they have returned a render image
   //  to the current state by a key, and the state object pointer.
   smStateKey++;

   // Save off the base state...
   SceneState::ZoneState saveBase = state->getBaseZoneState();

   SceneObject* pTraversalRoot = baseObject;
   U32          rootZone       = baseZone;
   while (true) 
   {
      if (pTraversalRoot->prepRenderImage(state, smStateKey, rootZone, true)) 
      {
         if (pTraversalRoot->getNumCurrZones() != 1)
            Con::errorf(ConsoleLogEntry::General,
                        "Error, must have one and only one zone to be a traversal root.  %s has %d",
                        pTraversalRoot->getName(), pTraversalRoot->getNumCurrZones());

         rootZone       = pTraversalRoot->getCurrZone(0);
         pTraversalRoot = getZoneOwner(rootZone);
      } 
      else 
      {
         break;
      }
   }

   // Restore the base state...
   SceneState::ZoneState& rState = state->getBaseZoneStateNC();
   rState = saveBase;

   // Ok.  Now we have renderimages for anything north of the object in the
   //  tree.  Create the query polytope, and clip it to the bounding box of
   //  the traversalRoot object.
   PotentialRenderList prl;
   prl.setupClipPlanes(state);

   // We only have to clip the mBox field
   AssertFatal(prl.mBox.isOverlapped(pTraversalRoot->getZoneBox()),
               "Error, prl box must overlap the traversal root");
   prl.mBox.minExtents.setMax(pTraversalRoot->getZoneBox().minExtents);
   prl.mBox.maxExtents.setMin(pTraversalRoot->getZoneBox().maxExtents);
   prl.mBox.minExtents -= Point3F(5, 5, 5);
   prl.mBox.maxExtents += Point3F(5, 5, 5);
   AssertFatal(prl.mBox.isValidBox(), "Error, invalid query box created!");

   // Query against the container database, storing the objects in the
   //  potentially rendered list.  Note: we can query against the client
   //  container without testing, since only the client will be calling this
   //  function.  This is assured by the assert at the top...
   gClientContainer.findObjects(prl.mBox, objectMask, prlInsertionCallback, &prl);

   // Clear the object colors
   U32 i;
   for (i = 0; i < prl.mList.size(); i++)
      prl.mList[i]->setTraversalState( SceneObject::Pending );

   // If the connection's control object got culled, but we're in first person,
   // add it back in.  This happens when the eye node travels outside the object's
   // bounding box.

   GameConnection* serverConnection = GameConnection::getConnectionToServer();
   if( serverConnection->isFirstPerson() )
   {
      GameBase* controlObject = serverConnection->getControlObject();
      if( controlObject && controlObject->getTraversalState() != SceneObject::Pending )
      {
         prl.mList.push_back( controlObject );
         controlObject->setTraversalState( SceneObject::Pending );
      }
   }

   for (i = 0; i < prl.mList.size(); i++)
      if( prl.mList[i]->getTraversalState() == SceneObject::Pending )
         treeTraverseVisit(prl.mList[i], state, smStateKey);

   if (  currDepth < csmMaxTraversalDepth && 
         state->mTransformPortals.size() != 0 ) 
   {
      Frustum tempFrustum;
      
      // Need to handle the transform portals here.
      //
      for (U32 i = 0; i < state->mTransformPortals.size(); i++) {
         const SceneState::TransformPortal& rPortal  = state->mTransformPortals[i];
         const SceneState::ZoneState&       rPZState = state->getZoneState(rPortal.globalZone);
         AssertFatal(rPZState.render == true, "Error, should not have returned a portal if the zone isn't rendering!");

         Point3F cameraPosition = state->getCameraPosition();
         rPortal.owner->transformPosition(rPortal.portalIndex, cameraPosition);

         // Setup the new modelview matrix...
         MatrixF oldMV = GFX->getWorldMatrix();
         MatrixF newMV;
         rPortal.owner->transformModelview(rPortal.portalIndex, oldMV, &newMV);

         // Here's the tricky bit.  We have to derive a new frustum and viewport
         //  from the portal, but we have to do it in the NEW coordinate space.
         //  Seems easiest to dump the responsibility on the object that was rude
         //  enough to make us go to all this trouble...
         F64   newFrustum[4];
         RectI newViewport;

         bool goodPortal = rPortal.owner->computeNewFrustum(rPortal.portalIndex, // which portal?
                                                            rPZState.frustum,    // old view params
                                                            state->getNearPlane(),
                                                            state->getFarPlane(),
                                                            rPZState.viewport,
                                                            newFrustum,          // new view params
                                                            newViewport,
                                                            state->mFlipCull);

         if (goodPortal == false) 
         {
            // Portal isn't visible, or is clipped out by the zone parameters...
            continue;
         }

         tempFrustum.set( false, newFrustum[0], newFrustum[1], newFrustum[3], newFrustum[2], state->getNearPlane(), state->getFarPlane(), newMV );

         SceneState* newState = new SceneState( state,
                                                this,
                                                state->getScenePassType(),
                                                mCurrZoneEnd,
                                                tempFrustum,
                                                newViewport,
                                                state->usePostEffects(),
                                                state->mFlipCull ^ rPortal.flipCull );

         newState->setPortal(rPortal.owner, rPortal.portalIndex);

         GFX->pushWorldMatrix();

         GFX->setWorldMatrix( newMV );

         // Find the start zone.  Note that in a traversal descent, we start from
         //  the traversePoint of the transform portal, which is conveniently in
         //  world space...
         SceneObject* startObject;
         U32          startZone;
         findZone(rPortal.traverseStart, startObject, startZone);

         _buildSceneTree(newState, objectMask, startObject, startZone, currDepth + 1 );

         // Pop off the new modelview
         GFX->popWorldMatrix();

         // Push the subsidiary...
         state->mSubsidiaries.push_back(newState);
      }
   }

   // Ok, that's it!
}

bool terrCheck(TerrainBlock* pBlock,
               SceneObject*  pObj,
               const Point3F camPos);

void SceneGraph::treeTraverseVisit(SceneObject* obj,
                                   SceneState*  state,
                                   const U32    stateKey)
{
   if (obj->getNumCurrZones() == 0) 
   {
      obj->setTraversalState( SceneObject::Done );
      return;
   }

   PROFILE_START(treeTraverseVisit);

   AssertFatal(obj->getTraversalState() == SceneObject::Pending,
               "Wrong state for this stage of the traversal!");
   obj->setTraversalState(SceneObject::Working); //	TraversalState Not being updated correctly 'Gonzo'

   SceneObjectRef* pWalk = obj->mZoneRefHead;
   AssertFatal(pWalk != NULL, "Error, must belong to something!");
   while (pWalk) 
   {
      // Determine who owns this zone...
      SceneObject* pOwner = getZoneOwner(pWalk->zone);
      if( pOwner->getTraversalState() == SceneObject::Pending )
         treeTraverseVisit(pOwner, state, stateKey);

      pWalk = pWalk->nextInObj;
   }

   obj->setTraversalState( SceneObject::Done );

   // Cull it, but not if it's too low or there's no terrain to occlude against, or if it's global...
   if (getCurrentTerrain() != NULL && obj->getZoneBox().minExtents.x > -1e5 && !obj->isGlobalBounds())
   {
      bool doTerrCheck = true;
      SceneObjectRef* pRef = obj->mZoneRefHead;
      while (pRef != NULL)
      {
         if (pRef->zone != 0)
         {
            doTerrCheck = false;
            break;
         }
         pRef = pRef->nextInObj;
      }

      if (doTerrCheck == true && terrCheck(getCurrentTerrain(), obj, state->getCameraPosition()) == true)
      {
         PROFILE_END();
         return;
      }
   }

   PROFILE_START(treeTraverseVisit_prepRenderImage);
   obj->prepRenderImage(state, stateKey, 0xFFFFFFFF);
   PROFILE_END();

   PROFILE_END();
}

bool terrCheck(TerrainBlock* pBlock,
               SceneObject*  pObj,
               const Point3F camPos)
{
   PROFILE_START(terrCheck);

   // Don't try to occlude globally bounded objects.
   if(pObj->isGlobalBounds())
   {
      PROFILE_END();
      return false;
   }

   Point3F localCamPos = camPos;
   pBlock->getWorldTransform().mulP(localCamPos);
   F32 height;
   pBlock->getHeight(Point2F(localCamPos.x, localCamPos.y), &height);
   bool aboveTerrain = (height <= localCamPos.z);

   // Don't occlude if we're below the terrain.  This prevents problems when
   //  looking out from underground bases...
   if (aboveTerrain == false)
   {
      PROFILE_END();
      return false;
   }

   const Box3F& oBox = pObj->getObjBox();
   F32 minSide = getMin(oBox.len_x(), oBox.len_y());
   if (minSide > 85.0f)
   {
      PROFILE_END();
      return false;
   }

   const Box3F& rBox = pObj->getWorldBox();
   Point3F ul(rBox.minExtents.x, rBox.minExtents.y, rBox.maxExtents.z);
   Point3F ur(rBox.minExtents.x, rBox.maxExtents.y, rBox.maxExtents.z);
   Point3F ll(rBox.maxExtents.x, rBox.minExtents.y, rBox.maxExtents.z);
   Point3F lr(rBox.maxExtents.x, rBox.maxExtents.y, rBox.maxExtents.z);

   pBlock->getWorldTransform().mulP(ul);
   pBlock->getWorldTransform().mulP(ur);
   pBlock->getWorldTransform().mulP(ll);
   pBlock->getWorldTransform().mulP(lr);

   Point3F xBaseL0_s = ul - localCamPos;
   Point3F xBaseL0_e = lr - localCamPos;
   Point3F xBaseL1_s = ur - localCamPos;
   Point3F xBaseL1_e = ll - localCamPos;

   static F32 checkPoints[3] = {0.75, 0.5, 0.25};
   RayInfo rinfo;
   for (U32 i = 0; i < 3; i++)
   {
      Point3F start = (xBaseL0_s * checkPoints[i]) + localCamPos;
      Point3F end   = (xBaseL0_e * checkPoints[i]) + localCamPos;

      if (pBlock->castRay(start, end, &rinfo))
         continue;

      pBlock->getHeight(Point2F(start.x, start.y), &height);
      if ((height <= start.z) == aboveTerrain)
         continue;

      start = (xBaseL1_s * checkPoints[i]) + localCamPos;
      end   = (xBaseL1_e * checkPoints[i]) + localCamPos;

      if (pBlock->castRay(start, end, &rinfo))
         continue;

      Point3F test = (start + end) * 0.5;
      if (pBlock->castRay(localCamPos, test, &rinfo) == false)
         continue;

      PROFILE_END();
      return true;
   }

   PROFILE_END();
   return false;
}
