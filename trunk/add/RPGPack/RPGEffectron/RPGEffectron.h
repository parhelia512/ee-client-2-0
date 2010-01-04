#ifndef __RPGEFFECTRON_H__
#define __RPGEFFECTRON_H__

#include "../RPGBase.h"
#include "../EffectPhrase/EffectWrapperPhrase.h"

class RPGEffectronData;

//this class is not ghosted , only on client or only on server

class RPGEffectron : public RPGBase , public RPGDefs
{
	typedef RPGBase        Parent;
protected:
	U32												_mTimePassed;
	RPGEffectronData*						mDataBlock;
	EffectWrapperPhrase					_mPhrases;

	//===============rpg stuff================
	void						_processServer();
	void						_processClient(U32 dt);

	void						_prepareFhrasese();
	void						_clearPhrases();
	//=====================================
public:
	RPGEffectron();
	~RPGEffectron();
	bool            onNewDataBlock(GameBaseData* dptr);
	void            processTick(const Move*);
	void			   interpolateTick(F32 delta);
	void            advanceTime(F32 dt);

	bool            onAdd();
	void            onRemove();

	U32             packUpdate(NetConnection*, U32, BitStream*);
	void            unpackUpdate(NetConnection*, BitStream*);

	static void             initPersistFields();

	DECLARE_CONOBJECT(RPGEffectron);
};

#endif