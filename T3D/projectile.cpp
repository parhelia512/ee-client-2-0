//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sceneGraph/sceneState.h"
#include "sceneGraph/sceneGraph.h"
#include "lighting/lightInfo.h"
#include "lighting/lightManager.h"
#include "console/consoleTypes.h"
#include "console/typeValidators.h"
#include "core/resourceManager.h"
#include "core/stream/bitStream.h"
#include "T3D/fx/explosion.h"
#include "T3D/shapeBase.h"
#include "ts/tsShapeInstance.h"
#include "T3D/projectile.h"
#include "sfx/sfxSystem.h"
#include "math/mathUtils.h"
#include "math/mathIO.h"
#include "sim/netConnection.h"
#include "T3D/fx/particleEmitter.h"
#include "T3D/fx/splash.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsWorld.h"
#include "gfx/gfxTransformSaver.h"
#include "T3D/containerQuery.h"
#include "T3D/decal/decalManager.h"
#include "T3D/decal/decalData.h"
#include "T3D/lightDescription.h"

IMPLEMENT_CO_DATABLOCK_V1(ProjectileData);
IMPLEMENT_CO_NETOBJECT_V1(Projectile);

const U32 Projectile::csmStaticCollisionMask =  TerrainObjectType    |
                                                InteriorObjectType   |
                                                StaticObjectType;

const U32 Projectile::csmDynamicCollisionMask = PlayerObjectType        |
                                                VehicleObjectType       |
                                                DamagableItemObjectType;

const U32 Projectile::csmDamageableMask = Projectile::csmDynamicCollisionMask;

U32 Projectile::smProjectileWarpTicks = 5;


//--------------------------------------------------------------------------
//
ProjectileData::ProjectileData()
{
   projectileShapeName = NULL;

   sound = NULL;
   soundId = 0;

   explosion = NULL;
   explosionId = 0;

   waterExplosion = NULL;
   waterExplosionId = 0;

   //hasLight = false;
   //lightRadius = 1;
   //lightColor.set(1, 1, 1);
   lightDesc = NULL;

   faceViewer = false;
   scale.set( 1.0f, 1.0f, 1.0f );

   isBallistic = false;

	velInheritFactor = 1.0f;
	muzzleVelocity = 50;
   impactForce = 0.0f;

	armingDelay = 0;
   fadeDelay = 20000 / 32;
   lifetime = 20000 / 32;

   activateSeq = -1;
   maintainSeq = -1;

   gravityMod = 1.0;
   bounceElasticity = 0.999f;
   bounceFriction = 0.3f;

   particleEmitter = NULL;
   particleEmitterId = 0;

   particleWaterEmitter = NULL;
   particleWaterEmitterId = 0;

   splash = NULL;
   splashId = 0;

   decal = NULL;
   decalId = 0;

   lightDesc = NULL;
   lightDescId = 0;
}

//--------------------------------------------------------------------------
IMPLEMENT_CONSOLETYPE(ProjectileData)
IMPLEMENT_GETDATATYPE(ProjectileData)
IMPLEMENT_SETDATATYPE(ProjectileData)

void ProjectileData::initPersistFields()
{
   addNamedField(particleEmitter,  TypeParticleEmitterDataPtr, ProjectileData);
   addNamedField(particleWaterEmitter, TypeParticleEmitterDataPtr, ProjectileData);

   addNamedField(projectileShapeName, TypeFilename, ProjectileData);
   addNamedField(scale, TypePoint3F, ProjectileData);

   addNamedField(sound, TypeSFXProfilePtr, ProjectileData);

   addNamedField(explosion, TypeExplosionDataPtr, ProjectileData);
   addNamedField(waterExplosion, TypeExplosionDataPtr, ProjectileData);

   addNamedField(splash, TypeSplashDataPtr, ProjectileData);

   addNamedField(decal, TypeDecalDataPtr, ProjectileData);

   addNamedField(lightDesc, TypeLightDescriptionPtr, ProjectileData );

   static FRangeValidator lightRadiusValidator( 1, 20 );
   static FRangeValidator velInheritFactorValidator( 0, 1 );
   static FRangeValidator muzzleVelocityValidator( 0, 10000 );

   addNamedField(isBallistic, TypeBool, ProjectileData);
	addNamedFieldV(velInheritFactor, TypeF32, ProjectileData, &velInheritFactorValidator );
   addNamedFieldV(muzzleVelocity, TypeF32, ProjectileData, &muzzleVelocityValidator );
   addNamedField(impactForce, TypeF32, ProjectileData);

   static IRangeValidatorScaled lifetimeValidator( TickMs, 0, Projectile::MaxLivingTicks );
   static IRangeValidatorScaled armingDelayValidator( TickMs, 0, Projectile::MaxLivingTicks );
   static IRangeValidatorScaled fadeDelayValidator( TickMs, 0, Projectile::MaxLivingTicks );

   char message[1024];
   dSprintf(message, sizeof(message), "Milliseconds, values will be adjusted to fit %d millisecond tick intervals", TickMs);
   addProtectedField("lifetime", TypeS32, Offset(lifetime, ProjectileData), &setLifetime, &getScaledValue, message);
   addProtectedField("armingDelay", TypeS32, Offset(armingDelay, ProjectileData), &setArmingDelay, &getScaledValue, message);
   addProtectedField("fadeDelay", TypeS32, Offset(fadeDelay, ProjectileData), &setFadeDelay, &getScaledValue, message);

   static FRangeValidator bounceElasticityValidator( 0, 0.999f );
   static FRangeValidator bounceFrictionValidator( 0, 1 );
   static FRangeValidator gravityModValidator( 0, 1 );

   addNamedFieldV(bounceElasticity, TypeF32, ProjectileData, &bounceElasticityValidator );
   addNamedFieldV(bounceFriction, TypeF32, ProjectileData, &bounceFrictionValidator );
   addNamedFieldV(gravityMod, TypeF32, ProjectileData, &gravityModValidator );

   Parent::initPersistFields();
}


