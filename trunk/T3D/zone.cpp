//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------


#include "platform/platform.h"
#include "T3D/zone.h"

#include "console/consoleTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxTransformSaver.h"
#include "core/stream/bitStream.h"
#include "math/mathIO.h"
#include "T3D/portal.h"
#include "renderInstance/renderPassManager.h"
#include "gfx/gfxDrawUtil.h"
#include "math/mathUtils.h"
#include "gfx/gfxTransformSaver.h"

IMPLEMENT_CO_NETOBJECT_V1( Zone );

U32 Zone::smZoneKey = 0;
bool Zone::smRenderZones = false;

static Point3F gSortPoint;

int QSORT_CALLBACK cmpZonePortalDistance( const void *p1, const void *p2 )
{
   const Portal** pd1 = (const Portal**)p1;
   const Portal** pd2 = (const Portal**)p2;

   F32 dist1 = ( (*pd1)->getPosition() - gSortPoint ).lenSquared();
   F32 dist2 = ( (*pd2)->getPosition() - gSortPoint ).lenSquared();

   return dist2 - dist1;
}

Zone::Zone()
{
   mNetFlags.set( Ghostable | ScopeAlways );
   mTypeMask |= StaticObjectType;

   mObjScale.set( 10, 10, 10 );

   mZoneKey = 0;
   mOutdoorZoneVisible = false;
}

Zone::~Zone()
{
}

void Zone::initPersistFields()
{
   Parent::initPersistFields();
}

void Zone::consoleInit()
{
   Con::addVariable( "$Zone::renderZones", TypeBool, &smRenderZones );
}

bool Zone::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   mObjBox.set(   Point3F( -0.5f, -0.5f, -0.5f ),
                  Point3F( 0.5f, 0.5f, 0.5f ) );

   resetWorldBox();

   mZoneBox = mWorldBox;

   addToScene();
   mSceneManager->registerZones( this, 1 );

   return true;
}

void Zone::onRemove()
{
   mSceneManager->unregisterZones( this );
   removeFromScene();

   Parent::onRemove();
}

void Zone::setTransform(const MatrixF & mat)
{  
   Parent::setTransform( mat );
   setMaskBits( TransformMask );
}

U32 Zone::packUpdate( NetConnection *con, U32 mask, BitStream *stream )
{
   U32 retMask = Parent::packUpdate( con, mask, stream );

   if ( stream->writeFlag( mask & TransformMask ) ) 
   {
      mathWrite( *stream, mObjToWorld );
      mathWrite( *stream, mObjScale );
   }

   return retMask;
}

void Zone::unpackUpdate( NetConnection *con, BitStream *stream )
{
   Parent::unpackUpdate( con, stream );

   if ( stream->readFlag() )  // TransformMask
   {
      mathRead(*stream, &mObjToWorld);
      mathRead(*stream, &mObjScale);

      setTransform( mObjToWorld );
   }
}

U32 Zone::getPointZone( const Point3F &p )
{
   Point3F objPoint( 0, 0, 0 );
   getWorldTransform().mulP( p, &objPoint );
   objPoint.convolveInverse( getScale() );

   if ( mObjBox.isContained( objPoint ) )
      return mZoneRangeStart;

   return 0;
}

bool Zone::getOverlappingZones(  SceneObject *obj,
                                 U32 *zones,
                                 U32 *numZones )
{
   const bool isOverlapped = mWorldBox.isOverlapped( obj->getWorldBox() );
   const bool isContained = isOverlapped && mWorldBox.isContained( obj->getWorldBox() );

   const bool isPortal = dynamic_cast<Portal*>( obj );
   const bool isZone = dynamic_cast<Zone*>( obj ) && !isPortal;

   if (  !obj->isGlobalBounds() &&
         !isContained && 
         isOverlapped && 
         !isZone )
   {
      *numZones = 1;
      zones[0] = mZoneRangeStart;
      return true;
   }

   if ( isContained && !isZone )
   {
      *numZones = 1;
      zones[0] = mZoneRangeStart;      
      return false;
   }

   *numZones = 0;
   return true;
}

