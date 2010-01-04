//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PXPLANE_H_
#define _T3D_PHYSICS_PXPLANE_H_

#ifndef _T3D_PHYSICS_PHYSICSSTATIC_H_
#include "T3D/physics/physicsStatic.h"
#endif
#ifndef _PXUSERDATA_H_
#include "T3D/physics/physx/pxUserData.h"
#endif

class NxActor;
class PxWorld;
class GroundPlane;

class PxPlane : public PhysicsStatic
{

protected:

   PxWorld *mWorld;
   NxActor *mActor;
   
   /// The userdata object assigned to our actor.
   PxUserData mUserData;

   void _releaseActor();

   PxPlane();

public:

   virtual ~PxPlane();
   
   // PhysicsStatic
   void setTransform( const MatrixF &xfm );
   void setScale( const Point3F &scale );

   bool init( GroundPlane *groundPlane, PxWorld *world );

   static PxPlane* create( GroundPlane *plane, PxWorld *world );

};

#endif // _T3D_PHYSICS_PXPLANE_H_
