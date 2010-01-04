#include "EffectWrapperDesc.h"
#include "EffectWrapperData.h"
#include "EffectWrapper.h"
#include "../Constraint/Constraint.h"
#include "math/mMath.h"

std::vector< EffectWrapperDesc * >				EffectWrapperDesc::smEffectWrapperDescs;

EffectWrapperDesc::EffectWrapperDesc()
{
	smEffectWrapperDescs.push_back(this);
}

EffectWrapper * EffectWrapperDesc::getEffectWrapper( SimDataBlock * pData, const char * szConstraint ,bool isServer,const U32 & casterSimID,const U32 & targetSimID)
{
	EffectWrapper * pRet = NULL;
	Constraint * pConstraint = NULL;
	for (U32 i = 0 ; i < smEffectWrapperDescs.size() ; ++i)
	{
		if (smEffectWrapperDescs[i]->isMatch(pData))
		{
			if ((isServer && smEffectWrapperDescs[i]->canRunOnServer()) ||
				(!isServer && smEffectWrapperDescs[i]->canRunOnClient()))
			{
				pRet = smEffectWrapperDescs[i]->createEffectWrapper(pData);
				pConstraint = ConstraintDesc::getConstraint(szConstraint);
				pConstraint->setCasterSimID(casterSimID);
				pConstraint->setTargetSimID(targetSimID);
				pConstraint->onCasterAndTargetSetted();
				pRet->ea_setConstraint(pConstraint);
			}
		}
	}
	return pRet;
}

EffectWrapper * EffectWrapperDesc::getEffectWrapper( EffectWrapperData * pData,bool isServer ,const U32 & casterSimID,const U32 & targetSimID)
{
	return getEffectWrapper(pData->getEffectWrapperData(), pData->getConstraintString(), isServer,casterSimID,targetSimID);
}