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
		ERR_NONE = 0,					//�޴���
		ERR_UNKOWN,					//δ֪����
		ERR_NOTENOUGHMANA,	//ħ������
		ERR_DATANOTMATCH,		//���ݲ�ƥ��
	};
	enum EFFECTRUN
	{
		ONCLIENT = 1 << 0 ,
		ONSERVER = 1 << 1 ,
	};
	enum EFFECTPHRASE
	{
		PHRASE_CAST = 0,				//�����׶�
		PHRASE_LAUNCH,				//ʩ���׶�
		PHRASE_IMPACT,					//���н׶�
		PHRASE_RESIDUE,				//�����׶�
		PHRASE_MAX, 
	};
	enum RPGBASESTATU
	{
		STATU_NORMAL = 0,			//ĳ����Ʒ����״̬
		STATU_COOLDOWN,			//������ȴ��
		STATU_INVALID,					//������
	};
	enum BOOKTYPE
	{
		BOOK_PACK = 0,					//����
		BOOK_SHOTCUTBAR1,			//�·��Ŀ����
	};
	enum
	{
		BOOK_MAX = 50,
		MAX_EFFECTS_PERPHRASE = 1024,
		MAX_EEFECTS_PERPHRASEBITS = 10,
	};
};

#endif