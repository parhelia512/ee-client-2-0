//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxWorld.h"

#include "T3D/physics/physX/px.h"
#include "T3D/physics/physX/pxPlugin.h"
#include "T3D/physics/physX/pxMaterial.h"
#include "T3D/physics/physX/pxContactReporter.h"
#include "T3D/physics/physX/pxUserData.h"
#include "T3D/physics/physX/pxTSStatic.h"
#include "T3D/physics/physX/pxTerrain.h"
#include "T3D/physics/physX/pxStream.h"

#ifdef TORQUE_DEBUG
#include <crtdbg.h>
#endif
#include <NxUserAllocatorDefault.h>
#include <CCTAllocator.h>
#include <NxControllerManager.h>
#include <CharacterControllerManager.h>
#include <NxController.h>

#include "core/stream/bitStream.h"
#include "platform/profiler.h"
#include "sim/netConnection.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/util/safeDelete.h"
#include "T3D/tsstatic.h"
#include "T3D/gameProcess.h"
#include "gfx/sim/debugDraw.h"

#include <NXU_helper.h>


static const F32 PhysicsStepTime = (F32)TickMs / 1000.0f;
static const U32 PhysicsMaxIterations = 8;
static const F32 PhysicsMaxTimeStep = PhysicsStepTime / 2.0f;

// NOTE: This matches the gravity used for player objects.
const VectorF PxWorld::smDefaultGravity( 0, 0, -20.0 );

NxPhysicsSDK *gPhysicsSDK = NULL;

// The one and only physX console output stream for the process.
PxConsoleStream *gPxConsoleStream = NULL;


   
PxWorld::PxWorld() :
   mScene( NULL ),
   mRigidCompartment( NULL ),
   mConactReporter( NULL ),
   mProcessList( NULL ),
   mIsSimulating( false ),
   mErrorReport( false ),
   mTickCount( 0 ),
   mIsEnabled( false ),
   mEditorTimeScale( 1.0f )
{
	if ( !CCTAllocator::mAllocator )
		CCTAllocator::mAllocator = new NxUserAllocatorDefault();
	mControllerManager = new CharacterControllerManager( CCTAllocator::mAllocator );
} 

PxWorld::~PxWorld()
{
	delete mControllerManager;
}

