//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/fx/particleEmitter.h"

#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "math/mRandom.h"
#include "gfx/gfxDevice.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "renderInstance/renderPassManager.h"
#include "T3D/gameProcess.h"
#include "lighting/lightInfo.h"

#if defined(TORQUE_OS_XENON)
#  include "gfx/D3D9/360/gfx360MemVertexBuffer.h"
#endif

static ParticleEmitterData gDefaultEmitterData;
Point3F ParticleEmitter::mWindVelocity( 0.0, 0.0, 0.0 );

IMPLEMENT_CO_DATABLOCK_V1(ParticleEmitterData);

//-----------------------------------------------------------------------------
// ParticleEmitterData
//-----------------------------------------------------------------------------
ParticleEmitterData::ParticleEmitterData()
{
   VECTOR_SET_ASSOCIATION(particleDataBlocks);
   VECTOR_SET_ASSOCIATION(dataBlockIds);

   ejectionPeriodMS = 100;    // 10 Particles Per second
   periodVarianceMS = 0;      // exactly

   ejectionVelocity = 2.0f;   // From 1.0 - 3.0 meters per sec
   velocityVariance = 1.0f;
   ejectionOffset   = 0.0f;   // ejection from the emitter point

   thetaMin         = 0.0f;   // All heights
   thetaMax         = 90.0f;

   phiReferenceVel  = 0.0f;   // All directions
   phiVariance      = 360.0f;

   softnessDistance = 1.0f;
   ambientFactor = 0.0f;

   lifetimeMS           = 0;
   lifetimeVarianceMS   = 0;

   overrideAdvance  = true;
   orientParticles  = false;
   orientOnVelocity = true;
   useEmitterSizes  = false;
   useEmitterColors = false;
   particleString   = NULL;
   partListInitSize = 0;

   // These members added for support of user defined blend factors
   // and optional particle sorting.
   blendStyle = ParticleRenderInst::BlendUndefined;
   sortParticles = false;
   reverseOrder = false;
   textureName = 0;
   textureHandle = 0;
   highResOnly = true;
   
   alignParticles = false;
   alignDirection = Point3F(0.0f, 1.0f, 0.0f);
}


IMPLEMENT_CONSOLETYPE(ParticleEmitterData)
IMPLEMENT_GETDATATYPE(ParticleEmitterData)
IMPLEMENT_SETDATATYPE(ParticleEmitterData)


// Enum tables used for fields blendStyle, srcBlendFactor, dstBlendFactor.
// Note that the enums for srcBlendFactor and dstBlendFactor are consistent
// with the blending enums used in Torque Game Builder.

static EnumTable::Enums blendStyleLookup[] =
{
    { ParticleRenderInst::BlendNormal,         "NORMAL"                },
    { ParticleRenderInst::BlendAdditive,       "ADDITIVE"              },
    { ParticleRenderInst::BlendSubtractive,    "SUBTRACTIVE"           },
    { ParticleRenderInst::BlendPremultAlpha,   "PREMULTALPHA"          },
};
EnumTable blendStyleTable(sizeof(blendStyleLookup) / sizeof(EnumTable::Enums), &blendStyleLookup[0]);

//-----------------------------------------------------------------------------
// initPersistFields
//-----------------------------------------------------------------------------
void ParticleEmitterData::initPersistFields()
{
   addField("ejectionPeriodMS",     TypeS32,    Offset(ejectionPeriodMS,   ParticleEmitterData));
   addField("periodVarianceMS",     TypeS32,    Offset(periodVarianceMS,   ParticleEmitterData));
   addField("ejectionVelocity",     TypeF32,    Offset(ejectionVelocity,   ParticleEmitterData));
   addField("velocityVariance",     TypeF32,    Offset(velocityVariance,   ParticleEmitterData));
   addField("ejectionOffset",       TypeF32,    Offset(ejectionOffset,     ParticleEmitterData));
   addField("thetaMin",             TypeF32,    Offset(thetaMin,           ParticleEmitterData));
   addField("thetaMax",             TypeF32,    Offset(thetaMax,           ParticleEmitterData));
   addField("phiReferenceVel",      TypeF32,    Offset(phiReferenceVel,    ParticleEmitterData));
   addField("phiVariance",          TypeF32,    Offset(phiVariance,        ParticleEmitterData));
   addField("softnessDistance",     TypeF32,    Offset(softnessDistance,   ParticleEmitterData));
   addField("ambientFactor",        TypeF32,    Offset(ambientFactor,      ParticleEmitterData));
   addField("overrideAdvance",      TypeBool,   Offset(overrideAdvance,    ParticleEmitterData));
   addField("orientParticles",      TypeBool,   Offset(orientParticles,    ParticleEmitterData));
   addField("orientOnVelocity",     TypeBool,   Offset(orientOnVelocity,   ParticleEmitterData));
   addField("particles",            TypeString, Offset(particleString,     ParticleEmitterData));
   addField("lifetimeMS",           TypeS32,    Offset(lifetimeMS,         ParticleEmitterData));
   addField("lifetimeVarianceMS",   TypeS32,    Offset(lifetimeVarianceMS, ParticleEmitterData));
   addField("useEmitterSizes",      TypeBool,   Offset(useEmitterSizes,    ParticleEmitterData));
   addField("useEmitterColors",     TypeBool,   Offset(useEmitterColors,   ParticleEmitterData));

   // These fields added for support of user defined blend factors and optional particle sorting.
   addField("blendStyle",         TypeEnum,     Offset(blendStyle,         ParticleEmitterData), 1, &blendStyleTable);
   addField("sortParticles",      TypeBool,     Offset(sortParticles,      ParticleEmitterData));
   addField("reverseOrder",       TypeBool,     Offset(reverseOrder,       ParticleEmitterData));
   addField("textureName",        TypeFilename, Offset(textureName,        ParticleEmitterData));
   
   addField("alignParticles",       TypeBool,    Offset(alignParticles,    ParticleEmitterData));
   addField("alignDirection",       TypePoint3F, Offset(alignDirection,    ParticleEmitterData));

   addField("highResOnly",          TypeBool,   Offset(highResOnly,        ParticleEmitterData), "This particle system should not use the mixed-resolution renderer. If your particle system has large amounts of overdraw, consider disabling this option.");

   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------
// packData
//-----------------------------------------------------------------------------
void ParticleEmitterData::packData(BitStream* stream)
{
   Parent::packData(stream);

   stream->writeInt(ejectionPeriodMS, 10);
   stream->writeInt(periodVarianceMS, 10);
   stream->writeInt((S32)(ejectionVelocity * 100), 16);
   stream->writeInt((S32)(velocityVariance * 100), 14);
   if( stream->writeFlag(ejectionOffset != gDefaultEmitterData.ejectionOffset) )
      stream->writeInt((S32)(ejectionOffset * 100), 16);
   stream->writeRangedU32((U32)thetaMin, 0, 180);
   stream->writeRangedU32((U32)thetaMax, 0, 180);
   if( stream->writeFlag(phiReferenceVel != gDefaultEmitterData.phiReferenceVel) )
      stream->writeRangedU32((U32)phiReferenceVel, 0, 360);
   if( stream->writeFlag(phiVariance != gDefaultEmitterData.phiVariance) )
      stream->writeRangedU32((U32)phiVariance, 0, 360);

   stream->write( softnessDistance );
   stream->write( ambientFactor );

   stream->writeFlag(overrideAdvance);
   stream->writeFlag(orientParticles);
   stream->writeFlag(orientOnVelocity);
   stream->write( lifetimeMS );
   stream->write( lifetimeVarianceMS );
   stream->writeFlag(useEmitterSizes);
   stream->writeFlag(useEmitterColors);

   stream->write(dataBlockIds.size());
   for (U32 i = 0; i < dataBlockIds.size(); i++)
      stream->write(dataBlockIds[i]);
   stream->writeFlag(sortParticles);
   stream->writeFlag(reverseOrder);
   if (stream->writeFlag(textureName != 0))
     stream->writeString(textureName);

   if (stream->writeFlag(alignParticles))
   {
      stream->write(alignDirection.x);
      stream->write(alignDirection.y);
      stream->write(alignDirection.z);
   }
   stream->writeFlag(highResOnly);
}

//-----------------------------------------------------------------------------
// unpackData
//-----------------------------------------------------------------------------
void ParticleEmitterData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

   ejectionPeriodMS = stream->readInt(10);
   periodVarianceMS = stream->readInt(10);
   ejectionVelocity = stream->readInt(16) / 100.0f;
   velocityVariance = stream->readInt(14) / 100.0f;
   if( stream->readFlag() )
      ejectionOffset = stream->readInt(16) / 100.0f;
   else
      ejectionOffset = gDefaultEmitterData.ejectionOffset;

   thetaMin = (F32)stream->readRangedU32(0, 180);
   thetaMax = (F32)stream->readRangedU32(0, 180);
   if( stream->readFlag() )
      phiReferenceVel = (F32)stream->readRangedU32(0, 360);
   else
      phiReferenceVel = gDefaultEmitterData.phiReferenceVel;

   if( stream->readFlag() )
      phiVariance = (F32)stream->readRangedU32(0, 360);
   else
      phiVariance = gDefaultEmitterData.phiVariance;

   stream->read( &softnessDistance );
   stream->read( &ambientFactor );

   overrideAdvance = stream->readFlag();
   orientParticles = stream->readFlag();
   orientOnVelocity = stream->readFlag();
   stream->read( &lifetimeMS );
   stream->read( &lifetimeVarianceMS );
   useEmitterSizes = stream->readFlag();
   useEmitterColors = stream->readFlag();

   U32 size; stream->read(&size);
   dataBlockIds.setSize(size);
   for (U32 i = 0; i < dataBlockIds.size(); i++)
      stream->read(&dataBlockIds[i]);
   sortParticles = stream->readFlag();
   reverseOrder = stream->readFlag();
   textureName = (stream->readFlag()) ? stream->readSTString() : 0;

   alignParticles = stream->readFlag();
   if (alignParticles)
   {
      stream->read(&alignDirection.x);
      stream->read(&alignDirection.y);
      stream->read(&alignDirection.z);
   }
   highResOnly = stream->readFlag();
}