bool Zone::scopeObject( const Point3F &rootPosition,
                           const F32 rootDistance,
                           bool *zoneScopeState )
{
   if ( getPointZone( rootPosition ) != 0 )
   {
      zoneScopeState[mZoneRangeStart] = true;

      Vector<Zone*> zoneStack;
      zoneStack.push_back( this );

      while ( zoneStack.size() )
      {
         Zone *zone = zoneStack.last();
         if ( !zone )
            continue;

         zone->mLastStateKey = mLastStateKey;

         if ( (zone->getPosition() - rootPosition).len() <= rootDistance )
            zoneScopeState[zone->mZoneRangeStart] = true;

         zoneStack.pop_back();

         // Go through this zone's portals
         // and determine if the camera point
         // is on the same side of the portal's 
         // plane as this zone.
         for ( U32 i = 0; i < zone->mPortals.size(); i++ )
         {
            Portal *portal = zone->mPortals[i];
            
            PlaneF portalPlane( portal->getPosition(), portal->getTransform().getForwardVector() );

            PlaneF::Side camSide = portalPlane.whichSide( rootPosition );
            PlaneF::Side zoneSide = portalPlane.whichSide( getPosition() );

            if ( camSide == zoneSide )
            {
               // Push back the zones for this portal.
               Zone *z0 = portal->getZone( 0 );
               Zone *z1 = portal->getZone( 1 );

               if ( z0 && z0->mLastStateKey != mLastStateKey )
                  zoneStack.push_back( z0 );

               if ( z1 && z1->mLastStateKey != mLastStateKey )
                  zoneStack.push_back( z1 );
            }
         }
      }
        
      return true;
   }

   return false;
}

bool Zone::prepRenderImage(   SceneState* state, 
                              const U32 stateKey,
                              const U32 startZone, 
                              const bool modifyBaseState )
{
   if ( isLastState(state, stateKey ) )
      return false;

   setLastState( state, stateKey );

   // This flag will be set 
   // if the zone traversal
   // determines that a portal
   // linking to the outside
   // zone is currently visible.
   bool renderOutside = false;

   if ( startZone == mZoneRangeStart )
      renderOutside = _traverseZones( state );

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if (state->isObjectRendered(this)) 
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &Zone::renderObject );
      ri->type = RenderPassManager::RIT_Object;
      ri->defaultKey = 0;
      ri->defaultKey2 = 0;
      state->getRenderPass()->addInst( ri );
   }

   return renderOutside;
}

void Zone::renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance* overrideMat )
{
   if (overrideMat)
      return;

   // Only render if the zone render
   // flag is enabled, or this object
   // is currently selected in the editor.
   if ( !smRenderZones && !isSelected() )
      return;
   
   GFXTransformSaver saver;

   MatrixF mat = getRenderTransform();
   mat.scale( getScale() );

   GFX->multWorld( mat );

   GFXStateBlockDesc desc;
   desc.setZReadWrite( true, false );
   desc.setBlend( true );
   desc.setCullMode( GFXCullNone );
   //desc.fillMode = GFXFillWireframe;

   GFXDrawUtil *drawer = GFX->getDrawUtil();
   drawer->drawCube( desc, mObjBox, ColorI( 255, 0, 0, 45 ) );
}

void Zone::_addPortal( Portal *p )
{
   mPortals.push_back( p );

   if ( !p->getZone( 0 ) || !p->getZone( 1 ) )
      mOutdoorZoneVisible = true;
}

void Zone::_removePortal( Portal *p )
{
   bool outdoorVisible = false;
   bool removePortalOutdoorVisible = false;

   if ( !p->getZone( 0 ) || !p->getZone( 1 ) )
      removePortalOutdoorVisible = true;

   mPortals.remove( p );

   for ( U32 i = 0; i < mPortals.size(); i++ )
   {
      Portal *portal = mPortals[i];
      if ( !portal->getZone( 0 ) || !portal->getZone( 1 ) )  
         outdoorVisible = true;
   }

   if ( !outdoorVisible && removePortalOutdoorVisible )
      mOutdoorZoneVisible = false;
}