//--------------------------------------------------------------------------
bool ProjectileData::onAdd()
{
   if(!Parent::onAdd())
      return false;

   if (!particleEmitter && particleEmitterId != 0)
      if (Sim::findObject(particleEmitterId, particleEmitter) == false)
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::onAdd: Invalid packet, bad datablockId(particleEmitter): %d", particleEmitterId);

   if (!particleWaterEmitter && particleWaterEmitterId != 0)
      if (Sim::findObject(particleWaterEmitterId, particleWaterEmitter) == false)
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::onAdd: Invalid packet, bad datablockId(particleWaterEmitter): %d", particleWaterEmitterId);

   if (!explosion && explosionId != 0)
      if (Sim::findObject(explosionId, explosion) == false)
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::onAdd: Invalid packet, bad datablockId(explosion): %d", explosionId);

   if (!waterExplosion && waterExplosionId != 0)
      if (Sim::findObject(waterExplosionId, waterExplosion) == false)
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::onAdd: Invalid packet, bad datablockId(waterExplosion): %d", waterExplosionId);

   if (!splash && splashId != 0)
      if (Sim::findObject(splashId, splash) == false)
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::onAdd: Invalid packet, bad datablockId(splash): %d", splashId);

   if (!decal && decalId != 0)
      if (Sim::findObject(decalId, decal) == false)
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::onAdd: Invalid packet, bad datablockId(decal): %d", decalId);

   if (!sound && soundId != 0)
      if (Sim::findObject(soundId, sound) == false)
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::onAdd: Invalid packet, bad datablockid(sound): %d", soundId);

   if (!lightDesc && lightDescId != 0)
      if (Sim::findObject(lightDescId, lightDesc) == false)
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::onAdd: Invalid packet, bad datablockid(lightDesc): %d", lightDescId);   

   return true;
}


bool ProjectileData::preload(bool server, String &errorStr)
{
   if (Parent::preload(server, errorStr) == false)
      return false;

   if (projectileShapeName && projectileShapeName[0] != '\0')
   {
      projectileShape = ResourceManager::get().load(projectileShapeName);
      if (bool(projectileShape) == false)
      {
         errorStr = String::ToString("ProjectileData::load: Couldn't load shape \"%s\"", projectileShapeName);
         return false;
      }
      activateSeq = projectileShape->findSequence("activate");
      maintainSeq = projectileShape->findSequence("maintain");
   }

   if (bool(projectileShape)) // create an instance to preload shape data
   {
      TSShapeInstance* pDummy = new TSShapeInstance(projectileShape, !server);
      delete pDummy;
   }

   return true;
}

//--------------------------------------------------------------------------
void ProjectileData::packData(BitStream* stream)
{
   Parent::packData(stream);

   stream->writeString(projectileShapeName);
   stream->writeFlag(faceViewer);
   if(stream->writeFlag(scale.x != 1 || scale.y != 1 || scale.z != 1))
   {
      stream->write(scale.x);
      stream->write(scale.y);
      stream->write(scale.z);
   }

   if (stream->writeFlag(particleEmitter != NULL))
      stream->writeRangedU32(particleEmitter->getId(), DataBlockObjectIdFirst,
                                                   DataBlockObjectIdLast);

   if (stream->writeFlag(particleWaterEmitter != NULL))
      stream->writeRangedU32(particleWaterEmitter->getId(), DataBlockObjectIdFirst,
                                                   DataBlockObjectIdLast);

   if (stream->writeFlag(explosion != NULL))
      stream->writeRangedU32(explosion->getId(), DataBlockObjectIdFirst,
                                                 DataBlockObjectIdLast);

   if (stream->writeFlag(waterExplosion != NULL))
      stream->writeRangedU32(waterExplosion->getId(), DataBlockObjectIdFirst,
                                                      DataBlockObjectIdLast);

   if (stream->writeFlag(splash != NULL))
      stream->writeRangedU32(splash->getId(), DataBlockObjectIdFirst,
                                              DataBlockObjectIdLast);

   if (stream->writeFlag(decal != NULL))
      stream->writeRangedU32(decal->getId(), DataBlockObjectIdFirst,
                                              DataBlockObjectIdLast);

   if (stream->writeFlag(sound != NULL))
      stream->writeRangedU32(sound->getId(), DataBlockObjectIdFirst,
                                             DataBlockObjectIdLast);

   if ( stream->writeFlag(lightDesc != NULL))
      stream->writeRangedU32(lightDesc->getId(), DataBlockObjectIdFirst,
                                                 DataBlockObjectIdLast);

   stream->write(impactForce);
   
//    stream->writeRangedU32(lifetime, 0, Projectile::MaxLivingTicks);
//    stream->writeRangedU32(armingDelay, 0, Projectile::MaxLivingTicks);
//    stream->writeRangedU32(fadeDelay, 0, Projectile::MaxLivingTicks);

   // [tom, 3/21/2007] Changing these to write all 32 bits as the previous
   // code limited these to a max value of 4095.
   stream->write(lifetime);
   stream->write(armingDelay);
   stream->write(fadeDelay);

   if(stream->writeFlag(isBallistic))
   {
      stream->write(gravityMod);
      stream->write(bounceElasticity);
      stream->write(bounceFriction);
   }

}

