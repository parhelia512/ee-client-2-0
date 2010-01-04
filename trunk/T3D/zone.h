//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------


#ifndef _ZONE_H_
#define _ZONE_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif

#ifndef _MATHUTIL_FRUSTUM_H_
#include "math/util/frustum.h"
#endif

class Portal;
class SceneState;

///
class Zone : public SceneObject
{
   typedef SceneObject Parent;

   friend class Portal;

protected:

   static U32 smZoneKey;
   static bool smRenderZones;

   enum
   {
      TransformMask = Parent::NextFreeMask << 0,
      NextFreeMask = Parent::NextFreeMask << 1,
   };

   Vector<Portal*> mPortals;

   U32 mZoneKey;
   U32 mPortalKey;

   Box3F mZoneBox;

   bool mOutdoorZoneVisible;

   void _removePortal( Portal *p );

   void _addPortal( Portal *p );

   bool _traverseZones( SceneState *state );

public:

   Zone();
   virtual ~Zone();

   // SimObject
   DECLARE_CONOBJECT( Zone );
   
   static void consoleInit();

   static void initPersistFields();
   bool onAdd();
   void onRemove();

   // NetObject
   U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   void unpackUpdate( NetConnection *conn, BitStream *stream );

   // SceneObject
   void setTransform( const MatrixF &mat );
   bool prepRenderImage(   SceneState* state, 
                           const U32 stateKey,
                           const U32 startZone, 
                           const bool modifyBaseState );
   U32 getPointZone( const Point3F &p );
   bool getOverlappingZones(  SceneObject *obj,
                              U32 *zones,
                              U32 *numZones );
   const Box3F& getZoneBox() const { return mZoneBox; }
   bool scopeObject( const Point3F &rootPosition,
                     const F32 rootDistance,
                     bool *zoneScopeState );

   void renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance* overrideMat );

   const Vector<Portal*>& getPortals() const { return mPortals; }

   U32 getPortalKey() const { return mPortalKey; }
   void setPortalKey( U32 portalKey ) { mPortalKey = portalKey; }
};

#endif // _ZONE_H_

