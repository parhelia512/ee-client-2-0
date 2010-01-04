#include "RPGBase.h"
#include "RPGBaseData.h"
#include "core/stream/bitStream.h"
#include "T3D/gameConnection.h"

IMPLEMENT_CO_NETOBJECT_V1(RPGBase);

RPGBase::RPGBase()
{
	mDataBlock = NULL;
	mCasterSimID = mTargetSimID = 0;
}

RPGBase::~RPGBase()
{

}

bool RPGBase::onNewDataBlock( GameBaseData* dptr )
{
	mDataBlock = dynamic_cast<RPGBaseData*>(dptr);
	if (!mDataBlock || !Parent::onNewDataBlock(dptr))
		return false;

	return true;
}

void RPGBase::processTick( const Move* m )
{	
	Parent::processTick(m);
}

void RPGBase::advanceTime( F32 dt )
{
	Parent::advanceTime(dt);
}

bool RPGBase::onAdd()
{
	if (!Parent::onAdd()) 
		return(false);

	addToScene();

	GameBase * pCaster = dynamic_cast<GameBase *>( Sim::findObject(mCasterSimID) );
	if (pCaster)
	{
		pCaster->pushRPGBase(this);
	}

	return(true);
}

void RPGBase::onRemove()
{
	GameBase * pCaster = dynamic_cast<GameBase *>( Sim::findObject(mCasterSimID) );
	if (pCaster)
	{
		pCaster->removeRPGBase(this);
	}
	removeFromScene();
	Parent::onRemove();
}

U32 RPGBase::packUpdate( NetConnection* con, U32 mask, BitStream* stream)
{
	U32 retMask = Parent::packUpdate(con, mask, stream);
	if (stream->writeFlag(mask & InitialUpdateMask))
	{
		NetObject* pObject;
		if (stream->writeFlag(Sim::findObject( getCasterSimID(), pObject)))
		{
			stream->write(con->getGhostIndex(pObject));
		}
		if (stream->writeFlag(Sim::findObject( getTargetSimID(), pObject)))
		{
			stream->write(con->getGhostIndex(pObject));
		}
	}
	return(retMask);
}

void RPGBase::unpackUpdate( NetConnection* con, BitStream* stream )
{
	Parent::unpackUpdate(con, stream);

	if (stream->readFlag())
	{
		S32 nIndex;
		if (stream->readFlag())
		{
			stream->read(&nIndex);
			NetObject* pObject = NULL;
			if( con->isGhostingTo())
				pObject = con->resolveGhost(nIndex);
			else if( con->isGhostingFrom())
				pObject = con->resolveObjectFromGhostIndex(nIndex);
			if (pObject)
			{
				setCasterSimID(pObject->getId());
			}
		}
		if (stream->readFlag())
		{
			stream->read(&nIndex);
			NetObject* pObject = NULL;
			if( con->isGhostingTo())
				pObject = con->resolveGhost(nIndex);
			else if( con->isGhostingFrom())
				pObject = con->resolveObjectFromGhostIndex(nIndex);
			if (pObject)
			{
				setCasterSimID(pObject->getId());
			}
		}
	}
}

void RPGBase::initPersistFields()
{
	Parent::initPersistFields();
}

const U32 & RPGBase::getCasterSimID()
{
	return mCasterSimID;
}

const U32 & RPGBase::getTargetSimID()
{
	return mTargetSimID;
}

void RPGBase::setCasterSimID( const U32 & id )
{
	mCasterSimID = id;
}

void RPGBase::setTargetSimID( const U32 & id )
{
	mTargetSimID = id;
}

void RPGBase::onInterrupt()
{

}

void RPGBase::interpolateTick( F32 delta )
{
	Parent::interpolateTick(delta);
}