//-----------------------------------------------------------------------------
// onAdd
//-----------------------------------------------------------------------------
bool ParticleEmitterData::onAdd()
{
   if( Parent::onAdd() == false )
      return false;

//   if (overrideAdvance == true) {
//      Con::errorf(ConsoleLogEntry::General, "ParticleEmitterData: Not going to work.  Fix it!");
//      return false;
//   }

   // Validate the parameters...
   //
   if( ejectionPeriodMS < 1 )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) period < 1 ms", getName());
      ejectionPeriodMS = 1;
   }
   if( periodVarianceMS >= ejectionPeriodMS )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) periodVariance >= period", getName());
      periodVarianceMS = ejectionPeriodMS - 1;
   }
   if( ejectionVelocity < 0.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) ejectionVelocity < 0.0f", getName());
      ejectionVelocity = 0.0f;
   }
   if( velocityVariance > ejectionVelocity )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) velocityVariance > ejectionVelocity", getName());
      velocityVariance = ejectionVelocity;
   }
   if( ejectionOffset < 0.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) ejectionOffset < 0", getName());
      ejectionOffset = 0.0f;
   }
   if( thetaMin < 0.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) thetaMin < 0.0", getName());
      thetaMin = 0.0f;
   }
   if( thetaMax > 180.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) thetaMax > 180.0", getName());
      thetaMax = 180.0f;
   }
   if( thetaMin > thetaMax )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) thetaMin > thetaMax", getName());
      thetaMin = thetaMax;
   }
   if( phiVariance < 0.0f || phiVariance > 360.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) invalid phiVariance", getName());
      phiVariance = phiVariance < 0.0f ? 0.0f : 360.0f;
   }

   if ( softnessDistance < 0.0f )
   {
      Con::warnf( ConsoleLogEntry::General, "ParticleEmitterData(%s) invalid softnessDistance", getName() );
      softnessDistance = 0.0f;
   }

   if ( ambientFactor < 0.0f )
   {
      Con::warnf( ConsoleLogEntry::General, "ParticleEmitterData(%s) invalid ambientFactor", getName() );
      ambientFactor = 0.0f;
   }

   if (particleString == NULL && dataBlockIds.size() == 0) 
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) no particleString, invalid datablock", getName());
      return false;
   }
   if (particleString && particleString[0] == '\0') 
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) no particleString, invalid datablock", getName());
      return false;
   }
   if (particleString && dStrlen(particleString) > 255) 
   {
      Con::errorf(ConsoleLogEntry::General, "ParticleEmitterData(%s) particle string too long [> 255 chars]", getName());
      return false;
   }

   if( lifetimeMS < 0 )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) lifetimeMS < 0.0f", getName());
      lifetimeMS = 0;
   }
   if( lifetimeVarianceMS > lifetimeMS )
   {
      Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) lifetimeVarianceMS >= lifetimeMS", getName());
      lifetimeVarianceMS = lifetimeMS;
   }


   // load the particle datablocks...
   //
   if( particleString != NULL )
   {
      //   particleString is once again a list of particle datablocks so it
      //   must be parsed to extract the particle references.

      // First we parse particleString into a list of particle name tokens 
      Vector<char*> dataBlocks(__FILE__, __LINE__);
      char* tokCopy = new char[dStrlen(particleString) + 1];
      dStrcpy(tokCopy, particleString);

      char* currTok = dStrtok(tokCopy, " \t");
      while (currTok != NULL) 
      {
         dataBlocks.push_back(currTok);
         currTok = dStrtok(NULL, " \t");
      }
      if (dataBlocks.size() == 0) 
      {
         Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) invalid particles string.  No datablocks found", getName());
         delete [] tokCopy;
         return false;
      }    

      // Now we convert the particle name tokens into particle datablocks and IDs 
      particleDataBlocks.clear();
      dataBlockIds.clear();

      for (U32 i = 0; i < dataBlocks.size(); i++) 
      {
         ParticleData* pData = NULL;
         if (Sim::findObject(dataBlocks[i], pData) == false) 
         {
            Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) unable to find particle datablock: %s", getName(), dataBlocks[i]);
         }
         else 
         {
            particleDataBlocks.push_back(pData);
            dataBlockIds.push_back(pData->getId());
         }
      }

      // cleanup
      delete [] tokCopy;

      // check that we actually found some particle datablocks
      if (particleDataBlocks.size() == 0) 
      {
         Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) unable to find any particle datablocks", getName());
         return false;
      }
   }

   return true;
}

