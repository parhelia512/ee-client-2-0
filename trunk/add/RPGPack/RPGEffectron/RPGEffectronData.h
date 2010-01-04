#ifndef __RPGEFFECTRONDATA_H__
#define __RPGEFFECTRONDATA_H__

#include "../RPGBaseData.h"
#include "../EffectPhrase/EffectWrapperDataPhrase.h"
#include "console/typeValidators.h"

class RPGEffectronData : public RPGBaseData
{
	typedef RPGBaseData  Parent;
	EffectWrapperDataPhrase			_mDataPhrase;
	EffectWrapperData *					_mDummyPtr;
	bool												_doConvert;
public:
	class ewValidator : public TypeValidator
	{
	public:
		void validateType(SimObject *object, void *typePtr);
	};

	RPGEffectronData();
	void						packData(BitStream*);
	void						unpackData(BitStream*);
	static void			initPersistFields();
	bool						preload(bool server, String &errorStr);
	//===============rpg stuff=================
	//�������Ч��
	virtual U32			onActivate(ERRORS & error , const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	//�������Ч��
	virtual bool			onDeActivate(ERRORS & error ,const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	EffectWrapperDataPhrase * getDataPhrases(){return &_mDataPhrase;}
	//���Ч��
	void						addEffect(EffectWrapperData * pEffectWrapperData);
	//effectlist�Ĵ������
	void						packEffects(BitStream * stream);
	void						unPackEffects(BitStream * stream);
	void						expandEffects();
	//======================================
	DECLARE_CONOBJECT(RPGEffectronData);
};
DECLARE_CONSOLETYPE(RPGEffectronData);
#endif