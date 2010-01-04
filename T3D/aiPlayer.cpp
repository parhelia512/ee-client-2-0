//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "T3D/aiPlayer.h"
#include "console/consoleInternal.h"
#include "math/mMatrix.h"
#include "T3D/moveManager.h"

IMPLEMENT_CO_NETOBJECT_V1(AIPlayer);

/**
 * Constructor
 */
AIPlayer::AIPlayer():
m_fCamDistance(0.f),
m_fCamDistanceToReach(0.f),
m_ptCamRot(0.f,0.f,0.f),
m_bControlByKey(false)
{
   mMoveDestination.set( 0.0f, 0.0f, 0.0f );
   mMoveSpeed = 1.0f;
   mMoveTolerance = 0.25f;
   mMoveSlowdown = true;
   mMoveState = ModeStop;

   mAimObject = 0;
   mAimLocationSet = false;
   mTargetInLOS = false;
   mAimOffset = Point3F(0.0f, 0.0f, 0.0f);

   mTypeMask |= AIObjectType;
}

/**
 * Destructor
 */
AIPlayer::~AIPlayer()
{
}

/**
 * Sets the speed at which this AI moves
 *
 * @param speed Speed to move, default player was 10
 */
void AIPlayer::setMoveSpeed( F32 speed )
{
   mMoveSpeed = getMax(0.0f, getMin( 1.0f, speed ));
}

/**
 * Stops movement for this AI
 */
void AIPlayer::stopMove()
{
   mMoveState = ModeStop;
}

/**
 * Sets how far away from the move location is considered
 * "on target"
 *
 * @param tolerance Movement tolerance for error
 */
void AIPlayer::setMoveTolerance( const F32 tolerance )
{
   mMoveTolerance = getMax( 0.1f, tolerance );
}

/**
 * Sets the location for the bot to run to
 *
 * @param location Point to run to
 */
void AIPlayer::setMoveDestination( const Point3F &location, bool slowdown )
{
   mMoveDestination = location;
   mMoveState = ModeMove;
   mMoveSlowdown = slowdown;
}

/**
 * Sets the object the bot is targeting
 *
 * @param targetObject The object to target
 */
void AIPlayer::setAimObject( GameBase *targetObject )
{
   mAimObject = targetObject;
   mTargetInLOS = false;
   mAimOffset = Point3F(0.0f, 0.0f, 0.0f);
}

/**
 * Sets the object the bot is targeting and an offset to add to target location
 *
 * @param targetObject The object to target
 * @param offset       The offest from the target location to aim at
 */
void AIPlayer::setAimObject( GameBase *targetObject, Point3F offset )
{
   mAimObject = targetObject;
   mTargetInLOS = false;
   mAimOffset = offset;
}

/**
 * Sets the location for the bot to aim at
 *
 * @param location Point to aim at
 */
void AIPlayer::setAimLocation( const Point3F &location )
{
   mAimObject = 0;
   mAimLocationSet = true;
   mAimLocation = location;
   mAimOffset = Point3F(0.0f, 0.0f, 0.0f);
}

/**
 * Clears the aim location and sets it to the bot's
 * current destination so he looks where he's going
 */
void AIPlayer::clearAim()
{
   mAimObject = 0;
   mAimLocationSet = false;
   mAimOffset = Point3F(0.0f, 0.0f, 0.0f);
}

/**
 * This method calculates the moves for the AI player
 *
 * @param movePtr Pointer to move the move list into
 */