//-----------------------------------------------------------------------------
// preload
//-----------------------------------------------------------------------------
bool ParticleEmitterData::preload(bool server, String &errorStr)
{
   if( Parent::preload(server, errorStr) == false )
      return false;

   particleDataBlocks.clear();
   for (U32 i = 0; i < dataBlockIds.size(); i++) 
   {
      ParticleData* pData = NULL;
      if (Sim::findObject(dataBlockIds[i], pData) == false)
         Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) unable to find particle datablock: %d", getName(), dataBlockIds[i]);
      else
         particleDataBlocks.push_back(pData);
   }

   if (!server)
   {
     // load emitter texture if specified
     if (textureName && textureName[0])
     {
       textureHandle = GFXTexHandle(textureName, &GFXDefaultStaticDiffuseProfile, avar("%s() - textureHandle (line %d)", __FUNCTION__, __LINE__));
       if (!textureHandle)
       {
         errorStr = String::ToString("Missing particle emitter texture: %s", textureName);
         return false;
       }
     }
     // otherwise, check that all particles refer to the same texture
     else if (particleDataBlocks.size() > 1)
     {
       StringTableEntry txr_name = particleDataBlocks[0]->textureName;
       for (S32 i = 1; i < particleDataBlocks.size(); i++)
       {
         // warn if particle textures are inconsistent
         if (particleDataBlocks[i]->textureName != txr_name)
         {
           Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) particles reference different textures.", getName());
           break;
         }
       }
     }
   }

   // if blend-style is undefined check legacy useInvAlpha settings
   if (blendStyle == ParticleRenderInst::BlendUndefined && particleDataBlocks.size() > 0)
   {
     bool useInvAlpha = particleDataBlocks[0]->useInvAlpha;
     for (S32 i = 1; i < particleDataBlocks.size(); i++)
     {
       // warn if blend-style legacy useInvAlpha settings are inconsistent
       if (particleDataBlocks[i]->useInvAlpha != useInvAlpha)
       {
         Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) particles have inconsistent useInvAlpha settings.", getName());
         break;
       }
     }
     blendStyle = (useInvAlpha) ? ParticleRenderInst::BlendNormal : ParticleRenderInst::BlendAdditive;
   }
   
   if( !server )
   {
      allocPrimBuffer();
   }

   return true;
}

//-----------------------------------------------------------------------------
// alloc PrimitiveBuffer
// The datablock allocates this static index buffer because it's the same
// for all of the emitters - each particle quad uses the same index ordering
//-----------------------------------------------------------------------------
void ParticleEmitterData::allocPrimBuffer( S32 overrideSize )
{
   // TODO: Rewrite

   // calculate particle list size
   AssertFatal(particleDataBlocks.size() > 0, "Error, no particles found." );
   U32 maxPartLife = particleDataBlocks[0]->lifetimeMS + particleDataBlocks[0]->lifetimeVarianceMS;
   for (S32 i = 1; i < particleDataBlocks.size(); i++)
   {
     U32 mpl = particleDataBlocks[i]->lifetimeMS + particleDataBlocks[i]->lifetimeVarianceMS;
     if (mpl > maxPartLife)
       maxPartLife = mpl;
   }

   partListInitSize = maxPartLife / (ejectionPeriodMS - periodVarianceMS);
   partListInitSize += 8; // add 8 as "fudge factor" to make sure it doesn't realloc if it goes over by 1

   // if override size is specified, then the emitter overran its buffer and needs a larger allocation
   if( overrideSize != -1 )
   {
      partListInitSize = overrideSize;
   }

   // create index buffer based on that size
   U32 indexListSize = partListInitSize * 6; // 6 indices per particle
   U16 *indices = new U16[ indexListSize ];

   for( U32 i=0; i<partListInitSize; i++ )
   {
      // this index ordering should be optimal (hopefully) for the vertex cache
      U16 *idx = &indices[i*6];
      volatile U32 offset = i * 4;  // set to volatile to fix VC6 Release mode compiler bug
      idx[0] = 0 + offset;
      idx[1] = 1 + offset;
      idx[2] = 3 + offset;
      idx[3] = 1 + offset;
      idx[4] = 3 + offset;
      idx[5] = 2 + offset;
   }


   U16 *ibIndices;
   GFXBufferType bufferType = GFXBufferTypeStatic;

#ifdef TORQUE_OS_XENON
   // Because of the way the volatile buffers work on Xenon this is the only
   // way to do this.
   bufferType = GFXBufferTypeVolatile;
#endif
   primBuff.set( GFX, indexListSize, 0, bufferType );
   primBuff.lock( &ibIndices );
   dMemcpy( ibIndices, indices, indexListSize * sizeof(U16) );
   primBuff.unlock();

   delete [] indices;
}


//-----------------------------------------------------------------------------
// ParticleEmitter
//-----------------------------------------------------------------------------
ParticleEmitter::ParticleEmitter()
{
   mDeleteWhenEmpty  = false;
   mDeleteOnTick     = false;

   mInternalClock    = 0;
   mNextParticleTime = 0;

   mLastPosition.set(0, 0, 0);
   mHasLastPosition = false;

   mLifetimeMS = 0;
   mElapsedTimeMS = 0;

   part_store = 0;
   part_freelist = NULL;
   part_list_head.next = NULL;
   n_part_capacity = 0;
   n_parts = 0;

   mCurBuffSize = 0;

   mDead = false;
}

//-----------------------------------------------------------------------------
// destructor
//-----------------------------------------------------------------------------
ParticleEmitter::~ParticleEmitter()
{
   for( S32 i = 0; i < part_store.size(); i++ )
   {
      delete [] part_store[i];
   }
}

//-----------------------------------------------------------------------------
// onAdd
//-----------------------------------------------------------------------------
bool ParticleEmitter::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   // add to client side mission cleanup
   SimGroup *cleanup = dynamic_cast<SimGroup *>( Sim::findObject( "ClientMissionCleanup") );
   if( cleanup != NULL )
   {
      cleanup->addObject( this );
   }
   else
   {
      AssertFatal( false, "Error, could not find ClientMissionCleanup group" );
      return false;
   }


   removeFromProcessList();

   mLifetimeMS = mDataBlock->lifetimeMS;
   if( mDataBlock->lifetimeVarianceMS )
   {
      mLifetimeMS += S32( gRandGen.randI() % (2 * mDataBlock->lifetimeVarianceMS + 1)) - S32(mDataBlock->lifetimeVarianceMS );
   }

   //   Allocate particle structures and init the freelist. Member part_store
   //   is a Vector so that we can allocate more particles if partListInitSize
   //   turns out to be too small. 
   //
   if (mDataBlock->partListInitSize > 0)
   {
      for( S32 i = 0; i < part_store.size(); i++ )
      {
         delete [] part_store[i];
      }
      part_store.clear();
      n_part_capacity = mDataBlock->partListInitSize;
      Particle* store_block = new Particle[n_part_capacity];
      part_store.push_back(store_block);
      part_freelist = store_block;
      Particle* last_part = part_freelist;
      Particle* part = last_part+1;
      for( S32 i = 1; i < n_part_capacity; i++, part++, last_part++ )
      {
         last_part->next = part;
      }
      store_block[n_part_capacity-1].next = NULL;
      part_list_head.next = NULL;
      n_parts = 0;
   }

   F32 radius = 5.0;
   mObjBox.minExtents = Point3F(-radius, -radius, -radius);
   mObjBox.maxExtents = Point3F(radius, radius, radius);
   resetWorldBox();

   return true;
}


//-----------------------------------------------------------------------------
// onRemove
//-----------------------------------------------------------------------------
void ParticleEmitter::onRemove()
{
   removeFromScene();
   Parent::onRemove();
}


//-----------------------------------------------------------------------------
// onNewDataBlock
//-----------------------------------------------------------------------------
bool ParticleEmitter::onNewDataBlock(GameBaseData* dptr)
{
   mDataBlock = dynamic_cast<ParticleEmitterData*>(dptr);
   if( !mDataBlock || !Parent::onNewDataBlock(dptr) )
      return false;

   scriptOnNewDataBlock();
   return true;
}

//-----------------------------------------------------------------------------
// getCollectiveColor
//-----------------------------------------------------------------------------
ColorF ParticleEmitter::getCollectiveColor()
{
	U32 count = 0;
	ColorF color = ColorF(0.0f, 0.0f, 0.0f);

   count = n_parts;
   for( Particle* part = part_list_head.next; part != NULL; part = part->next )
   {
      color += part->color;
   }

	if(count > 0)
   {
      color /= F32(count);
   }

	//if(color.red == 0.0f && color.green == 0.0f && color.blue == 0.0f)
	//	color = color;

	return color;
}


