#include "RPGSpell.h"
#include "RPGSpellData.h"
#include "core/stream/bitStream.h"
#include "T3D/gameConnection.h"

IMPLEMENT_CO_NETOBJECT_V1(RPGSpell);

RPGSpell::RPGSpell()
{
	mDataBlock = NULL;
	_mPhrase = PHRASE_CAST;
	_mTimePassed = 0;
}

RPGSpell::~RPGSpell()
{

}

bool RPGSpell::onNewDataBlock( GameBaseData* dptr )
{
	mDataBlock = dynamic_cast<RPGSpellData*>(dptr);
	if (!mDataBlock || !Parent::onNewDataBlock(dptr))
		return false;

	return true;
}

void RPGSpell::processTick( const Move* m )
{	
	Parent::processTick(m);

	if (isClientObject())
		return;

	_processServer();
	_mTimePassed += TickMs;
}

void RPGSpell::advanceTime( F32 dt )
{
	Parent::advanceTime(dt);
	AssertFatal(isClientObject(),"RPGSpell::advanceTime");
	//client don't delete by self , this will be deleted by ghost
	if (PHRASE_MAX == _mPhrase)
	{
		return;
	}
	U32 ndt = dt * 1000;
	_processClient(ndt);
	_mTimePassed += ndt;
}

bool RPGSpell::onAdd()
{
	if (!Parent::onAdd()) 
		return(false);

	_prepareFhrasese();

	if (isServerObject())
	{
		Con::printf("servser === RPGSpell::onAdd");
		_enter_cast_s();
	}
	else
	{
		Con::printf("client === RPGSpell::onAdd");
		_enter_cast_c();
	}

	return(true);
}

void RPGSpell::onRemove()
{
	_clearPhrases();
	Parent::onRemove();
	Con::printf("RPGSpell::onRemove()");
}

U32 RPGSpell::packUpdate( NetConnection* con, U32 mask, BitStream* stream)
{
	U32 retMask = Parent::packUpdate(con, mask, stream);

	return(retMask);
}

void RPGSpell::unpackUpdate( NetConnection* con, BitStream* stream )
{
	Parent::unpackUpdate(con, stream);
}

void RPGSpell::initPersistFields()
{
	Parent::initPersistFields();
}

void RPGSpell::interpolateTick( F32 delta )
{
	Parent::interpolateTick(delta);
}

void RPGSpell::_processServer()
{
	const U32 & castTime = _mPhrases[PHRASE_CAST].getLastingTime();
	const U32 & launchTime = _mPhrases[PHRASE_LAUNCH].getLastingTime();
	const U32 & impactTime = _mPhrases[PHRASE_IMPACT].getLastingTime();
	const U32 & residueTime = _mPhrases[PHRASE_RESIDUE].getLastingTime();
	EFFECTPHRASE oldPhrase = _mPhrase;
	switch(_mPhrase)
	{
	case PHRASE_MAX:
		Con::printf("server === this->safeDeleteObject()");
		this->safeDeleteObject();
		break;
	case PHRASE_CAST:
		if (_mTimePassed >= castTime)
		{
			_leave_cast_s();
			_enter_launch_s();
		}
		break;
	case PHRASE_LAUNCH:
		if (_mTimePassed >= castTime + launchTime)
		{
			_leave_launch_s();
			_enter_impact_s();
		}
		break;
	case PHRASE_IMPACT:
		if (_mTimePassed >= castTime + launchTime + impactTime)
		{
			_leave_impact_s();
			_enter_residue_s();
		}
		break;
	case PHRASE_RESIDUE:
		if (_mTimePassed >= castTime + launchTime + impactTime + residueTime)
		{
			_leave_residue_s();
		}
		break;
	}
	if (oldPhrase == _mPhrase && PHRASE_MAX != _mPhrase)
		_mPhrases[_mPhrase].phraseUpdate(TickMs);
}

