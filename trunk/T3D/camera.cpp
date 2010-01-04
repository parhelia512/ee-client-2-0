//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "app/game.h"
#include "math/mMath.h"
#include "core/stream/bitStream.h"
#include "T3D/fx/cameraFXMgr.h"
#include "T3D/camera.h"
#include "T3D/gameConnection.h"
#include "math/mathIO.h"
#include "gui/worldEditor/editor.h"
#include "console/consoleTypes.h"
#include "math/mathUtils.h"

#define MaxPitch 1.5706f
#define CameraRadius 0.05f;


//----------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(CameraData);

void CameraData::initPersistFields()
{
   Parent::initPersistFields();
}

void CameraData::packData(BitStream* stream)
{
   Parent::packData(stream);
}

void CameraData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);
}


//----------------------------------------------------------------------------

IMPLEMENT_CO_NETOBJECT_V1(Camera);
F32 Camera::mMovementSpeed = 40.0f;

Camera::Camera()
{
   mNetFlags.clear(Ghostable);
   mTypeMask |= CameraObjectType;
   delta.pos = Point3F(0.0f, 0.0f, 100.0f);
   delta.rot = Point3F(0.0f, 0.0f, 0.0f);
   delta.posVec = delta.rotVec = VectorF(0.0f, 0.0f, 0.0f);
   mObjToWorld.setColumn(3, delta.pos);
   mRot = delta.rot;

   mOffset.set(0.0f, 0.0f, 0.0f);

   mMinOrbitDist = 0.0f;
   mMaxOrbitDist = 0.0f;
   mCurOrbitDist = 0.0f;
   mOrbitObject = NULL;
   mPosition.set(0.0f, 0.0f, 0.0f);
   mObservingClientObject = false;
   mode = FlyMode;

   // For NewtonFlyMode
   mNewtonRotation = false;
   mAngularVelocity.set(0.0f, 0.0f, 0.0f);
   mAngularForce = 100.0f;
   mAngularDrag = 2.0f;
   mVelocity.set(0.0f, 0.0f, 0.0f);
   mNewtonMode = false;
   mMass = 10.0f;
   mDrag = 2.0;
   mFlyForce = 500.0f;
   mSpeedMultiplier = 2.0f;
   mBrakeMultiplier = 2.0f;

   // For EditOrbitMode
   mValidEditOrbitPoint = false;
   mEditOrbitPoint.set(0.0f, 0.0f, 0.0f);
   mCurrentEditOrbitDist = 2.0;

   mLocked = false;
}

Camera::~Camera()
{
}


//----------------------------------------------------------------------------

bool Camera::onAdd()
{
   if(!Parent::onAdd())
      return false;

   mObjBox.maxExtents = mObjScale;
   mObjBox.minExtents = mObjScale;
   mObjBox.minExtents.neg();
   resetWorldBox();

   if(isClientObject())
      gClientContainer.addObject(this);
   else
      gServerContainer.addObject(this);

 //  addToScene();
   return true;
}

void Camera::onEditorEnable()
{
   mNetFlags.set(Ghostable);
}

void Camera::onEditorDisable()
{
   mNetFlags.clear(Ghostable);
}

void Camera::onRemove()
{
//   removeFromScene();
   if (getContainer())
      getContainer()->removeObject(this);

   Parent::onRemove();
}


//----------------------------------------------------------------------------
// check if the object needs to be observed through its own camera...
void Camera::getCameraTransform(F32* pos, MatrixF* mat)
{
   // The camera doesn't support a third person mode,
   // so we want to override the default ShapeBase behavior.
   ShapeBase * obj = dynamic_cast<ShapeBase*>(static_cast<SimObject*>(mOrbitObject));
   if(obj && static_cast<ShapeBaseData*>(obj->getDataBlock())->observeThroughObject)
      obj->getCameraTransform(pos, mat);
   else
      getRenderEyeTransform(mat);

   // Apply Camera FX.
   mat->mul( gCamFXMgr.getTrans() );
}

F32 Camera::getCameraFov()
{
   ShapeBase * obj = dynamic_cast<ShapeBase*>(static_cast<SimObject*>(mOrbitObject));
   if(obj && static_cast<ShapeBaseData*>(obj->getDataBlock())->observeThroughObject)
      return(obj->getCameraFov());
   else
      return(Parent::getCameraFov());
}

F32 Camera::getDefaultCameraFov()
{
   ShapeBase * obj = dynamic_cast<ShapeBase*>(static_cast<SimObject*>(mOrbitObject));
   if(obj && static_cast<ShapeBaseData*>(obj->getDataBlock())->observeThroughObject)
      return(obj->getDefaultCameraFov());
   else
      return(Parent::getDefaultCameraFov());
}

bool Camera::isValidCameraFov(F32 fov)
{
   ShapeBase * obj = dynamic_cast<ShapeBase*>(static_cast<SimObject*>(mOrbitObject));
   if(obj && static_cast<ShapeBaseData*>(obj->getDataBlock())->observeThroughObject)
      return(obj->isValidCameraFov(fov));
   else
      return(Parent::isValidCameraFov(fov));
}

void Camera::setCameraFov(F32 fov)
{
   ShapeBase * obj = dynamic_cast<ShapeBase*>(static_cast<SimObject*>(mOrbitObject));
   if(obj && static_cast<ShapeBaseData*>(obj->getDataBlock())->observeThroughObject)
      obj->setCameraFov(fov);
   else
      Parent::setCameraFov(fov);
}

