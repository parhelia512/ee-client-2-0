#ifndef __EFFECTWRAPPERPHRASE_H__
#define __EFFECTWRAPPERPHRASE_H__

#include <vector>
#include "math/mMath.h"
#include "../RPGDefs.h"

class EffectWrapper;
class EffectWrapperDataPhrase;

class EffectWrapperPhrase : public RPGDefs
{
public:
	typedef 	std::vector< EffectWrapper * > EWLIST;
protected:
	U32													_nLastTime;
	EWLIST												_mEffectWrappers;
	const EffectWrapperDataPhrase *	_mEWDList;
	void													_addEffectWrapper(EffectWrapper * pEW);
	void					constructEWList(EffectWrapperDataPhrase * EWDList , bool isServer,const U32 & casterSimID,const U32 & targetSimID);
	void					destructEWList();
public:
	EffectWrapperPhrase();
	void					phraseInit(EffectWrapperDataPhrase * EWDList );
	void					phraseStart(EffectWrapperDataPhrase * EWDList , bool isServer,const U32 & casterSimID,const U32 & targetSimID);
	void					phraseUpdate(const U32 & dt);
	void					phraseEnd();
	const U32 &		getLastingTime();
	void					setLastingTime(const U32 & time);
};

#endif