bool Zone::_traverseZones( SceneState *state )
{
   const Frustum &frust = state->getFrustum();
   Frustum currFrustum( frust );

   Vector<Zone*> zoneStack;
   zoneStack.push_back( this );
   
   smZoneKey++;
   
   bool renderOutside = false;

   while ( zoneStack.size() )
   {
      Zone *zone = zoneStack.last();
      if ( !zone )
      {
         renderOutside = true;
         zoneStack.pop_back();
         continue;
      }

      zone->mZoneKey = smZoneKey;

      SceneState::ZoneState &zoneState = state->getZoneStateNC( zone->mZoneRangeStart );
      zoneState.render = true;

      // We extend the zone box
      // by the bounds of any
      // zones this zone connects
      // to in order to properly
      // grab the potentially rendered
      // objects during scene traversal.
      mZoneBox.extend( zone->getWorldBox().maxExtents );
      mZoneBox.extend( zone->getWorldBox().minExtents );

      zoneStack.pop_back();

      // Only do this sort if the
      // zone has more than one portal.
      Vector<Portal*> tmpPortals;
      if ( zone->mPortals.size() > 1 )
      {
         gSortPoint = state->getCameraPosition();
         for ( U32 i = 0; i < zone->mPortals.size(); i++ )
         {
            Portal *portal = zone->mPortals[i];
            if ( !frust.intersectOBB( portal->getOBBPoints() ) )
               continue;

            tmpPortals.push_back( portal );
         }

         // Sort portals by distance from near to far.
         dQsort( tmpPortals.address(), tmpPortals.size(), sizeof( Portal* ), cmpZonePortalDistance ); 
      }
      else
         tmpPortals.merge( zone->mPortals );

      for ( U32 i = 0; i < tmpPortals.size(); i++ )
      {
         // Is the Portal in the frustum?
         // If so, we need to process
         // the Zone that it's connected to that
         // is not us.
         Portal *portal = tmpPortals[i];
         SceneState::ZoneState &portalState = state->getZoneStateNC( portal->getZoneRangeStart() );

         // If the portal is in the frustum
         // go ahead and set its zone state render
         // variable to true.
         if ( portal->getPointZone( frust.getPosition() ) )
            portalState.render = true;

         if ( !currFrustum.intersectOBB( portal->getOBBPoints() ) )
            continue;

         // Also set it if the current frustum
         // intersects the portal.
         portalState.render = true;

         Frustum newFrustum( frust );
         portal->generatePortalFrustum( state, &newFrustum );
         newFrustum.invert();

         // We set the currFrustum 
         // to the newFrustum in order
         // to ensure that the visibility
         // of portals further down the chain
         // is determined by the clipped down
         // frustum.
         currFrustum = newFrustum;

         // If this zone is not us,
         // and is not null, set the
         // new zone key on it, and set
         // the sized down frustum on it as well.
         Zone *z0 = portal->getZone( 0 );
         if ( z0 && z0->mZoneKey != smZoneKey )
         {
            SceneState::ZoneState &z0State = state->getZoneStateNC( z0->mZoneRangeStart );
            z0State.frustum = newFrustum;

            z0->mZoneKey = smZoneKey;
            zoneStack.push_back( z0 );
         }
         else if ( !z0 )
         {
            // If the zone is null,
            // set the sized down frustum
            // on the outside zone instead,
            // since this portal links to the outside.
            SceneState::ZoneState &z0State = state->getZoneStateNC( 0 );
        
            z0State.frustum = newFrustum;
            zoneStack.push_back( NULL );
         }

         // Same thing as above, but
         // for the second zone the 
         // portal holds on to.
         Zone *z1 = portal->getZone( 1 );
         if ( z1 && z1->mZoneKey != smZoneKey )
         {
            SceneState::ZoneState &z1State = state->getZoneStateNC( z1->mZoneRangeStart );
            
            z1State.frustum = newFrustum;
            z1->mZoneKey = smZoneKey;
            zoneStack.push_back( z1 );
         }
         else if ( !z1 )
         {            
            SceneState::ZoneState &zoneState = state->getZoneStateNC( 0 );
      
            zoneState.frustum = newFrustum;
            zoneStack.push_back( NULL );
         }
      }
   }

   return renderOutside;
}