bool PxWorld::_init( bool isServer, ProcessList *processList )
{
   if ( !gPhysicsSDK )
   {
      Con::errorf( "PhysXWorld::init - PhysXSDK not initialized!" );
      return false;
   }

   // Create the scene description.
   NxSceneDesc sceneDesc;
   sceneDesc.userData = this;

   // Set up default gravity.
   sceneDesc.gravity.set( smDefaultGravity.x, smDefaultGravity.y, smDefaultGravity.z );

   // The master scene is always on the CPU and is used
   // mostly for static shapes.
   sceneDesc.simType = NX_SIMULATION_SW;

   // Threading... seems to improve performance.
   //
   // TODO: I was getting random crashes in debug when doing
   // edit and continue... lets see if i still get them with
   // the threading disabled.
   //
   sceneDesc.flags |= NX_SF_ENABLE_MULTITHREAD | NX_SF_DISABLE_SCENE_MUTEX;
   sceneDesc.threadMask = 0xfffffffe;
   sceneDesc.internalThreadCount = 2;

   // Create the scene.
   mScene = gPhysicsSDK->createScene(sceneDesc);
   if ( !mScene )
   {
      Con::errorf( "PhysXWorld - %s world createScene returned a null scene!", isServer ? "Server" : "Client" );
      return false;
   }

   /*
   // Make note of what we've created.
   String simType = sceneDesc.simType == NX_SIMULATION_SW ? "software" : "hardware";
   String clientOrServer = this == isServer ? "server" : "client";
   Con::printf( "PhysXWorld::init() - Created %s %s simulation!", 
      clientOrServer.c_str(), 
      simType.c_str() );
   */

   mScene->setTiming( PhysicsMaxTimeStep, PhysicsMaxIterations, NX_TIMESTEP_FIXED );

   // TODO: Add dummy actor with scene name!

   // Set the global contact reporter.

      mConactReporter = new PxContactReporter();
      mScene->setUserContactReport( mConactReporter );

   // Set the global PxUserNotify

      mUserNotify = new PxUserNotify();
      mScene->setUserNotify( mUserNotify );

   // Now create the dynamic rigid body compartment which
   // can reside on the hardware when hardware is present.
   /*
   NxCompartmentDesc compartmentDesc;
   compartmentDesc.type = NX_SCT_RIGIDBODY;
   compartmentDesc.deviceCode = NX_DC_PPU_AUTO_ASSIGN; 
   mRigidCompartment = mScene->createCompartment( compartmentDesc );
   if ( !mRigidCompartment )
   {
      Con::errorf( "PhysXWorld - Creation of rigid body compartment failed!" );
      return false;
   }
   */

   // Hook up the tick processing signals for advancing physics.
   //
   // First an overview of how physics and the game ticks
   // interact with each other.
   //
   // In Torque you normally tick the server and then the client
   // approximately every 32ms.  So before the game tick we call 
   // getPhysicsResults() to get the new physics state and call 
   // tickPhysics() when the game tick is done to start processing
   // the next physics state.  This means PhysX is happily doing
   // physics in a separate thread while we're doing rendering,
   // sound, input, networking, etc.
   // 
   // Because your frame rate is rarely perfectly even you can
   // get cases where you may tick the server or the client 
   // several times in a row.  This happens most often in debug
   // mode, but can also happen in release.
   //
   // The simple implementation is to do a getPhysicsResults() and
   // tickPhysics() for each tick.  But this very bad!  It forces
   // immediate results from PhysX which blocks the primary thread
   // and further slows down processing.  It leads to slower and
   // slower frame rates as the simulation is never able to catch
   // up to the current tick.
   //
   // The trick is processing physics once for backlogged ticks
   // with the total of the elapsed tick time.  This is a huge
   // performance gain and keeps you from blocking on PhysX.
   //
   // This does have a side effect that when it occurs you'll get
   // ticks where the physics state hasn't advanced, but this beats
   // single digit frame rates.
   //
   AssertFatal( processList, "PhysXWorld::init() - We need a process list to create the world!" );
   mProcessList = processList;
   mProcessList->preTickSignal().notify( this, &PxWorld::getPhysicsResults );
   mProcessList->postTickSignal().notify( this, &PxWorld::tickPhysics, 1000.0f );
   //mProcessList->catchupModeSignal().notify( this, &PxWorld::enableCatchupMode, 1001.0f );

   return true;
}

void PxWorld::_destroy()
{
   // Make sure the simulation is stopped!
   getPhysicsResults();
   _releaseQueues();

   #ifdef TORQUE_DEBUG

      U32 actorCount = mScene->getNbActors();
      U32 jointCount = mScene->getNbJoints();
      
      if ( actorCount != 0 || jointCount != 0 )
      {
         // Dump the names of any actors or joints that
         // were not released before the destruction of
         // this scene.

         for ( U32 i=0; i < actorCount; i++ )
         {
            const NxActor *actor = mScene->getActors()[i];
            Con::errorf( "Orphan NxActor - '%s'!", actor->getName() );
         }

         mScene->resetJointIterator();
         for ( ;; )
         {
            const NxJoint *joint = mScene->getNextJoint();
            if ( !joint )
               break;

            Con::errorf( "Orphan NxJoint - '%s'!", joint->getName() );
         }

         AssertFatal( false, "PhysXWorld::_destroy() - Some actors and/or joints were not released!" );
      }

   #endif // TORQUE_DEBUG

   //NxCloseCooking();

   // Release the tick processing signals.
   if ( mProcessList )
   {
      mProcessList->preTickSignal().remove( this, &PxWorld::getPhysicsResults );
      mProcessList->postTickSignal().remove( this, &PxWorld::tickPhysics );
      //mProcessList->catchupModeSignal().remove( this, &PxWorld::enableCatchupMode );
      mProcessList = NULL;
   }

   // Destroy the scene.
   if ( mScene )
   {
      // You never destroy compartments... they are
      // released along with the scene.
      mRigidCompartment = NULL;

      // Delete the contact reporter.
      mScene->setUserContactReport( NULL );
      SAFE_DELETE( mConactReporter );

      // First shut down threads... this makes it 
      // safe to release the scene.
      mScene->shutdownWorkerThreads();

      // Release the scene.
      gPhysicsSDK->releaseScene( *mScene );
      mScene = NULL;
   }

   // Try to restart the sdk if we can.
   //restartSDK();
}

