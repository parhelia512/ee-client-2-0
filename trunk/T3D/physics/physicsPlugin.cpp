//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physicsPlugin.h"

#include "console/console.h"
#include "console/simSet.h"
#include "core/strings/stringFunctions.h"
#include "sceneGraph/sceneObject.h"
#include "T3D/physics/physicsObject.h"


PhysicsPlugin *gPhysicsPlugin = NULL;
PhysicsResetSignal PhysicsPlugin::smPhysicsResetSignal;

String PhysicsPlugin::smServerWorldName( "server" );
String PhysicsPlugin::smClientWorldName( "client" );
bool PhysicsPlugin::smSinglePlayer = true;


PhysicsPlugin::PhysicsPlugin()
{
   mPhysicsCleanup = new SimSet();
   mPhysicsCleanup->assignName( "PhysicsCleanupSet" );
   mPhysicsCleanup->registerObject();
   Sim::getRootGroup()->addObject( mPhysicsCleanup );
}

PhysicsPlugin::~PhysicsPlugin()
{
   if ( mPhysicsCleanup )
      mPhysicsCleanup->deleteObject();
}

void PhysicsPlugin::registerObjectType( AbstractClassRep *torqueType, CreatePhysicsObjectFn createFn )
{
   StringNoCase className( torqueType->getClassName() );
   mCreateFnMap.insert( className, createFn );
}

void PhysicsPlugin::unregisterObjectType( AbstractClassRep *torqueType )
{
   StringNoCase className( torqueType->getClassName() );
   CreateFnMap::Iterator iter = mCreateFnMap.find( className );
   if ( iter != mCreateFnMap.end() )
      mCreateFnMap.erase( iter );
}

PhysicsObject* PhysicsPlugin::createPhysicsObject( SceneObject *torqueObj )
{
   StringNoCase className( torqueObj->getClassName() );
   CreateFnMap::Iterator iter = mCreateFnMap.find( className );
   if ( iter == mCreateFnMap.end() )
   {
      Con::warnf( "PhysicsPlugin::createPhysicsObject - abstract class %s was not registered.", className.c_str() );
      return NULL;
   }

   return iter->value( torqueObj );
}

// Used to check if a physics plugin exists.
// This is useful for determining whether or not
// to initialize the Physics tools menu in the editor.
ConsoleFunction( physicsPluginPresent, bool, 1, 1, "bool ret = physicsPluginPresent()" )
{
   return gPhysicsPlugin != NULL;
}

ConsoleFunction( physicsInit, bool, 1, 1, "physicsInit()" )
{
   if ( gPhysicsPlugin )
   {
      Con::errorf( "Physics plugin already initialized!" );
      return false;
   }

   #ifdef TORQUE_PHYSICS_ENABLED
      extern bool physicsInitialize();
      return physicsInitialize();
   #else
      return false;
   #endif
}

ConsoleFunction( physicsDestroy, bool, 1, 1, "physicsDestroy()" )
{
   #ifdef TORQUE_PHYSICS_ENABLED
      extern bool physicsDestroy();
      return physicsDestroy();
   #else
      return false;
   #endif
}

ConsoleFunction( physicsInitWorld, bool, 2, 2, "physicsInitWorld( String worldName )" )
{
   return gPhysicsPlugin && gPhysicsPlugin->createWorld( String( argv[1] ) );
}

ConsoleFunction( physicsDestroyWorld, void, 2, 2, "physicsDestroyWorld( String worldName )" )
{
   if ( gPhysicsPlugin )
      gPhysicsPlugin->destroyWorld( String( argv[1] ) );
}


// Control/query of the stop/started state
// of the currently running simulation.
ConsoleFunction( physicsStartSimulation, void, 2, 2, "physicsStartSimulation( String worldName )" )
{
   if ( gPhysicsPlugin )
      gPhysicsPlugin->enableSimulation( String( argv[1] ), true );
}

ConsoleFunction( physicsStopSimulation, void, 2, 2, "physicsStopSimulation( String worldName )" )
{
   if ( gPhysicsPlugin )
      gPhysicsPlugin->enableSimulation( String( argv[1] ), false );
}

ConsoleFunction( physicsSimulationEnabled, bool, 1, 1, "physicsSimulationEnabled()" )
{
   return gPhysicsPlugin && gPhysicsPlugin->isSimulationEnabled();
}

// Used for slowing down time on the
// physics simulation, and for pausing/restarting
// the simulation.
ConsoleFunction( physicsSetTimeScale, void, 2, 2, "physicsSetTimeScale( F32 scale )" )
{
   if ( gPhysicsPlugin )
      gPhysicsPlugin->setTimeScale( dAtof( argv[1] ) );
}

// Get the currently set time scale.
ConsoleFunction( physicsGetTimeScale, F32, 1, 1, "physicsGetTimeScale()" )
{
   return gPhysicsPlugin && gPhysicsPlugin->getTimeScale();
}

// Used to send a signal to objects in the
// physics simulation that they should store
// their current state for later restoration,
// such as when the editor is closed.
ConsoleFunction( physicsStoreState, void, 1, 1, "physicsStoreState()" )
{
   PhysicsPlugin::getPhysicsResetSignal().trigger( PhysicsResetEvent_Store );
}

// Used to send a signal to objects in the
// physics simulation that they should restore
// their saved state, such as when the editor is opened.
ConsoleFunction( physicsRestoreState, void, 1, 1, "physicsRestoreState()" )
{
   // First delete all the cleanup objects.
   if ( gPhysicsPlugin && gPhysicsPlugin->getPhysicsCleanup() )
      gPhysicsPlugin->getPhysicsCleanup()->deleteAllObjects();

   PhysicsPlugin::getPhysicsResetSignal().trigger( PhysicsResetEvent_Restore );
}
