//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PHYSICSWORLD_H_
#define _T3D_PHYSICS_PHYSICSWORLD_H_

#ifndef _TORQUE_TYPES_H_
#include "platform/types.h"
#endif

class ProcessList;
class Point3F;
struct RayInfo;

class PhysicsWorld
{

public:

   virtual ~PhysicsWorld() {};

   virtual bool initWorld( bool isServer, ProcessList *processList ) = 0;
   virtual void destroyWorld() = 0;

   /// An abstract way to raycast into any type of PhysicsWorld, in a way 
   /// that mirrors a Torque-style raycast.  
   //
   /// This method is not fully developed or very sophisticated. For example, 
   /// there is no system of collision groups or raycast masks, which could 
   /// be very complex to write in a PhysicsPlugin-Abstract way...
   //
   // Optional forceAmt parameter will also apply a force to hit objects.

   virtual bool castRay( const Point3F &startPnt, const Point3F &endPnt, RayInfo *ri, const Point3F &impulse ) = 0;

   virtual void explosion( const Point3F &pos, F32 radius, F32 forceMagnitude ) = 0;
};


#endif // _T3D_PHYSICS_PHYSICSWORLD_H_