bool PxWorld::restartSDK( bool destroyOnly, PxWorld *clientWorld, PxWorld *serverWorld )
{
   // If either the client or the server still exist
   // then we cannot reset the SDK.
   if ( clientWorld || serverWorld )
      return false;

   // Destroy the existing SDK.
   if ( gPhysicsSDK )
   {
      PxTSStatic::freeMeshCache();

      NXU::releasePersistentMemory();
      gPhysicsSDK->release();
      gPhysicsSDK = NULL;

      SAFE_DELETE( gPxConsoleStream );
   }

   // If we're not supposed to restart... return.
   if ( destroyOnly )
      return true;

   gPxConsoleStream = new PxConsoleStream();

   NxPhysicsSDKDesc sdkDesc;
   sdkDesc.flags |= NX_SDKF_NO_HARDWARE;

   NxSDKCreateError error;
   gPhysicsSDK = NxCreatePhysicsSDK(   NX_PHYSICS_SDK_VERSION, 
                                       NULL,
                                       gPxConsoleStream,
                                       sdkDesc,
                                       &error );
   if ( !gPhysicsSDK )
   {
      Con::errorf( "PhysX failed to initialize!  Error code: %d", error );
      Platform::messageBox(   Con::getVariable( "$appName" ),
                              avar("PhysX could not be started!\r\n"
                              "Please be sure you have the latest version of PhysX installed.\r\n"
                              "Error Code: %d", error),
                              MBOk, MIStop );
      Platform::forceShutdown( -1 );
   }

   // Set the default skin width for all actors.
   gPhysicsSDK->setParameter( NX_SKIN_WIDTH, 0.01f );

   return true;
}

void PxWorld::tickPhysics( U32 elapsedMs )
{
   if ( !mScene || !mIsEnabled )
      return;

   // Did we forget to call getPhysicsResults somewhere?
   AssertFatal( !mIsSimulating, "PhysXWorld::tickPhysics() - Already simulating!" );

   // The elapsed time should be non-zero and 
   // a multiple of TickMs!
   AssertFatal(   elapsedMs != 0 &&
                  ( elapsedMs % TickMs ) == 0 , "PhysXWorld::tickPhysics() - Got bad elapsed time!" );

   PROFILE_SCOPE(PxWorld_TickPhysics);

   // Convert it to seconds.
   const F32 elapsedSec = (F32)elapsedMs * 0.001f;

   // For some reason this gets reset all the time
   // and it must be called before the simulate.
   mScene->setFilterOps(   NX_FILTEROP_OR, 
                           NX_FILTEROP_OR, 
                           NX_FILTEROP_AND );
   mScene->setFilterBool( false );
   NxGroupsMask zeroMask;
   zeroMask.bits0=zeroMask.bits1=zeroMask.bits2=zeroMask.bits3=0;
   mScene->setFilterConstant0( zeroMask );
   mScene->setFilterConstant1( zeroMask );

   // Simulate it.
   if ( mEditorTimeScale <= 0.0f )
      Con::printf( "Editor time scale is %g", mEditorTimeScale );

   mScene->simulate( elapsedSec * mEditorTimeScale );
   mScene->flushStream();
   mIsSimulating = true;

   //Con::printf( "%s PhysXWorld::tickPhysics!", this == smClientWorld ? "Client" : "Server" );
}

