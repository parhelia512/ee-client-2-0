#include "RPGEffectron.h"
#include "RPGEffectronData.h"
#include "core/stream/bitStream.h"
#include "T3D/gameConnection.h"

IMPLEMENT_CO_NETOBJECT_V1(RPGEffectron);

RPGEffectron::RPGEffectron()
{
	//²»¹ã²¥
	mNetFlags.clear(Ghostable);
#ifdef CLIENT_ONLY_CODE
	mNetFlags = IsGhost;
#endif
	mDataBlock = NULL;
	_mTimePassed = 0;
}

RPGEffectron::~RPGEffectron()
{

}

bool RPGEffectron::onNewDataBlock( GameBaseData* dptr )
{
	mDataBlock = dynamic_cast<RPGEffectronData*>(dptr);
	if (!mDataBlock || !Parent::onNewDataBlock(dptr))
		return false;

	return true;
}

void RPGEffectron::processTick( const Move* m )
{	
	Parent::processTick(m);

	if (isClientObject())
		return;

	_processServer();
	_mTimePassed += TickMs;
}

void RPGEffectron::advanceTime( F32 dt )
{
	Parent::advanceTime(dt);
	AssertFatal(isClientObject(),"RPGEffectron::advanceTime");
	U32 ndt = dt * 1000;

	_processClient(ndt);
	_mTimePassed += ndt;
}

bool RPGEffectron::onAdd()
{
	if (!Parent::onAdd()) 
		return(false);

	_prepareFhrasese();
	_mPhrases.phraseStart(mDataBlock->getDataPhrases(),isServerObject(),0,0);

	return(true);
}

void RPGEffectron::onRemove()
{
	_clearPhrases();
	Parent::onRemove();
	Con::printf("RPGEffectron::onRemove()");
}

U32 RPGEffectron::packUpdate( NetConnection* con, U32 mask, BitStream* stream)
{
	U32 retMask = Parent::packUpdate(con, mask, stream);

	return(retMask);
}

void RPGEffectron::unpackUpdate( NetConnection* con, BitStream* stream )
{
	Parent::unpackUpdate(con, stream);
}

void RPGEffectron::initPersistFields()
{
	Parent::initPersistFields();
}

void RPGEffectron::interpolateTick( F32 delta )
{
	Parent::interpolateTick(delta);
}

void RPGEffectron::_processServer()
{
	const U32 & castTime = _mPhrases.getLastingTime();
	if (_mTimePassed >= castTime)
	{
		Con::printf("server === RPGEffectron->safeDeleteObject()");
		this->safeDeleteObject();
	}
	else
		_mPhrases.phraseUpdate(TickMs);
}

void RPGEffectron::_processClient(U32 dt)
{
	const U32 & castTime = _mPhrases.getLastingTime();
	if (_mTimePassed >= castTime)
	{
		Con::printf("client === RPGEffectron->safeDeleteObject()");
		this->safeDeleteObject();
	}
	else
		_mPhrases.phraseUpdate(dt);
}

void RPGEffectron::_clearPhrases()
{
	_mPhrases.phraseEnd();
}

void RPGEffectron::_prepareFhrasese()
{
	_mPhrases.phraseInit(mDataBlock->getDataPhrases());
}