//----------------------------------------------------------------------------
void Camera::processTick(const Move* move)
{
   Parent::processTick(move);

   if ( isMounted() )
   {
      // Fetch Mount Transform.
      MatrixF mat;
      mMount.object->getMountTransform( mMount.node, &mat );

      if ( isClientObject() ) 
      {
         delta.rotVec = mRot;
         mObjToWorld.getColumn( 3, &delta.posVec );
      }

      // Apply.
      setTransform( mat );

      if ( isClientObject() ) 
      {
         delta.pos    = mat.getPosition();
         delta.rot    = mRot;
         delta.posVec = delta.posVec - delta.pos;
         delta.rotVec = delta.rotVec - delta.rot;
      }

      // Update Container.
      updateContainer();
      return;
   }

   Point3F vec,pos;
   if (move) 
   {
      bool strafeMode = move->trigger[2];

      // If using editor then force camera into fly mode, unless using EditOrbitMode
      if(gEditingMission && mode != FlyMode && mode != EditOrbitMode)
         setFlyMode();

      // Massage the mode if we're in EditOrbitMode
      CameraMode virtualMode = mode;
      if(mode == EditOrbitMode)
      {
         if(!mValidEditOrbitPoint)
         {
            virtualMode = FlyMode;
         }
         else
         {
            // Reset any Newton camera velocities for when we switch
            // out of EditOrbitMode.
            mNewtonRotation = false;
            mVelocity.set(0.0f, 0.0f, 0.0f);
            mAngularVelocity.set(0.0f, 0.0f, 0.0f);
         }
      }

      // Update orientation
      delta.rotVec = mRot;

      VectorF rotVec(0, 0, 0);

      // process input/determine rotation vector
      if(virtualMode != StationaryMode &&
         virtualMode != TrackObjectMode &&
         (!mLocked || virtualMode != OrbitObjectMode && virtualMode != OrbitPointMode))
      {
         if(!strafeMode)
         {
            rotVec.x = move->pitch;
            rotVec.z = move->yaw;
         }
      }
      else if(virtualMode == TrackObjectMode && bool(mOrbitObject))
      {
         // orient the camera to face the object
         Point3F objPos;
         // If this is a shapebase, use its render eye transform
         // to avoid jittering.
         ShapeBase *shape = dynamic_cast<ShapeBase*>((GameBase*)mOrbitObject);
         if( shape != NULL )
         {
            MatrixF ret;
            shape->getRenderEyeTransform( &ret );
            objPos = ret.getPosition();
         }
         else
         {
            mOrbitObject->getWorldBox().getCenter(&objPos);
         }
         mObjToWorld.getColumn(3,&pos);
         vec = objPos - pos;
         vec.normalizeSafe();
         F32 pitch, yaw;
         MathUtils::getAnglesFromVector(vec, yaw, pitch);
         rotVec.x = -pitch - mRot.x;
         rotVec.z = yaw - mRot.z;
         if(rotVec.z > M_PI_F)
            rotVec.z -= M_2PI_F;
         else if(rotVec.z < -M_PI_F)
            rotVec.z += M_2PI_F;
      }

      // apply rotation vector according to physics rules
      if(mNewtonRotation)
      {
         const F32 force = mAngularForce;
         const F32 drag = mAngularDrag;

         VectorF acc(0.0f, 0.0f, 0.0f);

         rotVec.x *= 2.0f;   // Assume that our -2PI to 2PI range was clamped to -PI to PI in script
         rotVec.z *= 2.0f;   // Assume that our -2PI to 2PI range was clamped to -PI to PI in script

         F32 rotVecL = rotVec.len();
         if(rotVecL > 0)
         {
            acc = (rotVec * force / mMass) * TickSec;
         }

         // Accelerate
         mAngularVelocity += acc;

         // Drag
         mAngularVelocity -= mAngularVelocity * drag * TickSec;

         // Rotate
         mRot += mAngularVelocity * TickSec;
         if(mRot.x > MaxPitch)
            mRot.x = MaxPitch;
         else if(mRot.x < -MaxPitch)
            mRot.x = -MaxPitch;
      }
      else
      {
         mRot.x += rotVec.x;
         mRot.z += rotVec.z;
         if(mRot.x > MaxPitch)
            mRot.x = MaxPitch;
         else if(mRot.x < -MaxPitch)
            mRot.x = -MaxPitch;
      }

      // Update position
      VectorF posVec(0, 0, 0);
      bool mustValidateEyePoint = false;
      bool serverInterpolate = false;

      // process input/determine translation vector
      if(virtualMode == OrbitObjectMode || virtualMode == OrbitPointMode)
      {
         pos = delta.pos;
         if(virtualMode == OrbitObjectMode && bool(mOrbitObject)) 
         {
            // If this is a shapebase, use its render eye transform
            // to avoid jittering.
            GameBase *castObj = mOrbitObject;
            ShapeBase* shape = dynamic_cast<ShapeBase*>(castObj);
            if( shape != NULL ) 
            {
               MatrixF ret;
               shape->getRenderEyeTransform( &ret );
               mPosition = ret.getPosition();
            }
            else 
            {
               // Hopefully this is a static object that doesn't move,
               // because the worldbox doesn't get updated between ticks.
               mOrbitObject->getWorldBox().getCenter(&mPosition);
            }
         }

         posVec = (mPosition + mOffset) - pos;
         mustValidateEyePoint = true;
         serverInterpolate = mNewtonMode;
      }
      else if(virtualMode == EditOrbitMode && mValidEditOrbitPoint)
      {
         bool faster = move->trigger[0] || move->trigger[1];
         F32 scale = mMovementSpeed * (faster + 1);
         mCurrentEditOrbitDist -= move->y * TickSec * scale;
         mCurrentEditOrbitDist -= move->roll * TickSec * scale;   // roll will be -Pi to Pi and we'll attempt to scale it here to be in line with the move->y calculation above
         if(mCurrentEditOrbitDist < 0.0f)
            mCurrentEditOrbitDist = 0.0f;

         mPosition = mEditOrbitPoint;
         setPosition(mPosition, mRot);
         calcEditOrbitPoint(&mObjToWorld, mRot);
         pos = mPosition;
      }
      else if(virtualMode == FlyMode)
      {
         bool faster = move->trigger[0] || move->trigger[1];
         F32 scale = mMovementSpeed * (faster + 1);

         mObjToWorld.getColumn(3,&pos);
         mObjToWorld.getColumn(0,&vec);
         posVec = vec * move->x * TickSec * scale + vec * (strafeMode ? move->yaw * 2.0f * TickSec * scale : 0.0f);
         mObjToWorld.getColumn(1,&vec);
         posVec += vec * move->y * TickSec * scale + vec * move->roll * TickSec * scale;
         mObjToWorld.getColumn(2,&vec);
         posVec += vec * move->z * TickSec * scale - vec * (strafeMode ? move->pitch * 2.0f * TickSec * scale : 0.0f);
      }
      else if(virtualMode == OverheadMode)
      {
         bool faster = move->trigger[0] || move->trigger[1];
         F32 scale = mMovementSpeed * (faster + 1);

         mObjToWorld.getColumn(3,&pos);
         mObjToWorld.getColumn(0,&vec);
         vec = vec * move->x * TickSec * scale + (strafeMode ? vec * move->yaw * 2.0f * TickSec * scale : Point3F(0, 0, 0));
         vec.z = 0;
         vec.normalizeSafe();
         posVec = vec;
         mObjToWorld.getColumn(2,&vec);
         vec = vec * move->y * TickSec * scale - (strafeMode ? vec * move->pitch * 2.0f * TickSec * scale : Point3F(0, 0, 0));
         vec.z = 0;
         vec.normalizeSafe();
         posVec += vec;
         posVec.z += move->z * TickSec * scale + move->roll * TickSec * scale;
      }
      else // ignore input
      {
         mObjToWorld.getColumn(3,&pos);
      }

      // apply translation vector according to physics rules
      delta.posVec = pos;
      if(mNewtonMode)
      {
         bool faster = move->trigger[0];
         bool brake = move->trigger[1];

         const F32 movementSpeedMultiplier = mMovementSpeed / 40.0f; // Using the starting value as the base
         const F32 force = faster ? mFlyForce * movementSpeedMultiplier * mSpeedMultiplier : mFlyForce * movementSpeedMultiplier;
         const F32 drag = brake ? mDrag * mBrakeMultiplier : mDrag;

         VectorF acc(0.0f, 0.0f, 0.0f);

         F32 posVecL = posVec.len();
         if(posVecL > 0)
         {
            acc = (posVec * force / mMass) * TickSec;
         }

         // Accelerate
         mVelocity += acc;

         // Drag
         mVelocity -= mVelocity * drag * TickSec;

         // Move
         pos += mVelocity * TickSec;
      }
      else
      {
         pos += posVec;
      }

      setPosition(pos,mRot);

      // If on the client, calc delta for backstepping
      if (serverInterpolate || isClientObject())
      {
         delta.pos = pos;
         delta.rot = mRot;
         delta.posVec = delta.posVec - delta.pos;
         delta.rotVec = delta.rotVec - delta.rot;
      }

      if(mustValidateEyePoint)
         validateEyePoint(1.0f, &mObjToWorld);

      setMaskBits(MoveMask);
   }

   if(getControllingClient() && mContainer)
      updateContainer();
}