//-----------------------------------------------------------------------------
// prepRenderImage
//-----------------------------------------------------------------------------
bool ParticleEmitter::prepRenderImage(SceneState* state, const U32 stateKey,
                                      const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
   if( isLastState(state, stateKey) )
      return false;

   PROFILE_SCOPE(ParticleEmitter_prepRenderImage);

   setLastState(state, stateKey);

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if( state->isObjectRendered(this) )
   {
      const LightInfo *sunlight = state->getLightManager()->getSpecialLight( LightManager::slSunLightType );
      prepBatchRender( state, sunlight->getAmbient() );
   }

   return false;
}

//-----------------------------------------------------------------------------
// prepBatchRender
//-----------------------------------------------------------------------------
void ParticleEmitter::prepBatchRender( SceneState *state, const ColorF &ambientColor )
{
   if (  mDead ||
         n_parts == 0 || 
         part_list_head.next == NULL )
      return;

   RenderPassManager *renderManager = state->getRenderPass();
   const Point3F &camPos = state->getCameraPosition();

   // Only update the particle vertex buffer once per frame
   if(state->isDiffusePass())
      copyToVB( camPos, ambientColor );

   ParticleRenderInst *ri = renderManager->allocInst<ParticleRenderInst>();

   ri->vertBuff = &mVertBuff;
   ri->primBuff = &getDataBlock()->primBuff;
   ri->translucentSort = true;
   ri->type = RenderPassManager::RIT_Particle;
   ri->sortDistSq = getRenderWorldBox().getSqDistanceToPoint( camPos );

   // TODO: Pass primitive data to RPM the way TSMesh does so # of prims/primtype is hidden

   // Draw the system offscreen unless the highResOnly flag is set on the datablock
   ri->systemState = ( getDataBlock()->highResOnly ? ParticleRenderInst::AwaitingHighResDraw : ParticleRenderInst::AwaitingOffscreenDraw );

   // TODO: change this to use shared transforms
   ri->modelViewProj = renderManager->allocUniqueXform(  GFX->getProjectionMatrix() * 
                                                         GFX->getViewMatrix() * 
                                                         GFX->getWorldMatrix() );

   Point3F boxExtents = getRenderWorldBox().getExtents();
   ri->systemSphere.radius = getMax(boxExtents.x, getMax(boxExtents.y, boxExtents.z));
   ri->systemSphere.center = getRenderWorldBox().getCenter();

   // Update position on the matrix before multiplying it
   mBBObjToWorld.setPosition(mLastPosition);

   ri->bbModelViewProj = renderManager->allocUniqueXform( *ri->modelViewProj * mBBObjToWorld );

   ri->count = n_parts;

   ri->blendStyle = mDataBlock->blendStyle;

   // use first particle's texture unless there is an emitter texture to override it
   if (mDataBlock->textureHandle)
     ri->diffuseTex = &*(mDataBlock->textureHandle);
   else
     ri->diffuseTex = &*(part_list_head.next->dataBlock->textureHandle);

   ri->softnessDistance = mDataBlock->softnessDistance; 

   // Sort by texture too.
   ri->defaultKey = ri->diffuseTex ? (U32)ri->diffuseTex : (U32)ri->vertBuff;

   renderManager->addInst( ri );

}

//-----------------------------------------------------------------------------
// setSizes
//-----------------------------------------------------------------------------
void ParticleEmitter::setSizes( F32 *sizeList )
{
   for( int i=0; i<ParticleData::PDC_NUM_KEYS; i++ )
   {
      sizes[i] = sizeList[i];
   }
}

//-----------------------------------------------------------------------------
// setColors
//-----------------------------------------------------------------------------
void ParticleEmitter::setColors( ColorF *colorList )
{
   for( int i=0; i<ParticleData::PDC_NUM_KEYS; i++ )
   {
      colors[i] = colorList[i];
   }
}

//-----------------------------------------------------------------------------
// deleteWhenEmpty
//-----------------------------------------------------------------------------
void ParticleEmitter::deleteWhenEmpty()
{
   // if the following asserts fire, there is a reasonable chance that you are trying to delete a particle emitter
   // that has already been deleted (possibly by ClientMissionCleanup). If so, use a SimObjectPtr to the emitter and check it
   // for null before calling this function.
   AssertFatal(isProperlyAdded(), "ParticleEmitter must be registed before calling deleteWhenEmpty");
   AssertFatal(!mDead, "ParticleEmitter already deleted");
   AssertFatal(!isDeleted(), "ParticleEmitter already deleted");
   AssertFatal(!isRemoved(), "ParticleEmitter already removed");

   // this check is for non debug case, so that we don't write in to freed memory
   bool okToDelete = !mDead && isProperlyAdded() && !isDeleted() && !isRemoved();
   if (okToDelete)
   {
      mDeleteWhenEmpty = true;
      if( !n_parts )
      {
         // We're already empty, so delete us now.

         mDead = true;
         deleteObject();
      }
      else
         AssertFatal( mSceneManager != NULL, "ParticleEmitter not on process list and won't get ticked to death" );
   }
}

//-----------------------------------------------------------------------------
// emitParticles
//-----------------------------------------------------------------------------
void ParticleEmitter::emitParticles(const Point3F& point,
                                    const bool     useLastPosition,
                                    const Point3F& axis,
                                    const Point3F& velocity,
                                    const U32      numMilliseconds)
{
   if( mDead ) return;

   // lifetime over - no more particles
   if( mLifetimeMS > 0 && mElapsedTimeMS > mLifetimeMS )
   {
      return;
   }

   Point3F realStart;
   if( useLastPosition && mHasLastPosition )
      realStart = mLastPosition;
   else
      realStart = point;

   emitParticles(realStart, point,
                 axis,
                 velocity,
                 numMilliseconds);
}

//-----------------------------------------------------------------------------
// emitParticles
//-----------------------------------------------------------------------------
void ParticleEmitter::emitParticles(const Point3F& start,
                                    const Point3F& end,
                                    const Point3F& axis,
                                    const Point3F& velocity,
                                    const U32      numMilliseconds)
{
   if( mDead ) return;

   // lifetime over - no more particles
   if( mLifetimeMS > 0 && mElapsedTimeMS > mLifetimeMS )
   {
      return;
   }

   U32 currTime = 0;
   bool particlesAdded = false;

   Point3F axisx;
   if( mFabs(axis.z) < 0.9f )
      mCross(axis, Point3F(0, 0, 1), &axisx);
   else
      mCross(axis, Point3F(0, 1, 0), &axisx);
   axisx.normalize();

   if( mNextParticleTime != 0 )
   {
      // Need to handle next particle
      //
      if( mNextParticleTime > numMilliseconds )
      {
         // Defer to next update
         //  (Note that this introduces a potential spatial irregularity if the owning
         //   object is accelerating, and updating at a low frequency)
         //
         mNextParticleTime -= numMilliseconds;
         mInternalClock += numMilliseconds;
         mLastPosition = end;
         mHasLastPosition = true;
         return;
      }
      else
      {
         currTime       += mNextParticleTime;
         mInternalClock += mNextParticleTime;
         // Emit particle at curr time

         // Create particle at the correct position
         Point3F pos;
         pos.interpolate(start, end, F32(currTime) / F32(numMilliseconds));
         addParticle(pos, axis, velocity, axisx);
         particlesAdded = true;
         mNextParticleTime = 0;
      }
   }

   while( currTime < numMilliseconds )
   {
      S32 nextTime = mDataBlock->ejectionPeriodMS;
      if( mDataBlock->periodVarianceMS != 0 )
      {
         nextTime += S32(gRandGen.randI() % (2 * mDataBlock->periodVarianceMS + 1)) -
                     S32(mDataBlock->periodVarianceMS);
      }
      AssertFatal(nextTime > 0, "Error, next particle ejection time must always be greater than 0");

      if( currTime + nextTime > numMilliseconds )
      {
         mNextParticleTime = (currTime + nextTime) - numMilliseconds;
         mInternalClock   += numMilliseconds - currTime;
         AssertFatal(mNextParticleTime > 0, "Error, should not have deferred this particle!");
         break;
      }

      currTime       += nextTime;
      mInternalClock += nextTime;

      // Create particle at the correct position
      Point3F pos;
      pos.interpolate(start, end, F32(currTime) / F32(numMilliseconds));
      addParticle(pos, axis, velocity, axisx);
      particlesAdded = true;

      //   This override-advance code is restored in order to correctly adjust
      //   animated parameters of particles allocated within the same frame
      //   update. Note that ordering is important and this code correctly 
      //   adds particles in the same newest-to-oldest ordering of the link-list.
      //
      // NOTE: We are assuming that the just added particle is at the head of our
      //  list.  If that changes, so must this...
      U32 advanceMS = numMilliseconds - currTime;
      if (mDataBlock->overrideAdvance == false && advanceMS != 0) 
      {
         Particle* last_part = part_list_head.next;
         if (advanceMS > last_part->totalLifetime) 
         {
           part_list_head.next = last_part->next;
           n_parts--;
           last_part->next = part_freelist;
           part_freelist = last_part;
         } 
         else 
         {
            if (advanceMS != 0)
            {
              F32 t = F32(advanceMS) / 1000.0;

              Point3F a = last_part->acc;
              a -= last_part->vel * last_part->dataBlock->dragCoefficient;
              a -= mWindVelocity * last_part->dataBlock->windCoefficient;
              a += Point3F(0.0f, 0.0f, -9.81f) * last_part->dataBlock->gravityCoefficient;

              last_part->vel += a * t;
              last_part->pos += last_part->vel * t;

              updateKeyData( last_part );
            }
         }
      }
   }

   // DMMFIX: Lame and slow...
   if( particlesAdded == true )
      updateBBox();


   if( n_parts > 0 && mSceneManager == NULL )
   {
      gClientSceneGraph->addObjectToScene(this);
      gClientContainer.addObject(this);
      gClientProcessList.addObject(this);
   }

   mLastPosition = end;
   mHasLastPosition = true;
}

