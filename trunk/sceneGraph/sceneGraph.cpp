//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "sceneGraph/sceneGraph.h"

#include "sceneGraph/sceneObject.h"
#include "sceneGraph/sceneRoot.h"
#include "sceneGraph/sceneState.h"
#include "lighting/lightManager.h"
#include "core/strings/unicode.h"
#include "sim/netConnection.h"
#include "terrain/terrData.h"
#include "core/stream/fileStream.h"
#include "platform/profiler.h"
#include "renderInstance/renderPassManager.h"
#include "core/util/swizzle.h"
#include "gfx/gfxDevice.h"
#include "gfx/bitmap/gBitmap.h"


const U32 SceneGraph::csmMaxTraversalDepth = 4;
U32 SceneGraph::smStateKey = 0;
SceneGraph* gClientSceneGraph = NULL;
SceneGraph* gServerSceneGraph = NULL;
const U32 SceneGraph::csmRefPoolBlockSize = 4096;


SceneGraph::SceneGraph( bool isClient )
{
   VECTOR_SET_ASSOCIATION( mRefPoolBlocks );
   VECTOR_SET_ASSOCIATION( mZoneManagers );
   VECTOR_SET_ASSOCIATION( mZoneLists );
   VECTOR_SET_ASSOCIATION( mRenderPassStack );

   mLightManager = NULL;

   mSceneState = NULL;

   mCurrZoneEnd        = 0;
   mNumActiveZones     = 0;

   mIsClient = isClient;

   mFogData.density = 0.0f;
   mFogData.densityOffset = 0.0f;
   mFogData.atmosphereHeight = 0.0f;
   mFogData.color.set( 128, 128, 128 );
  
   dMemset( &mWaterFogData, 0, sizeof( WaterFogData ) );   
   mWaterFogData.color.set( 128, 128, 128 );
   mWaterFogData.plane.set( 0, 0, 1 );

   mUsePostEffectFog = true;

   mNearClip = 0.1f;
   mVisibleDistance = 500.0f;

   mCurrTerrain = NULL;
   mFreeRefPool = NULL;
   addRefPoolBlock();
 
   mDisplayTargetResolution.set(0,0);
}

SceneGraph::~SceneGraph()
{
   mCurrZoneEnd        = 0;
   mNumActiveZones     = 0;

   for (U32 i = 0; i < mRefPoolBlocks.size(); i++) {
      SceneObjectRef* pool = mRefPoolBlocks[i];
      for (U32 j = 0; j < csmRefPoolBlockSize; j++)
         AssertFatal(pool[j].object == NULL, "Error, some object isn't properly out of the bins!");

      delete [] pool;
   }
   mFreeRefPool = NULL;   
   
   if (mLightManager)
      mLightManager->deactivate();   
}

void SceneGraph::addRefPoolBlock()
{
   mRefPoolBlocks.push_back(new SceneObjectRef[csmRefPoolBlockSize]);
   for (U32 i = 0; i < csmRefPoolBlockSize-1; i++) 
   {
      mRefPoolBlocks.last()[i].object    = NULL;
      mRefPoolBlocks.last()[i].prevInBin = NULL;
      mRefPoolBlocks.last()[i].nextInBin = NULL;
      mRefPoolBlocks.last()[i].nextInObj = &(mRefPoolBlocks.last()[i+1]);
   }
   mRefPoolBlocks.last()[csmRefPoolBlockSize-1].object    = NULL;
   mRefPoolBlocks.last()[csmRefPoolBlockSize-1].prevInBin = NULL;
   mRefPoolBlocks.last()[csmRefPoolBlockSize-1].nextInBin = NULL;
   mRefPoolBlocks.last()[csmRefPoolBlockSize-1].nextInObj = mFreeRefPool;

   mFreeRefPool = &(mRefPoolBlocks.last()[0]);
}

