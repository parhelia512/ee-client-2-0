#include "RPGEffectronData.h"
#include "RPGEffectron.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "../RPGUtils.h"
#include "../EffectWrapper/EffectWrapperData.h"

IMPLEMENT_CONSOLETYPE(RPGEffectronData)
IMPLEMENT_GETDATATYPE(RPGEffectronData)
IMPLEMENT_SETDATATYPE(RPGEffectronData)
IMPLEMENT_CO_DATABLOCK_V1(RPGEffectronData);

RPGEffectronData::RPGEffectronData():
_mDummyPtr(NULL),
_doConvert(false)
{

}

void RPGEffectronData::packData( BitStream* stream)
{
	Parent::packData(stream);
	packEffects(stream);
}

void RPGEffectronData::unpackData( BitStream* stream)
{
	Parent::unpackData(stream);
	_doConvert = true;
	unPackEffects(stream);
}

#define myOffset(field) Offset(field, RPGEffectronData)
void RPGEffectronData::initPersistFields()
{
	Parent::initPersistFields();

	static ewValidator _ewPhrase;

	addFieldV("addEffect",  TypeGameBaseDataPtr ,   myOffset(_mDummyPtr),&_ewPhrase);
}

U32 RPGEffectronData::onActivate( ERRORS & error , const U32 & casterSimID /*= 0*/, const U32 & targetSimID /*= 0*/ )
{
	error = ERR_NONE;

	RPGEffectron * pSpell = new RPGEffectron();
	pSpell->setCasterSimID(casterSimID);
	pSpell->setTargetSimID(targetSimID);
	Con::printf("=== RPGEffectronData::onActivate");
	if (pSpell->onNewDataBlock(this) && pSpell->registerObject())
	{
		error = ERR_NONE;
		return 0;
	}
	delete pSpell;
	return 0;
}

bool RPGEffectronData::onDeActivate( ERRORS & error ,const U32 & casterSimID /*= 0*/, const U32 & targetSimID /*= 0*/ )
{
	error = ERR_NONE;
	return true;
}

void RPGEffectronData::addEffect( EffectWrapperData * pEffectWrapperData)
{
	_mDataPhrase.addEffectWrapperData(pEffectWrapperData);
}

void RPGEffectronData::packEffects( BitStream * stream )
{
		EffectWrapperDataPhrase::EWDATALIST & ewdList = _mDataPhrase.getEffectWrapperDatas();
		S32 size = ewdList.size();
		stream->writeInt(size,MAX_EEFECTS_PERPHRASEBITS);
		for (S32 j = 0 ; j < size ; ++j)
		{
			RPGUtils::writeDatablockID(stream,(SimObject*)ewdList[j],packed);
		}
}

void RPGEffectronData::unPackEffects( BitStream * stream )
{
		EffectWrapperDataPhrase::EWDATALIST & ewdList = _mDataPhrase.getEffectWrapperDatas();
		ewdList.clear();
		S32 size = stream->readInt(MAX_EEFECTS_PERPHRASEBITS);
		for (S32 j = 0 ; j < size ; ++j)
		{
			ewdList.push_back( ( EffectWrapperData *)RPGUtils::readDatablockID(stream) );
		}
}

void RPGEffectronData::expandEffects()
{
		EffectWrapperDataPhrase::EWDATALIST & ewdList = _mDataPhrase.getEffectWrapperDatas();
		S32 size = ewdList.size();
		for (S32 j = 0 ; j < size ; ++j)
		{
			SimObjectId id = (SimObjectId)ewdList[j];
			if (id != 0)
			{
				if (!Sim::findObject(id, ewdList[j]))
				{
					Con::errorf("RPGEffectronData::expandEffects -- bad datablockId: 0x%x ",id);
				}
			}
		}
}

bool RPGEffectronData::preload( bool server, String &errorStr )
{
	if ( !server && _doConvert)
	{
		expandEffects();
		_doConvert = false;
	}
	return Parent::preload(server,errorStr);
}

void RPGEffectronData::ewValidator::validateType( SimObject *object, void *typePtr )
{
	RPGEffectronData* spelldata = dynamic_cast<RPGEffectronData*>(object);
	EffectWrapperData** ew = (EffectWrapperData**)(typePtr);
	spelldata->addEffect(*ew);
}

ConsoleMethod(RPGEffectronData,start,void,2,2,"%effectronData.start();")
{
	RPGDefs::ERRORS error;
	object->onActivate(error,0,0);
}