void ProjectileData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

   projectileShapeName = stream->readSTString();

   faceViewer = stream->readFlag();
   if(stream->readFlag())
   {
      stream->read(&scale.x);
      stream->read(&scale.y);
      stream->read(&scale.z);
   }
   else
      scale.set(1,1,1);

   if (stream->readFlag())
      particleEmitterId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      particleWaterEmitterId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      explosionId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      waterExplosionId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   
   if (stream->readFlag())
      splashId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      decalId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   
   if (stream->readFlag())
      soundId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      lightDescId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   
   // [tom, 3/21/2007] See comment in packData()
//    lifetime = stream->readRangedU32(0, Projectile::MaxLivingTicks);
//    armingDelay = stream->readRangedU32(0, Projectile::MaxLivingTicks);
//    fadeDelay = stream->readRangedU32(0, Projectile::MaxLivingTicks);

   stream->read(&impactForce);

   stream->read(&lifetime);
   stream->read(&armingDelay);
   stream->read(&fadeDelay);

   isBallistic = stream->readFlag();
   if(isBallistic)
   {
      stream->read(&gravityMod);
      stream->read(&bounceElasticity);
      stream->read(&bounceFriction);
   }
}

bool ProjectileData::setLifetime( void *obj, const char *data )
{
	S32 value = dAtoi(data);
   value = scaleValue(value);
   
   ProjectileData *object = static_cast<ProjectileData*>(obj);
   object->lifetime = value;

   return false;
}

bool ProjectileData::setArmingDelay( void *obj, const char *data )
{
	S32 value = dAtoi(data);
   value = scaleValue(value);

   ProjectileData *object = static_cast<ProjectileData*>(obj);
   object->armingDelay = value;

   return false;
}

bool ProjectileData::setFadeDelay( void *obj, const char *data )
{
	S32 value = dAtoi(data);
   value = scaleValue(value);

   ProjectileData *object = static_cast<ProjectileData*>(obj);
   object->fadeDelay = value;

   return false;
}

const char *ProjectileData::getScaledValue( void *obj, const char *data)
{

	S32 value = dAtoi(data);
   value = scaleValue(value, false);

   String stringData = String::ToString(value);
   char *strBuffer = Con::getReturnBuffer(stringData.size());
   dMemcpy( strBuffer, stringData, stringData.size() );

   return strBuffer;
}

S32 ProjectileData::scaleValue( S32 value, bool down )
{
   S32 minV = 0;
   S32 maxV = Projectile::MaxLivingTicks;
   
   // scale down to ticks before we validate
   if( down )
      value /= TickMs;
   
   if(value < minV || value > maxV)
	{
      Con::errorf("ProjectileData::scaleValue(S32 value = %d, bool down = %b) - Scaled value must be between %d and %d", value, down, minV, maxV);
		if(value < minV)
			value = minV;
		else if(value > maxV)
			value = maxV;
	}

   // scale up from ticks after we validate
   if( !down )
      value *= TickMs;

   return value;
}

//--------------------------------------------------------------------------
//--------------------------------------
//
Projectile::Projectile()
 : mPhysicsWorld( NULL ),
   mCurrPosition( 0, 0, 0 ),
   mCurrVelocity( 0, 0, 1 ),
   mSourceObjectId( -1 ),
   mSourceObjectSlot( -1 ),
   mCurrTick( 0 ),
   mParticleEmitter( NULL ),
   mParticleWaterEmitter( NULL ),
   mSound( NULL ),
   mProjectileShape( NULL ),
   mActivateThread( NULL ),
   mMaintainThread( NULL ),
   mHidden( false ),
   mFadeValue( 1.0f )
{
   // Todo: ScopeAlways?
   mNetFlags.set(Ghostable);
   mTypeMask |= ProjectileObjectType | LightObjectType;

   mLight = LightManager::createLightInfo();
   mLight->setType( LightInfo::Point );   

   mLightState.clear();
   mLightState.setLightInfo( mLight );
}

Projectile::~Projectile()
{
   SAFE_DELETE(mLight);

   delete mProjectileShape;
   mProjectileShape = NULL;
}

