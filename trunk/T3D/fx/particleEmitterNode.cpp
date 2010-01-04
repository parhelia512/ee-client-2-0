//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "particleEmitterNode.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "T3D/fx/particleEmitter.h"
#include "math/mathIO.h"

IMPLEMENT_CO_DATABLOCK_V1(ParticleEmitterNodeData);
IMPLEMENT_CO_NETOBJECT_V1(ParticleEmitterNode);

//-----------------------------------------------------------------------------
// ParticleEmitterNodeData
//-----------------------------------------------------------------------------
ParticleEmitterNodeData::ParticleEmitterNodeData()
{
   timeMultiple = 1.0;
}

ParticleEmitterNodeData::~ParticleEmitterNodeData()
{

}

//-----------------------------------------------------------------------------
// initPersistFields
//-----------------------------------------------------------------------------
void ParticleEmitterNodeData::initPersistFields()
{
   addField("timeMultiple", TypeF32, Offset(timeMultiple, ParticleEmitterNodeData));

   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------
// onAdd
//-----------------------------------------------------------------------------
bool ParticleEmitterNodeData::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   if( timeMultiple < 0.01 || timeMultiple > 100 )
   {
      Con::warnf("ParticleEmitterNodeData::onAdd(%s): timeMultiple must be between 0.01 and 100", getName());
      timeMultiple = timeMultiple < 0.01 ? 0.01 : 100;
   }

   return true;
}


//-----------------------------------------------------------------------------
// preload
//-----------------------------------------------------------------------------
bool ParticleEmitterNodeData::preload(bool server, String &errorStr)
{
   if( Parent::preload(server, errorStr) == false )
      return false;

   return true;
}


//-----------------------------------------------------------------------------
// packData
//-----------------------------------------------------------------------------
void ParticleEmitterNodeData::packData(BitStream* stream)
{
   Parent::packData(stream);

   stream->write(timeMultiple);
}

//-----------------------------------------------------------------------------
// unpackData
//-----------------------------------------------------------------------------
void ParticleEmitterNodeData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

   stream->read(&timeMultiple);
}


