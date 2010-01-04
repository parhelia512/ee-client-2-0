//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxPlayer.h"

#include "T3D/physics/physX/pxCapsulePlayer.h"
#include "T3D/physics/physX/pxBoxPlayer.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physX/pxWorld.h"
#include "T3D/physics/physX/pxUserData.h"
#include "T3D/physics/physX/pxCasts.h"
#include "T3D/physics/physX/pxSingleActor.h"
#include "T3D/physics/physicsStatic.h"
#include "gfx/gfxDrawUtil.h"
#include "sim/netConnection.h"
#include "collision/collision.h"
#include "T3D/player.h"

#include <NxControllerManager.h>
#include <CharacterControllerManager.h>
#include <NxCapsuleController.h>
#include <ControllerManager.h>


PxPlayer::PxPlayer( Player *player, PxWorld *world )
 : PhysicsPlayer( player ),
   mController( NULL ),
   mSkinWidth( 0.1f ),
   mSize(1,1,1),
   mWorld( world ),
   mDummyMove( false ),
   mPositionSaved( false ),
   mSavedServerPosition( 0,0,0 )
{
   PhysicsStatic::smDeleteSignal.notify( this, &PxPlayer::_onStaticDeleted );
}

PxPlayer::~PxPlayer()
{
   // If a single player (dummy) PxPlayer we might not have a controller.
   if ( mController )
	   mWorld->releaseController( *mController );

   // JCFHACK: see PxPlayer::create
   if ( mSinglePlayer && mServerObject )
   {
      gClientProcessList.preTickSignal().remove( this, &PxPlayer::_savePosition );
      gServerProcessList.preTickSignal().remove( this, &PxPlayer::_restorePosition );
   }

   PhysicsStatic::smDeleteSignal.remove( this, &PxPlayer::_onStaticDeleted );
}

PxPlayer* PxPlayer::create( Player *player, PxWorld *world )
{
   // Determine type of CharacterController to create...

   const char *typeStr = static_cast<PlayerData*>( player->getDataBlock() )->physicsPlayerType;
   PxPlayer *pxPlayer = NULL;

   if ( dStricmp( typeStr, "None" ) == 0 )
      return pxPlayer;

   if ( gPhysicsPlugin->isSinglePlayer() && player->isClientObject() )
   {
      pxPlayer = new PxPlayer( player, world );
      pxPlayer->mSinglePlayer = true;
      pxPlayer->mServerObject = false;
      
      return pxPlayer;
   }
   
   if ( dStricmp( typeStr, "Capsule" ) == 0 )  
      pxPlayer = new PxCapsulePlayer( player, world );   
   else
      pxPlayer = new PxBoxPlayer( player, world );

   pxPlayer->mSinglePlayer = gPhysicsPlugin->isSinglePlayer();
   pxPlayer->mServerObject = player->isServerObject();   

   if ( gPhysicsPlugin->isSinglePlayer() && player->isServerObject() )
   {
      // JCFHACK: The server pxPlayer saves its position (if it hasn't already)
      // when the client begins a tick (preTickSignal), because the client will
      // reuse the server object for calculating its moved position.  Then the server
      // pxPlayer will restore its saved position before it begins processing 
      // ITS tick ( ServerProcessList preTickSignal ).
      gClientProcessList.preTickSignal().notify( pxPlayer, &PxPlayer::_savePosition );
      gServerProcessList.preTickSignal().notify( pxPlayer, &PxPlayer::_restorePosition );
   }

   return pxPlayer;
}

Point3F PxPlayer::move( const VectorF &displacement, Collision *outCol )
{
   if ( !isSinglePlayer() )
      return _move( displacement, outCol );
   
   if ( isClientObject() )
   {
      PxPlayer *sister = getServerObj();
      if ( !sister )
         return mPlayer->getPosition();
      
      sister->mDummyMove = true;
      Point3F endPos = sister->_move( displacement, outCol );
      sister->mDummyMove = false;      

      return endPos;

      //Player *serverPlayer = NetConnection::getServerObject( mPlayer );
      //return serverPlayer->getPosition();      
   }
   else
   {
      return _move( displacement, outCol );
   }
}

Point3F PxPlayer::_move( const VectorF &displacement, Collision *outCol )
{
   if ( mWorld )
      mWorld->releaseWriteLock();

   mLastCollision = outCol;

   NxVec3 dispNx( displacement.x, displacement.y, displacement.z );
   NxU32 activeGroups = 0xFFFFFFFF;
   NxU32 collisionFlags = NXCC_COLLISION_SIDES | NXCC_COLLISION_DOWN | NXCC_COLLISION_UP;

   mController->move( dispNx, activeGroups, 0.0001f, collisionFlags );

   Point3F newPos = pxCast<Point3F>( mController->getDebugPosition() );
   newPos.z -= mSize.z * 0.5f;   

   mLastCollision = NULL;

   return newPos;
}

void PxPlayer::setPosition( const MatrixF &mat )
{
   // No physics objects to set position on.
   if ( isSinglePlayer() && isClientObject() )
      return;

   if ( mWorld )
      mWorld->releaseWriteLock();

   Point3F newPos = mat.getPosition();
   newPos.z += mSize.z * 0.5f;

   const Point3F &curPos = pxCast<Point3F>(mController->getDebugPosition());
   
   if ( !(newPos - curPos ).isZero() )
      mController->setPosition( pxCast<NxExtendedVec3>(newPos) );
}

