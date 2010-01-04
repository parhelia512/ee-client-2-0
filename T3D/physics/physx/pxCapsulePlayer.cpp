//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxCapsulePlayer.h"

#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physX/pxWorld.h"
#include "T3D/physics/physX/pxUserData.h"
#include "T3D/physics/physX/pxCasts.h"
#include "gfx/gfxDrawUtil.h"
#include "collision/collision.h"
#include "T3D/player.h"

#include <NxControllerManager.h>
#include <CharacterControllerManager.h>
#include <NxCapsuleController.h>
#include <ControllerManager.h>


PxCapsulePlayer::PxCapsulePlayer( Player *player, PxWorld *world )
 : PxPlayer( player, world ),
   mCapsuleController( NULL )      
{
   world->releaseWriteLock();

   PlayerData *datablock = dynamic_cast<PlayerData*>( player->getDataBlock() );
  
   // Don't forget to initialize base member!
   mSize = datablock->boxSize;

   const Point3F &pos = player->getPosition();

   NxCapsuleControllerDesc desc;
   desc.skinWidth = mSkinWidth;
   desc.radius = getMax( mSize.x, mSize.y ) * 0.5f;
   desc.radius -= desc.skinWidth;
   desc.height = mSize.z - ( desc.radius * 2.0f );
   desc.height -= desc.skinWidth * 2.0f;

   desc.climbingMode = CLIMB_CONSTRAINED;
   desc.position.set( pos.x, pos.y, pos.z + ( mSize.z * 0.5f ) );
   desc.upDirection = NX_Z;
   desc.callback = this; // TODO: Fix this as well!
	desc.slopeLimit = datablock->runSurfaceCos;
	desc.stepOffset = datablock->maxStepHeight;

   mUserData.setObject( player );
   desc.userData = &mUserData;

   // Don't forget to initialize base member!
   mController = mWorld->createController( desc );

   // Cast to a NxCapsuleController for our convenience
      mCapsuleController = static_cast<NxCapsuleController*>( mController );

   mCapsuleController->setInteraction( NXIF_INTERACTION_INCLUDE );
   mCapsuleController->getActor()->setMass( datablock->mass );
}

void PxCapsulePlayer::_findContact( SceneObject **contactObject, VectorF *contactNormal ) const
{
   // Do a short capsule sweep downward...

   // Calculate the sweep motion...

   F32 halfCapSize = mSize.z * 0.5f;
   F32 halfSmallCapSize = halfCapSize * 0.8f;
   F32 diff = halfCapSize - halfSmallCapSize;

   F32 offsetDist = diff + mSkinWidth + 0.01f; 
   NxVec3 motion(0,0,-offsetDist);

   // Construct the capsule...

   F32 radius = mCapsuleController->getRadius();
   F32 halfHeight = mCapsuleController->getHeight() * 0.5f;

   NxCapsule capsule;
   capsule.p0 = capsule.p1 = pxCast<NxVec3>( mCapsuleController->getDebugPosition() );
   capsule.p0.z -= halfHeight;
   capsule.p1.z += halfHeight;
   capsule.radius = radius;

   NxSweepQueryHit sweepHit;

   NxU32 hitCount = mWorld->getScene()->linearCapsuleSweep( capsule, motion, NX_SF_STATICS | NX_SF_DYNAMICS, NULL, 1, &sweepHit, NULL );

   if ( hitCount > 0 )
   {
      PxUserData *data = PxUserData::getData( sweepHit.hitShape->getActor() );
      if ( data )
      {
         (*contactObject) = data->getObject();
         (*contactNormal) = pxCast<Point3F>( sweepHit.normal );
      }
   }
}

bool PxCapsulePlayer::testSpacials( const Point3F &nPos, const Point3F &nSize ) const
{
   AssertFatal( nSize.least() > 0.0f, "PxCapsulePlayer::testSpacials(), invalid extents!" );

   F32 radius = getMax( nSize.x, nSize.y ) / 2.0f;
   radius -= mSkinWidth;
   radius = getMax(radius,0.01f);
   F32 height = nSize.z - ( radius * 2.0f );
   height -= mSkinWidth * 2.0f;
   height = getMax( height, 0.01f );   

   F32 halfHeight = height * 0.5f;   

   // We are assuming the position passed in is at the bottom of the object
   // box, like a standard player.
   Point3F adjustedPos = nPos;
   adjustedPos.z += nSize.z * 0.5f;
   NxVec3 origin = pxCast<NxVec3>( adjustedPos );

   NxCapsule worldCapsule( NxSegment(origin,origin), radius );   
   worldCapsule.p0.z -= halfHeight;
   worldCapsule.p1.z += halfHeight;      

   bool hit = mWorld->getScene()->checkOverlapCapsule( worldCapsule, NX_STATIC_SHAPES, 0xffffffff, NULL );
   return !hit;
}

void PxCapsulePlayer::setSpacials( const Point3F &nPos, const Point3F &nSize )
{
   AssertFatal( nSize.least() > 0.0f, "PxCapsulePlayer::setSpacials(), invalid extents!" );

   if ( mWorld )
      mWorld->releaseWriteLock();

   mSize = nSize;

   F32 radius = getMax( nSize.x, nSize.y ) * 0.5f;
   radius -= mSkinWidth;
   radius = getMax(radius,0.01f);
   F32 height = nSize.z - ( radius * 2.0f );
   height -= mSkinWidth * 2.0f;
   height = getMax(height,0.01f);

   // The CapsuleController's 'actual' position we are setting.
   // We are assuming the position passed in is at the bottom of the object
   // box, like a standard player.
   Point3F adjustedPos = nPos;
   adjustedPos.z += mSize.z * 0.5f;

   mCapsuleController->setPosition( pxCast<NxExtendedVec3>( adjustedPos ) );
   mCapsuleController->setRadius( radius );
   mCapsuleController->setHeight( height );
}

void PxCapsulePlayer::renderDebug( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{
   Point3F center = pxCast<Point3F>( mCapsuleController->getDebugPosition() );   
   F32 radius = mCapsuleController->getRadius();
   F32 height = mCapsuleController->getHeight();

   GFXStateBlockDesc desc;
   desc.setBlend( true );
   desc.setZReadWrite( true, false );

   GFX->getDrawUtil()->drawCapsule( desc, center, radius, height, ColorI(100,100,200,160) );
}