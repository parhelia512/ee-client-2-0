#include "EFX_Particle.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "../Constraint/Constraint.h"
#include "../RPGUtils.h"

IMPLEMENT_CO_DATABLOCK_V1(EFX_Particle_Data);

EFX_Particle_Data::EFX_Particle_Data()
{

}

void EFX_Particle_Data::packData( BitStream* stream)
{
	Parent::packData(stream);
}

void EFX_Particle_Data::unpackData( BitStream* stream)
{
	Parent::unpackData(stream);
}

#define myOffset(field) Offset(field, EFX_Particle_Data)
void EFX_Particle_Data::initPersistFields()
{
	Parent::initPersistFields();
}

bool EFX_Particle_Data::preload( bool server, String &errorStr )
{
	return Parent::preload(server,errorStr);
}

IMPLEMENT_CONOBJECT(EFX_Particle_Wrapper);

EFX_Particle_Wrapper::EFX_Particle_Wrapper():
mDataBlock(NULL),
mEmitter(NULL)
{

}

bool EFX_Particle_Wrapper::ea_SetData( SimDataBlock * pData , ERRORS & error )
{
	mDataBlock = dynamic_cast<EFX_Particle_Data*>(pData);
	if (mDataBlock)
	{
		error = ERR_NONE;
		return true;
	}
	error = ERR_DATANOTMATCH;
	return false;
}

bool EFX_Particle_Wrapper::ea_Start( ERRORS & error )
{
	Constraint * pContraint = ea_getConstraint();
	if (pContraint)
	{
		ParticleEmitter* pEmitter = new ParticleEmitter;
		if( pEmitter->setDataBlock(mDataBlock) && pEmitter->registerObject())
		{
			mEmitter = pEmitter;
			error = ERR_NONE;
			return true;
		}
		delete pEmitter;
	}
	error = ERR_UNKOWN;
	return false;
}

void EFX_Particle_Wrapper::ea_Update( const U32 & dt )
{
	Constraint * pContraint = ea_getConstraint();
	if (pContraint && !mEmitter.isNull())
	{
		Point3F emitPoint, emitVelocity;
		Point3F emitAxis(0, 0, 1);
		MatrixF mat = pContraint->getConstraintTransform();
		mat.mulV(emitAxis);
		mat.getColumn(3, &emitPoint);
		emitVelocity = emitAxis;

		mEmitter->emitParticles(emitPoint, emitPoint,
			emitAxis,
			emitVelocity, dt);
	}
}

void EFX_Particle_Wrapper::ea_End()
{
	if( mEmitter )
	{
		mEmitter->safeDeleteObject();
		mEmitter = NULL;
	}
}

bool EFX_Particle_Desc::canRunOnServer()
{
	return false;
}

bool EFX_Particle_Desc::canRunOnClient()
{
	return true;
}

bool EFX_Particle_Desc::isMatch( SimDataBlock * pData )
{
	return dynamic_cast<EFX_Particle_Data*>(pData) != NULL;
}

EffectWrapper * EFX_Particle_Desc::createEffectWrapper( SimDataBlock * pData )
{
	EFX_Particle_Wrapper * pEffectWrapper = new EFX_Particle_Wrapper;
	ERRORS error = ERR_UNKOWN;
	if (pEffectWrapper->ea_SetData(pData,error))
	{
		return pEffectWrapper;
	}
	delete pEffectWrapper;
	return NULL;
}

RPGDefs::EFFECTRUN EFX_Particle_Desc::getEffectRunsOn()
{
	return RPGDefs::EFFECTRUN::ONCLIENT;
}

IMPLEMENT_EFXDESC(EFX_Particle_Desc);