bool AIPlayer::getAIMove(Move *movePtr)
{
	if (mMoveState == ModeStop)
	{
		return true;
	}
	*movePtr = NullMove;

	// Use the eye as the current position.
	MatrixF eye;
	getEyeTransform(&eye);
	Point3F location = eye.getPosition();
	Point3F rotation = getRotation();

	// Orient towards the aim point, aim object, or towards
	// our destination.
	if (mAimObject || mAimLocationSet || mMoveState == ModeMove) {

		// Update the aim position if we're aiming for an object
		if (mAimObject)
			mAimLocation = mAimObject->getPosition() + mAimOffset;
		else
			if (!mAimLocationSet)
				mAimLocation = mMoveDestination;

		F32 xDiff = mAimLocation.x - location.x;
		F32 yDiff = mAimLocation.y - location.y;
		if (!mIsZero(xDiff) || !mIsZero(yDiff)) {

			// First do Yaw
			// use the cur yaw between -Pi and Pi
			F32 curYaw = rotation.z;
			while (curYaw > M_2PI_F)
				curYaw -= M_2PI_F;
			while (curYaw < -M_2PI_F)
				curYaw += M_2PI_F;

			// find the yaw offset
			F32 newYaw = mAtan2( xDiff, yDiff );
			F32 yawDiff = newYaw - curYaw;

			// make it between 0 and 2PI
			if( yawDiff < 0.0f )
				yawDiff += M_2PI_F;
			else if( yawDiff >= M_2PI_F )
				yawDiff -= M_2PI_F;

			// now make sure we take the short way around the circle
			if( yawDiff > M_PI_F )
				yawDiff -= M_2PI_F;
			else if( yawDiff < -M_PI_F )
				yawDiff += M_2PI_F;

			movePtr->yaw = yawDiff > 0 ? mClampF(yawDiff,0,M_PI_F / 20) : mClampF(yawDiff,-M_PI_F / 20 ,0);

			// Next do pitch.
			if (!mAimObject && !mAimLocationSet) {
				// Level out if were just looking at our next way point.
				Point3F headRotation = getHeadRotation();
				movePtr->pitch = -headRotation.x;
			}
			else {
				// This should be adjusted to run from the
				// eye point to the object's center position. Though this
				// works well enough for now.
				F32 vertDist = mAimLocation.z - location.z;
				F32 horzDist = mSqrt(xDiff * xDiff + yDiff * yDiff);
				F32 newPitch = mAtan2( horzDist, vertDist ) - ( M_PI_F / 2.0f );
				if (mFabs(newPitch) > 0.01f) {
					Point3F headRotation = getHeadRotation();
					movePtr->pitch = newPitch - headRotation.x;
				}
			}
		}
	}
	else {
		// Level out if we're not doing anything else
		Point3F headRotation = getHeadRotation();
		movePtr->pitch = -headRotation.x;
	}

	// Move towards the destination
	if (mMoveState == ModeMove) {
		F32 xDiff = mMoveDestination.x - location.x;
		F32 yDiff = mMoveDestination.y - location.y;

		F32 zDiff = mMoveDestination.z - location.z;

		// Check if we should mMove, or if we are 'close enough'
		if (mFabs(xDiff) < mMoveTolerance && mFabs(yDiff) < mMoveTolerance) {
			mMoveState = ModeStop;
			throwCallback("onReachDestination");
		}
		else {
			// Build move direction in world space


			if (!mIsZero(zDiff))
			{
				movePtr->z = zDiff > 0 ? 1 : -1;
			}

			if (mIsZero(xDiff))
				movePtr->y = (location.y > mMoveDestination.y) ? -1.0f : 1.0f;
			else
				if (mIsZero(yDiff))
					movePtr->x = (location.x > mMoveDestination.x) ? -1.0f : 1.0f;
				else
					if (mFabs(xDiff) > mFabs(yDiff)) {
						F32 value = mFabs(yDiff / xDiff);
						movePtr->y = (location.y > mMoveDestination.y) ? -value : value;
						movePtr->x = (location.x > mMoveDestination.x) ? -1.0f : 1.0f;
					}
					else {
						F32 value = mFabs(xDiff / yDiff);
						movePtr->x = (location.x > mMoveDestination.x) ? -value : value;
						movePtr->y = (location.y > mMoveDestination.y) ? -1.0f : 1.0f;
					}

					// Rotate the move into object space (this really only needs
					// a 2D matrix)
					Point3F newMove;
					MatrixF moveMatrix;
					moveMatrix.set(EulerF(0.0f, 0.0f, -(rotation.z + movePtr->yaw)));
					moveMatrix.mulV( Point3F( movePtr->x, movePtr->y, 0.0f ), &newMove );
					movePtr->x = newMove.x;
					movePtr->y = newMove.y;

					// Set movement speed.  We'll slow down once we get close
					// to try and stop on the spot...
					if (mMoveSlowdown) {
						F32 speed = mMoveSpeed;
						F32 dist = mSqrt(xDiff*xDiff + yDiff*yDiff);
						F32 maxDist = 5.0f;
						if (dist < maxDist)
							speed *= dist / maxDist;
						movePtr->x *= speed;
						movePtr->y *= speed;
					}
					else {
						movePtr->x *= mMoveSpeed;
						movePtr->y *= mMoveSpeed;
					}

					// We should check to see if we are stuck...
					if (location == mLastLocation) {
						throwCallback("onMoveStuck");
						mMoveState = ModeStop;
					}
		}
	}

	// Test for target location in sight if it's an object. The LOS is
	// run from the eye position to the center of the object's bounding,
	// which is not very accurate.
	if (mAimObject) {
		MatrixF eyeMat;
		getEyeTransform(&eyeMat);
		eyeMat.getColumn(3,&location);
		Point3F targetLoc = mAimObject->getBoxCenter();

		// This ray ignores non-static shapes. Cast Ray returns true
		// if it hit something.
		RayInfo dummy;
		if (getContainer()->castRay( location, targetLoc,
			InteriorObjectType | StaticShapeObjectType | StaticObjectType |
			 TerrainObjectType, &dummy)) {
				if (mTargetInLOS) {
					throwCallback( "onTargetExitLOS" );
					mTargetInLOS = false;
				}
		}
		else
			if (!mTargetInLOS) {
				throwCallback( "onTargetEnterLOS" );
				mTargetInLOS = true;
			}
	}

	// Replicate the trigger state into the move so that
	// triggers can be controlled from scripts.
	for( int i = 0; i < MaxTriggerKeys; i++ )
		movePtr->trigger[i] = getImageTriggerState(i);

	return true;
}