SceneState* SceneGraph::createBaseState( ScenePassType passType, bool inverted /* = false */ )
{
   // Determine the camera position, and store off render state...
   const MatrixF modelview(GFX->getWorldMatrix());

   MatrixF mv(modelview);      
   mv.inverse();   
   const Point3F cp(mv.getPosition());

   // Set up the base SceneState.
   F32 left, right, top, bottom, nearPlane, farPlane;
   bool isOrtho;

   GFX->getFrustum( &left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho );
   const RectI &viewport = GFX->getViewport();

   Frustum frust( isOrtho, 
                  left, right, top, bottom, 
                  nearPlane, farPlane, mv );

   SceneState* baseState = new SceneState(
      NULL,
      this,
      passType,
      mCurrZoneEnd,
      frust,
      viewport,
      true,
      inverted );

   AssertFatal( !mRenderPassStack.empty(), "SceneGraph::createBaseState() - Render pass stack is empty!" );


   // Assign shared matrix data to the render manager
   RenderPassManager *renderPass = baseState->getRenderPass();
   renderPass->assignSharedXform(RenderPassManager::View, modelview);
   renderPass->assignSharedXform(RenderPassManager::Projection, GFX->getProjectionMatrix());

   return baseState;
}

void SceneGraph::renderScene( ScenePassType passType, U32 objectMask )
{
   PROFILE_SCOPE(SceneGraphRender);

   // If we don't have a render pass then set the diffuse.
   if ( mRenderPassStack.empty() )
      _setDiffuseRenderPass();

   // TODO: We should reset the counters on the canvas 
   // signal for all render passes right?
   //mRenderManager->resetCounters();

   SceneState *sceneState = createBaseState( passType );

   // Get the lights for rendering the scene.
   PROFILE_START(RegisterLights);
   getLightManager()->registerGlobalLights( &sceneState->getFrustum(), false);
   PROFILE_END();

   PROFILE_START(SceneGraphRender_PreRenderSignal);
      getPreRenderSignal().trigger( this, sceneState );
   PROFILE_END();

   renderScene( sceneState, objectMask );
   //DebugDrawer::get()->render();

   PROFILE_START(SceneGraphRender_PostRenderSignal);
      getPostRenderSignal().trigger( this, sceneState );
   PROFILE_END();

   // Remove the previously registered lights.
   PROFILE_START(UnregisterLights);
   getLightManager()->unregisterAllLights();
   PROFILE_END();

   delete sceneState;
}

void SceneGraph::renderScene( SceneState *sceneState, U32 objectMask )
{
   // Set the current state.
   mSceneState = sceneState;

   // This finds objects in the view frustum and calls 
   // prepRenderImage on each so that they can submit
   // their render instances to the render bins.
   //
   // Note that internally buildSceneTree deals with zoning
   // and finds the corrent start zone for interior support.
   PROFILE_START(BuildSceneTree);
   _buildSceneTree( mSceneState, objectMask );
   PROFILE_END();
   
   // This fires off rendering the active render pass with
   // the render instances submitted above.
   PROFILE_START(TraverseScene);
   _traverseSceneTree( mSceneState );
   PROFILE_END();

   mSceneState = NULL;
}

void SceneGraph::_traverseSceneTree(SceneState* pState)
{
   // DMM FIX: only handles trees one deep for now

   if (pState->mSubsidiaries.size() != 0) {
      for (U32 i = 0; i < pState->mSubsidiaries.size(); i++)
         _traverseSceneTree(pState->mSubsidiaries[i]);
   }

   if (pState->mParent != NULL) {
      // Comes from a transform portal.  Let's see if we need to flip the cull

      // Now, the index gives the TransformPortal index in the Parent...
      SceneObject* pPortalOwner = pState->mPortalOwner;
      U32 portalIndex = pState->mPortalIndex;
      AssertFatal(pPortalOwner != NULL && portalIndex != 0xFFFFFFFF,
         "Hm, this should never happen.  We should always have an owner and an index here");

      // Ok, open the portal.  Opening and closing the portals is a tricky bit of
      //  work, since we have to get the z values just right.  We're going to toss
      //  the responsibility onto the shoulders of the object that owns the portal.
      pPortalOwner->openPortal(portalIndex, pState, pState->mParent);

      // Render the objects in this subsidiary...
      PROFILE_START(RenderCurrentImages);
      pState->renderCurrentImages();
      PROFILE_END();
      
      // close the portal
      pPortalOwner->closePortal(portalIndex, pState, pState->mParent);
   } else {
      PROFILE_START(RenderCurrentImages);
      pState->renderCurrentImages();
      PROFILE_END();
   }

}


