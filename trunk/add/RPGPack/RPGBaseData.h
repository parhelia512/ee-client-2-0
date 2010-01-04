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
	//������rpgdata������
	const U8 &			getRPGDataType();
	//��һ��item���������ܹ����error����0
	//U32������ȴʱ�䣬0��ʾ����ȴʱ�䣬��λMS��-1��ʾ��Զִ��
	virtual U32			onActivate(ERRORS & error , const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	//��һ��item��������ֱ����������ܹ�������򷵻�true�����򷵻�false
	virtual bool			onDeActivate(ERRORS & error , const U32 & casterSimID = 0, const U32 &  targetSimID = 0);
	//�����item����һ���ط��ƶ�����һ���ط�ʱ���ᱻ����
	//�Ѿ���srcBook�ƶ�����destBook
	virtual void			onItemMoved(RPGBook * pSrcBook, S32 srcIdx, RPGBook * pDestBook, S32 destIdx);
	DECLARE_CONOBJECT(RPGBaseData);
};
DECLARE_CONSOLETYPE(RPGBaseData);
#endif