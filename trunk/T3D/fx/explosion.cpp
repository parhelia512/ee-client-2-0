//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/fx/explosion.h"

#include "core/resourceManager.h"
#include "console/consoleTypes.h"
#include "console/typeValidators.h"
#include "sfx/sfxSystem.h"
#include "sfx/sfxProfile.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "lighting/lightInfo.h"
#include "lighting/lightManager.h"
#include "core/stream/bitStream.h"
#include "sim/netConnection.h"
#include "ts/tsShape.h"
#include "ts/tsShapeInstance.h"
#include "math/mRandom.h"
#include "math/mathIO.h"
#include "math/mathUtils.h"
#include "T3D/debris.h"
#include "T3D/gameConnection.h"
#include "T3D/fx/particleEmitter.h"
#include "T3D/fx/cameraFXMgr.h"
#include "T3D/debris.h"
#include "T3D/shapeBase.h"
#include "T3D/gameProcess.h"
#include "renderInstance/renderPassManager.h"

IMPLEMENT_CONOBJECT(Explosion);

#define MaxLightRadius 20

MRandomLCG sgRandom(0xdeadbeef);

ConsoleFunction( calcExplosionCoverage, F32, 4, 4, "(Point3F source, SceneObject originator, bitset coverageMask)")
{
   Point3F pos, center;

   dSscanf(argv[1], "%f %f %f", &pos.x, &pos.y, &pos.z);
   S32 id      = dAtoi(argv[2]);
   U32 covMask = (U32)dAtoi(argv[3]);

   SceneObject* sceneObject = NULL;
   if (Sim::findObject(id, sceneObject) == false) {
      Con::warnf(ConsoleLogEntry::General, "calcExplosionCoverage: couldn't find object: %s", argv[2]);
      return 1.0f;
   }
   if (sceneObject->isClientObject() || sceneObject->getContainer() == NULL) {
      Con::warnf(ConsoleLogEntry::General, "calcExplosionCoverage: object is on the client, or not in the container system");
      return 1.0f;
   }

   sceneObject->getObjBox().getCenter(&center);
   center.convolve(sceneObject->getScale());
   sceneObject->getTransform().mulP(center);

   RayInfo rayInfo;
   sceneObject->disableCollision();
   if (sceneObject->getContainer()->castRay(pos, center, covMask, &rayInfo) == true) {
      // Try casting up and then out
      if (sceneObject->getContainer()->castRay(pos, pos + Point3F(0.0f, 0.0f, 1.0f), covMask, &rayInfo) == false)
      {
         if (sceneObject->getContainer()->castRay(pos + Point3F(0.0f, 0.0f, 1.0f), center, covMask, &rayInfo) == false)
         {
            sceneObject->enableCollision();
            return 1.0f;
         }
      }

      sceneObject->enableCollision();
      return 0.0f;
   } else {
      sceneObject->enableCollision();
      return 1.0f;
   }
}

//----------------------------------------------------------------------------
//
IMPLEMENT_CO_DATABLOCK_V1(ExplosionData);

ExplosionData::ExplosionData()
{
   dtsFileName  = NULL;
   particleDensity = 10;
   particleRadius = 1.0f;

   faceViewer   = false;

   soundProfile      = NULL;
   particleEmitter   = NULL;
   soundProfileId    = 0;
   particleEmitterId = 0;

   explosionScale.set(1.0f, 1.0f, 1.0f);
   playSpeed = 1.0f;

   dMemset( emitterList, 0, sizeof( emitterList ) );
   dMemset( emitterIDList, 0, sizeof( emitterIDList ) );
   dMemset( debrisList, 0, sizeof( debrisList ) );
   dMemset( debrisIDList, 0, sizeof( debrisIDList ) );

   debrisThetaMin = 0.0f;
   debrisThetaMax = 90.0f;
   debrisPhiMin = 0.0f;
   debrisPhiMax = 360.0f;
   debrisNum = 1;
   debrisNumVariance = 0;
   debrisVelocity = 2.0f;
   debrisVelocityVariance = 0.0f;

   dMemset( explosionList, 0, sizeof( explosionList ) );
   dMemset( explosionIDList, 0, sizeof( explosionIDList ) );

   delayMS = 0;
   delayVariance = 0;
   lifetimeMS = 1000;
   lifetimeVariance = 0;
   offset = 0.0f;

   shockwave = NULL;
   shockwaveID = 0;
   shockwaveOnTerrain = false;

   shakeCamera = false;
   camShakeFreq.set( 10.0f, 10.0f, 10.0f );
   camShakeAmp.set( 1.0f, 1.0f, 1.0f );
   camShakeDuration = 1.5f;
   camShakeRadius = 10.0f;
   camShakeFalloff = 10.0f;

   for( U32 i=0; i<EC_NUM_TIME_KEYS; i++ )
   {
      times[i] = 1.0f;
   }
   times[0] = 0.0f;

   for( U32 j=0; j<EC_NUM_TIME_KEYS; j++ )
   {
      sizes[j].set( 1.0f, 1.0f, 1.0f );
   }

   //
   lightStartRadius = lightEndRadius = 0.0f;
   lightStartColor.set(1.0f,1.0f,1.0f);
   lightEndColor.set(1.0f,1.0f,1.0f);
   lightStartBrightness = 1.0f;
   lightEndBrightness = 1.0f;
   lightNormalOffset = 0.1f;
}