//----------------------------------------------------------------------------
struct ScopingInfo {
   Point3F        scopePoint;
   F32            scopeDist;
   F32            scopeDistSquared;
   const bool*    zoneScopeStates;
   NetConnection* connection;
};


inline void scopeCallback(SceneObject* obj, ScopingInfo* pInfo)
{
   NetConnection* ptr = pInfo->connection;

   if (obj->isScopeable()) {
      F32 difSq = (obj->getWorldSphere().center - pInfo->scopePoint).lenSquared();
      if (difSq < pInfo->scopeDistSquared) {
         // Not even close, it's in...
         ptr->objectInScope(obj);
      } else {
         // Check a little more closely...
         F32 realDif = mSqrt(difSq);
         if (realDif - obj->getWorldSphere().radius < pInfo->scopeDist) {
            ptr->objectInScope(obj);
         }
      }
   }
}

void SceneGraph::scopeScene(const Point3F& scopePosition,
                            const F32      scopeDistance,
                            NetConnection* netConnection)
{
   // Find the start zone...
   SceneObject* startObject;
   U32          startZone;
   findZone(scopePosition, startObject, startZone);

   // Search proceeds from the baseObject, and starts in the baseZone.
   // General Outline:
   //    - Traverse up the tree, stopping at either the root, or the last zone manager
   //       that prevents traversal outside
   //    - This will set up the array of zone states, either scoped or unscoped.
   //       loop through all the objects, placing them in scope if they are in
   //       a scoped zone.

   // Objects (in particular, those managers that are part of the initial up
   //  traversal) keep track of whether or not they have done their scope traversal
   //  by a key which is the same key used for renderState determination

   SceneObject* pTraversalRoot = startObject;
   U32          rootZone       = startZone;
   bool* zoneScopeState = new bool[mCurrZoneEnd];
   dMemset(zoneScopeState, 0, sizeof(bool) * mCurrZoneEnd);

   smStateKey++;
   while (true) {
      // Anything that we encounter in our up traversal is scoped
      if (pTraversalRoot->isScopeable())
         netConnection->objectInScope(pTraversalRoot);

      pTraversalRoot->mLastStateKey = smStateKey;
      if (pTraversalRoot->scopeObject(scopePosition,   scopeDistance,
                                      zoneScopeState)) {
         // Continue upwards
         if (pTraversalRoot->getNumCurrZones() != 1) {
            Con::errorf(ConsoleLogEntry::General,
                        "Error, must have one and only one zone to be a traversal root.  %s has %d",
                        pTraversalRoot->getName(), pTraversalRoot->getNumCurrZones());
         }

         rootZone = pTraversalRoot->getCurrZone(0);
         pTraversalRoot = getZoneOwner(rootZone);
      } else {
         // Terminate.  This is the traversal root...
         break;
      }
   }

   S32 i;

   // Note that we start at 1 here rather than 0, since if the root was going to be
   //  scoped, it would have been scoped in the up traversal rather than at this stage.
   //  Also, it doesn't have a CurrZone(0), so that's bad... :)
   for (i = 1; i < mZoneManagers.size(); i++) {
      if (mZoneManagers[i].obj->mLastStateKey != smStateKey &&
          zoneScopeState[mZoneManagers[i].obj->getCurrZone(0)] == true) {
         // Scope the zones in this manager...
         mZoneManagers[i].obj->scopeObject(scopePosition, scopeDistance, zoneScopeState);
      }
   }


   ScopingInfo info;
   info.scopePoint       = scopePosition;
   info.scopeDist        = scopeDistance;
   info.scopeDistSquared = scopeDistance * scopeDistance;
   info.zoneScopeStates  = zoneScopeState;
   info.connection       = netConnection;

   for (i = 0; i < mCurrZoneEnd; i++) {
      // Zip through the zone lists...
      if (zoneScopeState[i] == true) {
         // Scope zone i...
         SceneObjectRef* pList = mZoneLists[i];
         SceneObjectRef* pWalk = pList->nextInBin;
         while (pWalk != NULL) {
            SceneObject* pObject = pWalk->object;
            if (pObject->mLastStateKey != smStateKey) {
               pObject->mLastStateKey = smStateKey;
               scopeCallback(pObject, &info);
            }

            pWalk = pWalk->nextInBin;
         }
      }
   }

   delete [] zoneScopeState;
   zoneScopeState = NULL;
}