//--------------------------------------------------------------------------
void Projectile::initPersistFields()
{
   addGroup("Physics");
   addField("initialPosition",  TypePoint3F, Offset(mCurrPosition, Projectile));
   addField("initialVelocity", TypePoint3F, Offset(mCurrVelocity, Projectile));
   endGroup("Physics");

   addGroup("Source");
   addField("sourceObject",     TypeS32,     Offset(mSourceObjectId, Projectile));
   addField("sourceSlot",       TypeS32,     Offset(mSourceObjectSlot, Projectile));
   endGroup("Source");

   Parent::initPersistFields();
}

bool Projectile::calculateImpact(float,
                                 Point3F& pointOfImpact,
                                 float&   impactTime)
{
   Con::warnf(ConsoleLogEntry::General, "Projectile::calculateImpact: Should never be called");

   impactTime = 0;
   pointOfImpact.set(0, 0, 0);
   return false;
}


//--------------------------------------------------------------------------
F32 Projectile::getUpdatePriority(CameraScopeQuery *camInfo, U32 updateMask, S32 updateSkips)
{
   F32 ret = Parent::getUpdatePriority(camInfo, updateMask, updateSkips);
   // if the camera "owns" this object, it should have a slightly higher priority
   if(mSourceObject == camInfo->camera)
      return ret + 0.2;
   return ret;
}

bool Projectile::onAdd()
{
   if(!Parent::onAdd())
      return false;

   if (isServerObject())
   {
      ShapeBase* ptr;
      if (Sim::findObject(mSourceObjectId, ptr))
         mSourceObject = ptr;
      else
      {
         if (mSourceObjectId != -1)
            Con::errorf(ConsoleLogEntry::General, "Projectile::onAdd: mSourceObjectId is invalid");
         mSourceObject = NULL;
      }

      // If we're on the server, we need to inherit some of our parent's velocity
      //
      mCurrTick = 0;
   }
   else
   {
      if (bool(mDataBlock->projectileShape))
      {
         mProjectileShape = new TSShapeInstance(mDataBlock->projectileShape, isClientObject());

         if (mDataBlock->activateSeq != -1)
         {
            mActivateThread = mProjectileShape->addThread();
            mProjectileShape->setTimeScale(mActivateThread, 1);
            mProjectileShape->setSequence(mActivateThread, mDataBlock->activateSeq, 0);
         }
      }
      if (mDataBlock->particleEmitter != NULL)
      {
         ParticleEmitter* pEmitter = new ParticleEmitter;
         pEmitter->onNewDataBlock(mDataBlock->particleEmitter);
         if (pEmitter->registerObject() == false)
         {
            Con::warnf(ConsoleLogEntry::General, "Could not register particle emitter for particle of class: %s", mDataBlock->getName());
            delete pEmitter;
            pEmitter = NULL;
         }
         mParticleEmitter = pEmitter;
      }

      if (mDataBlock->particleWaterEmitter != NULL)
      {
         ParticleEmitter* pEmitter = new ParticleEmitter;
         pEmitter->onNewDataBlock(mDataBlock->particleWaterEmitter);
         if (pEmitter->registerObject() == false)
         {
            Con::warnf(ConsoleLogEntry::General, "Could not register particle emitter for particle of class: %s", mDataBlock->getName());
            delete pEmitter;
            pEmitter = NULL;
         }
         mParticleWaterEmitter = pEmitter;
      }
   }
   if (mSourceObject.isValid())
      processAfter(mSourceObject);

   // Setup our bounding box
   if (bool(mDataBlock->projectileShape) == true)
      mObjBox = mDataBlock->projectileShape->bounds;
   else
      mObjBox = Box3F(Point3F(0, 0, 0), Point3F(0, 0, 0));
   resetWorldBox();
   addToScene();

   if ( gPhysicsPlugin )
      mPhysicsWorld = gPhysicsPlugin->getWorld( isServerObject() ? "Server" : "Client" );

   return true;
}


void Projectile::onRemove()
{
   if( !mParticleEmitter.isNull() )
   {
      mParticleEmitter->deleteWhenEmpty();
      mParticleEmitter = NULL;
   }

   if( !mParticleWaterEmitter.isNull() )
   {
      mParticleWaterEmitter->deleteWhenEmpty();
      mParticleWaterEmitter = NULL;
   }

   SFX_DELETE( mSound );

   removeFromScene();
   Parent::onRemove();
}


bool Projectile::onNewDataBlock(GameBaseData* dptr)
{
   mDataBlock = dynamic_cast<ProjectileData*>(dptr);
   if (!mDataBlock || !Parent::onNewDataBlock(dptr))
      return false;

   if ( isGhost() )
   {
      // Create the sound ahead of time.  This reduces runtime
      // costs and makes the system easier to understand.

      SFX_DELETE( mSound );

      if ( mDataBlock->sound )
         mSound = SFX->createSource( mDataBlock->sound );
   }

   return true;
}

void Projectile::submitLights( LightManager *lm, bool staticLighting )
{
   if ( staticLighting || mHidden || !mDataBlock->lightDesc )
      return;
   
   mDataBlock->lightDesc->submitLight( &mLightState, getRenderTransform(), lm, this );   
}