/*
void PxWorld::enableCatchupMode( GameBase *obj )
{
   PxUserData *testUserData = NULL; 
   NxActor *testActor = NULL;

   if ( obj )
   {
      U32 actorCount = mScene->getNbActors();
      NxActor **actors = mScene->getActors();
    
      for ( U32 i = 0; i < actorCount; i++ )
      {
         testActor = actors[i];

         testUserData = static_cast<PxUserData*>( testActor->userData );

         // If this actor is for our control object
         // or it's a kinematic, skip it.
         if (  !testUserData ||
               testUserData->getObject() == (SceneObject*)obj || 
               testUserData->isStatic() ||
               testActor->readBodyFlag( NX_BF_KINEMATIC ) )
            continue;

         // If we got down here, the actor
         // isn't for our control object,
         // it's not a static, and it's not kinematic.
         
         // So turn it kinematic and 
         // push it into the actor list.
         testActor->raiseBodyFlag( NX_BF_KINEMATIC );
         mCatchupQueue.push_back( testActor );
      }
   }
   else if ( !obj )
   {
      // Easy case, we want to restore the
      // state of all the actors in the catchup queue.
      for ( U32 i = 0; i < mCatchupQueue.size(); i++ )
      {
         testActor = mCatchupQueue[i];
         testActor->clearBodyFlag( NX_BF_KINEMATIC );
      }
   }
}
*/

void PxWorld::releaseWriteLocks()
{
   PxWorld *world = dynamic_cast<PxWorld*>( gPhysicsPlugin->getWorld( "server" ) );

   if ( world )
      world->releaseWriteLock();

   world = dynamic_cast<PxWorld*>( gPhysicsPlugin->getWorld( "client" ) );

   if ( world )
      world->releaseWriteLock();
}

void PxWorld::releaseWriteLock()
{
   if ( !mScene || !mIsSimulating ) 
      return;

   PROFILE_SCOPE(PxWorld_ReleaseWriteLock);

   // We use checkResults here to release the write lock
   // but we do not change the simulation flag or increment
   // the tick count... we may have gotten results, but the
   // simulation hasn't really ticked!
   mScene->checkResults( NX_RIGID_BODY_FINISHED, true );
   AssertFatal( mScene->isWritable(), "PhysXWorld::releaseWriteLock() - We should have been writable now!" );
}

void PxWorld::getPhysicsResults()
{
   if ( !mScene || !mIsSimulating ) 
      return;

   PROFILE_SCOPE(PxWorld_GetPhysicsResults);

   // Get results from scene.
   mScene->fetchResults( NX_RIGID_BODY_FINISHED, true );
   mIsSimulating = false;
   mTickCount++;

   // Take this opportunity to update PxTerrain objects.   
   _updateTerrain();

   // Release any joints/actors that were waiting
   // for the scene to become writable.
   _releaseQueues();

   //Con::printf( "%s PhysXWorld::getPhysicsResults!", this == smClientWorld ? "Client" : "Server" );
}

NxMaterial* PxWorld::createMaterial( NxMaterialDesc &material )
{
   if ( !mScene )
      return NULL;

   // We need the writelock to create a material!
   releaseWriteLock();

   NxMaterial *mat = mScene->createMaterial( material );
   if ( !mat )
      return NULL;

   return mat;
}

NxController* PxWorld::createController( NxControllerDesc &desc )
{
   if ( !mScene )
      return NULL;

   // We need the writelock!
   releaseWriteLock();

   return mControllerManager->createController( mScene, desc );
}