//------------------------------------------------------------------------------
bool SceneGraph::addObjectToScene(SceneObject* obj)
{
   if (obj->getType() & TerrainObjectType) {
      // Double check
      AssertFatal(dynamic_cast<TerrainBlock*>(obj) != NULL, "Not a terrain, but a terrain type?");
      mCurrTerrain = static_cast<TerrainBlock*>(obj);
   }

   return obj->onSceneAdd(this);
}


//------------------------------------------------------------------------------
void SceneGraph::removeObjectFromScene(SceneObject* obj)
{
   if (obj->mSceneManager != NULL) {
      AssertFatal(obj->mSceneManager == this, "Error, removing from the wrong sceneGraph!");

      if (obj->getType() & TerrainObjectType) {
         // Double check
         AssertFatal(dynamic_cast<TerrainBlock*>(obj) != NULL, "Not a terrain, but a terrain type?");
         if (mCurrTerrain == static_cast<TerrainBlock*>(obj))
            mCurrTerrain = NULL;
      }

      obj->onSceneRemove();
   }
}


//------------------------------------------------------------------------------
void SceneGraph::registerZones(SceneObject* obj, U32 numZones)
{
   AssertFatal(alreadyManagingZones(obj) == false, "Error, added zones twice!");
   compactZonesCheck();

   U32 i;
   U32 retVal       = mCurrZoneEnd;
   mCurrZoneEnd    += numZones;
   mNumActiveZones += numZones;

   mZoneLists.increment(numZones);
   for (i = mCurrZoneEnd - numZones; i < mCurrZoneEnd; i++) {
      mZoneLists[i] = new SceneObjectRef;
      mZoneLists[i]->object    = obj;
      mZoneLists[i]->nextInBin = NULL;
      mZoneLists[i]->prevInBin = NULL;
      mZoneLists[i]->nextInObj = NULL;
   }

   ZoneManager newEntry;
   newEntry.obj            = obj;
   newEntry.numZones       = numZones;
   newEntry.zoneRangeStart = retVal;
   mZoneManagers.push_back(newEntry);
   obj->mZoneRangeStart = retVal;

   // Since we now have new zones in this space, we need to rezone any intersecting
   //  objects.  Query the container database to find all intersecting/contained
   //  objects, and rezone them
   // query
   SimpleQueryList list;
   getContainer()->findObjects(obj->mWorldBox, 0xFFFFFFFF, SimpleQueryList::insertionCallback, &list);

   // DMM: Horrendously inefficient.  We should do the rejection test against
   //  obj here
   for (i = 0; i < list.mList.size(); i++) {
      SceneObject* rezoneObj = list.mList[i];

      // Make sure this is actually a SceneObject, is not the zone manager,
      //  and is added to the scene manager.
      if (rezoneObj != NULL && rezoneObj != obj &&
          rezoneObj->mSceneManager != NULL)
         rezoneObject(rezoneObj);
   }
}


