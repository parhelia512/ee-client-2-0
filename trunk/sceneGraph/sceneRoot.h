//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SCENEROOT_H_
#define _SCENEROOT_H_

//Includes
#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif

class Portal;

/// Root of scene graph.
class SceneRoot : public SceneObject
{
   typedef SceneObject Parent;
   friend class Portal;

  protected:

   static U32 smPortalKey;

   Vector<Portal*> mPortals;

   bool onSceneAdd(SceneGraph *graph);
   void onSceneRemove();

   bool getOverlappingZones(SceneObject *obj, U32 *zones, U32 *numZones);

   bool prepRenderImage(SceneState *state, const U32 stateKey, const U32 startZone,
                        const bool modifyBaseZoneState);

   bool scopeObject(const Point3F&        rootPosition,
                    const F32             rootDistance,
                    bool*                 zoneScopeState);

   void _addPortal( Portal *p );
   void _removePortal( Portal *p );
   void _traverseZones( SceneState *state );

  public:
   SceneRoot();
   ~SceneRoot();
};

extern SceneRoot* gClientSceneRoot;     ///< Client's scene graph root.
extern SceneRoot* gServerSceneRoot;     ///< Server's scene graph root.

#endif //_SCENEROOT_H_