//-----------------------------------------------------------------------------
// emitParticles
//-----------------------------------------------------------------------------
void ParticleEmitter::emitParticles(const Point3F& rCenter,
                                    const Point3F& rNormal,
                                    const F32      radius,
                                    const Point3F& velocity,
                                    S32 count)
{
   if( mDead ) return;

   // lifetime over - no more particles
   if( mLifetimeMS > 0 && mElapsedTimeMS > mLifetimeMS )
   {
      return;
   }


   Point3F axisx, axisy;
   Point3F axisz = rNormal;

   if( axisz.isZero() )
   {
      axisz.set( 0.0, 0.0, 1.0 );
   }

   if( mFabs(axisz.z) < 0.98 )
   {
      mCross(axisz, Point3F(0, 0, 1), &axisy);
      axisy.normalize();
   }
   else
   {
      mCross(axisz, Point3F(0, 1, 0), &axisy);
      axisy.normalize();
   }
   mCross(axisz, axisy, &axisx);
   axisx.normalize();

   // Should think of a better way to distribute the
   // particles within the hemisphere.
   for( S32 i = 0; i < count; i++ )
   {
      Point3F pos = axisx * (radius * (1 - (2 * gRandGen.randF())));
      pos        += axisy * (radius * (1 - (2 * gRandGen.randF())));
      pos        += axisz * (radius * gRandGen.randF());

      Point3F axis = pos;
      axis.normalize();
      pos += rCenter;

      addParticle(pos, axis, velocity, axisz);
   }

   // Set world bounding box
   mObjBox.minExtents = rCenter - Point3F(radius, radius, radius);
   mObjBox.maxExtents = rCenter + Point3F(radius, radius, radius);
   resetWorldBox();

   // Make sure we're part of the world
   if( n_parts > 0 && mSceneManager == NULL )
   {
      gClientSceneGraph->addObjectToScene(this);
      gClientContainer.addObject(this);
      gClientProcessList.addObject(this);
   }

   mHasLastPosition = false;
}

//-----------------------------------------------------------------------------
// updateBBox - SLOW, bad news
//-----------------------------------------------------------------------------
void ParticleEmitter::updateBBox()
{
   Point3F minPt(1e10,   1e10,  1e10);
   Point3F maxPt(-1e10, -1e10, -1e10);

   for (Particle* part = part_list_head.next; part != NULL; part = part->next)
   {
      Point3F particleSize(part->size * 0.5f, 0.0f, part->size * 0.5f);
      minPt.setMin( part->pos - particleSize );
      maxPt.setMax( part->pos + particleSize );
   }
   
   mObjBox = Box3F(minPt, maxPt);
   MatrixF temp = getTransform();
   setTransform(temp);

   mBBObjToWorld.identity();
   Point3F boxScale = mObjBox.getExtents();
   boxScale.x = getMax(boxScale.x, 1.0f);
   boxScale.y = getMax(boxScale.y, 1.0f);
   boxScale.z = getMax(boxScale.z, 1.0f);
   mBBObjToWorld.scale(boxScale);
}

//-----------------------------------------------------------------------------
// addParticle
//-----------------------------------------------------------------------------
void ParticleEmitter::addParticle(const Point3F& pos,
                                  const Point3F& axis,
                                  const Point3F& vel,
                                  const Point3F& axisx)
{
   n_parts++;
   if (n_parts > n_part_capacity || n_parts > mDataBlock->partListInitSize)
   {
      // In an emergency we allocate additional particles in blocks of 16.
      // This should happen rarely.
      Particle* store_block = new Particle[16];
      part_store.push_back(store_block);
      n_part_capacity += 16;
      for (S32 i = 0; i < 16; i++)
      {
        store_block[i].next = part_freelist;
        part_freelist = &store_block[i];
      }
      mDataBlock->allocPrimBuffer(n_part_capacity); // allocate larger primitive buffer or will crash 
   }
   Particle* pNew = part_freelist;
   part_freelist = pNew->next;
   pNew->next = part_list_head.next;
   part_list_head.next = pNew;

   Point3F ejectionAxis = axis;
   F32 theta = (mDataBlock->thetaMax - mDataBlock->thetaMin) * gRandGen.randF() +
               mDataBlock->thetaMin;

   F32 ref  = (F32(mInternalClock) / 1000.0) * mDataBlock->phiReferenceVel;
   F32 phi  = ref + gRandGen.randF() * mDataBlock->phiVariance;

   // Both phi and theta are in degs.  Create axis angles out of them, and create the
   //  appropriate rotation matrix...
   AngAxisF thetaRot(axisx, theta * (M_PI / 180.0));
   AngAxisF phiRot(axis,    phi   * (M_PI / 180.0));

   MatrixF temp(true);
   thetaRot.setMatrix(&temp);
   temp.mulP(ejectionAxis);
   phiRot.setMatrix(&temp);
   temp.mulP(ejectionAxis);

   F32 initialVel = mDataBlock->ejectionVelocity;
   initialVel    += (mDataBlock->velocityVariance * 2.0f * gRandGen.randF()) - mDataBlock->velocityVariance;

   pNew->pos = pos + (ejectionAxis * mDataBlock->ejectionOffset);
   pNew->vel = ejectionAxis * initialVel;
   pNew->orientDir = ejectionAxis;
   pNew->acc.set(0, 0, 0);
   pNew->currentAge = 0;

   // Choose a new particle datablack randomly from the list
   U32 dBlockIndex = gRandGen.randI() % mDataBlock->particleDataBlocks.size();
   mDataBlock->particleDataBlocks[dBlockIndex]->initializeParticle(pNew, vel);
   updateKeyData( pNew );

}


