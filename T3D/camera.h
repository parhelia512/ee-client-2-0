//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _CAMERA_H_
#define _CAMERA_H_

#ifndef _SHAPEBASE_H_
#include "T3D/shapeBase.h"
#endif

//----------------------------------------------------------------------------
struct CameraData: public ShapeBaseData {
   typedef ShapeBaseData Parent;

   //
   DECLARE_CONOBJECT(CameraData);
   static void initPersistFields();
   virtual void packData(BitStream* stream);
   virtual void unpackData(BitStream* stream);
};


//----------------------------------------------------------------------------
/// Implements a basic camera object.
class Camera: public ShapeBase
{
   typedef ShapeBase Parent;

   enum MaskBits {
      MoveMask          = Parent::NextFreeMask,
      UpdateMask        = Parent::NextFreeMask << 1,
      NewtonCameraMask  = Parent::NextFreeMask << 2,
      EditOrbitMask     = Parent::NextFreeMask << 3,
      NextFreeMask      = Parent::NextFreeMask << 4
   };

   struct StateDelta {
      Point3F pos;
      Point3F rot;
      VectorF posVec;
      VectorF rotVec;
   };
   Point3F mRot;
   StateDelta delta;

   Point3F mOffset;

   static F32 mMovementSpeed;

   void setPosition(const Point3F& pos,const Point3F& viewRot);
   void setRenderPosition(const Point3F& pos,const Point3F& viewRot);

   SimObjectPtr<GameBase> mOrbitObject;
   F32 mMinOrbitDist;
   F32 mMaxOrbitDist;
   F32 mCurOrbitDist;
   Point3F mPosition;
   bool mObservingClientObject;

   // Used by NewtonMode
   VectorF  mAngularVelocity;
   F32      mAngularForce;
   F32      mAngularDrag;
   VectorF  mVelocity;
   bool     mNewtonMode;
   bool     mNewtonRotation;
   F32      mMass;
   F32      mDrag;
   F32      mFlyForce;
   F32      mSpeedMultiplier;
   F32      mBrakeMultiplier;

   // Used by EditOrbitMode
   bool     mValidEditOrbitPoint;
   Point3F  mEditOrbitPoint;
   F32      mCurrentEditOrbitDist;

   bool mLocked;

public:
   enum CameraMode
   {
      StationaryMode  = 0,

      FreeRotateMode,
      FlyMode,
      OrbitObjectMode,
      OrbitPointMode,
      TrackObjectMode,
      OverheadMode,
      EditOrbitMode,       ///< Used by the World Editor

      CameraFirstMode = 0,
      CameraLastMode  = EditOrbitMode
   };

private:
   CameraMode  mode;

   void setPosition(const Point3F& pos,const Point3F& viewRot, MatrixF *mat);
   void setTransform(const MatrixF& mat);
   void setRenderTransform(const MatrixF& mat);
   F32 getCameraFov();
   F32 getDefaultCameraFov();
   bool isValidCameraFov(F32 fov);
   void setCameraFov(F32 fov);

   F32 getDamageFlash() const;
   F32 getWhiteOut() const;

public:
   DECLARE_CONOBJECT(Camera);

   Camera();
   ~Camera();
   static void initPersistFields();
   static void consoleInit();
   static bool setMode(void *obj, const char *data);
   static bool setNewtonProperty(void *obj, const char *data);

   void onEditorEnable();
   void onEditorDisable();

   bool onAdd();
   void onRemove();
   void processTick(const Move* move);
   void interpolateTick(F32 delta);
   void getCameraTransform(F32* pos,MatrixF* mat);

   void writePacketData(GameConnection *conn, BitStream *stream);
   void readPacketData(GameConnection *conn, BitStream *stream);
   U32  packUpdate(NetConnection *conn, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *conn, BitStream *stream);

   CameraMode getMode();

   Point3F getPosition();
   Point3F getRotation() { return mRot; };
   Point3F getOffset() { return mOffset; };
   void lookAt(const Point3F &pos);
   void setOffset(const Point3F &offset) { mOffset = offset; };
   void setFlyMode();
   void setNewtonFlyMode();
   void setOrbitMode(GameBase *obj, const Point3F &pos, const Point3F &rot, const Point3F& offset,
                     F32 minDist, F32 maxDist, F32 curDist, bool ownClientObject, bool locked = false);
   void setTrackObject(GameBase *obj, const Point3F &offset);
   void validateEyePoint(F32 pos, MatrixF *mat);
   void onDeleteNotify(SimObject *obj);

   GameBase * getOrbitObject()      { return(mOrbitObject); }
   bool isObservingClientObject()   { return(mObservingClientObject); }

   // Used by NewtonFlyMode
   VectorF getVelocity() const;
   void setVelocity(const VectorF& vel);
   VectorF getAngularVelocity() const;
   void setAngularVelocity(const VectorF& vel);
   bool isRotationDamped() {return mNewtonRotation;}
   void setAngularForce(F32 force) {mAngularForce = force; setMaskBits(NewtonCameraMask);}
   void setAngularDrag(F32 drag) {mAngularDrag = drag; setMaskBits(NewtonCameraMask);}
   void setMass(F32 mass) {mMass = mass; setMaskBits(NewtonCameraMask);}
   void setDrag(F32 drag) {mDrag = drag; setMaskBits(NewtonCameraMask);}
   void setFlyForce(F32 force) {mFlyForce = force; setMaskBits(NewtonCameraMask);}
   void setSpeedMultiplier(F32 mul) {mSpeedMultiplier = mul; setMaskBits(NewtonCameraMask);}
   void setBrakeMultiplier(F32 mul) {mBrakeMultiplier = mul; setMaskBits(NewtonCameraMask);}

   // Used by EditOrbitMode
   void setEditOrbitMode();
   bool isEditOrbitMode() {return mode == EditOrbitMode;}
   void calcEditOrbitPoint(MatrixF *mat, const Point3F& rot);
   bool getValidEditOrbitPoint() { return mValidEditOrbitPoint; }
   void setValidEditOrbitPoint(bool state);
   Point3F getEditOrbitPoint() const;
   void setEditOrbitPoint(const Point3F& pnt);

   // Orient the camera to view the given radius.  Requires that an
   // edit orbit point has been set.
   void autoFitRadius(F32 radius);
};


#endif
