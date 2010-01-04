//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sceneGraph/sceneRoot.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "T3D/portal.h"

SceneRoot* gClientSceneRoot = NULL;
SceneRoot* gServerSceneRoot = NULL;

U32 SceneRoot::smPortalKey = 0;

static Point3F gSortPoint;

int QSORT_CALLBACK cmpPortalDistance( const void *p1, const void *p2 )
{
   const Portal** pd1 = (const Portal**)p1;
   const Portal** pd2 = (const Portal**)p2;

   F32 dist1 = ( (*pd1)->getPosition() - gSortPoint ).lenSquared();
   F32 dist2 = ( (*pd2)->getPosition() - gSortPoint ).lenSquared();

   return dist2 - dist1;
}

SceneRoot::SceneRoot()
{
   setGlobalBounds();
   resetWorldBox();
}

SceneRoot::~SceneRoot()
{

}

bool SceneRoot::onSceneAdd(SceneGraph* pGraph)
{
   // _Cannot_ call the parent here.  Must handle this ourselves so we can keep out of
   //  the zone graph...
//   if (Parent::onSceneAdd(pGraph) == false)
//      return false;

   mSceneManager = pGraph;
   mSceneManager->registerZones(this, 1);
   AssertFatal(mZoneRangeStart == 0, "error, sceneroot must be first scene object zone manager!");

   return true;
}

void SceneRoot::onSceneRemove()
{
   AssertFatal(mZoneRangeStart == 0, "error, sceneroot must be first scene object zone manager!");
   mSceneManager->unregisterZones(this);
   mZoneRangeStart = 0xFFFFFFFF;
   mSceneManager = NULL;

   // _Cannot_ call the parent here.  Must handle this ourselves so we can keep out of
   //  the zone graph...
//   Parent::onSceneRemove();
}

bool SceneRoot::getOverlappingZones(SceneObject*, U32* zones, U32* numZones)
{
   // If we are here, we always return the global zone.
   zones[0]  = 0;
   *numZones = 1;

   return false;
}

bool SceneRoot::prepRenderImage(SceneState* state, const U32 stateKey,
                                const U32,
                                const bool modifyBaseZoneState)
{
   AssertFatal(modifyBaseZoneState == true, "error, should never be called unless in the upward traversal!");
   AssertFatal(isLastState(state, stateKey) == false, "Error, should have been colored black in order to prevent double calls!");
   setLastState(state, stateKey);

   //  We don't return a render image, or any portals, but we do setup the zone 0
   //  rendering parameters.  We simply copy them from what is in the states baseZoneState
   //  structure, and mark the zone as rendered.
   state->getZoneStateNC( 0 ).frustum = state->getBaseZoneState().frustum;

   // RLP/Sickhead NOTE: Do the zone traversal after setting
   // the base zone default frustum, otherwise
   // normal Interior rendering of portaled areas
   // will be borked!
   _traverseZones( state );

   state->getZoneStateNC(0).viewport = state->getBaseZoneState().viewport;
   state->getZoneStateNC(0).render   = true;

   return false;
}

bool SceneRoot::scopeObject(const Point3F&        /*rootPosition*/,
                            const F32             /*rootDistance*/,
                            bool*                 zoneScopeState)
{
   zoneScopeState[0] = true;
   return false;
}

void SceneRoot::_addPortal( Portal *p )
{
   mPortals.push_back( p );
}

void SceneRoot::_removePortal( Portal *p )
{
   mPortals.remove( p );
}

void SceneRoot::_traverseZones( SceneState *state )
{
   const Frustum &frust = state->getFrustum();
   Frustum currFrustum( frust );

   // Need to check somewhere if we're inside a zone
   // already, looking out into the outside zone
   // and if so, use that zone's portal frustum in
   // order to check to see if we can see through
   // the portals that connect to the outside.

   // Make a new list consisting
   // only of portals that we can
   // see that connect to the outside zone.
   // Only do the new list and sort if
   // there's more than one portal!
   Vector<Portal*> tmpPortals;
   if ( mPortals.size() > 1 )
   {
      gSortPoint = state->getCameraPosition();
      for ( U32 i = 0; i < mPortals.size(); i++ )
      {
         Portal *portal = mPortals[i];
         if ( !frust.intersectOBB( portal->getOBBPoints() ) )
            continue;

         tmpPortals.push_back( portal );
      }

      // Sort portals by distance from near to far.
      dQsort( tmpPortals.address(), tmpPortals.size(), sizeof( Portal* ), cmpPortalDistance ); 
   }
   else
      tmpPortals.merge( mPortals );

   Vector<Portal*> portalStack;
   portalStack.merge( tmpPortals );
   
   smPortalKey++;

   while ( portalStack.size() )
   {
      Portal *portal = portalStack.last();
      if ( !portal )
      {
         portalStack.pop_back();
         continue;
      }

      portal->setPortalKey( smPortalKey );

      portalStack.pop_back();

     // If this portal doesn't intersect
      // our frustum, nothing inside the zone
      // it connects to needs to be rendered.
      if ( !currFrustum.intersectOBB( portal->getOBBPoints() ) )
         continue;

      Frustum newFrustum( frust );
      portal->generatePortalFrustum( state, &newFrustum );
      newFrustum.invert();
    
      Zone *z0 = portal->getZone( 0 );
      Zone *z1 = portal->getZone( 1 );

      if ( z0 && z0->getPortalKey() != smPortalKey )
      {
         // If we got the zone that 
         // this portal we can see connects
         // to, then go ahead and set our
         // portaled frustum on its ZoneState.
         if ( !z0->getPointZone( state->getCameraPosition() ) )
         {
            SceneState::ZoneState &zoneState = state->getZoneStateNC( z0->getZoneRangeStart() );
            zoneState.render = true;
            zoneState.frustum = newFrustum;
            z0->setPortalKey( smPortalKey );
         }
         
         currFrustum = newFrustum;

         // Go through this zone's portals,
         // and see if any of them are visible 
         // using the new portaled frustum.
         const Vector<Portal*> &portals = z0->getPortals();
         for ( U32 i = 0; i < portals.size(); i++ )
         {
            Portal *subPortal = portals[i];

            // If this portal is visible through the
            // new portaled frustum, and it's not the current
            // upper level portal, stick it onto the stack.

            // Note: Don't need to do the frustum check
            // here, because it will check against this portal on 
            // the next loop through the portals on the stack.
            if ( subPortal->getPortalKey() != smPortalKey && currFrustum.intersectOBB( subPortal->getOBBPoints() ) )
               portalStack.push_back( subPortal );
         }
      }

      if ( z1 && z1->getPortalKey() != smPortalKey )
      {
         // If we got the zone that 
         // this portal we can see connects
         // to, then go ahead and set our
         // portaled frustum on its ZoneState.
         if ( !z1->getPointZone( state->getCameraPosition() ) )
         {
            SceneState::ZoneState &zoneState = state->getZoneStateNC( z1->getZoneRangeStart() );
            zoneState.render = true;
            zoneState.frustum = newFrustum;
            z1->setPortalKey( smPortalKey );
         }

         currFrustum = newFrustum;

         // Go through this zone's portals,
         // and see if any of them are visible 
         // using the new portaled frustum.
         const Vector<Portal*> &portals = z1->getPortals();
         for ( U32 i = 0; i < portals.size(); i++ )
         {
            Portal *subPortal = portals[i];

            // If this portal is visible through the
            // new portaled frustum, and it's not the current
            // upper level portal, stick it onto the stack.
            if ( subPortal->getPortalKey() != smPortalKey )
               portalStack.push_back( subPortal );
         }
      }
   }
}
