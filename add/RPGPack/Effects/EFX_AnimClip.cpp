#include "EFX_AnimClip.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "../Constraint/Constraint.h"

IMPLEMENT_CO_DATABLOCK_V1(EFX_AnimClip_Data);

EFX_AnimClip_Data::EFX_AnimClip_Data():
_anim_name(NULL)
{

}

void EFX_AnimClip_Data::packData( BitStream* stream)
{
	Parent::packData(stream);
	stream->writeString(_anim_name);
}

void EFX_AnimClip_Data::unpackData( BitStream* stream)
{
	Parent::unpackData(stream);
	_anim_name = stream->readSTString();
}

#define myOffset(field) Offset(field, EFX_AnimClip_Data)
void EFX_AnimClip_Data::initPersistFields()
{
	Parent::initPersistFields();
	addField("clipName",  TypeString,myOffset(_anim_name));
}

bool EFX_AnimClip_Data::preload( bool server, String &errorStr )
{
	return Parent::preload(server,errorStr);
}

IMPLEMENT_CONOBJECT(EFX_AnimClip_Wrapper);

EFX_AnimClip_Wrapper::EFX_AnimClip_Wrapper():
mDataBlock(NULL)
{

}

bool EFX_AnimClip_Wrapper::ea_SetData( SimDataBlock * pData , ERRORS & error )
{
	mDataBlock = dynamic_cast<EFX_AnimClip_Data*>(pData);
	if (mDataBlock)
	{
		error = ERR_NONE;
		return true;
	}
	error = ERR_DATANOTMATCH;
	return false;
}

bool EFX_AnimClip_Wrapper::ea_Start( ERRORS & error )
{
	Constraint * pContraint = ea_getConstraint();
	if (pContraint)
	{
		pContraint->setAnimClip(mDataBlock->_anim_name);
	}
	error = ERR_NONE;
	return true;
}

void EFX_AnimClip_Wrapper::ea_Update( const U32 & dt )
{

}

void EFX_AnimClip_Wrapper::ea_End()
{

}

bool EFX_AnimClip_Desc::canRunOnServer()
{
	return false;
}

bool EFX_AnimClip_Desc::canRunOnClient()
{
	return true;
}

bool EFX_AnimClip_Desc::isMatch( SimDataBlock * pData )
{
	return dynamic_cast<EFX_AnimClip_Data*>(pData) != NULL;
}

EffectWrapper * EFX_AnimClip_Desc::createEffectWrapper( SimDataBlock * pData )
{
	EFX_AnimClip_Wrapper * pEffectWrapper = new EFX_AnimClip_Wrapper;
	ERRORS error = ERR_UNKOWN;
	if (pEffectWrapper->ea_SetData(pData,error))
	{
		return pEffectWrapper;
	}
	delete pEffectWrapper;
	return NULL;
}

RPGDefs::EFFECTRUN EFX_AnimClip_Desc::getEffectRunsOn()
{
	return RPGDefs::EFFECTRUN::ONCLIENT;
}

IMPLEMENT_EFXDESC(EFX_AnimClip_Desc);