void PxPlayer::findContact( SceneObject **contactObject, VectorF *contactNormal ) const
{
   if ( isSinglePlayer() && isClientObject() )
   {
      // In single player the client actually calls findContact on the
      // serverObject.      
      PxPlayer *sister = getServerObj();
      if ( sister )
         sister->_findContact( contactObject, contactNormal );
   }
   else
      _findContact( contactObject, contactNormal );   
}

void PxPlayer::renderDebug( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{
   // Dummies render the server side PhysicsPlayer.
   if ( isSinglePlayer() && isClientObject() )
   {
      PhysicsPlayer *serverPhysicsPlayer = getServerObj();
      if ( serverPhysicsPlayer )
         serverPhysicsPlayer->renderDebug( ri, state, overrideMat );      
   }
}

NxControllerAction PxPlayer::onShapeHit( const NxControllerShapeHit& hit )
{
   // Shouldn't be called anyway since dummies have no shapes.
   if ( isSinglePlayer() && isClientObject() )
      return NX_ACTION_NONE;

   NxActor *controllerActor = mController->getActor();
   // TODO: Sometimes the shape or
   // actor is NULL? Why does this happen?
   //if ( !hit.shape )
      //return NX_ACTION_NONE;

   NxActor *actor = &hit.shape->getActor();
   PxUserData *userData = PxUserData::getData( *actor );

   // Fill out the Collision 
   // structure for use later.
   if ( mLastCollision )
   {
      mLastCollision->normal = pxCast<Point3F>( hit.worldNormal );
      mLastCollision->point.set( hit.worldPos.x, hit.worldPos.y, hit.worldPos.z );
      mLastCollision->distance = hit.length;      
      if ( userData )
         mLastCollision->object = userData->getObject();
   }

   if ( userData && 
        userData->mCanPush &&
        actor->isDynamic() && 
        !actor->readBodyFlag( NX_BF_KINEMATIC ) &&
        !mDummyMove )
   {
      // So the object is neither
      // a static or a kinematic,
      // meaning we need to figure out
      // if we have enough force to push it.
      
      // Get the hit object's force
      // and scale it by the amount
      // that it's acceleration is going
      // against our acceleration.
      const Point3F &hitObjLinVel = pxCast<Point3F>( actor->getLinearVelocity() );

      F32 hitObjMass = actor->getMass();

      VectorF hitObjDeltaVel = hitObjLinVel * TickSec;
      VectorF hitObjAccel = hitObjDeltaVel / TickSec;
      
      VectorF controllerLinVel = pxCast<Point3F>( controllerActor->getLinearVelocity() );
      VectorF controllerDeltaVel = controllerLinVel * TickSec;
      VectorF controllerAccel = controllerDeltaVel / TickSec;

      Point3F hitObjForce = (hitObjMass * hitObjAccel);
      Point3F playerForce = (controllerActor->getMass() * controllerAccel);

      VectorF normalizedObjVel( hitObjLinVel );
      normalizedObjVel.normalizeSafe();

      VectorF normalizedPlayerVel( pxCast<Point3F>( controllerActor->getLinearVelocity() ) );
      normalizedPlayerVel.normalizeSafe();

      F32 forceDot = mDot( normalizedObjVel, normalizedPlayerVel );

      hitObjForce *= forceDot;

      playerForce = playerForce - hitObjForce;

      if ( playerForce.x > 0.0f || playerForce.y > 0.0f || playerForce.z > 0.0f ) 
         actor->addForceAtPos( NxVec3( playerForce.x, playerForce.y, playerForce.z ), actor->getCMassGlobalPosition() );

      //Con::printf( "onShapeHit: %f %f %f", playerForce.x, playerForce.y, playerForce.z );
   }

   //if ( actor->getGroup() == PxGroup_ClientOnly )
      return NX_ACTION_PUSH;
   //else
   //   return NX_ACTION_NONE;
}

NxControllerAction PxPlayer::onControllerHit( const NxControllersHit& hit )
{
   return NX_ACTION_NONE;
}

PxPlayer* PxPlayer::getServerObj() const
{
   Player *sister = static_cast<Player*>( mPlayer->getServerObject() );
   if ( !sister )
      return NULL;

   return static_cast<PxPlayer*>( sister->getPhysicsPlayer() );
}

PxPlayer* PxPlayer::getClientObj() const
{   
   Player *sister = static_cast<Player*>( mPlayer->getClientObject() );
   if ( !sister )
      return NULL;

   return static_cast<PxPlayer*>( sister->getPhysicsPlayer() );
}

void PxPlayer::_restorePosition()
{
   // JCFHACK: see PxPlayer::create
   if ( mPositionSaved )
   {
      mController->setPosition( mSavedServerPosition );
      mPositionSaved = false;
   }
}

void PxPlayer::_savePosition()
{
   // JCFHACK: see PxPlayer::create
   if ( mPositionSaved )
      return;
   
   mSavedServerPosition = mController->getDebugPosition();
   mPositionSaved = true;
}

void PxPlayer::_onStaticDeleted()
{
   if ( mController )
      mController->reportSceneChanged();
}

void PxPlayer::enableCollision()
{
   if ( !mController )
      return;
   mController->setCollision( true );   
}

void PxPlayer::disableCollision()
{
   if ( !mController )
      return;
   mController->setCollision( false );   
}