//------------------------------------------------------------------------------
void SceneGraph::unregisterZones(SceneObject* obj)
{
   AssertFatal(alreadyManagingZones(obj) == true, "Error, not managing any zones!");

   // First, let's nuke the lists associated with this object.  We can leave the
   //  horizontal references in the objects in place, they'll be freed before too
   //  long.
   for (U32 i = 0; i < mZoneManagers.size(); i++) {
      if (obj == mZoneManagers[i].obj) {
         AssertFatal(mNumActiveZones >= mZoneManagers[i].numZones, "Too many zones removed");

         for (U32 j = mZoneManagers[i].zoneRangeStart;
              j < mZoneManagers[i].zoneRangeStart + mZoneManagers[i].numZones; j++) {
            SceneObjectRef* pList = mZoneLists[j];
            SceneObjectRef* pWalk = pList->nextInBin;

            // We have to tree pList a little differently, since it's not a traditional
            //  link.  We can just delete it at the end...
            pList->object = NULL;
            delete pList;
            mZoneLists[j] = NULL;

            while (pWalk) {
               AssertFatal(pWalk->object != NULL, "Error, must have an object!");
               SceneObjectRef* pTrash = pWalk;
               pWalk = pWalk->nextInBin;

               pTrash->nextInBin = pTrash;
               pTrash->prevInBin = pTrash;

               // Ok, now we need to zip through the list in the object to find
               //  this and remove it since we aren't doubly linked...
               SceneObjectRef** ppRef = &pTrash->object->mZoneRefHead;
               bool found = false;
               while (*ppRef) {
                  if (*ppRef == pTrash) {
                     // Remove it
                     *ppRef = (*ppRef)->nextInObj;
                     found = true;

                     pTrash->object    = NULL;
                     pTrash->nextInBin = NULL;
                     pTrash->prevInBin = NULL;
                     pTrash->nextInObj = NULL;
                     pTrash->zone      = 0xFFFFFFFF;
                     freeObjectRef(pTrash);
                     break;
                  }

                  ppRef = &(*ppRef)->nextInObj;
               }
               AssertFatal(found == true, "Error, should have found that reference!");
            }
         }

         mNumActiveZones -= mZoneManagers[i].numZones;
         mZoneManagers.erase(i);
         obj->mZoneRangeStart = 0xFFFFFFFF;

         // query
         if ((mIsClient == true  && obj != gClientSceneRoot) ||
             (mIsClient == false && obj != gServerSceneRoot))
         {
            SimpleQueryList list;
            getContainer()->findObjects(obj->mWorldBox, 0xFFFFFFFF, SimpleQueryList::insertionCallback, &list);
            for (i = 0; i < list.mList.size(); i++) {
               SceneObject* rezoneObj = list.mList[i];
               if (rezoneObj != NULL && rezoneObj != obj && rezoneObj->mSceneManager != NULL)
                  rezoneObject(rezoneObj);
            }
         }
         return;
      }
   }
   compactZonesCheck();

   // Other assert already ensured we will terminate properly...
   AssertFatal(false, "Error, impossible condition reached!");
}


//------------------------------------------------------------------------------
void SceneGraph::compactZonesCheck()
{
   if (mNumActiveZones > (mCurrZoneEnd / 2))
      return;

   // DMMTODO: Compact zones...
   //
}


//------------------------------------------------------------------------------
bool SceneGraph::alreadyManagingZones(SceneObject* obj) const
{
   for (U32 i = 0; i < mZoneManagers.size(); i++)
      if (obj == mZoneManagers[i].obj)
         return true;
   return false;
}


//------------------------------------------------------------------------------
void SceneGraph::findZone(const Point3F& p, SceneObject*& owner, U32& zone)
{
   // Since there is no zone information maintained by the sceneGraph
   //  any more, this is quite brain-dead.  Maybe fix this?  DMM
   //
   U32 currZone           = 0;
   SceneObject* currOwner = mZoneManagers[0].obj;

   PROFILE_START(SG_FindZone);
   while (true) 
   {
      bool cont = false;

      // Loop, but don't consider the root...
      for (U32 i = 1; i < mZoneManagers.size(); i++) 
      {
         // RLP/Sickhead NOTE: This warning is currently 
         // disabled to support the new Zone/Portal functionality
         // but needs to be investigated more thoroughly for any side effects.

         //AssertWarn(mZoneManagers[i].obj->getNumCurrZones() == 1 || (i == 0 && mZoneManagers[i].obj->getNumCurrZones() == 0),
         //           "ZoneManagers are only allowed to belong to one and only one zone!");
         
         if (mZoneManagers[i].obj->getCurrZone(0) == currZone) 
         {
            // Test to see if the point is inside
            U32 testZone = mZoneManagers[i].obj->getPointZone(p);
            if (testZone != 0) 
            {
               // Point is in this manager, reset, and descend
               cont = true;
               currZone  = testZone;
               currOwner = mZoneLists[currZone]->object;
               break;
            }
         }
      }

      // Have we gone as far as we can?
      if (cont == false)
         break;
   }

   zone  = currZone;
   owner = currOwner;
   PROFILE_END();
}