void RPGSpell::_processClient(U32 dt)
{
	const U32 & castTime = _mPhrases[PHRASE_CAST].getLastingTime();
	const U32 & launchTime = _mPhrases[PHRASE_LAUNCH].getLastingTime();
	const U32 & impactTime = _mPhrases[PHRASE_IMPACT].getLastingTime();
	const U32 & residueTime = _mPhrases[PHRASE_RESIDUE].getLastingTime();
	EFFECTPHRASE oldPhrase = _mPhrase;
	switch(_mPhrase)
	{
	case PHRASE_MAX:
		Con::printf("client === this->safeDeleteObject()");
		this->safeDeleteObject();
		break;
	case PHRASE_CAST:
		if (_mTimePassed >= castTime)
		{
			_leave_cast_c();
			_enter_launch_c();
		}
		break;
	case PHRASE_LAUNCH:
		if (_mTimePassed >= castTime + launchTime)
		{
			_leave_launch_c();
			_enter_impact_c();
		}
		break;
	case PHRASE_IMPACT:
		if (_mTimePassed >= castTime + launchTime + impactTime)
		{
			_leave_impact_c();
			_enter_residue_c();
		}
		break;
	case PHRASE_RESIDUE:
		if (_mTimePassed >= castTime + launchTime + impactTime + residueTime)
		{
			_leave_residue_c();
		}
		break;
	}
	if (oldPhrase == _mPhrase && PHRASE_MAX != _mPhrase)
		_mPhrases[_mPhrase].phraseUpdate(dt);
}

void RPGSpell::_enter_cast_s()
{
	_enter_cast_c();
}

void RPGSpell::_leave_cast_s()
{
	_leave_cast_c();
}

void RPGSpell::_enter_cast_c()
{
	_mPhrase = PHRASE_CAST;
	_mTimePassed = 0;
	_mPhrases[_mPhrase].phraseStart(mDataBlock->getDataPhrases() + _mPhrase,isServerObject(),getCasterSimID(),getTargetSimID());
}

void RPGSpell::_leave_cast_c()
{
	_mPhrases[PHRASE_CAST].phraseEnd();
}

void RPGSpell::_enter_launch_s()
{
	_enter_launch_c();
}

void RPGSpell::_leave_launch_s()
{
	_leave_launch_c();
}

void RPGSpell::_enter_launch_c()
{
	_mPhrase = PHRASE_LAUNCH;
	_mPhrases[_mPhrase].phraseStart(mDataBlock->getDataPhrases() + _mPhrase,isServerObject(),getCasterSimID(),getTargetSimID());
}

void RPGSpell::_leave_launch_c()
{
	_mPhrases[PHRASE_LAUNCH].phraseEnd();
}

void RPGSpell::_enter_impact_s()
{
	_enter_impact_c();
}

void RPGSpell::_leave_impact_s()
{
	_leave_impact_c();
}

void RPGSpell::_enter_impact_c()
{
	_mPhrase = PHRASE_IMPACT;
	_mPhrases[_mPhrase].phraseStart(mDataBlock->getDataPhrases() + _mPhrase,isServerObject(),getCasterSimID(),getTargetSimID());
}

void RPGSpell::_leave_impact_c()
{
	_mPhrases[PHRASE_IMPACT].phraseEnd();
}

void RPGSpell::_enter_residue_s()
{
	_enter_residue_c();
}

void RPGSpell::_leave_residue_s()
{
	_leave_residue_c();
}

void RPGSpell::_enter_residue_c()
{
	_mPhrase = PHRASE_RESIDUE;
	_mPhrases[_mPhrase].phraseStart(mDataBlock->getDataPhrases() + _mPhrase,isServerObject(),getCasterSimID(),getTargetSimID());
}

void RPGSpell::_leave_residue_c()
{
	_mPhrases[PHRASE_RESIDUE].phraseEnd();
	_mPhrase = PHRASE_MAX;
}

void RPGSpell::_prepareFhrasese()
{
	for (U8 i = PHRASE_CAST ; i < PHRASE_MAX ; ++i)
	{
		_mPhrases[i].phraseInit(mDataBlock->getDataPhrases() + i);
	}
}

void RPGSpell::_clearPhrases()
{
	if (isServerObject())
	{
		_leave_cast_s();
		_leave_launch_s();
		_leave_impact_s();
		_leave_residue_s();
	}
	else
	{
		_leave_cast_c();
		_leave_launch_c();
		_leave_impact_c();
		_leave_residue_c();
	}
}