IMPLEMENT_CONSOLETYPE(ExplosionData)
IMPLEMENT_SETDATATYPE(ExplosionData)
IMPLEMENT_GETDATATYPE(ExplosionData)

void ExplosionData::initPersistFields()
{
   addField("explosionShape",  TypeFilename,                Offset(dtsFileName,     ExplosionData));
   addField("soundProfile",    TypeSFXProfilePtr,         Offset(soundProfile,    ExplosionData));
   addField("faceViewer",      TypeBool,                    Offset(faceViewer,      ExplosionData));
   addField("particleEmitter", TypeParticleEmitterDataPtr,  Offset(particleEmitter, ExplosionData));
   addField("particleDensity", TypeS32,                     Offset(particleDensity, ExplosionData));
   addField("particleRadius",  TypeF32,                     Offset(particleRadius,  ExplosionData));
   addField("explosionScale",  TypePoint3F,                 Offset(explosionScale,  ExplosionData));
   addField("playSpeed",       TypeF32,                     Offset(playSpeed,       ExplosionData));

   addField("emitter",         TypeParticleEmitterDataPtr,  Offset(emitterList,     ExplosionData), EC_NUM_EMITTERS);
   addField("debris",          TypeDebrisDataPtr,           Offset(debrisList,      ExplosionData), EC_NUM_DEBRIS_TYPES);
//   addField("shockwave",       TypeShockwaveDataPtr,        Offset(shockwave,       ExplosionData));
//   addField("shockwaveOnTerrain", TypeBool,                 Offset(shockwaveOnTerrain, ExplosionData));

   addField("debrisThetaMin",    TypeF32, Offset(debrisThetaMin,     ExplosionData));
   addField("debrisThetaMax",    TypeF32, Offset(debrisThetaMax,     ExplosionData));
   addField("debrisPhiMin",      TypeF32, Offset(debrisPhiMin,       ExplosionData));
   addField("debrisPhiMax",      TypeF32, Offset(debrisPhiMax,       ExplosionData));
   addField("debrisNum",         TypeS32, Offset(debrisNum,          ExplosionData));
   addField("debrisNumVariance", TypeS32, Offset(debrisNumVariance,  ExplosionData));
   addField("debrisVelocity",    TypeF32, Offset(debrisVelocity,     ExplosionData));
   addField("debrisVelocityVariance",    TypeF32, Offset(debrisVelocityVariance,     ExplosionData));

   addField("subExplosion",      TypeExplosionDataPtr, Offset(explosionList, ExplosionData), EC_MAX_SUB_EXPLOSIONS );

   addField("delayMS",           TypeS32, Offset(delayMS,            ExplosionData));
   addField("delayVariance",     TypeS32, Offset(delayVariance,      ExplosionData));
   addField("lifetimeMS",        TypeS32, Offset(lifetimeMS,         ExplosionData));
   addField("lifetimeVariance",  TypeS32, Offset(lifetimeVariance,   ExplosionData));
   addField("offset",            TypeF32, Offset(offset,             ExplosionData));

   addField("times",             TypeF32,       Offset(times,        ExplosionData), EC_NUM_TIME_KEYS );
   addField("sizes",             TypePoint3F,   Offset(sizes,       ExplosionData), EC_NUM_TIME_KEYS );

   addField("shakeCamera",       TypeBool,      Offset(shakeCamera,        ExplosionData));
   addField("camShakeFreq",      TypePoint3F,   Offset(camShakeFreq,       ExplosionData));
   addField("camShakeAmp",       TypePoint3F,   Offset(camShakeAmp,        ExplosionData));
   addField("camShakeDuration",  TypeF32,       Offset(camShakeDuration,   ExplosionData));
   addField("camShakeRadius",    TypeF32,       Offset(camShakeRadius,     ExplosionData));
   addField("camShakeFalloff",   TypeF32,       Offset(camShakeFalloff,    ExplosionData));

   static FRangeValidator lightStartRadiusValidator( 0, MaxLightRadius );
   static FRangeValidator lightEndRadiusValidator( 0, MaxLightRadius );

   addNamedFieldV(lightStartRadius, TypeF32, ExplosionData, &lightStartRadiusValidator );
   addNamedFieldV(lightEndRadius, TypeF32, ExplosionData, &lightEndRadiusValidator );
   addNamedField(lightStartColor, TypeColorF, ExplosionData);
   addNamedField(lightEndColor, TypeColorF, ExplosionData);
   addNamedFieldV(lightStartBrightness, TypeF32, ExplosionData, &lightStartRadiusValidator );
   addNamedFieldV(lightEndBrightness, TypeF32, ExplosionData, &lightEndRadiusValidator );
   addNamedField(lightNormalOffset, TypeF32, ExplosionData );
   
   Parent::initPersistFields();
}