/**
 * Utility function to throw callbacks. Callbacks always occure
 * on the datablock class.
 *
 * @param name Name of script function to call
 */
void AIPlayer::throwCallback( const char *name )
{
   Con::executef(getDataBlock(), name, scriptThis());
}

ConsoleMethod( AIPlayer, setControlByKey, void, 3, 3, "(%byKey)")
{
	object->setControlByKey(dAtob(argv[2]));
}
void AIPlayer::setControlByKey( bool val )
{
	if (m_bControlByKey != val)
	{
		if (val)
		{
			backToCamera();
		}
		m_bControlByKey = val;
	}
}
void AIPlayer::backToCamera()
{
	faceToCamera(false);
}
void AIPlayer::faceToCamera(bool faceto)
{
	MatrixF mat;
	F32 pos;
	Point3F posCam,posPlayer;
	getCameraTransform(&pos,&mat);
	mat.getColumn(3,&posCam);
	getTransform().getColumn(3,&posPlayer);
	Point3F vec (posPlayer - posCam);
	vec.z = 0;
	vec.normalizeSafe();
	mat = MathUtils::createOrientFromDir(vec);
	mat.setColumn(3,posPlayer);
	setTransform(mat);
}

void AIPlayer::preprocessMove(Move * mv)
{
	getAIMove(mv);
	Parent::preprocessMove(mv);
}

#define M_PI_DIV180		0.01745329251994329576922222f
void AIPlayer::getCameraTransform(F32* pos,MatrixF* mat)
{
	VectorF vecCam(-10.f,-10.f,20.f);
	Point3F ptLookAt;

	m_ptCamRot.x += MoveManager::mPitchCam;
	m_ptCamRot.x = mClampF(m_ptCamRot.x,-45*M_PI_DIV180,89*M_PI_DIV180);

	m_ptCamRot.z += MoveManager::mYawCam;
	m_ptCamRot.z += MoveManager::mKeyYawCam;
	m_ptCamRot.z = mFmod(m_ptCamRot.z,360*M_PI_DIV180);

	MatrixF matPlayer = getTransform();
	EulerF rotPlayerZ(0,0,0);
	if (m_bControlByKey)
	{
		rotPlayerZ.z = -(MoveManager::mKeyYawCam + MoveManager::mYawCam);
	}
	else
	{
		rotPlayerZ.z = -MoveManager::mKeyYawCam;
	}
	MatrixF rotPlayer(rotPlayerZ);
	matPlayer.mul(rotPlayer);
	setTransform(matPlayer);

	PlayerData * pDataBlock =  dynamic_cast<PlayerData *> ( getDataBlock() );
	m_fCamDistanceToReach = mClampF(MoveManager::mDistanceCam,pDataBlock->cameraMinDist, pDataBlock->cameraMaxDist);
	MoveManager::mDistanceCam = m_fCamDistanceToReach;
	if (	mFabs(m_fCamDistance) < 0.00001 ||\
		mFabs(m_fCamDistance - m_fCamDistanceToReach) < 0.03)
	{
		m_fCamDistance = m_fCamDistanceToReach;
	}
	else if (m_fCamDistance > m_fCamDistanceToReach)
	{
		m_fCamDistance -= 0.03;
	}
	else if(m_fCamDistance < m_fCamDistanceToReach)
	{
		m_fCamDistance += 0.03;
	}

	MoveManager::mPitchCam = MoveManager::mYawCam = 0;

	F32 reflect = m_fCamDistance * mCos(m_ptCamRot.x);
	vecCam.z = m_fCamDistance * mSin(m_ptCamRot.x);
	vecCam.x = reflect * mCos(m_ptCamRot.z);
	vecCam.y = reflect * mSin(m_ptCamRot.z);

	vecCam.neg();


	MatrixF eye;
	getRenderEyeTransform(&eye);
	// Use the eye transform to orient the camera
	VectorF vp,vec;
	vp.x = vp.z = 0;
	vp.y = -7 * *pos;
	eye.mulV(vp,&vec);

	Point3F sp;
	eye.getColumn(3,&sp);
	Point3F camPos = sp - vecCam;

	disableCollision();
	if (isMounted())
		getObjectMount()->disableCollision();
	RayInfo collision;
	Point3F rayEnd = camPos - sp;
	rayEnd.normalize();
	rayEnd *= 0.2;
	rayEnd += camPos;
	// 	if (mContainer->castRay(sp, rayEnd,
	// 		(0xFFFFFFFF & ~(WaterObjectType      |
	// 		GameBaseObjectType   |
	// 		DefaultObjectType )),
	// 		&collision) == true) 
	if (mContainer->castRay(sp, rayEnd,
		WaterObjectType | TerrainObjectType | InteriorObjectType | StaticShapeObjectType,
		&collision) == true) 
	{
		F32 veclen = vec.len();
		F32 adj = (-mDot(vec, collision.normal) / veclen) * 0.1;
		F32 newPos = getMax(0.0f, collision.t - adj);
		if (newPos == 0.0f)
			eye.getColumn(3,&camPos);
		else
			camPos = sp + (vec * newPos);
	}
	if (isMounted())
		getObjectMount()->enableCollision();
	enableCollision();


	VectorF x,y(vecCam),z(0,0,1);
	y.normalize();
	mCross(y,z,&x);
	x.normalize();
	mCross(x,y,&z);
	z.normalize();

	mat->identity();
	mat->setColumn(0,x);
	mat->setColumn(1,y);
	mat->setColumn(2,z);

	mat->setColumn(3,camPos);
}
// --------------------------------------------------------------------------------------------
// Console Functions
// --------------------------------------------------------------------------------------------