//------------------------------------------------------------------------------
void SceneGraph::rezoneObject(SceneObject* obj)
{
   AssertFatal(obj->mSceneManager != NULL && obj->mSceneManager == this, "Error, bad or no scenemanager here!");
   PROFILE_START(SG_Rezone);

   if (obj->mZoneRefHead != NULL) 
   {
      // Remove the object from the zone lists...
      SceneObjectRef* walk = obj->mZoneRefHead;
      while (walk) 
      {
         SceneObjectRef* remove = walk;
         walk = walk->nextInObj;

         remove->prevInBin->nextInBin = remove->nextInBin;
         if (remove->nextInBin)
            remove->nextInBin->prevInBin = remove->prevInBin;

         remove->nextInObj = NULL;
         remove->nextInBin = NULL;
         remove->prevInBin = NULL;
         remove->object    = NULL;
         remove->zone      = U32(-1);

         freeObjectRef(remove);
      }

      obj->mZoneRefHead = NULL;
   }


   U32 numMasterZones = 0;
   SceneObject* masterZoneOwners[SceneObject::MaxObjectZones];
   U32          masterZoneBuffer[SceneObject::MaxObjectZones];

   S32 i;
   for (i = S32(mZoneManagers.size()) - 1; i >= 0; i--) 
   {
      // Careful, zone managers are in the list at this point...
      if (obj == mZoneManagers[i].obj)
         continue;

      if (mZoneManagers[i].obj->getWorldBox().isOverlapped(obj->getWorldBox()) == false)
         continue;

      // We have several possible outcomes here
      //  1: Object completely contained in zoneManager
      //  2: object overlaps manager. (outside zone is included)
      //  3: Object completely contains manager (outside zone not included)
      // In case 3, we ignore the possibility that the object resides in
      //  zones managed by the manager, and we can continue
      // In case 1 and 2, we need to query the manager for zones.
      // In case 1, we break out of the loop, unless the object is not a
      //  part of the managers interior zones.
      // In case 2, we need to continue querying the database until we
      //  stop due to one of the above conditions (guaranteed to happen
      //  when we reach the sceneRoot.  (Zone 0))
      
      // We need to make sure that floating point precision doesn't skip out
      // on two objects with global bounds
      /*
      if (!(mZoneManagers[i].obj->isGlobalBounds() && obj->isGlobalBounds()))
      {
         if (obj->getWorldBox().isContained(mZoneManagers[i].obj->getWorldBox())) 
         {
               // case 3
               continue; 
         }
      }
      */

      // Query the zones...
      U32 numZones = 0;
      U32 zoneBuffer[SceneObject::MaxObjectZones];

      bool outsideIncluded = mZoneManagers[i].obj->getOverlappingZones(obj, zoneBuffer, &numZones);
      AssertFatal(numZones != 0 || outsideIncluded == true, "Hm, no zones, but not in the outside zone?  Impossible!");

      // Copy the included zones out
      if (numMasterZones + numZones > SceneObject::MaxObjectZones)
         Con::errorf(ConsoleLogEntry::General, "Zone Overflow!  Object will NOT render correctly.  Copying out as many as possible");
      numZones = getMin(numZones, SceneObject::MaxObjectZones - numMasterZones);

      for (U32 j = 0; j < numZones; j++) 
      {
         masterZoneBuffer[numMasterZones]   = zoneBuffer[j];
         masterZoneOwners[numMasterZones++] = mZoneManagers[i].obj;
      }

      if (outsideIncluded == false) {
         // case 3.  We can stop the search at this point...
         break;
      } else {
         // Case 2.  We need to continue searching...
         // ...
      }
   }
   // Copy the found zones into the buffer...
   AssertFatal(numMasterZones != 0, "Error, no zones found?  Should always find root at least.");

   obj->mNumCurrZones = numMasterZones;
   for (i = 0; i < numMasterZones; i++) 
   {
      // Insert into zone masterZoneBuffer[i]
      SceneObjectRef* zoneList = mZoneLists[masterZoneBuffer[i]];
      AssertFatal(zoneList != NULL, "Error, no list for this zone!");

      SceneObjectRef* newRef = allocateObjectRef();

      // Get it into the list
      newRef->zone      = masterZoneBuffer[i];
      newRef->object    = obj;
      newRef->nextInBin = zoneList->nextInBin;
      newRef->prevInBin = zoneList;
      if (zoneList->nextInBin)
         zoneList->nextInBin->prevInBin = newRef;
      zoneList->nextInBin = newRef;

      // Now get it into the objects chain...
      newRef->nextInObj = obj->mZoneRefHead;
      obj->mZoneRefHead = newRef;
   }

   // Let the object know its zones have changed.
   obj->onRezone();

   PROFILE_END();
}