bool ExplosionData::onAdd()
{
   if (Parent::onAdd() == false)
      return false;

   if (!soundProfile && soundProfileId != 0)
      if (Sim::findObject(soundProfileId, soundProfile) == false)
         Con::errorf(ConsoleLogEntry::General, "Error, unable to load sound profile for explosion datablock");
   if (!particleEmitter && particleEmitterId != 0)
      if (Sim::findObject(particleEmitterId, particleEmitter) == false)
         Con::errorf(ConsoleLogEntry::General, "Error, unable to load particle emitter for explosion datablock");

   if (explosionScale.x < 0.01f || explosionScale.y < 0.01f || explosionScale.z < 0.01f)
   {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s)::onAdd: ExplosionScale components must be >= 0.01", getName());
      explosionScale.x = explosionScale.x < 0.01f ? 0.01f : explosionScale.x;
      explosionScale.y = explosionScale.y < 0.01f ? 0.01f : explosionScale.y;
      explosionScale.z = explosionScale.z < 0.01f ? 0.01f : explosionScale.z;
   }

   if (debrisThetaMin < 0.0f)
   {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisThetaMin < 0.0", getName());
      debrisThetaMin = 0.0f;
   }
   if (debrisThetaMax > 180.0f)
   {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisThetaMax > 180.0", getName());
      debrisThetaMax = 180.0f;
   }
   if (debrisThetaMin > debrisThetaMax) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisThetaMin > debrisThetaMax", getName());
      debrisThetaMin = debrisThetaMax;
   }
   if (debrisPhiMin < 0.0f)
   {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisPhiMin < 0.0", getName());
      debrisPhiMin = 0.0f;
   }
   if (debrisPhiMax > 360.0f)
   {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisPhiMax > 360.0", getName());
      debrisPhiMax = 360.0f;
   }
   if (debrisPhiMin > debrisPhiMax) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisPhiMin > debrisPhiMax", getName());
      debrisPhiMin = debrisPhiMax;
   }
   if (debrisNum > 1000) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisNum > 1000", getName());
      debrisNum = 1000;
   }
   if (debrisNumVariance > 1000) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisNumVariance > 1000", getName());
      debrisNumVariance = 1000;
   }
   if (debrisVelocity < 0.1f)
   {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisVelocity < 0.1", getName());
      debrisVelocity = 0.1f;
   }
   if (debrisVelocityVariance > 1000) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) debrisVelocityVariance > 1000", getName());
      debrisVelocityVariance = 1000;
   }
   if (playSpeed < 0.05f)
   {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) playSpeed < 0.05", getName());
      playSpeed = 0.05f;
   }
   if (lifetimeMS < 1) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) lifetimeMS < 1", getName());
      lifetimeMS = 1;
   }
   if (lifetimeVariance > lifetimeMS) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) lifetimeVariance > lifetimeMS", getName());
      lifetimeVariance = lifetimeMS;
   }
   if (delayMS < 0) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) delayMS < 0", getName());
      delayMS = 0;
   }
   if (delayVariance > delayMS) {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) delayVariance > delayMS", getName());
      delayVariance = delayMS;
   }
   if (offset < 0.0f)
   {
      Con::warnf(ConsoleLogEntry::General, "ExplosionData(%s) offset < 0.0", getName());
      offset = 0.0f;
   }

   S32 i;
   for( i=0; i<EC_NUM_DEBRIS_TYPES; i++ )
   {
      if( !debrisList[i] && debrisIDList[i] != 0 )
      {
         if( !Sim::findObject( debrisIDList[i], debrisList[i] ) )
         {
            Con::errorf( ConsoleLogEntry::General, "ExplosionData::onAdd: Invalid packet, bad datablockId(debris): 0x%x", debrisIDList[i] );
         }
      }
   }

   for( i=0; i<EC_NUM_EMITTERS; i++ )
   {
      if( !emitterList[i] && emitterIDList[i] != 0 )
      {
         if( Sim::findObject( emitterIDList[i], emitterList[i] ) == false)
         {
            Con::errorf( ConsoleLogEntry::General, "ExplosionData::onAdd: Invalid packet, bad datablockId(particle emitter): 0x%x", emitterIDList[i] );
         }
      }
   }

   for( S32 k=0; k<EC_MAX_SUB_EXPLOSIONS; k++ )
   {
      if( !explosionList[k] && explosionIDList[k] != 0 )
      {
         if( Sim::findObject( explosionIDList[k], explosionList[k] ) == false)
         {
            Con::errorf( ConsoleLogEntry::General, "ExplosionData::onAdd: Invalid packet, bad datablockId(explosion): 0x%x", explosionIDList[k] );
         }
      }
   }

   return true;
}