void Camera::onDeleteNotify(SimObject *obj)
{
   Parent::onDeleteNotify(obj);
   if (obj == (SimObject*)mOrbitObject)
   {
      mOrbitObject = NULL;

      if(mode == OrbitObjectMode)
         mode = OrbitPointMode;
   }
}

void Camera::interpolateTick(F32 dt)
{
   Parent::interpolateTick(dt);

   if ( isMounted() )
   {
      // Fetch Mount Transform.
      MatrixF mat;
      mMount.object->getMountTransform( mMount.node, &mat );

      // Apply.
      setTransform( mat );

      return;
   }

   Point3F rot = delta.rot + delta.rotVec * dt;

   if((mode == OrbitObjectMode || mode == OrbitPointMode) && !mNewtonMode)
   {
      if(mode == OrbitObjectMode && bool(mOrbitObject))
      {
         // If this is a shapebase, use its render eye transform
         // to avoid jittering.
         GameBase *castObj = mOrbitObject;
         ShapeBase* shape = dynamic_cast<ShapeBase*>(castObj);
         if( shape != NULL ) 
         {
            MatrixF ret;
            shape->getRenderEyeTransform( &ret );
            mPosition = ret.getPosition();
         } 
         else 
         {
            // Hopefully this is a static object that doesn't move,
            // because the worldbox doesn't get updated between ticks.
            mOrbitObject->getWorldBox().getCenter(&mPosition);
         }
      }
      setRenderPosition(mPosition + mOffset, rot);
      validateEyePoint(1.0f, &mRenderObjToWorld);
   }
   else if(mode == EditOrbitMode && mValidEditOrbitPoint)
   {
      mPosition = mEditOrbitPoint;
      setRenderPosition(mPosition, rot);
      calcEditOrbitPoint(&mRenderObjToWorld, rot);
   }
   else if(mode == TrackObjectMode && bool(mOrbitObject) && !mNewtonRotation)
   {
      // orient the camera to face the object
      Point3F objPos;
      // If this is a shapebase, use its render eye transform
      // to avoid jittering.
      ShapeBase *shape = dynamic_cast<ShapeBase*>((GameBase*)mOrbitObject);
      if( shape != NULL )
      {
         MatrixF ret;
         shape->getRenderEyeTransform( &ret );
         objPos = ret.getPosition();
      }
      else
      {
         mOrbitObject->getWorldBox().getCenter(&objPos);
      }
      Point3F pos = delta.pos + delta.posVec * dt;
      Point3F vec = objPos - pos;
      vec.normalizeSafe();
      F32 pitch, yaw;
      MathUtils::getAnglesFromVector(vec, yaw, pitch);
      rot.x = -pitch;
      rot.z = yaw;
      setRenderPosition(pos, rot);
   }
   else 
   {
      Point3F pos = delta.pos + delta.posVec * dt;
      setRenderPosition(pos,rot);
      if(mode == OrbitObjectMode || mode == OrbitPointMode)
         validateEyePoint(1.0f, &mRenderObjToWorld);
   }
}

void Camera::setPosition(const Point3F& pos, const Point3F& rot)
{
   MatrixF xRot, zRot;
   xRot.set(EulerF(rot.x, 0.0f, 0.0f));
   zRot.set(EulerF(0.0f, 0.0f, rot.z));
   
   MatrixF temp;
   temp.mul(zRot, xRot);
   temp.setColumn(3, pos);
   Parent::setTransform(temp);
   mRot = rot;
}

void Camera::setRenderPosition(const Point3F& pos,const Point3F& rot)
{
   MatrixF xRot, zRot;
   xRot.set(EulerF(rot.x, 0, 0));
   zRot.set(EulerF(0, 0, rot.z));
   MatrixF temp;
   temp.mul(zRot, xRot);
   temp.setColumn(3, pos);
   Parent::setRenderTransform(temp);
}

//----------------------------------------------------------------------------

void Camera::writePacketData(GameConnection *connection, BitStream *bstream)
{
   // Update client regardless of status flags.
   Parent::writePacketData(connection, bstream);

   Point3F pos;
   mObjToWorld.getColumn(3,&pos);
   bstream->setCompressionPoint(pos);
   mathWrite(*bstream, pos);
   bstream->write(mRot.x);
   bstream->write(mRot.z);

   U32 writeMode = mode;
   Point3F writePos = mPosition;
   S32 gIndex = -1;
   if(mode == OrbitObjectMode)
   {
      gIndex = bool(mOrbitObject) ? connection->getGhostIndex(mOrbitObject): -1;
      if(gIndex == -1)
      {
         writeMode = OrbitPointMode;
         if(bool(mOrbitObject))
            mOrbitObject->getWorldBox().getCenter(&writePos);
      }
   }
   else if(mode == TrackObjectMode)
   {
      gIndex = bool(mOrbitObject) ? connection->getGhostIndex(mOrbitObject): -1;
      if(gIndex == -1)
         writeMode = StationaryMode;
   }
   bstream->writeRangedU32(writeMode, CameraFirstMode, CameraLastMode);

   if (writeMode == OrbitObjectMode || writeMode == OrbitPointMode)
   {
      bstream->write(mMinOrbitDist);
      bstream->write(mMaxOrbitDist);
      bstream->write(mCurOrbitDist);
      if(writeMode == OrbitObjectMode)
      {
         bstream->writeFlag(mObservingClientObject);
         bstream->writeInt(gIndex, NetConnection::GhostIdBitSize);
      }
      if (writeMode == OrbitPointMode)
         bstream->writeCompressedPoint(writePos);
   }
   else if(writeMode == TrackObjectMode)
   {
      bstream->writeInt(gIndex, NetConnection::GhostIdBitSize);
   }

   if(bstream->writeFlag(mNewtonMode))
   {
      bstream->write(mVelocity.x);
      bstream->write(mVelocity.y);
      bstream->write(mVelocity.z);
   }
   if(bstream->writeFlag(mNewtonRotation))
   {
      bstream->write(mAngularVelocity.x);
      bstream->write(mAngularVelocity.y);
      bstream->write(mAngularVelocity.z);
   }

   bstream->writeFlag(mValidEditOrbitPoint);
   if(writeMode == EditOrbitMode)
   {
      bstream->write(mEditOrbitPoint.x);
      bstream->write(mEditOrbitPoint.y);
      bstream->write(mEditOrbitPoint.z);
      bstream->write(mCurrentEditOrbitDist);
   }
}

