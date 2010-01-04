#ifndef __RPGSPELLDATA_H__
#define __RPGSPELLDATA_H__

#include "../RPGBaseData.h"
#include "../EffectPhrase/EffectWrapperDataPhrase.h"
#include "console/typeValidators.h"

class RPGSpellData : public RPGBaseData
{
	typedef RPGBaseData  Parent;
	U32												_nCoolDownTime;
	EffectWrapperDataPhrase			_mDataPhrase[PHRASE_MAX];
	EffectWrapperData *					_mDummyPtr;
	bool												_doConvert;
public:
	class ewValidator : public TypeValidator
	{
		U32 _id;
	public:
		ewValidator(U32 id) { _id = id; }
		void validateType(SimObject *object, void *typePtr);
	};

	RPGSpellData();
	void						packData(BitStream*);
	void						unpackData(BitStream*);
	static void			initPersistFields();
	bool						preload(bool server, String &errorStr);
	//===============rpg stuff=================
	//��һ��item���������ܹ����error����0
	//U32������ȴʱ�䣬0��ʾ����ȴʱ�䣬��λMS��-1��ʾ��Զִ��
	virtual U32			onActivate(ERRORS & error , const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	//��һ��item��������ֱ����������ܹ�������򷵻�true�����򷵻�false
	virtual bool			onDeActivate(ERRORS & error ,const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	EffectWrapperDataPhrase * getDataPhrases(){return _mDataPhrase;}
	//��Ӹ����ڵ��Ч��
	void						addEffect(EffectWrapperData * pEffectWrapperData , EFFECTPHRASE phrase);
	//effectlist�Ĵ������
	void						packEffects(BitStream * stream);
	void						unPackEffects(BitStream * stream);
	void						expandEffects();
	//======================================
	DECLARE_CONOBJECT(RPGSpellData);
};
DECLARE_CONSOLETYPE(RPGSpellData);
#endif