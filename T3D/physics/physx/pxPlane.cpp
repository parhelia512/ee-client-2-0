//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxPlane.h"

#include "T3D/physics/physX/px.h"
#include "T3D/physics/physX/pxWorld.h"

#include "T3D/groundPlane.h"

PxPlane::PxPlane() :
   mActor( NULL ),
   mWorld( NULL )
{
}

PxPlane::~PxPlane()
{
   _releaseActor();
}

void PxPlane::_releaseActor()
{
   if ( !mWorld )
      return;

   if ( mActor )
      mWorld->releaseActor( *mActor );

   mWorld = NULL;
   mActor = NULL;
}

bool PxPlane::init( GroundPlane *plane, PxWorld *world )
{
   // Note: PhysX plane shapes DO NOT work with
   // character controllers! Thus a gigantic box
   // is used instead.

   NxActorDesc actorDesc;
   NxBoxShapeDesc boxDesc;
   boxDesc.dimensions = NxVec3( 20000.0f, 20000.0f, 100.0f);
   actorDesc.shapes.pushBack( &boxDesc );
   actorDesc.body = NULL;
   actorDesc.globalPose.id();
   actorDesc.globalPose.t = NxVec3( 0, 0, -100.0f );

   NxScene *scene = world->getScene();
   if ( !scene )
      return false;

   mUserData.setObject( plane );
   actorDesc.userData = &mUserData;

   mWorld = world;
   mActor = scene->createActor( actorDesc );

   return true;
}

PxPlane* PxPlane::create( GroundPlane *groundPlane, PxWorld *world )
{
   AssertFatal( world, "PxPlane::create() - No world found!" );

   PxPlane *plane = new PxPlane();

   if ( !plane->init( groundPlane, world ) )
   {
      delete plane;
      return NULL;
   }

   return plane;
}

void PxPlane::setTransform( const MatrixF &xfm )
{
   // GroundPlane cannot be transformed, 
   // so we don't do any work here.
}

void PxPlane::setScale( const Point3F &scale )
{
}