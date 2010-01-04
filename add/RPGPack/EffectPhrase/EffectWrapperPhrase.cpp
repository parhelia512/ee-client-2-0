#include "EffectWrapperPhrase.h"
#include "EffectWrapperDataPhrase.h"
#include "../EffectWrapper/EffectWrapperDesc.h"
#include "../EffectWrapper/EffectWrapper.h"

void EffectWrapperPhrase::_addEffectWrapper( EffectWrapper * pEW )
{
	_mEffectWrappers.push_back(pEW);
}

void EffectWrapperPhrase::constructEWList(EffectWrapperDataPhrase * EWDList , bool isServer,const U32 & casterSimID,const U32 & targetSimID )
{
	_mEWDList = EWDList;
	EffectWrapper * ew = NULL;
	EffectWrapperDataPhrase::EWDATALIST & EWDS = EWDList->getEffectWrapperDatas();
	for (U32 i = 0 ; i < EWDS.size() ; ++i)
	{
		ew = EffectWrapperDesc::getEffectWrapper(EWDS[i],isServer,casterSimID,targetSimID);
		if (ew)
		{
			_addEffectWrapper(ew);
		}
	}
}

void EffectWrapperPhrase::phraseStart(EffectWrapperDataPhrase * EWDList , bool isServer,const U32 & casterSimID,const U32 & targetSimID)
{
	constructEWList(EWDList,isServer,casterSimID,targetSimID);
	EffectWrapper * ew = NULL;
	ERRORS error;
	for (U32 i = 0 ; i < _mEffectWrappers.size() ; ++i)
	{
		ew = _mEffectWrappers[i];
		if (ew)
		{
			ew->ea_Start(error);
		}
	}
}

void EffectWrapperPhrase::phraseUpdate(const U32 & dt )
{
	EffectWrapper * ew = NULL;
	ERRORS error;
	for (U32 i = 0 ; i < _mEffectWrappers.size() ; ++i)
	{
		ew = _mEffectWrappers[i];
		if (ew)
		{
			ew->ea_Update(dt);
		}
	}
}

void EffectWrapperPhrase::phraseEnd()
{
	EffectWrapper * ew = NULL;
	ERRORS error;
	for (U32 i = 0 ; i < _mEffectWrappers.size() ; ++i)
	{
		ew = _mEffectWrappers[i];
		if (ew)
		{
			ew->ea_End();
		}
	}
	destructEWList();
}

void EffectWrapperPhrase::destructEWList()
{
	EffectWrapper * ew = NULL;
	for (U32 i = 0 ; i < _mEffectWrappers.size() ; ++i)
	{
		ew = _mEffectWrappers[i];
		if (ew)
		{
			delete ew;
		}
	}
	_mEffectWrappers.clear();
}

const U32 & EffectWrapperPhrase::getLastingTime()
{
	return _nLastTime;
}

void EffectWrapperPhrase::setLastingTime( const U32 & time )
{
	_nLastTime = time;
}

EffectWrapperPhrase::EffectWrapperPhrase():
_nLastTime(0)
{
}

void EffectWrapperPhrase::phraseInit(EffectWrapperDataPhrase * EWDList )
{
	setLastingTime(EWDList->getLastingTime());
}