#include "EFX_Decal.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "../Constraint/Constraint.h"
#include "T3D/decal/decalManager.h"

IMPLEMENT_CO_DATABLOCK_V1(EFX_Decal_Data);

EFX_Decal_Data::EFX_Decal_Data():
mSpinSpeed(0)
{

}

void EFX_Decal_Data::packData( BitStream* stream)
{
	Parent::packData(stream);
	stream->write(mSpinSpeed);
}

void EFX_Decal_Data::unpackData( BitStream* stream)
{
	Parent::unpackData(stream);
	stream->read(&mSpinSpeed);
}

#define myOffset(field) Offset(field, EFX_Decal_Data)
void EFX_Decal_Data::initPersistFields()
{
	Parent::initPersistFields();
	addField("spinSpeed",  TypeF32, myOffset(mSpinSpeed));
}

bool EFX_Decal_Data::preload( bool server, String &errorStr )
{
	return Parent::preload(server,errorStr);
}

IMPLEMENT_CONOBJECT(EFX_Decal_Wrapper);

EFX_Decal_Wrapper::EFX_Decal_Wrapper():
mDataBlock(NULL),
mDecalInstance(NULL)
{

}

bool EFX_Decal_Wrapper::ea_SetData( SimDataBlock * pData , ERRORS & error )
{
	mDataBlock = dynamic_cast<EFX_Decal_Data*>(pData);
	if (mDataBlock)
	{
		error = ERR_NONE;
		return true;
	}
	error = ERR_DATANOTMATCH;
	return false;
}

bool EFX_Decal_Wrapper::ea_Start( ERRORS & error )
{
	Constraint * pContraint = ea_getConstraint();
	if (pContraint)
	{
		mDecalInstance = pContraint->addGroundDecal(mDataBlock);
	}
	error = ERR_NONE;
	return true;
}

void EFX_Decal_Wrapper::ea_Update( const U32 & dt )
{
	Constraint * pContraint = ea_getConstraint();
	if (mDecalInstance && pContraint)
	{
		F32 angle = mDataBlock->mSpinSpeed * dt / 1000;
		Point3F tangent;
		MatrixF matOut;
		mDecalInstance->getWorldMatrix(&matOut);
		Point3F position = mDecalInstance->getPosition();
		MatrixF rot(EulerF(0,0,angle));
		matOut.mul(rot);
		matOut.getColumn(0,&tangent);
		tangent.normalizeSafe();
		mDecalInstance->setTangent(tangent);
		mDecalInstance->setPosition(pContraint->getConstraintPos());
	}
}

void EFX_Decal_Wrapper::ea_End()
{
	Constraint * pContraint = ea_getConstraint();
	if (pContraint && mDecalInstance)
	{
		gDecalManager->removeDecal(mDecalInstance);
	}
}

bool EFX_Decal_Desc::canRunOnServer()
{
	return false;
}

bool EFX_Decal_Desc::canRunOnClient()
{
	return true;
}

bool EFX_Decal_Desc::isMatch( SimDataBlock * pData )
{
	return dynamic_cast<EFX_Decal_Data*>(pData) != NULL;
}

EffectWrapper * EFX_Decal_Desc::createEffectWrapper( SimDataBlock * pData )
{
	EFX_Decal_Wrapper * pEffectWrapper = new EFX_Decal_Wrapper;
	ERRORS error = ERR_UNKOWN;
	if (pEffectWrapper->ea_SetData(pData,error))
	{
		return pEffectWrapper;
	}
	delete pEffectWrapper;
	return NULL;
}

RPGDefs::EFFECTRUN EFX_Decal_Desc::getEffectRunsOn()
{
	return RPGDefs::EFFECTRUN::ONCLIENT;
}

IMPLEMENT_EFXDESC(EFX_Decal_Desc);