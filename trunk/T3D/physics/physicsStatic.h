//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PHYSICSSTATIC_H_
#define _T3D_PHYSICS_PHYSICSSTATIC_H_

#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif


class MatrixF;
class Point3F;


/// Simple physics object that is normally static during gameplay.
class PhysicsStatic
{
public:

   /// A generic signal triggered when a PhysicsStatic object is
   /// deleted ( or modified ). Useful for other objects which cache static 
   /// physics objects as an optimization but need to not crash when in an editor
   /// situation where normally static objects can change.
   static Signal<void()> smDeleteSignal;

   virtual ~PhysicsStatic() { smDeleteSignal.trigger(); }

   /// This is not intended for movement during gameplay, but 
   /// only for infrequent changes when editing the mission.
   virtual void setTransform( const MatrixF &xfm ) = 0;

   /// This is not intended for scaling during gameplay, but 
   /// only for infrequent changes when editing the mission.
   virtual void setScale( const Point3F &scale ) = 0;

   /// This is used to signal that the Torque object has
   /// changed its collision shape and it needs to be updated.
   virtual void update() {}
};


#endif // _T3D_PHYSICS_PHYSICSSTATIC_H_