//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxPlugin.h"

#include "sim/netObject.h"
#include "T3D/tsStatic.h"
#include "terrain/terrData.h"
#include "T3D/player.h"
#include "environment/meshRoad.h"
#include "T3D/groundPlane.h"
#include "T3D/physics/physX/pxWorld.h"
#include "T3D/physics/physX/pxTSStatic.h"
#include "T3D/physics/physX/pxTerrain.h"
#include "T3D/physics/physX/pxPlayer.h"
#include "T3D/physics/physX/pxMeshRoad.h"
#include "T3D/physics/physX/pxPlane.h"
#include "T3D/gameProcess.h"

// This is a global function pre-declared and externed
// in the base PhysicsPlugin class, but only
// defined in the derived plugin class's source file.
bool physicsInitialize()
{
   AssertFatal( gPhysicsPlugin == NULL, "PxPlugin() - Physics plugin already present!" );

   // Only create the plugin if
   // it hasn't been set up AND
   // the PhysX world is successfully
   // initialized.
   bool success = PxWorld::restartSDK( false );

   if ( !success )
   {
      Con::errorf( "Plugin was already initialized!" );
      return false;
   }

   gPhysicsPlugin = new PxPlugin();
   return true;
}

bool physicsDestroy()
{
   PxWorld *clientWorld = static_cast<PxWorld*>( gPhysicsPlugin->getWorld( "client" ) );
   PxWorld *serverWorld = static_cast<PxWorld*>( gPhysicsPlugin->getWorld( "server" ) );

   PxWorld::restartSDK( true, clientWorld, serverWorld );
   delete gPhysicsPlugin;
   gPhysicsPlugin = NULL;

   return gPhysicsPlugin == NULL;
}

PxPlugin::PxPlugin()
{
}

PxPlugin::~PxPlugin()
{
}

PhysicsStatic* PxPlugin::createStatic( NetObject *object )
{
   // What sort of object is this?
   TSStatic *tsStatic = dynamic_cast<TSStatic*>( object );
   TerrainBlock *terrainBlock = dynamic_cast<TerrainBlock*>( object );
   MeshRoad *meshRoad = dynamic_cast<MeshRoad*>( object );
   GroundPlane *plane = dynamic_cast<GroundPlane*>( object );

   // Get the world to use.
   PxWorld *world = static_cast<PxWorld*>( gPhysicsPlugin->getWorld( object->isServerObject() ? "server" : "client" ) );

   // Now create the physics representation for it.

   if ( tsStatic )
      return PxTSStatic::create( tsStatic, world );
   else if ( terrainBlock )
      return PxTerrain::create( terrainBlock, world );
   else if ( meshRoad )
      return PxMeshRoad::create( meshRoad, world );
   else if ( plane ) 
      return PxPlane::create( plane, world );

   return NULL;
}

PhysicsPlayer* PxPlugin::createPlayer( Player *player )
{
   // Get the world to use.
   PxWorld *world = static_cast<PxWorld*>( gPhysicsPlugin->getWorld( player->isServerObject() ? "server" : "client" ) );
   return PxPlayer::create( player, world );
}

bool PxPlugin::isSimulationEnabled() const
{
   bool ret = false;
   PxWorld *world = static_cast<PxWorld*>( getWorld( smClientWorldName ) );
   if ( world )
   {
      ret = world->getEnabled();
      return ret;
   }

   world = static_cast<PxWorld*>( getWorld( smServerWorldName ) );
   if ( world )
   {
      ret = world->getEnabled();
      return ret;
   }

   return ret;
}

void PxPlugin::enableSimulation( const String &worldName, bool enable )
{
   PxWorld *world = static_cast<PxWorld*>( getWorld( worldName ) );
   if ( world )
      world->setEnabled( enable );
}

void PxPlugin::setTimeScale( const F32 timeScale )
{
   // Grab both the client and
   // server worlds and set their time
   // scales to the passed value.
   PxWorld *world = static_cast<PxWorld*>( getWorld( smClientWorldName ) );
   if ( world )
      world->setEditorTimeScale( timeScale );

   world = static_cast<PxWorld*>( getWorld( smServerWorldName ) );
   if ( world )
      world->setEditorTimeScale( timeScale );
}

const F32 PxPlugin::getTimeScale() const
{
   // Grab both the client and
   // server worlds and call 
   // setEnabled( true ) on them.
   PxWorld *world = static_cast<PxWorld*>( getWorld( smClientWorldName ) );
   if ( !world )
   {
      world = static_cast<PxWorld*>( getWorld( smServerWorldName ) );
      if ( !world )
         return 0.0f;
   }
   
   return world->getEditorTimeScale();
}

bool PxPlugin::createWorld( const String &worldName )
{
   Map<StringNoCase, PhysicsWorld*>::Iterator iter = mPhysicsWorldLookup.find( worldName );
   PhysicsWorld *world = NULL;
   
   iter != mPhysicsWorldLookup.end() ? world = (*iter).value : world = NULL; 

   if ( world ) 
   {
      Con::errorf( "PhysXWorld::initWorld - %s world already exists!", worldName.c_str() );
      return false;
   }

   world = new PxWorld();
   
   if ( worldName.equal( smServerWorldName, String::NoCase ) )
      world->initWorld( true, &gServerProcessList );
   else if ( worldName.equal( smClientWorldName, String::NoCase ) )
      world->initWorld( false, &gClientProcessList );
   else
      world->initWorld( true, &gServerProcessList );

   mPhysicsWorldLookup.insert( worldName, world );

   return world != NULL;
}

void PxPlugin::destroyWorld( const String &worldName )
{
   Map<StringNoCase, PhysicsWorld*>::Iterator iter = mPhysicsWorldLookup.find( worldName );
   if ( iter == mPhysicsWorldLookup.end() )
      return;

   PhysicsWorld *world = (*iter).value;
   world->destroyWorld();
   delete world;
   
   mPhysicsWorldLookup.erase( iter );
}

PhysicsWorld* PxPlugin::getWorld( const String &worldName ) const
{
   if ( mPhysicsWorldLookup.isEmpty() )
      return NULL;

   Map<StringNoCase, PhysicsWorld*>::ConstIterator iter = mPhysicsWorldLookup.find( worldName );

   return iter != mPhysicsWorldLookup.end() ? (*iter).value : NULL;
}

PhysicsWorld* PxPlugin::getWorld() const
{
   if ( mPhysicsWorldLookup.size() == 0 )
      return NULL;

   Map<StringNoCase, PhysicsWorld*>::ConstIterator iter = mPhysicsWorldLookup.begin();
   return iter->value;
}

U32 PxPlugin::getWorldCount() const
{ 
   return mPhysicsWorldLookup.size(); 
}