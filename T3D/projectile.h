//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PROJECTILE_H_
#define _PROJECTILE_H_

#ifndef _GAMEBASE_H_
#include "T3D/gameBase.h"
#endif
#ifndef _TSSHAPE_H_
#include "ts/tsShape.h"
#endif
#ifndef _SFXSOURCE_H_
#include "sfx/sfxSource.h"
#endif
#ifndef _H_PARTICLE_EMITTER
#include "T3D/fx/particleEmitter.h"
#endif
#ifndef _LIGHTDESCRIPTION_H_
#include "T3D/lightDescription.h"
#endif

class ExplosionData;
class SplashData;
class ShapeBase;
class TSShapeInstance;
class TSThread;
class PhysicsWorld;
class DecalData;
class LightDescription;

//--------------------------------------------------------------------------
/// Datablock for projectiles.  This class is the base class for all other projectiles.
class ProjectileData : public GameBaseData
{
   typedef GameBaseData Parent;

protected:
   bool onAdd();

public:
   // variables set in datablock definition:
   // Shape related
   const char* projectileShapeName;

   /// Set to true if it is a billboard and want it to always face the viewer, false otherwise
   bool faceViewer;
   Point3F scale;


   /// [0,1] scale of how much velocity should be inherited from the parent object
   F32 velInheritFactor;
   /// Speed of the projectile when fired
   F32 muzzleVelocity;

   /// Force imparted on a hit object.
   F32 impactForce;

   /// Should it arc?
   bool isBallistic;

   /// How HIGH should it bounce (parallel to normal), [0,1]
   F32 bounceElasticity;
   /// How much momentum should be lost when it bounces (perpendicular to normal), [0,1]
   F32 bounceFriction;

   /// Should this projectile fall/rise different than a default object?
   F32 gravityMod;

   /// How long the projectile should exist before deleting itself
   U32 lifetime;     // all times are internally represented as ticks
   /// How long it should not detonate on impact
   S32 armingDelay;  // the values are converted on initialization with
   S32 fadeDelay;    // the IRangeValidatorScaled field validator

   ExplosionData* explosion;
   S32 explosionId;

   ExplosionData* waterExplosion;      // Water Explosion Datablock
   S32 waterExplosionId;               // Water Explosion ID

   SplashData* splash;                 // Water Splash Datablock
   S32 splashId;                       // Water splash ID

   DecalData *decal;                   // (impact) Decal Datablock
   S32 decalId;                        // (impact) Decal ID

   SFXProfile* sound;                  // Projectile Sound
   S32 soundId;                        // Projectile Sound ID   
   
   LightDescription *lightDesc;
   S32 lightDescId;   

   // variables set on preload:
   Resource<TSShape> projectileShape;
   S32 activateSeq;
   S32 maintainSeq;

   ParticleEmitterData* particleEmitter;
   S32 particleEmitterId;

   ParticleEmitterData* particleWaterEmitter;
   S32 particleWaterEmitterId;

   ProjectileData();

   void packData(BitStream*);
   void unpackData(BitStream*);
   bool preload(bool server, String &errorStr);

   static bool setLifetime( void *obj, const char *data );
   static bool setArmingDelay( void *obj, const char *data );
   static bool setFadeDelay( void *obj, const char *data );
   static const char *getScaledValue( void *obj, const char *data);
   static S32 scaleValue( S32 value, bool down = true );

   static void initPersistFields();
   DECLARE_CONOBJECT(ProjectileData);
};
DECLARE_CONSOLETYPE(ProjectileData)


//--------------------------------------------------------------------------
/// Base class for all projectiles.
class Projectile : public GameBase, public ISceneLight
{
   typedef GameBase Parent;

public:
   // Initial conditions
   enum ProjectileConstants {
      SourceIdTimeoutTicks = 7,   // = 231 ms
      DeleteWaitTime       = 500, ///< 500 ms delete timeout (for network transmission delays)
      ExcessVelDirBits     = 7,
      MaxLivingTicks       = 4095,
   };
   enum UpdateMasks {
      BounceMask    = Parent::NextFreeMask,
      ExplosionMask = Parent::NextFreeMask << 1,
      NextFreeMask  = Parent::NextFreeMask << 2
   };
protected:

   PhysicsWorld *mPhysicsWorld;

   ProjectileData* mDataBlock;

   SimObjectPtr< ParticleEmitter > mParticleEmitter;
   SimObjectPtr< ParticleEmitter > mParticleWaterEmitter;

   SFXSource* mSound;

   Point3F  mCurrPosition;
   Point3F  mCurrVelocity;
   S32      mSourceObjectId;
   S32      mSourceObjectSlot;

   // Time related variables common to all projectiles, managed by processTick

   U32 mCurrTick;                         ///< Current time in ticks
   SimObjectPtr<ShapeBase> mSourceObject; ///< Actual pointer to the source object, times out after SourceIdTimeoutTicks

   // Rendering related variables
   TSShapeInstance* mProjectileShape;
   TSThread*        mActivateThread;
   TSThread*        mMaintainThread;

   /// ISceneLight
   virtual void submitLights( LightManager *lm, bool staticLighting );
   virtual LightInfo* getLight() { return mLight; }
   
   LightInfo *mLight;
   LightState mLightState;   

   bool             mHidden;        ///< set by the derived class, if true, projectile doesn't render
   F32              mFadeValue;     ///< set in processTick, interpolation between fadeDelay and lifetime
                                 ///< in data block

   // Warping and back delta variables.  Only valid on the client
   //
   Point3F mWarpStart;
   Point3F mWarpEnd;
   U32     mWarpTicksRemaining;

   Point3F mCurrDeltaBase;
   Point3F mCurrBackDelta;

   Point3F mExplosionPosition;
   Point3F mExplosionNormal;
   U32     mCollideHitType;

   bool onAdd();
   void onRemove();
   bool onNewDataBlock(GameBaseData *dptr);

   void processTick(const Move *move);
   void advanceTime(F32 dt);
   void interpolateTick(F32 delta);

   /// What to do once this projectile collides with something
   virtual void onCollision(const Point3F& p, const Point3F& n, SceneObject*);

   /// What to do when this projectile explodes
   virtual void explode(const Point3F& p, const Point3F& n, const U32 collideType );

   /// Returns the velocity of the projectile
   Point3F getVelocity() const;
   bool pointInWater(const Point3F &point);
   void emitParticles(const Point3F&, const Point3F&, const Point3F&, const U32);
   void updateSound();

   // Rendering
   bool prepRenderImage  ( SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseZoneState=false);
   void prepBatchRender  ( SceneState *state);



   U32  packUpdate  (NetConnection *conn, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *conn,           BitStream *stream);

public:
   F32 getUpdatePriority(CameraScopeQuery *focusObject, U32 updateMask, S32 updateSkips);

   Projectile();
   ~Projectile();

   DECLARE_CONOBJECT(Projectile);
   static void initPersistFields();

   virtual bool calculateImpact(float    simTime,
                                Point3F& pointOfImpact,
                                float&   impactTime);

   static U32 smProjectileWarpTicks;

protected:
   static const U32 csmStaticCollisionMask;
   static const U32 csmDynamicCollisionMask;
   static const U32 csmDamageableMask;
};

#endif // _H_PROJECTILE