bool Projectile::pointInWater(const Point3F &point)
{   
   // This is pretty much a hack so we can use the existing ContainerQueryInfo
   // and findObject router.
   
   // We only care if we intersect with water at all 
   // so build a box at the point that has only 1 z extent.
   // And test if water coverage is anything other than zero.

   Box3F boundsBox( point, point );
   boundsBox.maxExtents.z += 1.0f;

   ContainerQueryInfo info;
   info.box = boundsBox;
   info.mass = 0.0f;
   
   // Find and retreive physics info from intersecting WaterObject(s)
   if(mContainer != NULL)
   {
      mContainer->findObjects( boundsBox, WaterObjectType, findRouter, &info );
   }
   else
   {
      // Handle special case where the projectile has exploded prior to having
      // called onAdd() on the client.  This occurs when the projectile on the
      // server is created and then explodes in the same network update tick.
      // On the client end in NetConnection::ghostReadPacket() the ghost is
      // created and then Projectile::unpackUpdate() is called prior to the
      // projectile being registered.  Within unpackUpdate() the explosion
      // is triggered, but without being registered onAdd() isn't called and
      // the container is not set.  As all we're doing is checking if the
      // given explosion point is within water, we should be able to use the
      // global container here.  We could likely always get away with this,
      // but using the actual defined container when possible is the right
      // thing to do.  DAW
      AssertFatal(isClientObject(), "Server projectile has not been properly added");
      gClientContainer.findObjects( boundsBox, WaterObjectType, findRouter, &info );
   }

   return ( info.waterCoverage > 0.0f );
}

//----------------------------------------------------------------------------

void Projectile::emitParticles(const Point3F& from, const Point3F& to, const Point3F& vel, const U32 ms)
{
   if( mHidden )
      return;

   Point3F axis = -vel;

   if( axis.isZero() )
      axis.set( 0.0, 0.0, 1.0 );
   else
      axis.normalize();

   bool fromWater = pointInWater(from);
   bool toWater   = pointInWater(to);

   if (!fromWater && !toWater && bool(mParticleEmitter))                                        // not in water
      mParticleEmitter->emitParticles(from, to, axis, vel, ms);
   else if (fromWater && toWater && bool(mParticleWaterEmitter))                                // in water
      mParticleWaterEmitter->emitParticles(from, to, axis, vel, ms);
   else if (!fromWater && toWater && mDataBlock->splash)     // entering water
   {
      // cast the ray to get the surface point of the water
      RayInfo rInfo;
      if (gClientContainer.castRay(from, to, WaterObjectType, &rInfo))
      {
         MatrixF trans = getTransform();
         trans.setPosition(rInfo.point);

         Splash *splash = new Splash();
         splash->onNewDataBlock(mDataBlock->splash);
         splash->setTransform(trans);
         splash->setInitialState(trans.getPosition(), Point3F(0.0, 0.0, 1.0));
         if (!splash->registerObject())
         {
            delete splash;
            splash = NULL;
         }

         // create an emitter for the particles out of water and the particles in water
         if (mParticleEmitter)
            mParticleEmitter->emitParticles(from, rInfo.point, axis, vel, ms);

         if (mParticleWaterEmitter)
            mParticleWaterEmitter->emitParticles(rInfo.point, to, axis, vel, ms);
      }
   }
   else if (fromWater && !toWater && mDataBlock->splash)     // leaving water
   {
      // cast the ray in the opposite direction since that point is out of the water, otherwise
      //  we hit water immediately and wont get the appropriate surface point
      RayInfo rInfo;
      if (gClientContainer.castRay(to, from, WaterObjectType, &rInfo))
      {
         MatrixF trans = getTransform();
         trans.setPosition(rInfo.point);

         Splash *splash = new Splash();
         splash->onNewDataBlock(mDataBlock->splash);
         splash->setTransform(trans);
         splash->setInitialState(trans.getPosition(), Point3F(0.0, 0.0, 1.0));
         if (!splash->registerObject())
         {
            delete splash;
            splash = NULL;
         }

         // create an emitter for the particles out of water and the particles in water
         if (mParticleEmitter)
            mParticleEmitter->emitParticles(rInfo.point, to, axis, vel, ms);

         if (mParticleWaterEmitter)
            mParticleWaterEmitter->emitParticles(from, rInfo.point, axis, vel, ms);
      }
   }
}


//----------------------------------------------------------------------------

class ObjectDeleteEvent : public SimEvent
{
public:
   void process(SimObject *object)
   {
      object->deleteObject();
   }
};