void ExplosionData::packData(BitStream* stream)
{
   Parent::packData(stream);

   stream->writeString(dtsFileName);

   if (stream->writeFlag(soundProfile != NULL))
      stream->writeRangedU32(soundProfile->getId(), DataBlockObjectIdFirst,
                                                    DataBlockObjectIdLast);
   if (stream->writeFlag(particleEmitter))
      stream->writeRangedU32(particleEmitter->getId(),DataBlockObjectIdFirst,DataBlockObjectIdLast);

   stream->writeInt(particleDensity, 14);
   stream->write(particleRadius);
   stream->writeFlag(faceViewer);
   if(stream->writeFlag(explosionScale.x != 1 || explosionScale.y != 1 || explosionScale.z != 1))
   {
      stream->writeInt((S32)(explosionScale.x * 100), 16);
      stream->writeInt((S32)(explosionScale.y * 100), 16);
      stream->writeInt((S32)(explosionScale.z * 100), 16);
   }
   stream->writeInt((S32)(playSpeed * 20), 14);
   stream->writeRangedU32((U32)debrisThetaMin, 0, 180);
   stream->writeRangedU32((U32)debrisThetaMax, 0, 180);
   stream->writeRangedU32((U32)debrisPhiMin, 0, 360);
   stream->writeRangedU32((U32)debrisPhiMax, 0, 360);
   stream->writeRangedU32((U32)debrisNum, 0, 1000);
   stream->writeRangedU32(debrisNumVariance, 0, 1000);
   stream->writeInt((S32)(debrisVelocity * 10), 14);
   stream->writeRangedU32((U32)(debrisVelocityVariance * 10), 0, 10000);
   stream->writeInt(delayMS >> 5, 16);
   stream->writeInt(delayVariance >> 5, 16);
   stream->writeInt(lifetimeMS >> 5, 16);
   stream->writeInt(lifetimeVariance >> 5, 16);
   stream->write(offset);

   stream->writeFlag( shakeCamera );
   stream->write(camShakeFreq.x);
   stream->write(camShakeFreq.y);
   stream->write(camShakeFreq.z);
   stream->write(camShakeAmp.x);
   stream->write(camShakeAmp.y);
   stream->write(camShakeAmp.z);
   stream->write(camShakeDuration);
   stream->write(camShakeRadius);
   stream->write(camShakeFalloff);

   for( S32 j=0; j<EC_NUM_DEBRIS_TYPES; j++ )
   {
      if( stream->writeFlag( debrisList[j] ) )
      {
         stream->writeRangedU32( debrisList[j]->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
      }
   }

   S32 i;
   for( i=0; i<EC_NUM_EMITTERS; i++ )
   {
      if( stream->writeFlag( emitterList[i] != NULL ) )
      {
         stream->writeRangedU32( emitterList[i]->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
      }
   }

   for( i=0; i<EC_MAX_SUB_EXPLOSIONS; i++ )
   {
      if( stream->writeFlag( explosionList[i] != NULL ) )
      {
         stream->writeRangedU32( explosionList[i]->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
      }
   }
   U32 count;
   for(count = 0; count < EC_NUM_TIME_KEYS; count++)
      if(times[i] >= 1)
         break;
   count++;
   if(count > EC_NUM_TIME_KEYS)
      count = EC_NUM_TIME_KEYS;

   stream->writeRangedU32(count, 0, EC_NUM_TIME_KEYS);

   for( i=0; i<count; i++ )
      stream->writeFloat( times[i], 8 );

   for( i=0; i<count; i++ )
   {
      stream->writeRangedU32((U32)(sizes[i].x * 100), 0, 16000);
      stream->writeRangedU32((U32)(sizes[i].y * 100), 0, 16000);
      stream->writeRangedU32((U32)(sizes[i].z * 100), 0, 16000);
   }

   // Dynamic light info
   stream->writeFloat(lightStartRadius/MaxLightRadius, 8);
   stream->writeFloat(lightEndRadius/MaxLightRadius, 8);
   stream->writeFloat(lightStartColor.red,7);
   stream->writeFloat(lightStartColor.green,7);
   stream->writeFloat(lightStartColor.blue,7);
   stream->writeFloat(lightEndColor.red,7);
   stream->writeFloat(lightEndColor.green,7);
   stream->writeFloat(lightEndColor.blue,7);
   stream->writeFloat(lightStartBrightness/MaxLightRadius, 8);
   stream->writeFloat(lightEndBrightness/MaxLightRadius, 8);
   stream->write(lightNormalOffset);
}

void ExplosionData::unpackData(BitStream* stream)
{
	Parent::unpackData(stream);

   dtsFileName = stream->readSTString();

   if (stream->readFlag())
      soundProfileId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   else
      soundProfileId = 0;

   if (stream->readFlag())
      particleEmitterId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   else
      particleEmitterId = 0;

   particleDensity = stream->readInt(14);
   stream->read(&particleRadius);
   faceViewer = stream->readFlag();
   if(stream->readFlag())
   {
      explosionScale.x = stream->readInt(16) / 100.0f;
      explosionScale.y = stream->readInt(16) / 100.0f;
      explosionScale.z = stream->readInt(16) / 100.0f;
   }
   else
      explosionScale.set(1,1,1);
   playSpeed = stream->readInt(14) / 20.0f;
   debrisThetaMin = stream->readRangedU32(0, 180);
   debrisThetaMax = stream->readRangedU32(0, 180);
   debrisPhiMin = stream->readRangedU32(0, 360);
   debrisPhiMax = stream->readRangedU32(0, 360);
   debrisNum = stream->readRangedU32(0, 1000);
   debrisNumVariance = stream->readRangedU32(0, 1000);

   debrisVelocity = stream->readInt(14) / 10.0f;
   debrisVelocityVariance = stream->readRangedU32(0, 10000) / 10.0f;
   delayMS = stream->readInt(16) << 5;
   delayVariance = stream->readInt(16) << 5;
   lifetimeMS = stream->readInt(16) << 5;
   lifetimeVariance = stream->readInt(16) << 5;

   stream->read(&offset);

   shakeCamera = stream->readFlag();
   stream->read(&camShakeFreq.x);
   stream->read(&camShakeFreq.y);
   stream->read(&camShakeFreq.z);
   stream->read(&camShakeAmp.x);
   stream->read(&camShakeAmp.y);
   stream->read(&camShakeAmp.z);
   stream->read(&camShakeDuration);
   stream->read(&camShakeRadius);
   stream->read(&camShakeFalloff);


   for( S32 j=0; j<EC_NUM_DEBRIS_TYPES; j++ )
   {
      if( stream->readFlag() )
      {
         debrisIDList[j] = (S32) stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );
      }
   }

   U32 i;
   for( i=0; i<EC_NUM_EMITTERS; i++ )
   {
      if( stream->readFlag() )
      {
         emitterIDList[i] = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );
      }
   }

   for( S32 k=0; k<EC_MAX_SUB_EXPLOSIONS; k++ )
   {
      if( stream->readFlag() )
      {
         explosionIDList[k] = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );
      }
   }

   U32 count = stream->readRangedU32(0, EC_NUM_TIME_KEYS);

   for( i=0; i<count; i++ )
      times[i] = stream->readFloat(8);

   for( i=0; i<count; i++ )
   {
      sizes[i].x = stream->readRangedU32(0, 16000) / 100.0f;
      sizes[i].y = stream->readRangedU32(0, 16000) / 100.0f;
      sizes[i].z = stream->readRangedU32(0, 16000) / 100.0f;
   }

   //
   lightStartRadius = stream->readFloat(8) * MaxLightRadius;
   lightEndRadius = stream->readFloat(8) * MaxLightRadius;
   lightStartColor.red = stream->readFloat(7);
   lightStartColor.green = stream->readFloat(7);
   lightStartColor.blue = stream->readFloat(7);
   lightEndColor.red = stream->readFloat(7);
   lightEndColor.green = stream->readFloat(7);
   lightEndColor.blue = stream->readFloat(7);
   lightStartBrightness = stream->readFloat(8) * MaxLightRadius;
   lightEndBrightness = stream->readFloat(8) * MaxLightRadius;
   stream->read( &lightNormalOffset );
}