//-----------------------------------------------------------------------------
// processTick
//-----------------------------------------------------------------------------
void ParticleEmitter::processTick(const Move*)
{
   if( mDeleteOnTick == true )
   {
      mDead = true;
      deleteObject();
   }
}


//-----------------------------------------------------------------------------
// advanceTime
//-----------------------------------------------------------------------------
void ParticleEmitter::advanceTime(F32 dt)
{
   if( dt < 0.00001 ) return;

   Parent::advanceTime(dt);

   if( dt > 0.5 ) dt = 0.5;

   if( mDead ) return;

   mElapsedTimeMS += (S32)(dt * 1000.0f);

   U32 numMSToUpdate = (U32)(dt * 1000.0f);
   if( numMSToUpdate == 0 ) return;

   // TODO: Prefetch

   // remove dead particles
   Particle* last_part = &part_list_head;
   for (Particle* part = part_list_head.next; part != NULL; part = part->next)
   {
     part->currentAge += numMSToUpdate;
     if (part->currentAge > part->totalLifetime)
     {
       n_parts--;
       last_part->next = part->next;
       part->next = part_freelist;
       part_freelist = part;
       part = last_part;
     }
     else
     {
       last_part = part;
     }
   }

   AssertFatal( n_parts >= 0, "ParticleEmitter: negative part count!" );

   if (n_parts < 1 && mDeleteWhenEmpty)
   {
      mDeleteOnTick = true;
      return;
   }

   if( numMSToUpdate != 0 && n_parts > 0 )
   {
      update( numMSToUpdate );
   }
}

//-----------------------------------------------------------------------------
// Update key related particle data
//-----------------------------------------------------------------------------
void ParticleEmitter::updateKeyData( Particle *part )
{
	//Ensure that our lifetime is never below 0
	if( part->totalLifetime < 1 )
		part->totalLifetime = 1;

   F32 t = F32(part->currentAge) / F32(part->totalLifetime);
   AssertFatal(t <= 1.0f, "Out out bounds filter function for particle.");

   for( U32 i = 1; i < ParticleData::PDC_NUM_KEYS; i++ )
   {
      if( part->dataBlock->times[i] >= t )
      {
         F32 firstPart = t - part->dataBlock->times[i-1];
         F32 total     = part->dataBlock->times[i] -
                         part->dataBlock->times[i-1];

         firstPart /= total;

         if( mDataBlock->useEmitterColors )
         {
            part->color.interpolate(colors[i-1], colors[i], firstPart);
         }
         else
         {
            part->color.interpolate(part->dataBlock->colors[i-1],
                                    part->dataBlock->colors[i],
                                    firstPart);
         }

         if( mDataBlock->useEmitterSizes )
         {
            part->size = (sizes[i-1] * (1.0 - firstPart)) +
                         (sizes[i]   * firstPart);
         }
         else
         {
            part->size = (part->dataBlock->sizes[i-1] * (1.0 - firstPart)) +
                         (part->dataBlock->sizes[i]   * firstPart);
         }
         break;

      }
   }
}

//-----------------------------------------------------------------------------
// Update particles
//-----------------------------------------------------------------------------
void ParticleEmitter::update( U32 ms )
{
   // TODO: Prefetch

   for (Particle* part = part_list_head.next; part != NULL; part = part->next)
   {
      F32 t = F32(ms) / 1000.0;

      Point3F a = part->acc;
      a -= part->vel        * part->dataBlock->dragCoefficient;
      a -= mWindVelocity * part->dataBlock->windCoefficient;
      a += Point3F(0.0f, 0.0f, -9.81f) * part->dataBlock->gravityCoefficient;

      part->vel += a * t;
      part->pos += part->vel * t;

      updateKeyData( part );
   }
}

//-----------------------------------------------------------------------------
// Copy particles to vertex buffer
//-----------------------------------------------------------------------------

// structure used for particle sorting.
struct SortParticle
{
   Particle* p;
   F32       k;
};

// qsort callback function for particle sorting
int QSORT_CALLBACK cmpSortParticles(const void* p1, const void* p2)
{
   const SortParticle* sp1 = (const SortParticle*)p1;
   const SortParticle* sp2 = (const SortParticle*)p2;

   if (sp2->k > sp1->k)
      return 1;
   else if (sp2->k == sp1->k)
      return 0;
   else
      return -1;
}

