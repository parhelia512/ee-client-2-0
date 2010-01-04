#include "RPGSpellData.h"
#include "RPGSpell.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "../RPGUtils.h"
#include "../EffectWrapper/EffectWrapperData.h"

IMPLEMENT_CONSOLETYPE(RPGSpellData)
IMPLEMENT_GETDATATYPE(RPGSpellData)
IMPLEMENT_SETDATATYPE(RPGSpellData)
IMPLEMENT_CO_DATABLOCK_V1(RPGSpellData);

RPGSpellData::RPGSpellData():
_mDummyPtr(NULL),
_nCoolDownTime(0),
_doConvert(false)
{

}

void RPGSpellData::packData( BitStream* stream)
{
	Parent::packData(stream);
	stream->write(_nCoolDownTime);
	packEffects(stream);
}

void RPGSpellData::unpackData( BitStream* stream)
{
	Parent::unpackData(stream);
	_doConvert = true;
	stream->read(&_nCoolDownTime);
	unPackEffects(stream);
}

#define myOffset(field) Offset(field, RPGSpellData)
void RPGSpellData::initPersistFields()
{
	Parent::initPersistFields();

	static ewValidator _castingPhrase(PHRASE_CAST);
	static ewValidator _launchPhrase(PHRASE_LAUNCH);
	static ewValidator _impactPhrase(PHRASE_IMPACT);
	static ewValidator _residuePhrase(PHRASE_RESIDUE);

	addField("coolDownMS",  TypeS32,          myOffset(_nCoolDownTime));

	addFieldV("addCastingEffect",  TypeGameBaseDataPtr ,   myOffset(_mDummyPtr),&_castingPhrase);
	addFieldV("addLaunchEffect",  TypeGameBaseDataPtr,   myOffset(_mDummyPtr),&_launchPhrase);
	addFieldV("addImpactEffect",  TypeGameBaseDataPtr,   myOffset(_mDummyPtr),&_impactPhrase);
	addFieldV("addResidueEffect",  TypeGameBaseDataPtr,   myOffset(_mDummyPtr),&_residuePhrase);
}

U32 RPGSpellData::onActivate( ERRORS & error , const U32 & casterSimID /*= 0*/, const U32 & targetSimID /*= 0*/ )
{
	RPGSpell * pSpell = new RPGSpell();
	pSpell->setCasterSimID(casterSimID);
	pSpell->setTargetSimID(targetSimID);
	Con::printf("servser === RPGSpellData::onActivate");
	if (pSpell->onNewDataBlock(this) && pSpell->registerObject())
	{
		error = ERR_NONE;
		return _nCoolDownTime;
	}
	delete pSpell;
	error = ERR_UNKOWN;
	return 0;
}

bool RPGSpellData::onDeActivate( ERRORS & error ,const U32 & casterSimID /*= 0*/, const U32 & targetSimID /*= 0*/ )
{
	error = ERR_NONE;
	GameBase * pCaster = dynamic_cast<GameBase *>( Sim::findObject(casterSimID) );
	if (pCaster)
	{
		pCaster->onInterrupt();
	}
	return true;
}

void RPGSpellData::addEffect( EffectWrapperData * pEffectWrapperData , EFFECTPHRASE phrase )
{
	_mDataPhrase[phrase].addEffectWrapperData(pEffectWrapperData);
}

void RPGSpellData::packEffects( BitStream * stream )
{
	for (S32 i = 0 ; i < PHRASE_MAX ; ++i)
	{
		EffectWrapperDataPhrase::EWDATALIST & ewdList = _mDataPhrase[i].getEffectWrapperDatas();
		S32 size = ewdList.size();
		stream->writeInt(size,MAX_EEFECTS_PERPHRASEBITS);
		for (S32 j = 0 ; j < size ; ++j)
		{
			RPGUtils::writeDatablockID(stream,(SimObject*)ewdList[j],packed);
		}
	}
}

void RPGSpellData::unPackEffects( BitStream * stream )
{
	for (S32 i = 0 ; i < PHRASE_MAX ; ++i)
	{
		EffectWrapperDataPhrase::EWDATALIST & ewdList = _mDataPhrase[i].getEffectWrapperDatas();
		ewdList.clear();
		S32 size = stream->readInt(MAX_EEFECTS_PERPHRASEBITS);
		for (S32 j = 0 ; j < size ; ++j)
		{
			ewdList.push_back( ( EffectWrapperData *)RPGUtils::readDatablockID(stream) );
		}
	}
}

void RPGSpellData::expandEffects()
{
	for (S32 i = 0 ; i < PHRASE_MAX ; ++i)
	{
		EffectWrapperDataPhrase::EWDATALIST & ewdList = _mDataPhrase[i].getEffectWrapperDatas();
		S32 size = ewdList.size();
		for (S32 j = 0 ; j < size ; ++j)
		{
			SimObjectId id = (SimObjectId)ewdList[j];
			if (id != 0)
			{
				if (!Sim::findObject(id, ewdList[j]))
				{
					Con::errorf("RPGSpellData::expandEffects -- bad datablockId: 0x%x ",id);
				}
			}
		}
	}
}

bool RPGSpellData::preload( bool server, String &errorStr )
{
	if ( !server && _doConvert)
	{
		expandEffects();
		_doConvert = false;
	}
	return Parent::preload(server,errorStr);
}

void RPGSpellData::ewValidator::validateType( SimObject *object, void *typePtr )
{
	RPGSpellData* spelldata = dynamic_cast<RPGSpellData*>(object);
	EffectWrapperData** ew = (EffectWrapperData**)(typePtr);
	spelldata->addEffect(*ew,(EFFECTPHRASE)_id);
}