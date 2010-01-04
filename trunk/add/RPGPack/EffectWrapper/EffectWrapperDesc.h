#ifndef __EFFECTWD_H__
#define __EFFECTWD_H__

#include <vector>
#include "../RPGDefs.h"
#include "math/mMath.h"

class EffectWrapper;
class SimDataBlock;
class EffectWrapperData;

#define IMPLEMENT_EFXDESC(T) T g_##T

class EffectWrapperDesc : public RPGDefs
{
public:
	EffectWrapperDesc();
	static std::vector< EffectWrapperDesc * >				smEffectWrapperDescs;
	static EffectWrapper *												getEffectWrapper(SimDataBlock * pData,const char * szConstraint,bool isServer,const U32 & casterSimID,const U32 & targetSimID);
	static EffectWrapper *												getEffectWrapper(EffectWrapperData * pData,bool isServer,const U32 & casterSimID,const U32 & targetSimID);

	virtual bool																canRunOnServer() = 0;
	virtual bool																canRunOnClient() = 0;
	virtual bool																isMatch( SimDataBlock * pData) = 0;
	virtual EffectWrapper *											createEffectWrapper(SimDataBlock * pData) = 0;
	virtual EFFECTRUN													getEffectRunsOn() = 0;
};

#endif