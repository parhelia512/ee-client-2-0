//-----------------------------------------------------------------------------
// Torque Game Engine 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _RIGIDSHAPE_H_
#define _RIGIDSHAPE_H_

#ifndef _SHAPEBASE_H_
#include "T3D/shapeBase.h"
#endif
#ifndef _RIGID_H_
#include "T3D/rigid.h"
#endif
#ifndef _BOXCONVEX_H_
#include "collision/boxConvex.h"
#endif

#include "sfx/sfxSystem.h"
#include "T3D/fx/particleEmitter.h"

class ParticleEmitter;
class ParticleEmitterData;
class ClippedPolyList;

//----------------------------------------------------------------------------

class RigidShapeData : public ShapeBaseData
{
   typedef ShapeBaseData Parent;

  protected:
   bool onAdd();

   //-------------------------------------- Console set variables
  public:

   struct Body 
   {
      enum Sounds 
      {
         SoftImpactSound,
         HardImpactSound,
         MaxSounds,
      };
      SFXProfile* sound[MaxSounds];
      F32 restitution;
      F32 friction;
   } body;

   enum RigidShapeConsts
   {
      VC_NUM_DUST_EMITTERS = 1,
      VC_NUM_BUBBLE_EMITTERS = 1,
      VC_NUM_SPLASH_EMITTERS = 2,
      VC_BUBBLE_EMITTER = VC_NUM_BUBBLE_EMITTERS,
   };

  enum Sounds 
  {
      ExitWater,
      ImpactSoft,
      ImpactMedium,
      ImpactHard,
      Wake,
      MaxSounds
   };
   SFXProfile* waterSound[MaxSounds];

   F32 exitSplashSoundVel;
   F32 softSplashSoundVel;
   F32 medSplashSoundVel;
   F32 hardSplashSoundVel;
 
   F32 minImpactSpeed;
   F32 softImpactSpeed;
   F32 hardImpactSpeed;
   F32 minRollSpeed;
   
   bool cameraRoll;           ///< Roll the 3rd party camera
   F32 cameraLag;             ///< Amount of camera lag (lag += car velocity * lag)
   F32 cameraDecay;           ///< Rate at which camera returns to target pos.
   F32 cameraOffset;          ///< Vertical offset

   F32 minDrag;
   F32 maxDrag;
   S32 integration;           ///< # of physics steps per tick
   F32 collisionTol;          ///< Collision distance tolerance
   F32 contactTol;            ///< Contact velocity tolerance
   Point3F massCenter;        ///< Center of mass for rigid body
   Point3F massBox;           ///< Size of inertial box

   ParticleEmitterData * dustEmitter;
   S32 dustID;
   F32 triggerDustHeight;  ///< height shape has to be under to kick up dust
   F32 dustHeight;         ///< dust height above ground

   ParticleEmitterData* splashEmitterList[VC_NUM_SPLASH_EMITTERS];
   S32 splashEmitterIDList[VC_NUM_SPLASH_EMITTERS];
   F32 splashFreqMod;
   F32 splashVelEpsilon;


   F32 dragForce;
   F32 vertFactor;

   F32 normalForce;
   F32 restorativeForce;
   F32 rollForce;
   F32 pitchForce;
   
   ParticleEmitterData * dustTrailEmitter;
   S32                   dustTrailID;
   Point3F               dustTrailOffset;
   F32                   triggerTrailHeight;
   F32                   dustTrailFreqMod;

   //-------------------------------------- load set variables

  public:
   RigidShapeData();
   ~RigidShapeData();

   static void initPersistFields();
   void packData(BitStream*);
   void unpackData(BitStream*);
   bool preload(bool server, String &errorStr);

   DECLARE_CONOBJECT(RigidShapeData);

};


//----------------------------------------------------------------------------

class RigidShape: public ShapeBase
{
   typedef ShapeBase Parent;

  private:
   RigidShapeData* mDataBlock;
   ParticleEmitter * mDustTrailEmitter;

  protected:
   enum CollisionFaceFlags 
   {
      BodyCollision =  BIT(0),
      WheelCollision = BIT(1),
   };
   enum MaskBits {
      PositionMask = Parent::NextFreeMask << 0,
      EnergyMask   = Parent::NextFreeMask << 1,
      FreezeMask   = Parent::NextFreeMask << 2,
      NextFreeMask = Parent::NextFreeMask << 3
   };