//-----------------------------------------------------------------------------
// ParticleEmitterNode
//-----------------------------------------------------------------------------
ParticleEmitterNode::ParticleEmitterNode()
{
   // Todo: ScopeAlways?
   mNetFlags.set(Ghostable);
   mTypeMask |= EnvironmentObjectType;

   mActive = true;

   mDataBlock          = NULL;
   mEmitterDatablock   = NULL;
   mEmitterDatablockId = 0;
   mEmitter            = NULL;
   mVelocity           = 1.0;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
ParticleEmitterNode::~ParticleEmitterNode()
{
   //
}

//-----------------------------------------------------------------------------
// initPersistFields
//-----------------------------------------------------------------------------
void ParticleEmitterNode::initPersistFields()
{
   addField("active",   TypeBool,                   Offset(mActive,           ParticleEmitterNode));
   addField("emitter",  TypeParticleEmitterDataPtr, Offset(mEmitterDatablock, ParticleEmitterNode));
   addField("velocity", TypeF32,                    Offset(mVelocity,         ParticleEmitterNode));
   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------
// onAdd
//-----------------------------------------------------------------------------
bool ParticleEmitterNode::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   if( !mEmitterDatablock && mEmitterDatablockId != 0 )
   {
      if( Sim::findObject(mEmitterDatablockId, mEmitterDatablock) == false )
         Con::errorf(ConsoleLogEntry::General, "ParticleEmitterNode::onAdd: Invalid packet, bad datablockId(mEmitterDatablock): %d", mEmitterDatablockId);
   }

   if( mEmitterDatablock == NULL )
      return false;

   if( isClientObject() )
   {
      ParticleEmitter* pEmitter = new ParticleEmitter;
      pEmitter->onNewDataBlock(mEmitterDatablock);
      if( pEmitter->registerObject() == false )
      {
         Con::warnf(ConsoleLogEntry::General, "Could not register base emitter for particle of class: %s", mEmitterDatablock->getName());
         delete pEmitter;
         return false;
      }
      mEmitter = pEmitter;
   }

   mObjBox.minExtents.set(-0.5, -0.5, -0.5);
   mObjBox.maxExtents.set( 0.5,  0.5,  0.5);
   resetWorldBox();
   addToScene();

   return true;
}

//-----------------------------------------------------------------------------
// onRemove
//-----------------------------------------------------------------------------
void ParticleEmitterNode::onRemove()
{
   removeFromScene();
   if( isClientObject() )
   {
      if( mEmitter )
      {
         mEmitter->deleteWhenEmpty();
         mEmitter = NULL;
      }
   }

   Parent::onRemove();
}

//-----------------------------------------------------------------------------
// onNewDataBlock
//-----------------------------------------------------------------------------
bool ParticleEmitterNode::onNewDataBlock(GameBaseData* dptr)
{
   mDataBlock = dynamic_cast<ParticleEmitterNodeData*>(dptr);
   if( !mDataBlock || !Parent::onNewDataBlock(dptr) )
      return false;

   // Todo: Uncomment if this is a "leaf" class
   scriptOnNewDataBlock();
   return true;
}

//-----------------------------------------------------------------------------
// advanceTime
//-----------------------------------------------------------------------------
void ParticleEmitterNode::processTick(const Move* move)
{
   Parent::processTick(move);

   if ( isMounted() )
   {
      MatrixF mat;
      mMount.object->getRenderMountTransform( mMount.node, &mat );
      setTransform( mat );
   }
}

void ParticleEmitterNode::advanceTime(F32 dt)
{
   Parent::advanceTime(dt);
   
   if(!mActive || mEmitter.isNull() || !mDataBlock)
      return;

   Point3F emitPoint, emitVelocity;
   Point3F emitAxis(0, 0, 1);
   getTransform().mulV(emitAxis);
   getTransform().getColumn(3, &emitPoint);
   emitVelocity = emitAxis * mVelocity;

   mEmitter->emitParticles(emitPoint, emitPoint,
                           emitAxis,
                           emitVelocity, (U32)(dt * mDataBlock->timeMultiple * 1000.0f));
}

//-----------------------------------------------------------------------------
// packUpdate
//-----------------------------------------------------------------------------
U32 ParticleEmitterNode::packUpdate(NetConnection* con, U32 mask, BitStream* stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   if ( stream->writeFlag( mask & InitialUpdateMask ) )
   {
      mathWrite(*stream, getTransform());
	  mathWrite(*stream, getScale());
	  if( stream->writeFlag(mEmitterDatablock != NULL) )
	  {
		 stream->writeRangedU32(mEmitterDatablock->getId(), DataBlockObjectIdFirst,
								 DataBlockObjectIdLast);
	  }
   }

   if ( stream->writeFlag( mask & StateMask ) )
   {
      stream->writeFlag( mActive );
   }

   return retMask;
}

//-----------------------------------------------------------------------------
// unpackUpdate
//-----------------------------------------------------------------------------
void ParticleEmitterNode::unpackUpdate(NetConnection* con, BitStream* stream)
{
   Parent::unpackUpdate(con, stream);

   if ( stream->readFlag() )
   {
      MatrixF temp;
      Point3F tempScale;
      mathRead(*stream, &temp);
      mathRead(*stream, &tempScale);

      if( stream->readFlag() )
      {
         mEmitterDatablockId = stream->readRangedU32(DataBlockObjectIdFirst,
                                                     DataBlockObjectIdLast);
      }
      else
      {
         mEmitterDatablockId = 0;
      }

      setScale(tempScale);
      setTransform(temp);
   }

   if ( stream->readFlag() )
   {
      mActive = stream->readFlag();
   }
}

void ParticleEmitterNode::setEmitterDataBlock(ParticleEmitterData* data)
{
   if (!data)
      return;

   if (mEmitter)
   {
      mEmitterDatablock = data;
      ParticleEmitter* pEmitter = new ParticleEmitter;
      pEmitter->onNewDataBlock(mEmitterDatablock);
      if (pEmitter->registerObject() == false) {
         Con::warnf(ConsoleLogEntry::General, "Could not register base emitter for particle of class: %s", mEmitterDatablock->getName());
         delete pEmitter;
         pEmitter = NULL;
      }
      if (pEmitter)
      {
         mEmitter->deleteWhenEmpty();
         mEmitter = pEmitter;
      }
   }
}

ConsoleMethod(ParticleEmitterNode, setEmitterDataBlock, void, 3, 3, "(data)")
{
   ParticleEmitterData* data = dynamic_cast<ParticleEmitterData*>(Sim::findObject(dAtoi(argv[2])));
   if (!data)
      data = dynamic_cast<ParticleEmitterData*>(Sim::findObject(argv[2]));

   if (data)
      object->setEmitterDataBlock(data);
}

