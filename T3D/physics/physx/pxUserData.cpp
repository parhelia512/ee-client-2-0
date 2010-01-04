//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxUserData.h"

#include "math/mPoint3.h"
#include "T3D/fx/particleEmitter.h"
#include "core/strings/stringFunctions.h"
#include "T3D/physics/physX/px.h"
#include <typeinfo.h>


//-------------------------------------------------------------------------
// PxUserData
//-------------------------------------------------------------------------

PxUserData::PxUserData()
   : mObject( NULL ),
     mIsBroken( false ),
     mParticleEmitterData( NULL ),
     mCanPush( true )
{
}

PxUserData::~PxUserData()
{
}

PxUserData* PxUserData::getData( const NxActor &actor )
{
   PxUserData *data = (PxUserData*)actor.userData;

   AssertFatal(  !data || dStrcmp( typeid(*data).name(), "class PxUserData" ) == 0,
      String::ToString( "PxUserData::getData() - Wrong user data type on actor '%s'!", actor.getName() ).c_str() );

   return data;
}

/*
//-------------------------------------------------------------------------
// PxBreakableUserData
//-------------------------------------------------------------------------

PxBreakableUserData::PxBreakableUserData()
 : mBreakForce( NULL ),
   mParticleEmitterData( NULL ) 
{
}

PxBreakableUserData::~PxBreakableUserData()
{         
   if ( mBreakForce )
      delete mBreakForce;
}

PxBreakableUserData* PxBreakableUserData::getData( const NxActor &actor )
{
   PxBreakableUserData *data = (PxBreakableUserData*)actor.userData;

   AssertFatal(  !data || dStrcmp( typeid(*data).name(), "class PxBreakableUserData" ) == 0,
      String::ToString( "PxBreakableUserData::getData() - Wrong user data type on actor '%s'!", actor.getName() ).c_str() );

   return data;
}
*/

//-------------------------------------------------------------------------
// PxJointUserData
//-------------------------------------------------------------------------

PxJointUserData* PxJointUserData::getData( const NxJoint &joint )
{
   PxJointUserData *data = (PxJointUserData*)joint.userData;

   AssertFatal(  !data || dStrcmp( typeid(*data).name(), "class PxJointUserData" ) == 0,
      String::ToString( "PxJointUserData::getData() - Wrong user data type on joint '%s'!", joint.getName() ).c_str() );

   return data;
}