void Projectile::explode(const Point3F& p, const Point3F& n, const U32 collideType)
{
   // Make sure we don't explode twice...
   if (mHidden == true)
      return;

   mHidden = true;
   if (isServerObject()) {
      // Do what the server needs to do, damage the surrounding objects, etc.
      mExplosionPosition = p;
      mExplosionNormal = n;
      mCollideHitType  = collideType;

      char buffer[128];
      dSprintf(buffer, sizeof(buffer),  "%g %g %g", mExplosionPosition.x,
                                                    mExplosionPosition.y,
                                                    mExplosionPosition.z);
      Con::executef(mDataBlock, "onExplode", scriptThis(), buffer, Con::getFloatArg(mFadeValue));

      setMaskBits(ExplosionMask);
      safeDeleteObject();
   } 
   else 
   {
      // Client just plays the explosion at the right place...
      //       
      Explosion* pExplosion = NULL;

      if (mDataBlock->waterExplosion && pointInWater(p))
      {
         pExplosion = new Explosion;
         pExplosion->onNewDataBlock(mDataBlock->waterExplosion);
      }
      else
      if (mDataBlock->explosion)
      {
         pExplosion = new Explosion;
         pExplosion->onNewDataBlock(mDataBlock->explosion);
      }

      if( pExplosion )
      {
         MatrixF xform(true);
         xform.setPosition(p);
         pExplosion->setTransform(xform);
         pExplosion->setInitialState(p, n);
         pExplosion->setCollideType( collideType );
         if (pExplosion->registerObject() == false)
         {
            Con::errorf(ConsoleLogEntry::General, "Projectile(%s)::explode: couldn't register explosion",
                        mDataBlock->getName() );
            delete pExplosion;
            pExplosion = NULL;
         }
      }

      // Client (impact) decal.
      if ( mDataBlock->decal )     
         gDecalManager->addDecal( p, n, 0.0f, mDataBlock->decal );

      // Client object
      updateSound();
   }

   /*
   // Client and Server both should apply forces to PhysicsWorld objects
   // within the explosion. 
   if ( false && mPhysicsWorld )
   {
      F32 force = 200.0f;
      mPhysicsWorld->explosion( p, 15.0f, force );
   }
   */
}

void Projectile::updateSound()
{
   if (!mDataBlock->sound)
      return;

   if ( mHidden && mSound )
      mSound->stop();

   else if ( !mHidden && mSound )
   {
      if ( !mSound->isPlaying() )
         mSound->play();

      mSound->setVelocity( getVelocity() );
      mSound->setTransform( getRenderTransform() );
   }
}

Point3F Projectile::getVelocity() const
{
   return mCurrVelocity;
}

void Projectile::processTick(const Move* move)
{
   Parent::processTick(move);

   mCurrTick++;
   if(mSourceObject && mCurrTick > SourceIdTimeoutTicks)
   {
      mSourceObject = 0;
      mSourceObjectId = 0;
   }

   // See if we can get out of here the easy way ...
   if (isServerObject() && mCurrTick >= mDataBlock->lifetime)
   {
      deleteObject();
      return;
   }
   else if (mHidden == true)
      return;

   // ... otherwise, we have to do some simulation work.
   RayInfo rInfo;
   Point3F oldPosition;
   Point3F newPosition;

   oldPosition = mCurrPosition;
   if(mDataBlock->isBallistic)
      mCurrVelocity.z -= 9.81 * mDataBlock->gravityMod * (F32(TickMs) / 1000.0f);

   newPosition = oldPosition + mCurrVelocity * (F32(TickMs) / 1000.0f);

   // disable the source objects collision reponse while we determine
   // if the projectile is capable of moving from the old position
   // to the new position, otherwise we'll hit ourself
   if (mSourceObject.isValid())
      mSourceObject->disableCollision();
   disableCollision();

   // Determine if the projectile is going to hit any object between the previous
   // position and the new position. This code is executed both on the server
   // and on the client (for prediction purposes). It is possible that the server
   // will have registered a collision while the client prediction has not. If this
   // happens the client will be corrected in the next packet update.

   // Raycast the abstract PhysicsWorld if a PhysicsPlugin exists.
   bool hit = false;

   if ( gPhysicsPlugin )
   {
      // TODO: hacked for single player... fix me!
      PhysicsWorld *world = gPhysicsPlugin->getWorld( "Server"/*isServerObject() ? "Server" : "Client"*/ );         
      if ( world )
         hit = world->castRay( oldPosition, newPosition, &rInfo, Point3F( newPosition - oldPosition) * mDataBlock->impactForce );            

   }
   else 
      hit = getContainer()->castRay(oldPosition, newPosition, csmDynamicCollisionMask | csmStaticCollisionMask, &rInfo);

   if ( hit )
   {
      // make sure the client knows to bounce
      if(isServerObject() && (rInfo.object->getType() & csmStaticCollisionMask) == 0)
         setMaskBits(BounceMask);

      // Next order of business: do we explode on this hit?
      if(mCurrTick > mDataBlock->armingDelay)
      {
         MatrixF xform(true);
         xform.setColumn(3, rInfo.point);
         setTransform(xform);
         mCurrPosition    = rInfo.point;
         mCurrVelocity    = Point3F(0, 0, 0);

         // Get the object type before the onCollision call, in case
         // the object is destroyed.
         U32 objectType = rInfo.object->getType();

         // re-enable the collision response on the source object since
         // we need to process the onCollision and explode calls
         if(mSourceObject)
            mSourceObject->enableCollision();

         // Ok, here is how this works:
         // onCollision is called to notify the server scripts that a collision has occurred, then
         // a call to explode is made to start the explosion process. The call to explode is made
         // twice, once on the server and once on the client.
         // The server process is responsible for two things:
         //    1) setting the ExplosionMask network bit to guarantee that the client calls explode
         //    2) initiate the explosion process on the server scripts
         // The client process is responsible for only one thing:
         //    1) drawing the appropriate explosion

         // It is possible that during the processTick the server may have decided that a hit
         // has occurred while the client prediction has decided that a hit has not occurred.
         // In this particular scenario the client will have failed to call onCollision and
         // explode during the processTick. However, the explode function will be called
         // during the next packet update, due to the ExplosionMask network bit being set.
         // onCollision will remain uncalled on the client however, therefore no client
         // specific code should be placed inside the function!
         onCollision(rInfo.point, rInfo.normal, rInfo.object);
         explode(rInfo.point, rInfo.normal, objectType );

         // break out of the collision check, since we've exploded
         // we don't want to mess with the position and velocity
      }
      else
      {
         if(mDataBlock->isBallistic)
         {
            // Otherwise, this represents a bounce.  First, reflect our velocity
            //  around the normal...
            Point3F bounceVel = mCurrVelocity - rInfo.normal * (mDot( mCurrVelocity, rInfo.normal ) * 2.0);;
            mCurrVelocity = bounceVel;

            // Add in surface friction...
            Point3F tangent = bounceVel - rInfo.normal * mDot(bounceVel, rInfo.normal);
            mCurrVelocity  -= tangent * mDataBlock->bounceFriction;

            // Now, take elasticity into account for modulating the speed of the grenade
            mCurrVelocity *= mDataBlock->bounceElasticity;

            // Set the new position to the impact and the bounce
            // will apply on the next frame.
            //F32 timeLeft = 1.0f - rInfo.t;
            newPosition = oldPosition = rInfo.point + rInfo.normal * 0.05f;
         }
      }
   }

   // re-enable the collision response on the source object now
   // that we are done processing the ballistic movement
   if (mSourceObject.isValid())
      mSourceObject->enableCollision();
   enableCollision();

   if(isClientObject())
   {
      emitParticles(mCurrPosition, newPosition, mCurrVelocity, TickMs);
      updateSound();
   }

   mCurrDeltaBase = newPosition;
   mCurrBackDelta = mCurrPosition - newPosition;
   mCurrPosition = newPosition;

   MatrixF xform(true);
   xform.setColumn(3, mCurrPosition);
   setTransform(xform);
}