void SceneGraph::zoneInsert(SceneObject* obj)
{
   PROFILE_START(SG_ZoneInsert);
   AssertFatal(obj->mNumCurrZones == 0, "Error, already entered into zone list...");

   rezoneObject(obj);

   if (obj->isManagingZones()) {
      // Query the container database to find all intersecting/contained
      //  objects, and rezone them
      // query
      SimpleQueryList list;
      getContainer()->findObjects(obj->mWorldBox, 0xFFFFFFFF, SimpleQueryList::insertionCallback, &list);

      // DMM: Horrendously inefficient.  We should do the rejection test against
      //  obj here, but the zoneManagers are so infrequently inserted and removed that
      //  it really doesn't matter...
      for (U32 i = 0; i < list.mList.size(); i++)
      {
         SceneObject* rezoneObj = list.mList[i];

         // Make sure this is actually a SceneObject, is not the zone manager,
         //  and is added to the scene manager.
         if (rezoneObj != NULL && rezoneObj != obj &&
             rezoneObj->mSceneManager == this)
            rezoneObject(rezoneObj);
      }
   }
   PROFILE_END();
}


//------------------------------------------------------------------------------
void SceneGraph::zoneRemove(SceneObject* obj)
{
   PROFILE_START(SG_ZoneRemove);
   obj->mNumCurrZones = 0;

   // Remove the object from the zone lists...
   SceneObjectRef* walk = obj->mZoneRefHead;
   while (walk) {
      SceneObjectRef* remove = walk;
      walk = walk->nextInObj;

      remove->prevInBin->nextInBin = remove->nextInBin;
      if (remove->nextInBin)
         remove->nextInBin->prevInBin = remove->prevInBin;

      remove->nextInObj = NULL;
      remove->nextInBin = NULL;
      remove->prevInBin = NULL;
      remove->object    = NULL;
      remove->zone      = U32(-1);

      freeObjectRef(remove);
   }
   obj->mZoneRefHead = NULL;
   PROFILE_END();
}

void SceneGraph::setDisplayTargetResolution( const Point2I &size )
{
   mDisplayTargetResolution = size;
}

const Point2I & SceneGraph::getDisplayTargetResolution() const
{
   return mDisplayTargetResolution;
}

bool SceneGraph::setLightManager( const char *lmName )
{
   LightManager *lm = LightManager::findByName( lmName );
   if ( !lm )
      return false;

   return _setLightManager( lm );
}

bool SceneGraph::_setLightManager( LightManager *lm )
{
   // Avoid unessasary work reinitializing materials.
   if ( lm == mLightManager )
      return true;

   // Make sure its valid... else fail!
   if ( !lm->isCompatible() )
      return false;

   // We only deactivate it... all light managers are singletons
   // and will manager their own lifetime.
   if ( mLightManager )
      mLightManager->deactivate();

   mLightManager = lm;

   if ( mLightManager )
   {
      // HACK: We're activating the diffuse render pass
      // here so that its there for the light manager
      // activation.
      _setDiffuseRenderPass();

      mLightManager->activate( this );
   }

   return true;
}

LightManager* SceneGraph::getLightManager()
{
   AssertFatal( mIsClient, "SceneGraph::getLightManager() - You should never access the light manager via the server scene graph!" );
   return mLightManager;
}

void SceneGraph::_setDiffuseRenderPass()
{
   mRenderPassStack.clear();

   RenderPassManager *rpm;
   if ( Sim::findObject( "DiffuseRenderPassManager", rpm ) )
      pushRenderPass( rpm );
}

void SceneGraph::pushRenderPass( RenderPassManager *rpm )
{
   mRenderPassStack.push_back( rpm );
}

void SceneGraph::popRenderPass()
{
   AssertFatal( !mRenderPassStack.empty(), "SceneGraph::popRenderPass() - The stack is empty!" );        
   mRenderPassStack.pop_back();
}