void Camera::readPacketData(GameConnection *connection, BitStream *bstream)
{
   Parent::readPacketData(connection, bstream);
   Point3F pos,rot;
   mathRead(*bstream, &pos);
   bstream->setCompressionPoint(pos);
   bstream->read(&rot.x);
   bstream->read(&rot.z);

   GameBase* obj = 0;
   mode = (CameraMode)bstream->readRangedU32(CameraFirstMode, CameraLastMode);
   mObservingClientObject = false;
   if (mode == OrbitObjectMode || mode == OrbitPointMode)
   {
      bstream->read(&mMinOrbitDist);
      bstream->read(&mMaxOrbitDist);
      bstream->read(&mCurOrbitDist);

      if(mode == OrbitObjectMode)
      {
         mObservingClientObject = bstream->readFlag();
         S32 gIndex = bstream->readInt(NetConnection::GhostIdBitSize);
         obj = static_cast<GameBase*>(connection->resolveGhost(gIndex));
      }
      if (mode == OrbitPointMode)
         bstream->readCompressedPoint(&mPosition);
   }
   else if (mode == TrackObjectMode)
   {
      S32 gIndex = bstream->readInt(NetConnection::GhostIdBitSize);
      obj = static_cast<GameBase*>(connection->resolveGhost(gIndex));
   }

   if (obj != (GameBase*)mOrbitObject)
   {
      if (mOrbitObject)
      {
         clearProcessAfter();
         clearNotify(mOrbitObject);
      }

      mOrbitObject = obj;
      if (mOrbitObject)
      {
         processAfter(mOrbitObject);
         deleteNotify(mOrbitObject);
      }
   }

   mNewtonMode = bstream->readFlag();
   if(mNewtonMode)
   {
      bstream->read(&mVelocity.x);
      bstream->read(&mVelocity.y);
      bstream->read(&mVelocity.z);
   }

   mNewtonRotation = bstream->readFlag();
   if(mNewtonRotation)
   {
      bstream->read(&mAngularVelocity.x);
      bstream->read(&mAngularVelocity.y);
      bstream->read(&mAngularVelocity.z);
   }

   mValidEditOrbitPoint = bstream->readFlag();
   if(mode == EditOrbitMode)
   {
      bstream->read(&mEditOrbitPoint.x);
      bstream->read(&mEditOrbitPoint.y);
      bstream->read(&mEditOrbitPoint.z);
      bstream->read(&mCurrentEditOrbitDist);
   }

   setPosition(pos,rot);
   // Movement in OrbitObjectMode is not input-based - don't reset interpolation
   if(mode != OrbitObjectMode)
   {
      delta.pos = pos;
      delta.posVec.set(0.0f, 0.0f, 0.0f);
      delta.rot = rot;
      delta.rotVec.set(0.0f, 0.0f, 0.0f);
   }
}

U32 Camera::packUpdate(NetConnection *con, U32 mask, BitStream *bstream)
{
   Parent::packUpdate(con, mask, bstream);

   if (bstream->writeFlag(mask & UpdateMask))
   {
      bstream->writeFlag(mLocked);
      mathWrite(*bstream, mOffset);
   }

   if(bstream->writeFlag(mask & NewtonCameraMask))
   {
      bstream->write(mAngularForce);
      bstream->write(mAngularDrag);
      bstream->write(mMass);
      bstream->write(mDrag);
      bstream->write(mFlyForce);
      bstream->write(mSpeedMultiplier);
      bstream->write(mBrakeMultiplier);
   }

   if(bstream->writeFlag(mask & EditOrbitMask))
   {
      bstream->write(mEditOrbitPoint.x);
      bstream->write(mEditOrbitPoint.y);
      bstream->write(mEditOrbitPoint.z);
      bstream->write(mCurrentEditOrbitDist);
   }

   // The rest of the data is part of the control object packet update.
   // If we're controlled by this client, we don't need to send it.
   if(bstream->writeFlag(getControllingClient() == con && !(mask & InitialUpdateMask)))
      return 0;

   if (bstream->writeFlag(mask & MoveMask))
   {
      Point3F pos;
      mObjToWorld.getColumn(3,&pos);
      bstream->write(pos.x);
      bstream->write(pos.y);
      bstream->write(pos.z);
      bstream->write(mRot.x);
      bstream->write(mRot.z);

      // Only required if in NewtonFlyMode
      F32 len = mVelocity.len();
      if(bstream->writeFlag(mNewtonMode && len > 0.02f))
      {
         Point3F outVel = mVelocity;
         outVel *= 1.0f/len;
         bstream->writeNormalVector(outVel, 10);
         len *= 32.0f;  // 5 bits of fraction
         if(len > 8191)
            len = 8191;
         bstream->writeInt((S32)len, 13);
      }

      // Rotation
      len = mAngularVelocity.len();
      if(bstream->writeFlag(mNewtonRotation && len > 0.02f))
      {
         Point3F outVel = mAngularVelocity;
         outVel *= 1.0f/len;
         bstream->writeNormalVector(outVel, 10);
         len *= 32.0f;  // 5 bits of fraction
         if(len > 8191)
            len = 8191;
         bstream->writeInt((S32)len, 13);
      }
   }

   return 0;
}

void Camera::unpackUpdate(NetConnection *con, BitStream *bstream)
{
   Parent::unpackUpdate(con,bstream);

   if (bstream->readFlag())
   {
      mLocked = bstream->readFlag();
      mathRead(*bstream, &mOffset);
   }

   // NewtonCameraMask
   if(bstream->readFlag())
   {
      bstream->read(&mAngularForce);
      bstream->read(&mAngularDrag);
      bstream->read(&mMass);
      bstream->read(&mDrag);
      bstream->read(&mFlyForce);
      bstream->read(&mSpeedMultiplier);
      bstream->read(&mBrakeMultiplier);
   }

   // EditOrbitMask
   if(bstream->readFlag())
   {
      bstream->read(&mEditOrbitPoint.x);
      bstream->read(&mEditOrbitPoint.y);
      bstream->read(&mEditOrbitPoint.z);
      bstream->read(&mCurrentEditOrbitDist);
   }

   // controlled by the client?
   if(bstream->readFlag())
      return;

   // MoveMask
   if (bstream->readFlag())
   {
      Point3F pos,rot;
      bstream->read(&pos.x);
      bstream->read(&pos.y);
      bstream->read(&pos.z);
      bstream->read(&rot.x);
      bstream->read(&rot.z);
      setPosition(pos,rot);

      // NewtonMode
      if(bstream->readFlag())
      {
         bstream->readNormalVector(&mVelocity, 10);
         mVelocity *= bstream->readInt(13) / 32.0f;
      }

      // NewtonRotation
      mNewtonRotation = bstream->readFlag();
      if(mNewtonRotation)
      {
         bstream->readNormalVector(&mAngularVelocity, 10);
         mAngularVelocity *= bstream->readInt(13) / 32.0f;
      }

      if(mode != OrbitObjectMode)
      {
         // New delta for client side interpolation
         delta.pos = pos;
         delta.rot = rot;
         delta.posVec = delta.rotVec = VectorF(0.0f, 0.0f, 0.0f);
      }
   }
}


