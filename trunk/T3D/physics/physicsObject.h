//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PHYSICSOBJECT_H_
#define _T3D_PHYSICS_PHYSICSOBJECT_H_


///
class PhysicsObject
{
public:

   // No constructor.
   // You must initialize these base members to the correct values yourself.

   inline bool isServerObject() const { return mServerObject; }
   inline bool isClientObject() const { return !mServerObject; }
   inline bool isSinglePlayer() const { return mSinglePlayer; }

   static void registerTypes() {}

protected:

   /// Is this a client or server side object   
   bool mServerObject;
   /// Does this object do special work in a single player situation?
   bool mSinglePlayer;
};

#endif // _T3D_PHYSICS_PHYSICSOBJECT_H_