   void updateDustTrail( F32 dt );


   struct StateDelta 
   {
      Move move;                    ///< Last move from server
      F32 dt;                       ///< Last interpolation time
      // Interpolation data
      Point3F pos;
      Point3F posVec;
      QuatF rot[2];
      // Warp data
      S32 warpTicks;                ///< Number of ticks to warp
      S32 warpCount;                ///< Current pos in warp
      Point3F warpOffset;
      QuatF warpRot[2];
      //
      Point3F cameraOffset;
      Point3F cameraVec;
      Point3F cameraRot;
      Point3F cameraRotVec;
   };

   StateDelta mDelta;
   S32 mPredictionCount;            ///< Number of ticks to predict
   bool inLiquid;

   Point3F mCameraOffset; ///< 3rd person camera

   // Rigid Body
   bool mDisableMove;

   CollisionList mCollisionList;
   CollisionList mContacts;
   Rigid mRigid;
   ShapeBaseConvex mConvex;
   int restCount;

   ParticleEmitter *mDustEmitterList[RigidShapeData::VC_NUM_DUST_EMITTERS];
   ParticleEmitter *mSplashEmitterList[RigidShapeData::VC_NUM_SPLASH_EMITTERS];

   GFXStateBlockRef  mSolidSB;

   //
   bool onNewDataBlock(GameBaseData* dptr);
   void updatePos(F32 dt);
   bool updateCollision(F32 dt);
   bool resolveCollision(Rigid& ns,CollisionList& cList);
   bool resolveContacts(Rigid& ns,CollisionList& cList,F32 dt);
   bool resolveDisplacement(Rigid& ns,CollisionState *state,F32 dt);
   bool findContacts(Rigid& ns,CollisionList& cList);
   void checkTriggers();
   static void findCallback(SceneObject* obj,void * key);

   void setPosition(const Point3F& pos,const QuatF& rot);
   void setRenderPosition(const Point3F& pos,const QuatF& rot);
   void setTransform(const MatrixF& mat);

//   virtual bool collideBody(const MatrixF& mat,Collision* info) = 0;
   void updateMove(const Move* move);

   void writePacketData(GameConnection * conn, BitStream *stream);
   void readPacketData (GameConnection * conn, BitStream *stream);

   void updateLiftoffDust( F32 dt );

   void updateWorkingCollisionSet(const U32 mask);
   U32 getCollisionMask();

   void updateFroth( F32 dt );
   bool collidingWithWater( Point3F &waterHeight );

   void _renderMassAndContacts( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );

   void updateForces(F32);

public:
   // Test code...
   static ClippedPolyList* sPolyList;

   //
   RigidShape();
   ~RigidShape();

   static void initPersistFields();
   void processTick(const Move *move);
   bool onAdd();
   void onRemove();
   
   /// Interpolates between move ticks @see processTick
   /// @param   dt   Change in time between the last call and this call to the function
   void interpolateTick(F32 dt);
   void advanceTime(F32 dt);

   /// Disables collisions for this shape
   void disableCollision();
   
   /// Enables collisions for this shape
   void enableCollision();

   /// Returns the velocity of the shape
   Point3F getVelocity() const;

   void setEnergyLevel(F32 energy);
   
   void prepBatchRender(  SceneState *state, S32 mountedImageIndex );

   // xgalaxy cool hacks
   void reset();
   void freezeSim(bool frozen);

   ///@name Rigid body methods
   ///@{
   
   /// This method will get the velocity of the object, taking into account
   /// angular velocity.
   /// @param   r   Point on the object you want the velocity of, relative to Center of Mass
   /// @param   vel   Velocity (out)
   void getVelocity(const Point3F& r, Point3F* vel);
   
   /// Applies an impulse force 
   /// @param   r   Point on the object to apply impulse to, r is relative to Center of Mass
   /// @param   impulse   Impulse vector to apply.
   void applyImpulse(const Point3F &r, const Point3F &impulse);

   void getCameraParameters(F32 *min, F32* max, Point3F* offset, MatrixF* rot);
   void getCameraTransform(F32* pos, MatrixF* mat);
   ///@}

   U32  packUpdate  (NetConnection *conn, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *conn,           BitStream *stream);

   DECLARE_CONOBJECT(RigidShape);
};


#endif