//----------------------------------------------------------------------------

static EnumTable::Enums cameraTypeEnum[] = 
{
   { Camera::StationaryMode,  "Stationary"  },
   { Camera::FreeRotateMode,  "FreeRotate"  },
   { Camera::FlyMode,         "Fly"         },
   { Camera::OrbitObjectMode, "OrbitObject" },
   { Camera::OrbitPointMode,  "OrbitPoint"  },
   { Camera::TrackObjectMode, "TrackObject" },
   { Camera::OverheadMode,    "Overhead"    },
   { Camera::EditOrbitMode,   "EditOrbit"   },
};

static EnumTable gCameraTypeTable(Camera::CameraLastMode, &cameraTypeEnum[0]);

void Camera::initPersistFields()
{
   addProtectedField("controlMode",     TypeEnum,   Offset(mode, Camera), &setMode, &defaultProtectedGetFn, 1, &gCameraTypeTable,
      "The current camera control mode.");

   addGroup("Newton Mode");
   addField("newtonMode",               TypeBool,   Offset(mNewtonMode, Camera),
      "Apply smoothing (acceleration) to camera movements.");
   addField("newtonRotation",           TypeBool,   Offset(mNewtonRotation, Camera),
      "Apply smoothing (acceleration) to camera rotations.");
   addProtectedField("mass",            TypeF32,    Offset(mMass, Camera),            &setNewtonProperty, &defaultProtectedGetFn,
      "Camera mass.");
   addProtectedField("drag",            TypeF32,    Offset(mDrag, Camera),            &setNewtonProperty, &defaultProtectedGetFn,
      "Drag on camera when moving.");
   addProtectedField("force",           TypeF32,    Offset(mFlyForce, Camera),        &setNewtonProperty, &defaultProtectedGetFn,
      "Force on camera when moving.");
   addProtectedField("angularDrag",     TypeF32,    Offset(mAngularDrag, Camera),     &setNewtonProperty, &defaultProtectedGetFn,
      "Drag on camera when rotating.");
   addProtectedField("angularForce",    TypeF32,    Offset(mAngularForce, Camera),    &setNewtonProperty, &defaultProtectedGetFn,
      "Force on camera when rotating.");
   addProtectedField("speedMultiplier", TypeF32,    Offset(mSpeedMultiplier, Camera), &setNewtonProperty, &defaultProtectedGetFn,
      "Speed multiplier when triggering the accelerator.");
   addProtectedField("brakeMultiplier", TypeF32,    Offset(mBrakeMultiplier, Camera), &setNewtonProperty, &defaultProtectedGetFn,
      "Speed multiplier when triggering the brake.");
   endGroup("Newton Mode");

   Parent::initPersistFields();
}

void Camera::consoleInit()
{
   Con::addVariable("Camera::movementSpeed", TypeF32, &mMovementSpeed);
}

bool Camera::setNewtonProperty(void *obj, const char *data)
{
   static_cast<Camera*>(obj)->setMaskBits(NewtonCameraMask);
   return true;  // ok to set value
}

bool Camera::setMode(void *obj, const char *data)
{
   Camera *cam = static_cast<Camera*>(obj);

   if( dStricmp(data, "Fly") == 0 )
   {
      cam->setFlyMode();
      return false; // already changed mode
   }

   else if( dStricmp(data, "EditOrbit" ) == 0 )
   {
      cam->setEditOrbitMode();
      return false; // already changed mode
   }

   else if( (dStricmp(data, "OrbitObject") == 0 && cam->mode != OrbitObjectMode) ||
            (dStricmp(data, "TrackObject") == 0 && cam->mode != TrackObjectMode) ||
            (dStricmp(data, "OrbitPoint")  == 0 && cam->mode != OrbitPointMode)  )
   {
      Con::warnf("Couldn't change Camera mode to %s: required information missing.  Use camera.set%s().", data, data);
      return false; // don't change the mode - not valid
   }

   else if( dStricmp(data, "OrbitObject") != 0 &&
            dStricmp(data, "TrackObject") != 0 &&
            bool(cam->mOrbitObject) )
   {
      cam->clearProcessAfter();
      cam->clearNotify(cam->mOrbitObject);
      cam->mOrbitObject = NULL;
   }

   // make sure the requested mode is supported, and set it
   for( S32 i = (S32)CameraFirstMode; i <= CameraLastMode; i++ )
   {
      if( dStricmp(data, cameraTypeEnum[i].label) == 0 )
      {
         cam->mode = (CameraMode)cameraTypeEnum[i].index;
         return false;
      }
   }

   Con::warnf("Unsupported camera mode: %s", data);
   return false;
}

Camera::CameraMode Camera::getMode()
{
   return mode;
}

Point3F Camera::getPosition()
{
   static Point3F position;
   mObjToWorld.getColumn(3, &position);
   return position;
}

ConsoleMethod( Camera, getMode, const char*, 2, 2, "()"
              " - Returns the current camera control mode.\n\n")
{
   return cameraTypeEnum[object->getMode()].label;
}

ConsoleMethod( Camera, getPosition, const char *, 2, 2, "()"
              " - Get the position of the camera.\n\n"
              "@returns A string of form \"x y z\".")
{
   static char buffer[256];

   Point3F pos = object->getPosition();
   dSprintf(buffer, sizeof(buffer),"%g %g %g",pos.x,pos.y,pos.z);
   return buffer;
}

ConsoleMethod( Camera, getRotation, const char *, 2, 2, "()"
              " - Get the euler rotation of the camera.\n\n"
              "@returns A string of form \"x y z\".")
{
   static char buffer[256];

   Point3F rot = object->getRotation();
   dSprintf(buffer, sizeof(buffer),"%g %g %g",rot.x,rot.y,rot.z);
   return buffer;
}

ConsoleMethod( Camera, getOffset, const char *, 2, 2, "()"
              " - Get the offset for the camera.\n\n"
              "@returns A string of form \"x y z\".")
{
   static char buffer[256];

   Point3F offset = object->getOffset();
   dSprintf(buffer, sizeof(buffer),"%g %g %g",offset.x,offset.y,offset.z);
   return buffer;
}

ConsoleMethod( Camera, setOffset, void, 3, 3, "(Point3F offset)"
              " - Set the offset for the camera.")
{
   Point3F offset(0.0f, 0.0f, 0.0f);

   dSscanf(argv[2],"%g %g %g",
      &offset.x,&offset.y,&offset.z);

   object->setOffset(offset);
}

