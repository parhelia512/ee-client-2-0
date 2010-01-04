#ifndef __RPGBOOKDATA_H__
#define __RPGBOOKDATA_H__

#include "T3D/gameBase.h"
#include "RPGDefs.h"

class RPGBaseData;
/*
gRPGBookData ��Ϊÿ��book��datablock
��gRPGBookData�ֹ���gRPGBookData0~gRPGBookData10������
�����datablock����Ϊ���������㴫��
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