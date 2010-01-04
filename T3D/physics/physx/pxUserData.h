//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PXUSERDATA_H_
#define _PXUSERDATA_H_

#ifndef _SIGNAL_H_
#include "core/util/tSignal.h"
#endif
#ifndef _PHYSX_H_
#include "T3D/physics/physX/px.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif

class SceneObject;
class NxActor;
class NxJoint;
class NxController;
class Point3F;


//-------------------------------------------------------------------------
// PxUserData
//-------------------------------------------------------------------------

/// Signal used for contact reports.
///
/// @param ourActor The actor owned by the signaling object.
/// @param hitActor The other actor involved in the contact.
/// @param hitObject The SceneObject that was hit. 
/// @param hitPoint The approximate position of the impact.
/// @param hitForce
///
/// @see PxUserData
///
typedef Signal<void( NxActor *ourActor,
                    NxActor *hitActor,
                    SceneObject *hitObject,
                    const Point3F &hitPoint,
                    const Point3F &hitForce )> PxUserContactSignal;

class ParticleEmitterData;

class PxUserData
{
public:

   /// The constructor.
   PxUserData();

   /// The destructor.
   virtual ~PxUserData();

   /// Returns the user data on an actor.
   static PxUserData* getData( const NxActor &actor );

   void setObject( SceneObject *object ) { mObject = object; }

   const SceneObject* getObject() const { return mObject; }

   SceneObject* getObject() { return mObject; }

   PxUserContactSignal& getContactSignal() { return mContactSignal; }

   // Breakable stuff... does this belong here??
   Vector<NxActor*> mUnbrokenActors;
   Vector<NxActor*> mBrokenActors;
   Vector<NxMat34> mRelXfm;
   ParticleEmitterData *mParticleEmitterData;  
   bool mIsBroken;

   // Can the player push this actor?
   bool mCanPush;   

protected:

   PxUserContactSignal mContactSignal;

   SceneObject *mObject;
};


//-------------------------------------------------------------------------
// PxBreakableUserData
//-------------------------------------------------------------------------

/*
class ParticleEmitterData;

class PxBreakableUserData : public PxUserData
{
public:

   PxBreakableUserData();
   ~PxBreakableUserData();

   static PxBreakableUserData* getData( const NxActor &actor );

   Point3F *mBreakForce;

   Vector<NxActor*> mUnbrokenActors;
   Vector<NxActor*> mBrokenActors;
   ParticleEmitterData *mParticleEmitterData;   
};
*/


//-------------------------------------------------------------------------
// PxJointUserData
//-------------------------------------------------------------------------

typedef Signal<void(NxReal, NxJoint&)> PxOnJointBreakSignal;

class PxJointUserData : public PxUserData
{
public:

   PxJointUserData() {}

   static PxJointUserData* getData( const NxJoint &joint );

   PxOnJointBreakSignal& getOnJointBreakSignal() { return mOnJointBreakSignal; }

protected:

   PxOnJointBreakSignal mOnJointBreakSignal;
};

#endif // _PXUSERDATA_H_   