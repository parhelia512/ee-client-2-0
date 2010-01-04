#ifndef __RPGBASEDATA_H__
#define __RPGBASEDATA_H__

#include "T3D/gameBase.h"
#include "RPGDefs.h"

class GameConnection;
class RPGBook;

class RPGBaseData : public GameBaseData , public RPGDefs
{
	  typedef GameBaseData  Parent;
	  S32							_nDescIdx;
	  S8							_nRPGType;
	  StringTableEntry	_icon_name;
public:
	RPGBaseData();
	void						packData(BitStream*);
	void						unpackData(BitStream*);
	static void			initPersistFields();
	//icon name
	StringTableEntry	getIconName();
	//获得这个rpgdata的类型
	const U8 &			getRPGDataType();
	//当一个item被激活，如果能够激活，error返回0
	//U32返回冷却时间，0表示无冷却时间，单位MS，-1表示永远执行
	virtual U32			onActivate(ERRORS & error , const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	//当一个item被激活后，又被反激活，如果能够反激活，则返回true，否则返回false
	virtual bool			onDeActivate(ERRORS & error , const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	//当这个item被从一个地方移动到另一个地方时，会被调用
	//已经从srcBook移动到了destBook
	virtual void			onItemMoved(RPGBook * pSrcBook, S32 srcIdx, RPGBook * pDestBook, S32 destIdx);
	DECLARE_CONOBJECT(RPGBaseData);
};
DECLARE_CONSOLETYPE(RPGBaseData);
#endif