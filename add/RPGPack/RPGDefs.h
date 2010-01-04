#ifndef __RPGDEFS_H__
#define __RPGDEFS_H__

class RPGDefs
{
public:
	enum RPGDataType
	{
		RPGTYPE_ALL = -1,
		RPGTYPE_SPELL = 1 << 0,
		RPGTYPE_ITEM = 1 << 1,
	};
	enum ERRORS
	{
		ERR_NONE = 0,					//无错误
		ERR_UNKOWN,					//未知错误
		ERR_NOTENOUGHMANA,	//魔法不够
		ERR_DATANOTMATCH,		//数据不匹配
	};
	enum EFFECTRUN
	{
		ONCLIENT = 1 << 0 ,
		ONSERVER = 1 << 1 ,
	};
	enum EFFECTPHRASE
	{
		PHRASE_CAST = 0,				//吟唱阶段
		PHRASE_LAUNCH,				//施法阶段
		PHRASE_IMPACT,					//命中阶段
		PHRASE_RESIDUE,				//后续阶段
		PHRASE_MAX, 
	};
	enum RPGBASESTATU
	{
		STATU_NORMAL = 0,			//某个物品正常状态
		STATU_COOLDOWN,			//正在冷却中
		STATU_INVALID,					//不可用
	};
	enum BOOKTYPE
	{
		BOOK_PACK = 0,					//背包
		BOOK_SHOTCUTBAR1,			//下方的快捷栏
	};
	enum
	{
		BOOK_MAX = 50,
		MAX_EFFECTS_PERPHRASE = 1024,
		MAX_EEFECTS_PERPHRASEBITS = 10,
	};
};

#endif