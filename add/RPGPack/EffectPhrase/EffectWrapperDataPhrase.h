#ifndef __EFFECTDATAPHRASE_H__
#define __EFFECTDATAPHRASE_H__

#include <vector>
#include "math/mMath.h"

class EffectWrapperData;
class EffectWrapperDataPhrase
{
public:
	typedef std::vector< EffectWrapperData * > EWDATALIST;
protected:
	EWDATALIST			_mEffectWrapperDatasVec;
public:
	void												addEffectWrapperData(EffectWrapperData * pData);
	EWDATALIST &							getEffectWrapperDatas(){return _mEffectWrapperDatasVec;}
	U32												getLastingTime();
};

#endif