void ParticleEmitter::copyToVB( const Point3F &camPos, const ColorF &ambientColor )
{
   // TODO: Rewrite this whole thing...


   static Vector<SortParticle> orderedVector(__FILE__, __LINE__);

   PROFILE_START(ParticleEmitter_copyToVB);

   PROFILE_START(ParticleEmitter_copyToVB_Sort);
   // build sorted list of particles (far to near)
   if (mDataBlock->sortParticles)
   {
     orderedVector.clear();

     MatrixF modelview = GFX->getWorldMatrix();
     Point3F viewvec; modelview.getRow(1, &viewvec);

     // add each particle and a distance based sort key to orderedVector
     for (Particle* pp = part_list_head.next; pp != NULL; pp = pp->next)
     {
       orderedVector.increment();
       orderedVector.last().p = pp;
       orderedVector.last().k = mDot(pp->pos, viewvec);
     }

     // qsort the list into far to near ordering
     dQsort(orderedVector.address(), orderedVector.size(), sizeof(SortParticle), cmpSortParticles);
   }
   PROFILE_END();

#if defined(TORQUE_OS_XENON)
   // Allocate writecombined since we don't read back from this buffer (yay!)
   if(mVertBuff.isNull())
      mVertBuff = new GFX360MemVertexBuffer(GFX, 1, getGFXVertexFormat<ParticleVertexType>(), sizeof(ParticleVertexType), GFXBufferTypeDynamic, PAGE_WRITECOMBINE);
   if( n_parts > mCurBuffSize )
   {
      mCurBuffSize = n_parts;
      mVertBuff.resize(n_parts * 4);
   }

   ParticleVertexType *buffPtr = mVertBuff.lock();
#else
   static Vector<ParticleVertexType> tempBuff(2048);
   tempBuff.reserve( n_parts*4 + 64); // make sure tempBuff is big enough
   ParticleVertexType *buffPtr = tempBuff.address(); // use direct pointer (faster)
#endif
   
   if (mDataBlock->orientParticles)
   {
      PROFILE_START(ParticleEmitter_copyToVB_Orient);

      if (mDataBlock->reverseOrder)
      {
        buffPtr += 4*(n_parts-1);
        // do sorted-oriented particles
        if (mDataBlock->sortParticles)
        {
          SortParticle* partPtr = orderedVector.address();
          for (U32 i = 0; i < n_parts; i++, partPtr++, buffPtr-=4 )
             setupOriented(partPtr->p, camPos, ambientColor, buffPtr);
        }
        // do unsorted-oriented particles
        else
        {
          for (Particle* partPtr = part_list_head.next; partPtr != NULL; partPtr = partPtr->next, buffPtr-=4)
             setupOriented(partPtr, camPos, ambientColor, buffPtr);
        }
      }
      else
      {
        // do sorted-oriented particles
        if (mDataBlock->sortParticles)
        {
          SortParticle* partPtr = orderedVector.address();
          for (U32 i = 0; i < n_parts; i++, partPtr++, buffPtr+=4 )
             setupOriented(partPtr->p, camPos, ambientColor, buffPtr);
        }
        // do unsorted-oriented particles
        else
        {
          for (Particle* partPtr = part_list_head.next; partPtr != NULL; partPtr = partPtr->next, buffPtr+=4)
             setupOriented(partPtr, camPos, ambientColor, buffPtr);
        }
      }
	  PROFILE_END();
   }
   else if (mDataBlock->alignParticles)
   {
      PROFILE_START(ParticleEmitter_copyToVB_Aligned);

      if (mDataBlock->reverseOrder)
      {
         buffPtr += 4*(n_parts-1);

         // do sorted-oriented particles
         if (mDataBlock->sortParticles)
         {
            SortParticle* partPtr = orderedVector.address();
            for (U32 i = 0; i < n_parts; i++, partPtr++, buffPtr-=4 )
               setupAligned(partPtr->p, buffPtr);
         }
         // do unsorted-oriented particles
         else
         {
            Particle *partPtr = part_list_head.next;
            for (; partPtr != NULL; partPtr = partPtr->next, buffPtr-=4)
               setupAligned(partPtr, buffPtr);
         }
      }
      else
      {
         // do sorted-oriented particles
         if (mDataBlock->sortParticles)
         {
            SortParticle* partPtr = orderedVector.address();
            for (U32 i = 0; i < n_parts; i++, partPtr++, buffPtr+=4 )
               setupAligned(partPtr->p, buffPtr);
         }
         // do unsorted-oriented particles
         else
         {
            Particle *partPtr = part_list_head.next;
            for (; partPtr != NULL; partPtr = partPtr->next, buffPtr+=4)
               setupAligned(partPtr, buffPtr);
         }
      }
	  PROFILE_END();
   }
   else
   {
      PROFILE_START(ParticleEmitter_copyToVB_NonOriented);
      // somewhat odd ordering so that texture coordinates match the oriented
      // particles
      Point3F basePoints[4];
      basePoints[0] = Point3F(-1.0, 0.0,  1.0);
      basePoints[1] = Point3F(-1.0, 0.0, -1.0);
      basePoints[2] = Point3F( 1.0, 0.0, -1.0);
      basePoints[3] = Point3F( 1.0, 0.0,  1.0);

      MatrixF camView = GFX->getWorldMatrix();
      camView.transpose();  // inverse - this gets the particles facing camera

      if (mDataBlock->reverseOrder)
      {
        buffPtr += 4*(n_parts-1);
        // do sorted-billboard particles
        if (mDataBlock->sortParticles)
        {
          SortParticle *partPtr = orderedVector.address();
          for( U32 i=0; i<n_parts; i++, partPtr++, buffPtr-=4 )
             setupBillboard( partPtr->p, basePoints, camView, ambientColor, buffPtr );
        }
        // do unsorted-billboard particles
        else
        {
          for (Particle* partPtr = part_list_head.next; partPtr != NULL; partPtr = partPtr->next, buffPtr-=4)
             setupBillboard( partPtr, basePoints, camView, ambientColor, buffPtr );
        }
      }
      else
      {
        // do sorted-billboard particles
        if (mDataBlock->sortParticles)
        {
          SortParticle *partPtr = orderedVector.address();
          for( U32 i=0; i<n_parts; i++, partPtr++, buffPtr+=4 )
             setupBillboard( partPtr->p, basePoints, camView, ambientColor, buffPtr );
        }
        // do unsorted-billboard particles
        else
        {
          for (Particle* partPtr = part_list_head.next; partPtr != NULL; partPtr = partPtr->next, buffPtr+=4)
             setupBillboard( partPtr, basePoints, camView, ambientColor, buffPtr );
        }
      }

      PROFILE_END();
   }

#if defined(TORQUE_OS_XENON)
   mVertBuff.unlock();
#else
   PROFILE_START(ParticleEmitter_copyToVB_LockCopy);
   // create new VB if emitter size grows
   if( !mVertBuff || n_parts > mCurBuffSize )
   {
      mCurBuffSize = n_parts;
      mVertBuff.set( GFX, n_parts * 4, GFXBufferTypeDynamic );
   }
   // lock and copy tempBuff to video RAM
   ParticleVertexType *verts = mVertBuff.lock();
   dMemcpy( verts, tempBuff.address(), n_parts * 4 * sizeof(ParticleVertexType) );
   mVertBuff.unlock();
   PROFILE_END();
#endif

   PROFILE_END();
}

//-----------------------------------------------------------------------------
// Set up particle for billboard style render
//-----------------------------------------------------------------------------
void ParticleEmitter::setupBillboard( Particle *part,
                                      Point3F *basePts,
                                      const MatrixF &camView,
                                      const ColorF &ambientColor,
                                      ParticleVertexType *lVerts )
{
   // TODO: rewrite

   const F32 spinFactor = (1.0f/1000.0f) * (1.0f/360.0f) * M_PI_F * 2.0f;

   F32 width     = part->size * 0.5f;
   F32 spinAngle = part->spinSpeed * part->currentAge * spinFactor;

   F32 sy, cy;
   mSinCos(spinAngle, sy, cy);

   ColorF ambColor = ambientColor * mDataBlock->ambientFactor;
   if ( !ambColor.isValidColor() )
   {
      ambColor.set(  mClampF( ambientColor.red * mDataBlock->ambientFactor, 0, 1.0f ), 
                     mClampF( ambientColor.green * mDataBlock->ambientFactor, 0, 1.0f ),
                     mClampF( ambientColor.blue * mDataBlock->ambientFactor, 0, 1.0f ),
                     mClampF( ambientColor.alpha * mDataBlock->ambientFactor, 0, 1.0f ) );
   }

   // fill four verts, use macro and unroll loop
   #define fillVert(){ \
      lVerts->point.x = cy * basePts->x - sy * basePts->z;  \
      lVerts->point.y = 0.0f;                                \
      lVerts->point.z = sy * basePts->x + cy * basePts->z;  \
      camView.mulV( lVerts->point );                        \
      lVerts->point *= width;                               \
      lVerts->point += part->pos;                           \
      lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * ambColor : part->color; }  \

   // Here we deal with UVs for animated particle (billboard)
   if (part->dataBlock->animateTexture)
   { 
     S32 fm = (S32)(part->currentAge*(1.0/1000.0)*part->dataBlock->framesPerSec);
     U8 fm_tile = part->dataBlock->animTexFrames[fm % part->dataBlock->numFrames];
     S32 uv[4];
     uv[0] = fm_tile + fm_tile/part->dataBlock->animTexTiling.x;
     uv[1] = uv[0] + (part->dataBlock->animTexTiling.x + 1);
     uv[2] = uv[1] + 1;
     uv[3] = uv[0] + 1;

     fillVert();
     // Here and below, we copy UVs from particle datablock's current frame's UVs (billboard)
     lVerts->texCoord = part->dataBlock->animTexUVs[uv[0]];
     ++lVerts;
     ++basePts;

     fillVert();
     lVerts->texCoord = part->dataBlock->animTexUVs[uv[1]];
     ++lVerts;
     ++basePts;

     fillVert();
     lVerts->texCoord = part->dataBlock->animTexUVs[uv[2]];
     ++lVerts;
     ++basePts;

     fillVert();
     lVerts->texCoord = part->dataBlock->animTexUVs[uv[3]];
     ++lVerts;
     ++basePts;

     return;
   }

   fillVert();
   // Here and below, we copy UVs from particle datablock's texCoords (billboard)
   lVerts->texCoord = part->dataBlock->texCoords[0];
   ++lVerts;
   ++basePts;

   fillVert();
   lVerts->texCoord = part->dataBlock->texCoords[1];
   ++lVerts;
   ++basePts;

   fillVert();
   lVerts->texCoord = part->dataBlock->texCoords[2];
   ++lVerts;
   ++basePts;

   fillVert();
   lVerts->texCoord = part->dataBlock->texCoords[3];
   ++lVerts;
   ++basePts;
}

