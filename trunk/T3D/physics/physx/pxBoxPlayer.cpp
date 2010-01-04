//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxBoxPlayer.h"

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
#include <NxBoxController.h>
#include <ControllerManager.h>


PxBoxPlayer::PxBoxPlayer( Player *player, PxWorld *world )
 : PxPlayer( player, world ),
   mBoxController( NULL )
{
   world->releaseWriteLock();

   PlayerData *datablock = dynamic_cast<PlayerData*>( player->getDataBlock() );

   //NxScene *scene = world->getScene();

	mSize = datablock->boxSize;

	Point3F pos = player->getPosition();
	pos.z += datablock->boxSize.z * 0.5f;

	Point3F size;
	size.z = mSize.z - mSkinWidth;
	size.x = size.y = getMax( mSize.x, mSize.y ) - mSkinWidth;
	// BoxController is retarded, 'extents' is really halfExtents
	size *= 0.5f;

   NxBoxControllerDesc desc;
   desc.extents = pxCast<NxVec3>(size);
   desc.skinWidth = mSkinWidth;
   desc.position = pxCast<NxExtendedVec3>( pos );
   desc.upDirection = NX_Z;
   desc.callback = this; // TODO: Fix this as well!
	desc.slopeLimit = datablock->runSurfaceCos;
	desc.stepOffset = datablock->maxStepHeight;

   mUserData.setObject( player );
   desc.userData = &mUserData;

   mController = mWorld->createController( desc );
   mBoxController = static_cast<NxBoxController*>( mController );

   mController->setInteraction( NXIF_INTERACTION_INCLUDE );
   mController->getActor()->setMass( datablock->mass );
}

void PxBoxPlayer::_findContact( SceneObject **contactObject, VectorF *contactNormal ) const
{
   // Do a short box sweep downward...

   // Calculate the sweep motion...

   //F32 halfCapSize = mSize.z * 0.5f;
   //F32 halfSmallCapSize = halfCapSize * 0.8f;
   //F32 diff = halfCapSize - halfSmallCapSize;

   F32 offsetDist = mSkinWidth + 0.01f; 
   NxVec3 motion(0,0,-offsetDist);

   // Construct the box...

   NxBox worldBox;
   worldBox.center = pxCast<NxVec3>( mBoxController->getDebugPosition() );
   worldBox.extents = mBoxController->getExtents();
   worldBox.rot.id();

   NxSweepQueryHit sweepHit;

	//mBoxController->getActor()->raiseActorFlag( NX_AF_DISABLE_COLLISION );

   NxU32 hitCount = mWorld->getScene()->linearOBBSweep( worldBox, motion, NX_SF_STATICS | NX_SF_DYNAMICS, NULL, 1, &sweepHit, NULL );
	//if ( hitCount > 0 )
	//{
	//	void *data = sweepHit.hitShape->getActor().userData;
	//	int i = 0;
	//}
	//mBoxController->getActor()->clearActorFlag( NX_AF_DISABLE_COLLISION );

   if ( hitCount > 0 )
   {
      PxUserData *data = PxUserData::getData( sweepHit.hitShape->getActor() );
      if ( data )
         (*contactObject) = data->getObject();
      (*contactNormal) = pxCast<Point3F>( sweepHit.normal );
   }
}

bool PxBoxPlayer::testSpacials( const Point3F &nPos, const Point3F &nSize ) const
{
   AssertFatal( nSize.least() > 0.0f, "PxBoxPlayer::testSpacials(), invalid extents!" );
   
   // We are assuming the position passed in is at the bottom of the object
   // box, like a standard player.
   Point3F adjustedPos = nPos;
   adjustedPos.z += nSize.z * 0.5f;

	Point3F size;
	size.z = nSize.z - mSkinWidth;
	size.x = size.y = getMax( nSize.x, nSize.y ) - mSkinWidth; 

   Point3F halfSize = size * 0.5f;

   NxBounds3 worldBounds;
   worldBounds.min = pxCast<NxVec3>( adjustedPos - halfSize );
   worldBounds.max = pxCast<NxVec3>( adjustedPos + halfSize );

   bool hit = mWorld->getScene()->checkOverlapAABB( worldBounds, NX_STATIC_SHAPES, 0xffffffff, NULL );
   return !hit;
}

void PxBoxPlayer::setSpacials( const Point3F &nPos, const Point3F &nSize )
{
   AssertFatal( nSize.least() > 0.0f, "PxPlayer::setSpacials(), invalid extents!" );

   if ( mWorld )
      mWorld->releaseWriteLock();

	mSize = nSize;

	// We are assuming the position passed in is at the bottom of the object
	// box, like a standard player.
	Point3F adjustedPos = nPos;
	adjustedPos.z += mSize.z * 0.5f;	
	
	Point3F adjustedSize = nSize;
	adjustedSize.z = nSize.z - mSkinWidth;
	adjustedSize.x = adjustedSize.y = getMax( nSize.x, nSize.y ) - mSkinWidth;
	// BoxController is retarded, 'extents' is really halfExtents
	adjustedSize *= 0.5f;

   mBoxController->setPosition( pxCast<NxExtendedVec3>( adjustedPos ) );
   mBoxController->setExtents( pxCast<NxVec3>( adjustedSize ) );
}

void PxBoxPlayer::renderDebug( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{
   Point3F pos = pxCast<Point3F>( mBoxController->getDebugPosition() );
   Point3F size = pxCast<Point3F>( mBoxController->getExtents() );
	// BoxController is retarded, 'extents' is really halfExtents
	size *= 2;

   GFXStateBlockDesc desc;
   desc.setBlend( true );
   desc.setZReadWrite( true, false );

   GFX->getDrawUtil()->drawCube( desc, size, pos, ColorI(100,100,200,160) );
}