void Projectile::advanceTime(F32 dt)
{
   Parent::advanceTime(dt);

   if (mHidden == true || dt == 0.0)
      return;

   if (mActivateThread &&
         mProjectileShape->getDuration(mActivateThread) > mProjectileShape->getTime(mActivateThread) + dt)
   {
      mProjectileShape->advanceTime(dt, mActivateThread);
   }
   else
   {

      if (mMaintainThread)
      {
         mProjectileShape->advanceTime(dt, mMaintainThread);
      }
      else if (mActivateThread && mDataBlock->maintainSeq != -1)
      {
         mMaintainThread = mProjectileShape->addThread();
         mProjectileShape->setTimeScale(mMaintainThread, 1);
         mProjectileShape->setSequence(mMaintainThread, mDataBlock->maintainSeq, 0);
         mProjectileShape->advanceTime(dt, mMaintainThread);
      }
   }
}

void Projectile::interpolateTick(F32 delta)
{
   Parent::interpolateTick(delta);

   if(mHidden == true)
      return;

   Point3F interpPos = mCurrDeltaBase + mCurrBackDelta * delta;
   Point3F dir = mCurrVelocity;
   if(dir.isZero())
      dir.set(0,0,1);
   else
      dir.normalize();

   MatrixF xform(true);
	xform = MathUtils::createOrientFromDir(dir);
   xform.setPosition(interpPos);
   setRenderTransform(xform);

   // fade out the projectile image
   S32 time = (S32)(mCurrTick - delta);
   if(time > mDataBlock->fadeDelay)
   {
      F32 fade = F32(time - mDataBlock->fadeDelay);
      mFadeValue = 1.0 - (fade / F32(mDataBlock->lifetime));
   }
   else
      mFadeValue = 1.0;

   updateSound();
}



//--------------------------------------------------------------------------
void Projectile::onCollision(const Point3F& hitPosition, const Point3F& hitNormal, SceneObject* hitObject)
{
   // No client specific code should be placed or branched from this function
   if(isClientObject())
      return;

   if (hitObject != NULL && isServerObject())
   {
      char *posArg = Con::getArgBuffer(64);
      char *normalArg = Con::getArgBuffer(64);

      dSprintf(posArg, 64, "%g %g %g", hitPosition.x, hitPosition.y, hitPosition.z);
      dSprintf(normalArg, 64, "%g %g %g", hitNormal.x, hitNormal.y, hitNormal.z);

      Con::executef(mDataBlock, "onCollision",
         scriptThis(),
         Con::getIntArg(hitObject->getId()),
         Con::getFloatArg(mFadeValue),
         posArg,
         normalArg);
   }
}

