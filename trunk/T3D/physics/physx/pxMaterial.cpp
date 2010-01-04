//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxMaterial.h"

#include "T3D/physics/physX/px.h"
#include "T3D/physics/physX/pxWorld.h"
#include "T3D/physics/physicsPlugin.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"


IMPLEMENT_CO_DATABLOCK_V1( PxMaterial );

IMPLEMENT_CONSOLETYPE( PxMaterial )
IMPLEMENT_GETDATATYPE( PxMaterial )
IMPLEMENT_SETDATATYPE( PxMaterial )

PxMaterial::PxMaterial()
: mNxMat( NULL ),
  mNxMatId( -1 ),
  restitution( 0.0f ),
  staticFriction( 0.1f ),
  dynamicFriction( 0.95f ),
  mServer( false )
{
}

PxMaterial::~PxMaterial()
{
}

void PxMaterial::consoleInit()
{
   Parent::consoleInit();
}

void PxMaterial::initPersistFields()
{
   Parent::initPersistFields();

   addGroup("PxMaterial");		

      addField( "restitution", TypeF32, Offset( restitution, PxMaterial ) );
      addField( "staticFriction", TypeF32, Offset( staticFriction, PxMaterial ) );
      addField( "dynamicFriction", TypeF32,	Offset( dynamicFriction, PxMaterial ) );

   endGroup("PxMaterial");		
}

void PxMaterial::onStaticModified( const char *slotName, const char *newValue )
{
   if ( isProperlyAdded() && mNxMat != NULL )
   {      
      mNxMat->setRestitution( restitution );
      mNxMat->setStaticFriction( staticFriction );
      mNxMat->setDynamicFriction( dynamicFriction );
   }
}

bool PxMaterial::preload( bool server, String &errorBuffer )
{
   mServer = server;

   PxWorld *world = dynamic_cast<PxWorld*>( gPhysicsPlugin->getWorld( server ? "server" : "client" ) );   

   if ( !world )
   {
      // TODO: Error... in error buffer?
      return false;
   }   

   NxMaterialDesc	material;
   material.restitution = restitution;
   material.staticFriction	= staticFriction;
   material.dynamicFriction	= dynamicFriction;

   mNxMat = world->createMaterial( material );
   mNxMatId = mNxMat->getMaterialIndex();

   if ( mNxMatId == -1 )
   {
      errorBuffer = "PxMaterial::preload() - unable to create material!";
      return false;
   }

   return Parent::preload( server, errorBuffer );
}

void PxMaterial::packData( BitStream* stream )
{
   Parent::packData( stream );
   
   stream->write( restitution );
   stream->write( staticFriction );
   stream->write( dynamicFriction );
}

void PxMaterial::unpackData( BitStream* stream )
{
   Parent::unpackData( stream );

   stream->read( &restitution );
   stream->read( &staticFriction );
   stream->read( &dynamicFriction );
}