//-----------------------------------------------------------------------------
// Set up oriented particle
//-----------------------------------------------------------------------------
void ParticleEmitter::setupOriented( Particle *part,
                                     const Point3F &camPos,
                                     const ColorF &ambientColor,
                                     ParticleVertexType *lVerts )
{
   // TODO: Rewrite

   Point3F dir;

   if( mDataBlock->orientOnVelocity )
   {
      // don't render oriented particle if it has no velocity
      if( part->vel.magnitudeSafe() == 0.0 ) return;
      dir = part->vel;
   }
   else
   {
      dir = part->orientDir;
   }

   Point3F dirFromCam = part->pos - camPos;
   Point3F crossDir;
   mCross( dirFromCam, dir, &crossDir );
   crossDir.normalize();
   dir.normalize();


   F32 width = part->size * 0.5;
   dir *= width;
   crossDir *= width;
   Point3F start = part->pos - dir;
   Point3F end = part->pos + dir;

   // Here we deal with UVs for animated particle (oriented)
   if (part->dataBlock->animateTexture)
   { 
     // Let particle compute the UV indices for current frame
     S32 fm = (S32)(part->currentAge*(1.0/1000.0)*part->dataBlock->framesPerSec);
     U8 fm_tile = part->dataBlock->animTexFrames[fm % part->dataBlock->numFrames];
     S32 uv[4];
     uv[0] = fm_tile + fm_tile/part->dataBlock->animTexTiling.x;
     uv[1] = uv[0] + (part->dataBlock->animTexTiling.x + 1);
     uv[2] = uv[1] + 1;
     uv[3] = uv[0] + 1;

     lVerts->point = start + crossDir;
     lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * (ambientColor * mDataBlock->ambientFactor) : part->color;
     // Here and below, we copy UVs from particle datablock's current frame's UVs (oriented)
     lVerts->texCoord = part->dataBlock->animTexUVs[uv[0]];
     ++lVerts;

     lVerts->point = start - crossDir;
     lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * (ambientColor * mDataBlock->ambientFactor) : part->color;
     lVerts->texCoord = part->dataBlock->animTexUVs[uv[1]];
     ++lVerts;

     lVerts->point = end - crossDir;
     lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * (ambientColor * mDataBlock->ambientFactor) : part->color;
     lVerts->texCoord = part->dataBlock->animTexUVs[uv[2]];
     ++lVerts;

     lVerts->point = end + crossDir;
     lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * (ambientColor * mDataBlock->ambientFactor) : part->color;
     lVerts->texCoord = part->dataBlock->animTexUVs[uv[3]];
     ++lVerts;

     return;
   }

   lVerts->point = start + crossDir;
   lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * (ambientColor * mDataBlock->ambientFactor) : part->color;
   // Here and below, we copy UVs from particle datablock's texCoords (oriented)
   lVerts->texCoord = part->dataBlock->texCoords[0];
   ++lVerts;

   lVerts->point = start - crossDir;
   lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * (ambientColor * mDataBlock->ambientFactor) : part->color;
   lVerts->texCoord = part->dataBlock->texCoords[1];
   ++lVerts;

   lVerts->point = end - crossDir;
   lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * (ambientColor * mDataBlock->ambientFactor) : part->color;
   lVerts->texCoord = part->dataBlock->texCoords[2];
   ++lVerts;

   lVerts->point = end + crossDir;
   lVerts->color = mDataBlock->ambientFactor > 0.0f ? part->color * (ambientColor * mDataBlock->ambientFactor) : part->color;
   lVerts->texCoord = part->dataBlock->texCoords[3];
   ++lVerts;
}

void ParticleEmitter::setupAligned( const Particle *part, ParticleVertexType *lVerts )
{
   // TODO: Rewrite 

   Point3F dir = mDataBlock->alignDirection;

   Point3F cross(0.0f, 1.0f, 0.0f);
   if (mFabs(dir.y) > 0.9f)
      cross.set(0.0f, 0.0f, 1.0f);

   // optimization - this is the same for every particle
   Point3F crossDir;
   mCross(cross, dir, &crossDir);
   crossDir.normalize();
   dir.normalize();

   F32 width = part->size * 0.5f;
   dir *= width;
   crossDir *= width;
   Point3F start = part->pos - dir;
   Point3F end = part->pos + dir;

   // Here we deal with UVs for animated particle (aligned)
   if (part->dataBlock->animateTexture)
   { 
      // Let particle compute the UV indices for current frame
      S32 fm = (S32)(part->currentAge*(1.0f/1000.0f)*part->dataBlock->framesPerSec);
      U8 fm_tile = part->dataBlock->animTexFrames[fm % part->dataBlock->numFrames];
      S32 uv[4];
      uv[0] = fm_tile + fm_tile/part->dataBlock->animTexTiling.x;
      uv[1] = uv[0] + (part->dataBlock->animTexTiling.x + 1);
      uv[2] = uv[1] + 1;
      uv[3] = uv[0] + 1;

      lVerts->point = start + crossDir;
      lVerts->color = part->color;
      // Here and below, we copy UVs from particle datablock's current frame's UVs (aligned)
      lVerts->texCoord = part->dataBlock->animTexUVs[uv[0]];
      ++lVerts;

      lVerts->point = start - crossDir;
      lVerts->color = part->color;
      lVerts->texCoord = part->dataBlock->animTexUVs[uv[1]];
      ++lVerts;

      lVerts->point = end - crossDir;
      lVerts->color = part->color;
      lVerts->texCoord = part->dataBlock->animTexUVs[uv[2]];
      ++lVerts;

      lVerts->point = end + crossDir;
      lVerts->color = part->color;
      lVerts->texCoord = part->dataBlock->animTexUVs[uv[3]];
      ++lVerts;

      return;
   }

   lVerts->point = start + crossDir;
   lVerts->color = part->color;
   // Here and below, we copy UVs from particle datablock's texCoords (aligned)
   lVerts->texCoord = part->dataBlock->texCoords[0];
   ++lVerts;

   lVerts->point = start - crossDir;
   lVerts->color = part->color;
   lVerts->texCoord = part->dataBlock->texCoords[1];
   ++lVerts;

   lVerts->point = end - crossDir;
   lVerts->color = part->color;
   lVerts->texCoord = part->dataBlock->texCoords[2];
   ++lVerts;

   lVerts->point = end + crossDir;
   lVerts->color = part->color;
   lVerts->texCoord = part->dataBlock->texCoords[3];
   ++lVerts;
}

bool ParticleEmitterData::reload()
{
   particleDataBlocks.clear();
   // load the particle datablocks...
   //
   if( particleString != NULL )
   {
      //   particleString is once again a list of particle datablocks so it
      //   must be parsed to extract the particle references.

      // First we parse particleString into a list of particle name tokens 
      Vector<char*> dataBlocks(__FILE__, __LINE__);
      char* tokCopy = new char[dStrlen(particleString) + 1];
      dStrcpy(tokCopy, particleString);

      char* currTok = dStrtok(tokCopy, " \t");
      while (currTok != NULL) 
      {
         dataBlocks.push_back(currTok);
         currTok = dStrtok(NULL, " \t");
      }
      if (dataBlocks.size() == 0) 
      {
         Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) invalid particles string.  No datablocks found", getName());
         delete [] tokCopy;
         return false;
      }    

      // Now we convert the particle name tokens into particle datablocks and IDs 
      particleDataBlocks.clear();
      dataBlockIds.clear();

      for (U32 i = 0; i < dataBlocks.size(); i++) 
      {
         ParticleData* pData = NULL;
         if (Sim::findObject(dataBlocks[i], pData) == false) 
         {
            Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) unable to find particle datablock: %s", getName(), dataBlocks[i]);
         }
         else 
         {
            particleDataBlocks.push_back(pData);
            dataBlockIds.push_back(pData->getId());
         }
      }

      // cleanup
      delete [] tokCopy;

      // check that we actually found some particle datablocks
      if (particleDataBlocks.size() == 0) 
      {
         Con::warnf(ConsoleLogEntry::General, "ParticleEmitterData(%s) unable to find any particle datablocks", getName());
         return false;
      }
   }
   return true;
}


ConsoleMethod(ParticleEmitterData, reload, void, 2, 2, "(void)"
              "Reloads this emitter")
{
   object->reload();
}

