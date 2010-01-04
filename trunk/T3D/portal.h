//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------


#ifndef _PORTAL_H_
#define _PORTAL_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif

#ifndef _ZONE_H_
#include "T3D/zone.h"
#endif

///
class Portal : public Zone
{
   typedef Zone Parent;

protected:
   
   static bool smRenderPortals;

   enum
   {
      TransformMask = Parent::NextFreeMask << 0,
      NextFreeMask = Parent::NextFreeMask << 1,
   };

   U32 mPortalKey;

   Point3F mOBBPoints[8];
   Point3F mOrientedPortalPoints[4];

   SimObjectPtr<Zone> mZones[2];

   // SceneObject
   void onRezone();

   //
   void _clearZones();

public:

   Portal();
   virtual ~Portal();

   // SimObject
   DECLARE_CONOBJECT( Portal );
   
   static void consoleInit();

   static void initPersistFields();
   bool onAdd();
   void onRemove();

   // NetObject
   U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   void unpackUpdate( NetConnection *conn, BitStream *stream );

   // SceneObject
   U32 getPointZone( const Point3F &p );
   bool getOverlappingZones(  SceneObject *obj,
                              U32 *zones,
                              U32 *numZones );
   bool scopeObject( const Point3F &rootPosition,
                     const F32 rootDistance,
                     bool *zoneScopeState );
   void setTransform( const MatrixF &mat );
   bool prepRenderImage(   SceneState* state, 
                           const U32 stateKey,
                           const U32 startZone, 
                           const bool modifyBaseState );

   /// Renders the one and only sky zone if it
   /// exists on the client.
   void renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance* overrideMat );

   Zone* getZone( U32 zoneNum ) { return mZones[zoneNum]; }
   void generatePortalFrustum( SceneState *state, Frustum *outFrustum );

   void getBoxCorners( Point3F points[2] ) const;
   void generateOBBPoints();
   Point3F* getOBBPoints() { return &mOBBPoints[0]; }
   Point3F* getOrientedPortalPoints() { return &mOrientedPortalPoints[0]; }

   U32 getPortalKey() const { return mPortalKey; }
   void setPortalKey( U32 key ) { mPortalKey = key; }
};

#endif // _PORTAL_H_