bool ExplosionData::preload(bool server, String &errorStr)
{
   if (Parent::preload(server, errorStr) == false)
      return false;

   if (dtsFileName && dtsFileName[0]) {
      explosionShape = ResourceManager::get().load(dtsFileName);
      if (!bool(explosionShape)) {
         errorStr = String::ToString("ExplosionData: Couldn't load shape \"%s\"", dtsFileName);
         return false;
      }

      // Resolve animations
      explosionAnimation = explosionShape->findSequence("ambient");

      // Preload textures with a dummy instance...
      TSShapeInstance* pDummy = new TSShapeInstance(explosionShape, !server);
      delete pDummy;

   } else {
      explosionShape     = NULL;
      explosionAnimation = -1;
   }

   return true;
}


//--------------------------------------------------------------------------
//--------------------------------------
//
Explosion::Explosion()
{
   mTypeMask |= ExplosionObjectType | LightObjectType;

   mExplosionInstance = NULL;
   mExplosionThread   = NULL;

   dMemset( mEmitterList, 0, sizeof( mEmitterList ) );
   mMainEmitter = NULL;

   mFade = 1;
   mDelayMS = 0;
   mCurrMS = 0;
   mEndingMS = 1000;
   mActive = false;
   mCollideType = 0;

   mInitialNormal.set( 0.0f, 0.0f, 1.0f );
   mRandAngle = sgRandom.randF( 0.0f, 1.0f ) * M_PI_F * 2.0f;
   mLight = gClientSceneGraph->getLightManager()->createLightInfo();
}

