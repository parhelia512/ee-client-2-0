#ifndef __RPGBOOKDATA_H__
#define __RPGBOOKDATA_H__

#include "T3D/gameBase.h"
#include "RPGDefs.h"

class RPGBaseData;
/*
gRPGBookData 作为每个book的datablock
而gRPGBookData又管理gRPGBookData0~gRPGBookData10，甚至
更多的datablock，因为这样更方便传输
*/
class RPGBookData : public GameBaseData , public RPGDefs
{
	typedef GameBaseData  Parent;
	RPGBaseData *					_mRpgDatas[BOOK_MAX];
	bool									_mbDoIDConvert;
	S32									_nRPGBookDataIdx;
public:
	RPGBookData();
	void						packData(BitStream*);
	void						unpackData(BitStream*);
	static void			initPersistFields();
	bool						preload(bool server, String &errorStr);
	//===============RPG Stuff================
protected:
	RPGBaseData *					_getRpgBaseData(const U8 & idx);
public:
	RPGBaseData *					getRpgBaseData(const S32 & idx);
	//======================================
	DECLARE_CONOBJECT(RPGBookData);
};
DECLARE_CONSOLETYPE(RPGBookData);
#endif