//--------------------------------------------------------------------------
U32 Projectile::packUpdate(NetConnection* con, U32 mask, BitStream* stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   // Initial update
   if (stream->writeFlag(mask & GameBase::InitialUpdateMask))
   {
      Point3F pos;
      getTransform().getColumn(3, &pos);
      stream->writeCompressedPoint(pos);
      F32 len = mCurrVelocity.len();
      if(stream->writeFlag(len > 0.02))
      {
         Point3F outVel = mCurrVelocity;
         outVel *= 1 / len;
         stream->writeNormalVector(outVel, 10);
         len *= 32.0; // 5 bits for fraction
         if(len > 8191)
            len = 8191;
         stream->writeInt((S32)len, 13);
      }

      stream->writeRangedU32(mCurrTick, 0, MaxLivingTicks);
      if (mSourceObject.isValid())
      {
         // Potentially have to write this to the client, let's make sure it has a
         //  ghost on the other side...
         S32 ghostIndex = con->getGhostIndex(mSourceObject);
         if (stream->writeFlag(ghostIndex != -1))
         {
            stream->writeRangedU32(U32(ghostIndex), 0, NetConnection::MaxGhostCount);
            stream->writeRangedU32(U32(mSourceObjectSlot),
                                   0, ShapeBase::MaxMountedImages - 1);
         }
         else // havn't recieved the ghost for the source object yet, try again later
            retMask |= GameBase::InitialUpdateMask;
      }
      else
         stream->writeFlag(false);
   }

   // explosion update
   if (stream->writeFlag((mask & ExplosionMask) && mHidden))
   {
      mathWrite(*stream, mExplosionPosition);
      mathWrite(*stream, mExplosionNormal);
      stream->write(mCollideHitType);
   }

   // bounce update
   if (stream->writeFlag(mask & BounceMask))
   {
      // Bounce against dynamic object
      mathWrite(*stream, mCurrPosition);
      mathWrite(*stream, mCurrVelocity);
   }

   return retMask;
}

void Projectile::unpackUpdate(NetConnection* con, BitStream* stream)
{
   Parent::unpackUpdate(con, stream);

   // initial update
   if (stream->readFlag())
   {
      Point3F pos;
      stream->readCompressedPoint(&pos);
      if(stream->readFlag())
      {
         stream->readNormalVector(&mCurrVelocity, 10);
         mCurrVelocity *= stream->readInt(13) / 32.0f;
      }
      else
         mCurrVelocity.set(0, 0, 0);

      mCurrDeltaBase = pos;
      mCurrBackDelta = mCurrPosition - pos;
      mCurrPosition  = pos;
      setPosition(mCurrPosition);

      mCurrTick = stream->readRangedU32(0, MaxLivingTicks);
      if (stream->readFlag())
      {
         mSourceObjectId   = stream->readRangedU32(0, NetConnection::MaxGhostCount);
         mSourceObjectSlot = stream->readRangedU32(0, ShapeBase::MaxMountedImages - 1);

         NetObject* pObject = con->resolveGhost(mSourceObjectId);
         if (pObject != NULL)
            mSourceObject = dynamic_cast<ShapeBase*>(pObject);
      }
      else
      {
         mSourceObjectId   = -1;
         mSourceObjectSlot = -1;
         mSourceObject     = NULL;
      }
   }

   // explosion update
   if (stream->readFlag())
   {
      Point3F explodePoint;
      Point3F explodeNormal;
      mathRead(*stream, &explodePoint);
      mathRead(*stream, &explodeNormal);
      stream->read(&mCollideHitType);

      // start the explosion visuals
      explode(explodePoint, explodeNormal, mCollideHitType);
   }

   // bounce update
   if (stream->readFlag())
   {
      mathRead(*stream, &mCurrPosition);
      mathRead(*stream, &mCurrVelocity);
   }
}

//--------------------------------------------------------------------------
bool Projectile::prepRenderImage(SceneState* state, const U32 stateKey,
                                       const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
   if (isLastState(state, stateKey))
      return false;
   setLastState(state, stateKey);

   if (mHidden == true || mFadeValue <= (1.0/255.0))
      return false;

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if (state->isObjectRendered(this))
   {
      if ( mDataBlock->lightDesc )
      {
         mDataBlock->lightDesc->prepRender( state, &mLightState, getRenderTransform() );
      }

      /*
      if ( mFlareData )
      {
         mFlareState.fullBrightness = mDataBlock->lightDesc->mBrightness;
         mFlareState.scale = mFlareScale;
         mFlareState.lightInfo = mLight;
         mFlareState.lightMat = getTransform();

         mFlareData->prepRender( state, &mFlareState );
      }
      */

      prepBatchRender( state );
   }

   return false;
}

void Projectile::prepBatchRender( SceneState *state )
{
   GFXTransformSaver saver;

   // Set up our TS render state.
   TSRenderState rdata;
   rdata.setSceneState( state );

   MatrixF mat = getRenderTransform();
   mat.scale( mObjScale );
   mat.scale( mDataBlock->scale );
   GFX->setWorldMatrix( mat );

   if(mProjectileShape)
   {
      AssertFatal(mProjectileShape != NULL,
                  "Projectile::renderObject: Error, projectile shape should always be present in renderObject");
      mProjectileShape->setDetailFromPosAndScale( state, mat.getPosition(), mObjScale );
      mProjectileShape->animate();

      mProjectileShape->render( rdata );
   }
}