Explosion::~Explosion()
{
   if( mExplosionInstance )
   {
      delete mExplosionInstance;
      mExplosionInstance = NULL;
      mExplosionThread   = NULL;
   }
   
   SAFE_DELETE(mLight);
}


void Explosion::setInitialState(const Point3F& point, const Point3F& normal, const F32 fade)
{
   setPosition(point);
   mInitialNormal   = normal;
   mFade            = fade;
}

//--------------------------------------------------------------------------
void Explosion::initPersistFields()
{
   Parent::initPersistFields();

   //
}

//--------------------------------------------------------------------------
bool Explosion::onAdd()
{
   // first check if we have a server connection, if we dont then this is on the server
   //  and we should exit, then check if the parent fails to add the object
   GameConnection* conn = GameConnection::getConnectionToServer();
   if(!conn || !Parent::onAdd())
      return false;

   mDelayMS = mDataBlock->delayMS + sgRandom.randI( -mDataBlock->delayVariance, mDataBlock->delayVariance );
   mEndingMS = mDataBlock->lifetimeMS + sgRandom.randI( -mDataBlock->lifetimeVariance, mDataBlock->lifetimeVariance );

   if( mFabs( mDataBlock->offset ) > 0.001f )
   {
      MatrixF axisOrient = MathUtils::createOrientFromDir( mInitialNormal );

      MatrixF trans = getTransform();
      Point3F randVec;
      randVec.x = sgRandom.randF( -1.0f, 1.0f );
      randVec.y = sgRandom.randF( 0.0f, 1.0f );
      randVec.z = sgRandom.randF( -1.0f, 1.0f );
      randVec.normalize();
      randVec *= mDataBlock->offset;
      axisOrient.mulV( randVec );
      trans.setPosition( trans.getPosition() + randVec );
      setTransform( trans );
   }

   // shake camera
   if( mDataBlock->shakeCamera )
   {
      // first check if explosion is near player
      GameConnection* connection = GameConnection::getConnectionToServer();
      ShapeBase *obj = dynamic_cast<ShapeBase*>(connection->getControlObject());

      bool applyShake = true;

      if( obj )
      {
         ShapeBase* cObj = obj;
         while((cObj = cObj->getControlObject()) != 0)
         {
            if(cObj->useObjsEyePoint())
            {
               applyShake = false;
               break;
            }
         }
      }


      if( applyShake && obj )
      {
         VectorF diff = obj->getPosition() - getPosition();
         F32 dist = diff.len();
         if( dist < mDataBlock->camShakeRadius )
         {
            CameraShake *camShake = new CameraShake;
            camShake->setDuration( mDataBlock->camShakeDuration );
            camShake->setFrequency( mDataBlock->camShakeFreq );

            F32 falloff =  dist / mDataBlock->camShakeRadius;
            falloff = 1.0f + falloff * 10.0f;
            falloff = 1.0f / (falloff * falloff);

            VectorF shakeAmp = mDataBlock->camShakeAmp * falloff;
            camShake->setAmplitude( shakeAmp );
            camShake->setFalloff( mDataBlock->camShakeFalloff );
            camShake->init();
            gCamFXMgr.addFX( camShake );
         }
      }
   }


   if( mDelayMS == 0 )
   {
      if( !explode() )
      {
         return false;
      }
   }

   gClientContainer.addObject(this);
   gClientSceneGraph->addObjectToScene(this);

   removeFromProcessList();
   gClientProcessList.addObject(this);

   mRandomVal = sgRandom.randF();

   NetConnection* pNC = NetConnection::getConnectionToServer();
   AssertFatal(pNC != NULL, "Error, must have a connection to the server!");
   pNC->addObject(this);

   // Initialize the light structure and register as a dynamic light
   if (mDataBlock->lightStartRadius != 0.0f || mDataBlock->lightEndRadius)
   {
      mLight->setType( LightInfo::Point );
      mLight->setRange( mDataBlock->lightStartRadius );
      mLight->setColor( mDataBlock->lightStartColor );
   }

   return true;
}

void Explosion::onRemove()
{
   for( int i=0; i<ExplosionData::EC_NUM_EMITTERS; i++ )
   {
      if( mEmitterList[i] )
      {
         mEmitterList[i]->deleteWhenEmpty();
         mEmitterList[i] = NULL;
      }
   }

   if( mMainEmitter )
   {
      mMainEmitter->deleteWhenEmpty();
      mMainEmitter = NULL;
   }

   if (mSceneManager != NULL)
      mSceneManager->removeObjectFromScene(this);
   if (getContainer() != NULL)
      getContainer()->removeObject(this);

   Parent::onRemove();
}


