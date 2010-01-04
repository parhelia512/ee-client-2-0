#include "EffectWrapper.h"
#include "../Constraint/Constraint.h"

IMPLEMENT_CONOBJECT(EffectWrapper);

bool EffectWrapper::ea_SetData( SimDataBlock * pData , ERRORS & error )
{
	error = ERR_UNKOWN;
	return false;
}

bool EffectWrapper::ea_Start( ERRORS & error )
{
	error = ERR_UNKOWN;
	return false;
}

void EffectWrapper::ea_Update(const U32 & dt )
{
	
}

void EffectWrapper::ea_End()
{

}

void EffectWrapper::ea_setLastingTime( const U32 & time )
{
	_nLastTime = time;
}

const U32 & EffectWrapper::ea_getLastingTime()
{
	return _nLastTime;
}

EffectWrapper::EffectWrapper()
{
	_nLastTime = 0;
	_pConstraint = NULL;
}

void EffectWrapper::ea_setConstraint( Constraint * pConstraint )
{
	_pConstraint = pConstraint;
}

Constraint * EffectWrapper::ea_getConstraint()
{
	return _pConstraint;
}

EffectWrapper::~EffectWrapper()
{
	delete _pConstraint;
}