ConsoleMethod( Camera, setOrbitMode, void, 7, 10, "(GameBase orbitObject, transform mat, float minDistance,"
              " float maxDistance, float curDistance, [bool ownClientObject = false], [Point3F offset], [bool locked = false])\n"
              "Set the camera to orbit around some given object.  If the object passed\n"
              "is 0 or NULL, orbit around the point specified by mat.\n\n"
              "@param   orbitObject  Object we want to orbit.\n"
              "@param   mat          A set of fields: posX posY posZ rotX rotY rotZ\n"
              "@param   minDistance  Minimum distance to keep from object.\n"
              "@param   maxDistance  Maximum distance to keep from object.\n"
              "@param   curDistance  Distance to set initially from object.\n"
              "@param   ownClientObj Are we observing an object owned by us?\n"
              "@param   offset       An offset to add to our position\n"
              "@param   locked       Camera doesn't receive inputs from player")
{
   Point3F pos;
   Point3F rot;
   Point3F offset(0.0f, 0.0f, 0.0f);
   F32 minDis, maxDis, curDis;
   bool locked = false;

   GameBase *orbitObject = NULL;

   // See if we have an orbit object
   if ((dStricmp(argv[2], "NULL") == 0) || (dStricmp(argv[2], "0") == 0))
      orbitObject = NULL;
   else
   {
      if(Sim::findObject(argv[2],orbitObject) == false)
      {
         Con::warnf("Cannot orbit non-existing object.");
         object->setFlyMode();
         return;
      }
   }

   dSscanf(argv[3],"%g %g %g %g %g %g",
      &pos.x,&pos.y,&pos.z,&rot.x,&rot.y,&rot.z);
   minDis = dAtof(argv[4]);
   maxDis = dAtof(argv[5]);
   curDis = dAtof(argv[6]);

   if (argc == 9)
      dSscanf(argv[8],"%g %g %g",
         &offset.x,&offset.y,&offset.z);

   if (argc == 10)
      locked = dAtob(argv[9]);

   object->setOrbitMode(orbitObject, pos, rot, offset, minDis, maxDis, curDis, (argc == 8) ? dAtob(argv[7]) : false, locked);
}

ConsoleMethod( Camera, setOrbitObject, bool, 6, 10, "(GameBase orbitObject, vector rotation, float minDistance,"
              " float maxDistance, [float curDistance], [bool ownClientObject = false], [Point3F offset], [bool locked = false])\n"
              "Set the camera to orbit around some given object.\n\n"
              "@param   orbitObject  Object we want to orbit.\n"
              "@param   rotation     Initial camera angles in radians (x, y, z).\n"
              "@param   minDistance  Minimum distance to keep from object.\n"
              "@param   maxDistance  Maximum distance to keep from object.\n"
              "@param   curDistance  Initial distance from object (default=maxDistance).\n"
              "@param   ownClientObj Are we observing an object owned by us?\n"
              "@param   offset       An offset to add to our position\n"
              "@param   locked       Camera doesn't receive inputs from player")
{
   F32 minDis, maxDis, curDis;
   GameBase *orbitObject = NULL;
   Point3F offset(0.0f, 0.0f, 0.0f);
   Point3F rot;
   bool own = false;
   bool locked = false;

   if(Sim::findObject(argv[2],orbitObject) == false)
   {
      Con::warnf("Cannot orbit non-existing object.");
      object->setFlyMode();
      return false;
   }

   dSscanf(argv[3],"%g %g %g",
      &rot.x,&rot.y,&rot.z);

   minDis = dAtof(argv[4]);
   curDis = maxDis = dAtof(argv[5]);

   if (argc >= 7)
      curDis = dAtof(argv[6]);

   if (argc >= 8)
      own = dAtob(argv[7]);

   if (argc >= 9)
      dSscanf(argv[8],"%g %g %g",
         &offset.x,&offset.y,&offset.z);

   if (argc >= 10)
      locked = dAtob(argv[9]);

   object->setOrbitMode(orbitObject, Point3F(0,0,0), rot, offset, minDis, maxDis, curDis, own, locked);
   return true;
}

ConsoleMethod( Camera, setOrbitPoint, void, 5, 8, "(transform xform, float minDistance,"
              " float maxDistance, [float curDistance], [Point3F offset], [bool locked = false])\n"
              "Set the camera to orbit around some given point.\n\n"
              "@param   xform        A set of fields: posX posY posZ rotX rotY rotZ\n"
              "@param   minDistance  Minimum distance to keep from point.\n"
              "@param   maxDistance  Maximum distance to keep from point.\n"
              "@param   curDistance  Initial distance from point (default=maxDistance).\n"
              "@param   offset       An offset to add to our position\n"
              "@param   locked       Camera doesn't receive inputs from player")
{
   F32 minDis, maxDis, curDis;
   Point3F pos;
   Point3F rot;
   Point3F offset(0.0f, 0.0f, 0.0f);
   bool locked = false;

   dSscanf(argv[2],"%g %g %g %g %g %g",
      &pos.x,&pos.y,&pos.z,&rot.x,&rot.y,&rot.z);

   minDis = dAtof(argv[3]);
   curDis = maxDis = dAtof(argv[4]);

   if (argc >= 6)
      curDis = dAtof(argv[5]);

   if (argc >= 7)
      dSscanf(argv[6],"%g %g %g",
         &offset.x,&offset.y,&offset.z);

   if (argc >= 8)
      locked = dAtob(argv[7]);

   object->setOrbitMode(NULL, pos, rot, offset, minDis, maxDis, curDis, false, locked);
}

ConsoleMethod( Camera, setTrackObject, bool, 3, 4, "(GameBase object, [Point3F offset])\n"
              " - Set the camera to track some given object.\n\n"
              "@param   object       Object we want to track\n"
              "@param   offset       An offset to add to our object\n")
{
   Point3F offset(0.0f, 0.0f, 0.0f);
   GameBase *trackObject = NULL;

   if(Sim::findObject(argv[2],trackObject) == false)
   {
      Con::warnf("Cannot track non-existing object.");
      object->setFlyMode();
      return false;
   }

   if (argc >= 4)
      dSscanf(argv[3],"%g %g %g",
         &offset.x,&offset.y,&offset.z);

   object->setTrackObject(trackObject, offset);
   return true;
}

ConsoleMethod( Camera, setEditOrbitMode, void, 2, 2, "()"
              " - Set the editor camera to orbit around some point.\n\n")
{
   object->setEditOrbitMode();
}

ConsoleMethod( Camera, setFlyMode, void, 2, 2, "()"
              " - Set the camera to be able to fly freely.")
{
   object->setFlyMode();
}