void PxWorld::releaseActor( NxActor &actor )
{
   AssertFatal( &actor.getScene() == mScene, "PhysXWorld::releaseActor() - Bad scene!" );

   // Clear the userdata.
   actor.userData = NULL;   

   // If the scene is not simulating then we have the
   // write lock and can safely delete it now.
   if ( !mIsSimulating )
   {
      bool isStatic = !actor.isDynamic();
      mScene->releaseActor( actor );
      if ( isStatic )
         PhysicsStatic::smDeleteSignal.trigger();
   }
   else
      mReleaseActorQueue.push_back( &actor );
}

void PxWorld::releaseHeightField( NxHeightField &heightfield )
{
   // Always delay releasing a heightfield, for whatever reason,
   // it causes lots of deadlock asserts if we do it here, even if
   // the scene "says" its writable. 
   //
   // Actually this is probably because a heightfield is owned by the "sdk" and 
   // not an individual scene so if either the client "or" server scene are 
   // simulating it asserts, thats just my theory.

   mReleaseHeightFieldQueue.push_back( &heightfield );
}

void PxWorld::releaseJoint( NxJoint &joint )
{
   AssertFatal( &joint.getScene() == mScene, "PhysXWorld::releaseJoint() - Bad scene!" );

   AssertFatal( !mReleaseJointQueue.contains( &joint ), 
      "PhysXWorld::releaseJoint() - Joint already exists in the release queue!" );

   // Clear the userdata.
   joint.userData = NULL;

   // If the scene is not simulating then we have the
   // write lock and can safely delete it now.
   if ( !mIsSimulating )
      mScene->releaseJoint( joint );
   else
      mReleaseJointQueue.push_back( &joint );
}

void PxWorld::releaseCloth( NxCloth &cloth )
{
   AssertFatal( &cloth.getScene() == mScene, "PhysXWorld::releaseCloth() - Bad scene!" );

   // Clear the userdata.
   cloth.userData = NULL;

   // If the scene is not simulating then we have the
   // write lock and can safely delete it now.
   if ( !mIsSimulating )
      mScene->releaseCloth( cloth );
   else
      mReleaseClothQueue.push_back( &cloth );
}

void PxWorld::releaseClothMesh( NxClothMesh &clothMesh )
{
   // We need the writelock to release.
   releaseWriteLock();

   gPhysicsSDK->releaseClothMesh( clothMesh );
}

void PxWorld::releaseController( NxController &controller )
{
   // TODO: This isn't safe to do with actors and
   // joints, so we probably need a queue like we
   // do for them.

   // We need the writelock to release.
   releaseWriteLock();

   mControllerManager->releaseController( controller );
}

void PxWorld::_updateTerrain()
{
   for ( U32 i = 0; i < mTerrainUpdateQueue.size(); i++ )
   {
      mTerrainUpdateQueue[i]->_scheduledUpdate();
      mTerrainUpdateQueue.erase_fast(i);
      i--;
   }
}

void PxWorld::_releaseQueues()
{
   AssertFatal( mScene, "PhysXWorld::_releaseQueues() - The scene is null!" );

   // We release joints still pending in the queue 
   // first as they depend on the actors.
   for ( S32 i = 0; i < mReleaseJointQueue.size(); i++ )
   {
      NxJoint *currJoint = mReleaseJointQueue[i];
      mScene->releaseJoint( *currJoint );
   }
   
   // All the joints should be released, clear the queue.
   mReleaseJointQueue.clear();

   // Now release any actors still pending in the queue.
   for ( S32 i = 0; i < mReleaseActorQueue.size(); i++ )
   {
      NxActor *currActor = mReleaseActorQueue[i];
      
      bool isStatic = !currActor->isDynamic();

      mScene->releaseActor( *currActor );

      if ( isStatic )
         PhysicsStatic::smDeleteSignal.trigger();
   }

   // All the actors should be released, clear the queue.
   mReleaseActorQueue.clear();

   // Now release any cloth still pending in the queue.
   for ( S32 i = 0; i < mReleaseClothQueue.size(); i++ )
   {
      NxCloth *currCloth = mReleaseClothQueue[i];
      mScene->releaseCloth( *currCloth );
   }

   // All the actors should be released, clear the queue.
   mReleaseClothQueue.clear();

   // Release heightfields that don't still have references.
   for ( S32 i = 0; i < mReleaseHeightFieldQueue.size(); i++ )
   {
      NxHeightField *currHeightfield = mReleaseHeightFieldQueue[i];
      
      if ( currHeightfield->getReferenceCount() == 0 )
      {
         gPhysicsSDK->releaseHeightField( *currHeightfield );      
         mReleaseHeightFieldQueue.erase_fast( i );
         i--;
      }
   }
}

