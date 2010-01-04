//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PXUTILS_H_
#define _PXUTILS_H_


class NxActor;


namespace PxUtils {

   /// Debug render an actor, loops through all shapes
   /// and translates primitive types into drawUtil calls.
   void drawActor( NxActor *inActor );

} // namespace PxUtils

#endif // _PXUTILS_H_