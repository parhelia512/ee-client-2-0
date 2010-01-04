#include "RPGBookData.h"
#include "RPGBaseData.h"
#include "RPGUtils.h"
#include "console/consoleTypes.h"

IMPLEMENT_CONSOLETYPE(RPGBookData)
IMPLEMENT_GETDATATYPE(RPGBookData)
IMPLEMENT_SETDATATYPE(RPGBookData)
IMPLEMENT_CO_DATABLOCK_V1(RPGBookData);

RPGBookData::RPGBookData()
{
	_mbDoIDConvert = false;
	_nRPGBookDataIdx = -1;
	for (S32 i = 0 ; i < BOOK_MAX ; ++i)
	{
		_mRpgDatas[i] = NULL;
	}
}

void RPGBookData::packData( BitStream* stream)
{
	Parent::packData(stream);
	stream->write(_nRPGBookDataIdx);
	for (S32 i = 0 ; i < BOOK_MAX ; ++i)
	{
		RPGUtils::writeDatablockID(stream,_mRpgDatas[i],packed);
	}
}

void RPGBookData::unpackData( BitStream* stream)
{
	Parent::unpackData(stream);
	stream->read(&_nRPGBookDataIdx);
	_mbDoIDConvert = true;
	for (S32 i = 0 ; i < BOOK_MAX ; ++i)
	{
		_mRpgDatas[i] = (RPGBaseData*)RPGUtils::readDatablockID(stream);
	}
}

#define myOffset(field) Offset(field, RPGBookData)
void RPGBookData::initPersistFields()
{
	Parent::initPersistFields();
	addField("rpgBookDataIdx",  TypeS32,          myOffset(_nRPGBookDataIdx));
	addField("rpgDatas",TypeGameBaseDataPtr,myOffset(_mRpgDatas),BOOK_MAX);
}

RPGBaseData * RPGBookData::getRpgBaseData( const S32 & idx )
{
	if (idx == -1)
	{
		return NULL;
	}
	S32 dataBlockIdx = idx / BOOK_MAX;
	U8 offsetIdx = idx % BOOK_MAX;
	char szBookData[64];
	dSprintf(szBookData,64,"gRPGBookData%d",dataBlockIdx);
	RPGBookData * pBookData = dynamic_cast<RPGBookData *> (Sim::findObject(szBookData));
	return pBookData ? pBookData->_getRpgBaseData(offsetIdx) : NULL;
}

bool RPGBookData::preload( bool server, String &errorStr )
{
	if (!Parent::preload(server, errorStr))
		return false;

	// Resolve objects transmitted from server
	if (!server) 
	{
		if (_mbDoIDConvert)
		{
			SimObjectId db_id;
			for (S32 i = 0 ; i < BOOK_MAX ; ++i)
			{
				db_id = (SimObjectId)_mRpgDatas[i];
				if (!Sim::findObject(db_id, _mRpgDatas[i]))
				{
					//Con::errorf("RPGBookData::preload error _mRpgDatas[%d] = %d",i,db_id);
				}
			}
			_mbDoIDConvert = false;
		}

		//change the name
		if (_nRPGBookDataIdx == -1)
		{
			assignName("gRPGBookData");
		}
		else
		{
			char szName[64];
			dSprintf(szName,64,"gRPGBookData%d",_nRPGBookDataIdx);
			assignName(szName);
		}
	}

	return true;
}

RPGBaseData * RPGBookData::_getRpgBaseData( const U8 & idx )
{
	return _mRpgDatas[idx];
}