ConsoleMethod( AIPlayer, stop, void, 2, 2, "()"
              "Stop moving.")
{
   object->stopMove();
}

ConsoleMethod( AIPlayer, clearAim, void, 2, 2, "()"
              "Stop aiming at anything.")
{
   object->clearAim();
}

ConsoleMethod( AIPlayer, setMoveSpeed, void, 3, 3, "( float speed )"
              "Sets the move speed for an AI object.")
{
   object->setMoveSpeed( dAtof( argv[2] ) );
}

ConsoleMethod( AIPlayer, setMoveDestination, void, 3, 4, "(Point3F goal, bool slowDown=true)"
              "Tells the AI to move to the location provided.")
{
   Point3F v( 0.0f, 0.0f, 0.0f );
   dSscanf( argv[2], "%g %g %g", &v.x, &v.y, &v.z );
   bool slowdown = (argc > 3)? dAtob(argv[3]): true;
   object->setMoveDestination( v, slowdown);
}

ConsoleMethod( AIPlayer, getMoveDestination, const char *, 2, 2, "()"
              "Returns the point the AI is set to move to.")
{
   Point3F movePoint = object->getMoveDestination();

   char *returnBuffer = Con::getReturnBuffer( 256 );
   dSprintf( returnBuffer, 256, "%g %g %g", movePoint.x, movePoint.y, movePoint.z );

   return returnBuffer;
}

ConsoleMethod( AIPlayer, setAimLocation, void, 3, 3, "( Point3F target )"
              "Tells the AI to aim at the location provided.")
{
   Point3F v( 0.0f,0.0f,0.0f );
   dSscanf( argv[2], "%g %g %g", &v.x, &v.y, &v.z );

   object->setAimLocation( v );
}

ConsoleMethod( AIPlayer, getAimLocation, const char *, 2, 2, "()"
              "Returns the point the AI is aiming at.")
{
   Point3F aimPoint = object->getAimLocation();

   char *returnBuffer = Con::getReturnBuffer( 256 );
   dSprintf( returnBuffer, 256, "%g %g %g", aimPoint.x, aimPoint.y, aimPoint.z );

   return returnBuffer;
}

ConsoleMethod( AIPlayer, setAimObject, void, 3, 4, "( GameBase obj, [Point3F offset] )"
              "Sets the bot's target object. Optionally set an offset from target location.")
{
   Point3F off( 0.0f, 0.0f, 0.0f );

   // Find the target
   GameBase *targetObject;
   if( Sim::findObject( argv[2], targetObject ) )
   {
      if (argc == 4)
         dSscanf( argv[3], "%g %g %g", &off.x, &off.y, &off.z );

      object->setAimObject( targetObject, off );
   }
   else
      object->setAimObject( 0, off );
}

ConsoleMethod( AIPlayer, getAimObject, S32, 2, 2, "()"
              "Gets the object the AI is targeting.")
{
   GameBase* obj = object->getAimObject();
   return obj? obj->getId(): -1;
}

