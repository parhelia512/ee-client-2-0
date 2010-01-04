#include "RPGBaseData.h"
#include "T3D/gameConnection.h"
#include "RPGBook.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"

IMPLEMENT_CONSOLETYPE(RPGBaseData)
IMPLEMENT_GETDATATYPE(RPGBaseData)
IMPLEMENT_SETDATATYPE(RPGBaseData)
IMPLEMENT_CO_DATABLOCK_V1(RPGBaseData);

RPGBaseData::RPGBaseData():
_icon_name(NULL),
_nDescIdx(-1),
_nRPGType(RPGTYPE_ALL)
{

}

void RPGBaseData::packData( BitStream* stream)
{
	Parent::packData(stream);
	stream->write(_nDescIdx);
	stream->write(_nRPGType);
	stream->writeString(_icon_name);
}

void RPGBaseData::unpackData( BitStream* stream)
{
	Parent::unpackData(stream);
	stream->read(&_nDescIdx);
	stream->read(&_nRPGType);
	_icon_name = stream->readSTString();
}

#define myOffset(field) Offset(field, RPGBaseData)
void RPGBaseData::initPersistFields()
{
	Parent::initPersistFields();
	addField("iconName",    TypeString,     myOffset(_icon_name));
	addField("rpgType",  TypeS8,          myOffset(_nRPGType));
	addField("descStringIdx",  TypeS32,          myOffset(_nDescIdx));
}

U32 RPGBaseData::onActivate( ERRORS & error , const U32 & casterSimID, const U32 &  targetSimID)
{
	error = ERR_NONE;
	return 0;
}

bool RPGBaseData::onDeActivate( ERRORS & error , const U32 & casterSimID, const U32 &  targetSimID)
{
	error = ERR_NONE;
	return true;
}

void RPGBaseData::onItemMoved( RPGBook * pSrcBook, S32 srcIdx, RPGBook * pDestBook, S32 destIdx )
{

}

const U8 & RPGBaseData::getRPGDataType()
{
	return _nRPGType;
}

StringTableEntry RPGBaseData::getIconName()
{
	return _icon_name;
}