bool Explosion::onNewDataBlock(GameBaseData* dptr)
{
   mDataBlock = dynamic_cast<ExplosionData*>(dptr);
   if (!mDataBlock || !Parent::onNewDataBlock(dptr))
      return false;

   scriptOnNewDataBlock();
   return true;
}


//--------------------------------------------------------------------------
bool Explosion::prepRenderImage(SceneState* state, const U32 stateKey,
                                       const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
   if (isLastState(state, stateKey))
      return false;
   setLastState(state, stateKey);

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if ( state->isObjectRendered( this ) ) 
      prepBatchRender( state );

   return false;
}

void Explosion::setCurrentScale()
{
   F32 t = F32(mCurrMS) / F32(mEndingMS);

   for( U32 i = 1; i < ExplosionData::EC_NUM_TIME_KEYS; i++ )
   {
      if( mDataBlock->times[i] >= t )
      {
         F32 firstPart =   t - mDataBlock->times[i-1];
         F32 total     =   mDataBlock->times[i] -
                           mDataBlock->times[i-1];

         firstPart /= total;

         mObjScale =      (mDataBlock->sizes[i-1] * (1.0f - firstPart)) +
                          (mDataBlock->sizes[i]   * firstPart);

         return;
      }
   }

}

//--------------------------------------------------------------------------
// Make the explosion face the viewer (if desired)
//--------------------------------------------------------------------------
void Explosion::prepModelView(SceneState* state)
{
   MatrixF rotMatrix( true );
   Point3F targetVector;

   if( mDataBlock->faceViewer )
   {
      targetVector = getPosition() - state->getCameraPosition();
      targetVector.normalize();

      // rotate explosion each time so it's a little different
      rotMatrix.set( EulerF( 0.0f, mRandAngle, 0.0f ) );
   }
   else
   {
      targetVector = mInitialNormal;
   }

   MatrixF explOrient = MathUtils::createOrientFromDir( targetVector );
   explOrient.mul( rotMatrix );
   explOrient.setPosition( getPosition() );

   setCurrentScale();
   explOrient.scale( mObjScale );
   GFX->setWorldMatrix( explOrient );
}

//--------------------------------------------------------------------------
// Render object
//--------------------------------------------------------------------------
void Explosion::prepBatchRender(SceneState* state)
{
   MatrixF proj = GFX->getProjectionMatrix();

   RectI viewport = GFX->getViewport();

   // Set up our TS render state here.
   TSRenderState rdata;
   rdata.setSceneState( state );

   if( mExplosionInstance )
   {
      // render mesh
      GFX->pushWorldMatrix();

      prepModelView( state );

      mExplosionInstance->animate();
      mExplosionInstance->render( rdata );

      GFX->popWorldMatrix();
   }


   GFX->setProjectionMatrix( proj );
   GFX->setViewport( viewport );

}

void Explosion::submitLights( LightManager *lm, bool staticLighting )
{
   if ( staticLighting )
      return;

   // Update the light's info and add it to the scene, the light will
   // only be visible for this current frame.
   mLight->setPosition( getRenderTransform().getPosition() + mInitialNormal * mDataBlock->lightNormalOffset );
   F32 t = F32(mCurrMS) / F32(mEndingMS);
   mLight->setRange( mDataBlock->lightStartRadius +
      (mDataBlock->lightEndRadius - mDataBlock->lightStartRadius) * t );
   mLight->setColor( mDataBlock->lightStartColor +
      (mDataBlock->lightEndColor - mDataBlock->lightStartColor) * t );
   mLight->setBrightness( mDataBlock->lightStartBrightness +
      (mDataBlock->lightEndBrightness - mDataBlock->lightStartBrightness) * t );

   lm->registerGlobalLight( mLight, this );
}


//--------------------------------------------------------------------------
void Explosion::processTick(const Move*)
{
   mCurrMS += TickMs;

   if( mCurrMS >= mEndingMS )
   {
         deleteObject();
         return;
   }
         
   if( (mCurrMS > mDelayMS) && !mActive )
      explode();
}

void Explosion::advanceTime(F32 dt)
{
   if (dt == 0.0f)
      return;

   GameConnection* conn = GameConnection::getConnectionToServer();
   if(!conn)
      return;

   updateEmitters( dt );

   if( mExplosionInstance )
      mExplosionInstance->advanceTime(dt, mExplosionThread);
}

//----------------------------------------------------------------------------
// Update emitters
//----------------------------------------------------------------------------
void Explosion::updateEmitters( F32 dt )
{
   Point3F pos = getPosition();

   for( int i=0; i<ExplosionData::EC_NUM_EMITTERS; i++ )
   {
      if( mEmitterList[i] )
      {
         mEmitterList[i]->emitParticles( pos, pos, mInitialNormal, Point3F( 0.0f, 0.0f, 0.0f ), (U32)(dt * 1000));
      }
   }

}

