#ifndef __EFFECTWRAPPERDATA_H__
#define __EFFECTWRAPPERDATA_H__

#include "T3D/gameBase.h"

class EffectWrapperData : public GameBaseData
{
	typedef GameBaseData  Parent;
	U32									_nLastTime;
	SimDataBlock *				_effect;
	StringTableEntry				_szConstraint;
	bool									_doConvert;
public:
	EffectWrapperData();
	void						packData(BitStream*);
	void						unpackData(BitStream*);
	bool						preload(bool server, String &errorStr);
	static void			initPersistFields();
//=============rpg stuff==========
	const U32 &			getLastingTime();
	SimDataBlock *	getEffectWrapperData();
	StringTableEntry	getConstraintString();
//=============================
	DECLARE_CONOBJECT(EffectWrapperData);
};
DECLARE_CONSOLETYPE(EffectWrapperData);
#endif