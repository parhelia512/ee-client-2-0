#ifndef __RPGBASE_H__
#define __RPGBASE_H__

#include "T3D/gameBase.h"

class RPGBaseData;

class RPGBase : public GameBase
{
	 typedef GameBase        Parent;
public:
	 enum MaskBits 
	 {
		 NextFreeMask  = Parent::NextFreeMask << 0,
	 };
protected:
	RPGBaseData*       mDataBlock;
	U32						  mCasterSimID;
	U32						  mTargetSimID;
public:
	RPGBase();
	~RPGBase();
	bool            onNewDataBlock(GameBaseData* dptr);
	void			   interpolateTick(F32 delta);
	void            processTick(const Move*);
	void            advanceTime(F32 dt);

	bool            onAdd();
	void            onRemove();

	U32             packUpdate(NetConnection*, U32, BitStream*);
	void            unpackUpdate(NetConnection*, BitStream*);

	static void             initPersistFields();

	//===========rpg stuff============
	const U32 &						getCasterSimID();
	const U32 &						getTargetSimID();
	void									setCasterSimID(const U32 & id);
	void									setTargetSimID(const U32 & id);

	virtual void						onInterrupt();
	//============================
	DECLARE_CONOBJECT(RPGBase);
};

#endif