//----------------------------------------------------------------------------
// Launch Debris
//----------------------------------------------------------------------------
void Explosion::launchDebris( Point3F &axis )
{
   GameConnection* conn = GameConnection::getConnectionToServer();
   if(!conn)
      return;

   bool hasDebris = false;
   for( int j=0; j<ExplosionData::EC_NUM_DEBRIS_TYPES; j++ )
   {
      if( mDataBlock->debrisList[j] )
      {
         hasDebris = true;
         break;
      }
   }
   if( !hasDebris )
   {
      return;
   }

   Point3F axisx;
   if (mFabs(axis.z) < 0.999f)
      mCross(axis, Point3F(0.0f, 0.0f, 1.0f), &axisx);
   else
      mCross(axis, Point3F(0.0f, 1.0f, 0.0f), &axisx);
   axisx.normalize();

   Point3F pos( 0.0f, 0.0f, 0.5f );
   pos += getPosition();


   U32 numDebris = mDataBlock->debrisNum + sgRandom.randI( -mDataBlock->debrisNumVariance, mDataBlock->debrisNumVariance );

   for( int i=0; i<numDebris; i++ )
   {

      Point3F launchDir = MathUtils::randomDir( axis, mDataBlock->debrisThetaMin, mDataBlock->debrisThetaMax,
                                                mDataBlock->debrisPhiMin, mDataBlock->debrisPhiMax );

      F32 debrisVel = mDataBlock->debrisVelocity + mDataBlock->debrisVelocityVariance * sgRandom.randF( -1.0f, 1.0f );

      launchDir *= debrisVel;

      Debris *debris = new Debris;
      debris->setDataBlock( mDataBlock->debrisList[0] );
      debris->setTransform( getTransform() );
      debris->init( pos, launchDir );

      if( !debris->registerObject() )
      {
         Con::warnf( ConsoleLogEntry::General, "Could not register debris for class: %s", mDataBlock->getName() );
         delete debris;
         debris = NULL;
      }
   }
}

//----------------------------------------------------------------------------
// Spawn sub explosions
//----------------------------------------------------------------------------
void Explosion::spawnSubExplosions()
{
   GameConnection* conn = GameConnection::getConnectionToServer();
   if(!conn)
      return;

   for( S32 i=0; i<ExplosionData::EC_MAX_SUB_EXPLOSIONS; i++ )
   {
      if( mDataBlock->explosionList[i] )
      {
         MatrixF trans = getTransform();
         Explosion* pExplosion = new Explosion;
         pExplosion->setDataBlock( mDataBlock->explosionList[i] );
         pExplosion->setTransform( trans );
         pExplosion->setInitialState( trans.getPosition(), mInitialNormal, 1);
         if (!pExplosion->registerObject())
            delete pExplosion;
      }
   }
}

//----------------------------------------------------------------------------
// Explode
//----------------------------------------------------------------------------
bool Explosion::explode()
{
   mActive = true;

   GameConnection* conn = GameConnection::getConnectionToServer();
   if(!conn)
      return false;

   launchDebris( mInitialNormal );
   spawnSubExplosions();

   if (bool(mDataBlock->explosionShape) && mDataBlock->explosionAnimation != -1) {
      mExplosionInstance = new TSShapeInstance(mDataBlock->explosionShape, true);

      mExplosionThread   = mExplosionInstance->addThread();
      mExplosionInstance->setSequence(mExplosionThread, mDataBlock->explosionAnimation, 0);
      mExplosionInstance->setTimeScale(mExplosionThread, mDataBlock->playSpeed);

      mCurrMS   = 0;
      mEndingMS = U32(mExplosionInstance->getScaledDuration(mExplosionThread) * 1000.0f);

      mObjScale.convolve(mDataBlock->explosionScale);
      mObjBox = mDataBlock->explosionShape->bounds;
      resetWorldBox();
   }

   if (mDataBlock->soundProfile)
      SFX->playOnce( mDataBlock->soundProfile, &getTransform() );

   if (mDataBlock->particleEmitter) {
      mMainEmitter = new ParticleEmitter;
      mMainEmitter->setDataBlock(mDataBlock->particleEmitter);
      mMainEmitter->registerObject();

      mMainEmitter->emitParticles(getPosition(), mInitialNormal, mDataBlock->particleRadius,
                             Point3F(0.0f, 0.0f, 0.0f), U32(mDataBlock->particleDensity * mFade));
   }

   for( int i=0; i<ExplosionData::EC_NUM_EMITTERS; i++ )
   {
      if( mDataBlock->emitterList[i] != NULL )
      {
         ParticleEmitter * pEmitter = new ParticleEmitter;
         pEmitter->setDataBlock( mDataBlock->emitterList[i] );
         if( !pEmitter->registerObject() )
         {
            Con::warnf( ConsoleLogEntry::General, "Could not register emitter for particle of class: %s", mDataBlock->getName() );
            SAFE_DELETE(pEmitter);
         }
         mEmitterList[i] = pEmitter;
      }
   }

   return true;
}

