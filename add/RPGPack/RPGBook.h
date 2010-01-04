#ifndef __RPGBOOK_H__
#define __RPGBOOK_H__

#include "T3D/gameBase.h"
#include "RPGDefs.h"
#include <vector>

class RPGBookData;
class RPGBaseData;

class RPGBook : public GameBase  , public RPGDefs
{
	 typedef GameBase        Parent;
	 enum MaskBits 
	 {
		 BookChangedMask  = Parent::NextFreeMask << 0,
		 BookCoolingMask = Parent::NextFreeMask << 1,
		 NextFreeMask = Parent::NextFreeMask << 2,
	 };
	 S32									_mBookDataIdx[BOOK_MAX];
	 U32									_mBookDataFreezeTime[BOOK_MAX];
	 //client only , used for cd fill
	 U32									_mClientFreezeTimeTotal[BOOK_MAX];
	 U8									_mBookDataStatu[BOOK_MAX];
	 U8									_nRPGType;
	 U8									_nBookType;
	 std::vector<U8>				_vecChangedIdxs;
public:
	RPGBook();
	~RPGBook();
	bool            onNewDataBlock(GameBaseData* dptr);
	void            processTick(const Move*);
	void            advanceTime(F32 dt);

	bool            onAdd();
	void            onRemove();

	U32             packUpdate(NetConnection*, U32, BitStream*);
	void            unpackUpdate(NetConnection*, BitStream*);

	static void             initPersistFields();

	//============PRG Stuffs================
	//server only
	void						clearBook();
	//如果是个无尽的时间，则被onDeActivate
	bool						useBook(const U8 & idx , ERRORS & error ,const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	const U8 &			getBookRpgDataType();
	
	void						clearItem(const U8 & idx);
	void						insertItem(const U8 & idx , const S32 & item);
	const S32	&			getItem(const U8 & idx);
	bool						isItemEmpty(const U8 & idx);

	bool						swapItem(const U8 & idx , RPGBook * pBook2 , const U8 & idx2);

	RPGBaseData *		getRpgBaseData(const U8 & idx);
	F32						getRatioOfCDTime(const U8 & idx);
	//===================================
protected:
	RPGBookData * mDataBlock;
public:
	DECLARE_CONOBJECT(RPGBook);
};

#endif