void PxWorld::setEnabled( bool enabled )
{
   mIsEnabled = enabled;

   if ( !mIsEnabled )
      getPhysicsResults();
}

ConsoleFunction( physXRemoteDebuggerConnect, bool, 1, 3, "" )
{
   if ( !gPhysicsSDK )  
   {
      Con::errorf( "PhysX SDK not initialized!" );
      return false;
   }

   NxRemoteDebugger *debugger = gPhysicsSDK->getFoundationSDK().getRemoteDebugger();

   if ( debugger->isConnected() )
   {
      Con::errorf( "RemoteDebugger already connected... call disconnect first!" );
      return false;
   }

   const UTF8 *host = "localhost";
   U32 port = 5425;

   if ( argc >= 2 )
      host = argv[1];
   if ( argc >= 3 )
      port = dAtoi( argv[2] );

   // Before we connect we need to have write access
   // to both the client and server worlds.
   PxWorld::releaseWriteLocks();

   // Connect!
   debugger->connect( host, port );
   if ( !debugger->isConnected() )
   {
      Con::errorf( "RemoteDebugger failed to connect!" );
      return false;
   }

   Con::printf( "RemoteDebugger connected to %s at port %u!", host, port );
   return true;
}

ConsoleFunction( physXRemoteDebuggerDisconnect, void, 1, 1, "" )
{
   if ( !gPhysicsSDK )  
   {
      Con::errorf( "PhysX SDK not initialized!" );
      return;
   }

   NxRemoteDebugger *debugger = gPhysicsSDK->getFoundationSDK().getRemoteDebugger();

   if ( debugger->isConnected() )
   {
      debugger->flush();
      debugger->disconnect();
      Con::printf( "RemoteDebugger disconnected!" );
   }
}

bool PxWorld::initWorld( bool isServer, ProcessList *processList )
{
   /* This stuff is handled outside.
   PxWorld* world = PxWorld::getWorld( isServer );
   if ( world )
   {
      Con::errorf( "PhysXWorld::initWorld - %s world already exists!", isServer ? "Server" : "Client" );
      return false;
   }
   */
   
   if ( !_init( isServer, processList ) )
      return false;

   return true;
}

void PxWorld::destroyWorld()
{
   //PxWorld* world = PxWorld::getWorld( serverWorld );
   /*
   if ( !world )
   {
      Con::errorf( "PhysXWorld::destroyWorld - %s world already destroyed!", serverWorld ? "Server" : "Client" );
      return;
   }
   */
   //world->_destroy();
   //delete world;

   _destroy();
}