ConsoleMethod( Camera, setNewtonFlyMode, void, 2, 2, "()"
              " - Set the camera to be able to fly freely, but with ease-in and ease-out.")
{
   object->setNewtonFlyMode();
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//    NEW Observer Code
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void Camera::setFlyMode()
{
   mode = FlyMode;

   if (bool(mOrbitObject))
   {
      clearProcessAfter();
      clearNotify(mOrbitObject);
   }
   mOrbitObject = NULL;
}

void Camera::setNewtonFlyMode()
{
   mNewtonMode = true;
   setFlyMode();
}

void Camera::setOrbitMode(GameBase *obj, const Point3F &pos, const Point3F &rot, const Point3F& offset, F32 minDist, F32 maxDist, F32 curDist, bool ownClientObject, bool locked)
{
   mObservingClientObject = ownClientObject;

   if (bool(mOrbitObject))
   {
      clearProcessAfter();
      clearNotify(mOrbitObject);
   }

   mOrbitObject = obj;
   if(bool(mOrbitObject))
   {
      processAfter(mOrbitObject);
      deleteNotify(mOrbitObject);
      mOrbitObject->getWorldBox().getCenter(&mPosition);
      mode = OrbitObjectMode;
   }
   else
   {
      mode = OrbitPointMode;
      mPosition = pos;
   }

   setPosition(mPosition, rot);

   mMinOrbitDist = minDist;
   mMaxOrbitDist = maxDist;
   mCurOrbitDist = curDist;

   if (locked != mLocked || mOffset != offset)
   {
      mLocked = locked;
      mOffset = offset;
      setMaskBits(UpdateMask);
   }
}

void Camera::setTrackObject(GameBase *obj, const Point3F &offset)
{
   if(bool(mOrbitObject))
   {
      clearProcessAfter();
      clearNotify(mOrbitObject);
   }
   mOrbitObject = obj;
   if(bool(mOrbitObject))
   {
      processAfter(mOrbitObject);
      deleteNotify(mOrbitObject);
   }

   if (mOffset != offset)
   {
      mOffset = offset;
      setMaskBits(UpdateMask);
   }
   mode = TrackObjectMode;
}

void Camera::validateEyePoint(F32 pos, MatrixF *mat)
{
   if (pos != 0) 
   {
      // Use the eye transform to orient the camera
      Point3F dir;
      mat->getColumn(1, &dir);
      if (mMaxOrbitDist - mMinOrbitDist > 0.0f)
         pos *= mMaxOrbitDist - mMinOrbitDist;
      
      // Use the camera node's pos.
      Point3F startPos = getRenderPosition();
      Point3F endPos;

      // Make sure we don't extend the camera into anything solid
      if(mOrbitObject)
         mOrbitObject->disableCollision();
      disableCollision();
      RayInfo collision;
      U32 mask = TerrainObjectType |
                 InteriorObjectType |
                 WaterObjectType |
                 StaticShapeObjectType |
                 PlayerObjectType |
                 ItemObjectType |
                 VehicleObjectType;

      Container* pContainer = isServerObject() ? &gServerContainer : &gClientContainer;
      if (!pContainer->castRay(startPos, startPos - dir * 2.5 * pos, mask, &collision))
         endPos = startPos - dir * pos;
      else
      {
         float dot = mDot(dir, collision.normal);
         if (dot > 0.01f)
         {
            float colDist = mDot(startPos - collision.point, dir) - (1 / dot) * CameraRadius;
            if (colDist > pos)
               colDist = pos;
            if (colDist < 0.0f)
               colDist = 0.0f;
            endPos = startPos - dir * colDist;
         }
         else
            endPos = startPos - dir * pos;
      }
      mat->setColumn(3, endPos);
      enableCollision();
      if(mOrbitObject)
         mOrbitObject->enableCollision();
   }
}

void Camera::setPosition(const Point3F& pos, const Point3F& rot, MatrixF *mat)
{
   MatrixF xRot, zRot;
   xRot.set(EulerF(rot.x, 0.0f, 0.0f));
   zRot.set(EulerF(0.0f, 0.0f, rot.z));
   mat->mul(zRot, xRot);
   mat->setColumn(3,pos);
   mRot = rot;
}

void Camera::setTransform(const MatrixF& mat)
{
   // This method should never be called on the client.

   // This currently converts all rotation in the mat into
   // rotations around the z and x axis.
   Point3F pos,vec;
   mat.getColumn(1, &vec);
   mat.getColumn(3, &pos);
   Point3F rot(-mAtan2(vec.z, mSqrt(vec.x * vec.x + vec.y * vec.y)), 0.0f, -mAtan2(-vec.x, vec.y));
   setPosition(pos,rot);
}

void Camera::setRenderTransform(const MatrixF& mat)
{
   // This method should never be called on the client.

   // This currently converts all rotation in the mat into
   // rotations around the z and x axis.
   Point3F pos,vec;
   mat.getColumn(1,&vec);
   mat.getColumn(3,&pos);
   Point3F rot(-mAtan2(vec.z, mSqrt(vec.x*vec.x + vec.y*vec.y)),0,-mAtan2(-vec.x,vec.y));
   setRenderPosition(pos,rot);
}

F32 Camera::getDamageFlash() const
{
   if (mode == OrbitObjectMode && isServerObject() && bool(mOrbitObject))
   {
      const GameBase *castObj = mOrbitObject;
      const ShapeBase* psb = dynamic_cast<const ShapeBase*>(castObj);
      if (psb)
         return psb->getDamageFlash();
   }

   return mDamageFlash;
}

F32 Camera::getWhiteOut() const
{
   if (mode == OrbitObjectMode && isServerObject() && bool(mOrbitObject))
   {
      const GameBase *castObj = mOrbitObject;
      const ShapeBase* psb = dynamic_cast<const ShapeBase*>(castObj);
      if (psb)
         return psb->getWhiteOut();
   }

   return mWhiteOut;
}

//----------------------------------------------------------------------------

VectorF Camera::getVelocity() const
{
   return mVelocity;
}

void Camera::setVelocity(const VectorF& vel)
{
   mVelocity = vel;
   setMaskBits(MoveMask);
}

VectorF Camera::getAngularVelocity() const
{
   return mAngularVelocity;
}

void Camera::setAngularVelocity(const VectorF& vel)
{
   mAngularVelocity = vel;
   setMaskBits(MoveMask);
}

ConsoleMethod( Camera, isRotationDamped, bool, 2, 2, "()"
              " - Is this a Newton Fly Mode camera with damped rotation?")
{
   return object->isRotationDamped();
}

ConsoleMethod( Camera, getAngularVelocity, const char *, 2, 2, "()"
              " - Get the angular velocity of the camera.\n\n"
              "@returns A string of form \"x y z\".")
{
   static char buffer[256];

   VectorF vel = object->getAngularVelocity();
   dSprintf(buffer, sizeof(buffer),"%g %g %g",vel.x,vel.y,vel.z);
   return buffer;
}

ConsoleMethod( Camera, setAngularVelocity, void, 3, 3, "(VectorF velocity)"
              " - Set the angular velocity for the camera.\n\n"
              "@param velocity   Angular velocity in the form of: velX, velY, velZ")
{
   VectorF vel(0.0f, 0.0f, 0.0f);

   dSscanf(argv[2],"%g %g %g",
      &vel.x,&vel.y,&vel.z);

   object->setAngularVelocity(vel);
}

ConsoleMethod( Camera, setAngularForce, void, 3, 3, "(F32)"
              " - Angular force for Newton camera")
{
   object->setAngularForce(dAtof(argv[2]));
}

ConsoleMethod( Camera, setAngularDrag, void, 3, 3, "(F32)"
              " - Angular drag for Newton camera")
{
   object->setAngularDrag(dAtof(argv[2]));
}

ConsoleMethod( Camera, setMass, void, 3, 3, "(F32)"
              " - Mass of Newton camera")
{
   object->setMass(dAtof(argv[2]));
}

ConsoleMethod( Camera, getVelocity, const char *, 2, 2, "()"
              " - Get the velocity of the camera.\n\n"
              "@returns A string of form \"x y z\".")
{
   static char buffer[256];

   VectorF vel = object->getVelocity();
   dSprintf(buffer, sizeof(buffer),"%g %g %g",vel.x,vel.y,vel.z);
   return buffer;
}

ConsoleMethod( Camera, setVelocity, void, 3, 3, "(VectorF velocity)"
              " - Set the velocity for the camera.\n\n"
              "@param velocity   Velocity in the form of: 'x y z'")
{
   VectorF vel(0.0f, 0.0f, 0.0f);

   dSscanf(argv[2],"%g %g %g",
      &vel.x,&vel.y,&vel.z);

   object->setVelocity(vel);
}

ConsoleMethod( Camera, setDrag, void, 3, 3, "(F32)"
              " - Drag of Newton camera")
{
   object->setDrag(dAtof(argv[2]));
}

ConsoleMethod( Camera, setFlyForce, void, 3, 3, "(F32)"
              " - Force of Newton camera")
{
   object->setFlyForce(dAtof(argv[2]));
}

ConsoleMethod( Camera, setSpeedMultiplier, void, 3, 3, "(F32)"
              " - Newton camera speed multiplier when trigger[0] is active")
{
   object->setSpeedMultiplier(dAtof(argv[2]));
}

ConsoleMethod( Camera, setBrakeMultiplier, void, 3, 3, "(F32)"
              " - Newton camera brake multiplier when trigger[1] is active")
{
   object->setBrakeMultiplier(dAtof(argv[2]));
}

//----------------------------------------------------------------------------

void Camera::setEditOrbitMode()
{
   mode = EditOrbitMode;

   if (bool(mOrbitObject))
   {
      clearProcessAfter();
      clearNotify(mOrbitObject);
   }
   mOrbitObject = NULL;

   // If there is a valid orbit point, then point to it
   // rather than move the camera.
   if(mValidEditOrbitPoint)
   {
      Point3F currentPos;
      mObjToWorld.getColumn(3,&currentPos);

      Point3F dir = mEditOrbitPoint - currentPos;
      mCurrentEditOrbitDist = dir.len();
      dir.normalize();

      F32 yaw, pitch;
      MathUtils::getAnglesFromVector(dir, yaw, pitch);
      mRot.x = -pitch;
      mRot.z = yaw;
   }
}

void Camera::calcEditOrbitPoint(MatrixF *mat, const Point3F& rot)
{
   //Point3F dir;
   //mat->getColumn(1, &dir);

   //Point3F startPos = getRenderPosition();
   //Point3F endPos = startPos - dir * mCurrentEditOrbitDist;

   Point3F pos;
   pos.x = mCurrentEditOrbitDist * mSin(rot.x + mDegToRad(90.0f)) * mCos(-1.0f * (rot.z + mDegToRad(90.0f))) + mEditOrbitPoint.x;
   pos.y = mCurrentEditOrbitDist * mSin(rot.x + mDegToRad(90.0f)) * mSin(-1.0f * (rot.z + mDegToRad(90.0f))) + mEditOrbitPoint.y;
   pos.z = mCurrentEditOrbitDist * mSin(rot.x) + mEditOrbitPoint.z;

   mat->setColumn(3, pos);
}

void Camera::setValidEditOrbitPoint(bool state)
{
   mValidEditOrbitPoint = state;
   setMaskBits(EditOrbitMask);
}

Point3F Camera::getEditOrbitPoint() const
{
   return mEditOrbitPoint;
}

void Camera::setEditOrbitPoint(const Point3F& pnt)
{
   // Change the point that we orbit in EditOrbitMode.
   // We'll also change the facing and distance of the
   // camera so that it doesn't jump around.
   Point3F currentPos;
   mObjToWorld.getColumn(3,&currentPos);

   Point3F dir = pnt - currentPos;
   mCurrentEditOrbitDist = dir.len();

   if(mode == EditOrbitMode)
   {
      dir.normalize();

      F32 yaw, pitch;
      MathUtils::getAnglesFromVector(dir, yaw, pitch);
      mRot.x = -pitch;
      mRot.z = yaw;
   }

   mEditOrbitPoint = pnt;

   setMaskBits(EditOrbitMask);
}

ConsoleMethod( Camera, isEditOrbitMode, bool, 2, 2, "()"
              " - Is the camera in edit orbit mode")
{
   return object->isEditOrbitMode();
}

ConsoleMethod( Camera, setValidEditOrbitPoint, void, 3, 3, "(bool)"
              " - Indicate if there is a valid editor camera orbit point")
{
   object->setValidEditOrbitPoint(dAtob(argv[2]));
}

ConsoleMethod( Camera, setEditOrbitPoint, void, 3, 3, "(Point3F point)"
              " - Set the editor camera's orbit point.\n\n"
              "@param point   Orbit point in the form of: 'x y z'")
{
   Point3F pnt(0.0f, 0.0f, 0.0f);

   dSscanf(argv[2],"%g %g %g",
      &pnt.x,&pnt.y,&pnt.z);

   object->setEditOrbitPoint(pnt);
}

//----------------------------------------------------------------------------

void Camera::autoFitRadius(F32 radius)
{
   F32 fov = getCameraFov();
   F32 viewradius = (radius * 2.0f) / mTan(fov * 0.5f);

   // Be careful of infinite sized objects.  Clip to 16km
   if(viewradius > 16000.0f)
      viewradius = 16000.0f;

   if(mode == EditOrbitMode && mValidEditOrbitPoint)
   {
      mCurrentEditOrbitDist = viewradius;
   }
   else if(mValidEditOrbitPoint)
   {
      mCurrentEditOrbitDist = viewradius;

      Point3F currentPos;
      mObjToWorld.getColumn(3,&currentPos);

      Point3F dir = mEditOrbitPoint - currentPos;
      dir.normalize();

      F32 yaw, pitch;
      MathUtils::getAnglesFromVector(dir, yaw, pitch);
      mRot.x = -pitch;
      mRot.z = yaw;

      mPosition = mEditOrbitPoint;
      setPosition(mPosition, mRot);
      calcEditOrbitPoint(&mObjToWorld, mRot);
   }
}

ConsoleMethod( Camera, autoFitRadius, void, 3, 3, "(F32 radius)"
              " - Orient the camera to view the given radius.\n\n"
              "@param radius   Selection radius to view")
{
   object->autoFitRadius(dAtof(argv[2]));
}

//----------------------------------------------------------------------------

void Camera::lookAt(const Point3F &pos)
{
   Point3F vec;
   mObjToWorld.getColumn(3, &mPosition);
   vec = pos - mPosition;
   vec.normalizeSafe();
   F32 pitch, yaw;
   MathUtils::getAnglesFromVector(vec, yaw, pitch);
   mRot.x = -pitch;
   mRot.z = yaw;
   setPosition(mPosition, mRot);
}

ConsoleMethod( Camera, lookAt, void, 3, 3, "(point p)"
              " - Point the camera at the specified location. (does not work in Orbit or Track modes)")
{
   Point3F point;
   dSscanf(argv[2], "%g %g %g",
      &point.x, &point.y, &point.z);

   object->lookAt(point);
}