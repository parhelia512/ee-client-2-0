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
	//当一个item被激活，如果能够激活，error返回0
	//U32返回冷却时间，0表示无冷却时间，单位MS，-1表示永远执行
	virtual U32			onActivate(ERRORS & error , const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	//当一个item被激活后，又被反激活，如果能够反激活，则返回true，否则返回false
	virtual bool			onDeActivate(ERRORS & error ,const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	EffectWrapperDataPhrase * getDataPhrases(){return _mDataPhrase;}
	//添加各个节点的效果
	void						addEffect(EffectWrapperData * pEffectWrapperData , EFFECTPHRASE phrase);
	//effectlist的打包与解包
	void						packEffects(BitStream * stream);
	void						unPackEffects(BitStream * stream);
	void						expandEffects();
	//======================================
	DECLARE_CONOBJECT(RPGSpellData);
};
DECLARE_CONSOLETYPE(RPGSpellData);
#endif