bool PxWorld::castRay( const Point3F &startPnt, const Point3F &endPnt, RayInfo *ri, const Point3F &impulse )
{
   NxRay worldRay;    
   worldRay.orig = pxCast<NxVec3>( startPnt );
   worldRay.dir = pxCast<NxVec3>( endPnt - startPnt );
   NxF32 maxDist = worldRay.dir.magnitude();
   worldRay.dir.normalize();

   NxRaycastHit hitInfo;
   NxShape *hitShape = mScene->raycastClosestShape( worldRay, NX_ALL_SHAPES, hitInfo, 0xffffffff, maxDist );

   if ( !hitShape )
      return false;

   //if ( hitShape->userData != NULL )
   //   return false;

   NxActor &actor = hitShape->getActor();
   PxUserData *userData = PxUserData::getData( actor );

   if ( ri )
   {
      ri->object = ( userData != NULL ) ? userData->getObject() : NULL;
      
      // If we were passed a RayInfo, we can only return true signifying a collision
      // if we hit an object that actually has a torque object associated with it.
      //
      // In some ways this could be considered an error, either a physx object
      // has raycast-collision enabled that shouldn't or someone did not set
      // an object in this actor's userData.
      //
      if ( ri->object == NULL )
         return false;

      ri->distance = hitInfo.distance;
      ri->normal = pxCast<Point3F>( hitInfo.worldNormal );
      ri->point = pxCast<Point3F>( hitInfo.worldImpact );
      ri->t = maxDist / hitInfo.distance;
   }

   if ( impulse.isZero() ||
        !actor.isDynamic() ||
        actor.readBodyFlag( NX_BF_KINEMATIC ) )
      return true;
      
   NxVec3 force = pxCast<NxVec3>( impulse );//worldRay.dir * forceAmt; 
   actor.addForceAtPos( force, hitInfo.worldImpact, NX_IMPULSE );

   return true;
}

void PxWorld::explosion( const Point3F &pos, F32 radius, F32 forceMagnitude )
{
   // Find Actors at the position within the radius
   // and apply force to them.

   NxVec3 nxPos = pxCast<NxVec3>( pos );
   NxShape **shapes = (NxShape**)NxAlloca(10*sizeof(NxShape*));
   NxSphere worldSphere( nxPos, radius );

   NxU32 numHits = mScene->overlapSphereShapes( worldSphere, NX_ALL_SHAPES, 10, shapes, NULL );

   for ( NxU32 i = 0; i < numHits; i++ )
   {
      NxActor &actor = shapes[i]->getActor();
      
      bool dynamic = actor.isDynamic();
      
      if ( !dynamic )
         continue;

      bool kinematic = actor.readBodyFlag( NX_BF_KINEMATIC );
      
      if ( kinematic )
         continue;

      NxVec3 force = actor.getGlobalPosition() - nxPos;
      force.normalize();
      force *= forceMagnitude;

      actor.addForceAtPos( force, nxPos, NX_IMPULSE, true );
   }
}

void PxWorld::scheduleUpdate( PxTerrain *terrain )
{
   mTerrainUpdateQueue.push_back_unique( terrain );
}

void PxWorld::unscheduleUpdate( PxTerrain *terrain )
{
   mTerrainUpdateQueue.remove( terrain );
}

ConsoleFunction( castForceRay, const char*, 4, 4, "( Point3F startPnt, Point3F endPnt, VectorF impulseVec )" )
{
   PhysicsWorld *world = gPhysicsPlugin->getWorld( "server" );
   if ( !world )
      return NULL;
   
   char *returnBuffer = Con::getReturnBuffer(256);

   Point3F impulse;
   Point3F startPnt, endPnt;
   dSscanf( argv[1], "%f %f %f", &startPnt.x, &startPnt.y, &startPnt.z );
   dSscanf( argv[2], "%f %f %f", &endPnt.x, &endPnt.y, &endPnt.z );
   dSscanf( argv[3], "%f %f %f", &impulse.x, &impulse.y, &impulse.z );

   Point3F hitPoint;

   RayInfo rinfo;

   bool hit = world->castRay( startPnt, endPnt, &rinfo, impulse );

   DebugDrawer *ddraw = DebugDrawer::get();
   if ( ddraw )
   {
      ddraw->drawLine( startPnt, endPnt, hit ? ColorF::RED : ColorF::GREEN );
      ddraw->setLastTTL( 3000 );
   }

   if ( hit )
   {
      dSprintf(returnBuffer, 256, "%g %g %g",
            rinfo.point.x, rinfo.point.y, rinfo.point.z );
      return returnBuffer;
   }
   else 
      return NULL;
}