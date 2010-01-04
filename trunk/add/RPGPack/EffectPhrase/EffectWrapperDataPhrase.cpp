#include "EffectWrapperDataPhrase.h"
#include "../EffectWrapper/EffectWrapperData.h"

void EffectWrapperDataPhrase::addEffectWrapperData( EffectWrapperData * pData )
{
	_mEffectWrapperDatasVec.push_back(pData);
}

U32 EffectWrapperDataPhrase::getLastingTime()
{
	U32 maxTime = 0;
	U32 temp;
	for (S32 i =  0 ; i < _mEffectWrapperDatasVec.size() ; ++i)
	{
		temp = _mEffectWrapperDatasVec[i]->getLastingTime();
		maxTime = maxTime > temp ? maxTime : temp;
	}
	return maxTime;
}