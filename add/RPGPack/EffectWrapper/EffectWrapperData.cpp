#include "EffectWrapperData.h"
#include "../RPGUtils.h"
#include "console/consoleTypes.h"

IMPLEMENT_CONSOLETYPE(EffectWrapperData)
IMPLEMENT_GETDATATYPE(EffectWrapperData)
IMPLEMENT_SETDATATYPE(EffectWrapperData)
IMPLEMENT_CO_DATABLOCK_V1(EffectWrapperData);

EffectWrapperData::EffectWrapperData():
_effect(NULL),
_szConstraint(NULL),
_nLastTime(0),
_doConvert(false)
{

}

void EffectWrapperData::packData( BitStream* stream)
{
	Parent::packData(stream);
	RPGUtils::writeDatablockID(stream,_effect,packed);
	stream->writeString(_szConstraint);
	stream->write(_nLastTime);
}

void EffectWrapperData::unpackData( BitStream* stream)
{
	Parent::unpackData(stream);
	_doConvert = true;
	_effect = (GameBaseData*)RPGUtils::readDatablockID(stream);
	_szConstraint = stream->readSTString();
	stream->read(&_nLastTime);
}

#define myOffset(field) Offset(field, EffectWrapperData)
void EffectWrapperData::initPersistFields()
{
	Parent::initPersistFields();
	addField("lastingTime",  TypeS32,          myOffset(_nLastTime));
	addField("effect",  TypeSimObjectPtr,  myOffset(_effect));
	addField("constraint",    TypeString,     myOffset(_szConstraint));
}

const U32 & EffectWrapperData::getLastingTime()
{
	return _nLastTime;
}

SimDataBlock * EffectWrapperData::getEffectWrapperData()
{
	return _effect;
}

StringTableEntry EffectWrapperData::getConstraintString()
{
	return _szConstraint;
}

bool EffectWrapperData::preload( bool server, String &errorStr )
{
	if (!server) 
	{
		if (_doConvert)
		{
			SimObjectId db_id;
			db_id = (SimObjectId)_effect;
			if (!Sim::findObject(db_id, _effect))
			{
				Con::errorf("EffectWrapperData::preload error _effect = %d",db_id);
			}
			_doConvert = false;
		}
	}
	return Parent::preload(server,errorStr);
}