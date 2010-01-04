#ifndef __RPGSPELL_H__
#define __RPGSPELL_H__

#include "../RPGBase.h"
#include "../EffectPhrase/EffectWrapperPhrase.h"

class RPGSpellData;

class RPGSpell : public RPGBase , public RPGDefs
{
	typedef RPGBase        Parent;
public:
	enum MaskBits 
	{
		SpellPhraseMask  = Parent::NextFreeMask << 0,
		NextFreeMask = Parent::NextFreeMask << 1,
	};
protected:
	EFFECTPHRASE						_mPhrase;
	U32										_mTimePassed;
	RPGSpellData*						mDataBlock;
	EffectWrapperPhrase			_mPhrases[PHRASE_MAX];

	//===============rpg stuff================
	void						_processServer();
	void						_processClient(U32 dt);

	void						_prepareFhrasese();
	void						_clearPhrases();

	void						_enter_cast_s();
	void						_leave_cast_s();
	void						_enter_cast_c();
	void						_leave_cast_c();

	void						_enter_launch_s();
	void						_leave_launch_s();
	void						_enter_launch_c();
	void						_leave_launch_c();

	void						_enter_impact_s();
	void						_leave_impact_s();
	void						_enter_impact_c();
	void						_leave_impact_c();

	void						_enter_residue_s();
	void						_leave_residue_s();
	void						_enter_residue_c();
	void						_leave_residue_c();
	//=====================================
public:
	RPGSpell();
	~RPGSpell();
	bool            onNewDataBlock(GameBaseData* dptr);
	void            processTick(const Move*);
	void			   interpolateTick(F32 delta);
	void            advanceTime(F32 dt);

	bool            onAdd();
	void            onRemove();

	U32             packUpdate(NetConnection*, U32, BitStream*);
	void            unpackUpdate(NetConnection*, BitStream*);

	static void             initPersistFields();

	DECLARE_CONOBJECT(RPGSpell);
};

#endif