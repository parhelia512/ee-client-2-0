#include "Constraint.h"

IMPLEMENT_CONOBJECT(Constraint);

std::vector< ConstraintDesc * >				ConstraintDesc::smDescs;
ConstraintDesc::ConstraintDesc()
{
	smDescs.push_back(this);
}

Constraint * ConstraintDesc::getConstraint( const char * szConstraint)
{
	for (U32 i = 0 ; i < smDescs.size() ; ++i)
	{
		if (smDescs[i]->isMatchDesc(szConstraint))
		{
			return smDescs[i]->createConstraint(szConstraint);
		}
	}
	return NULL;
}

const Point3F & Constraint::getConstraintPos()
{
	return Point3F::Zero;
}

void Constraint::setCasterSimID( const U32 & id )
{
	_casterSimID = id;
}

void Constraint::setTargetSimID( const U32 & id )
{
	_targetSimID = id;
}

const U32 & Constraint::getCasterSimID()
{
	return _casterSimID;
}

const U32 & Constraint::getTargetSimID()
{
	return _targetSimID;
}

const MatrixF & Constraint::getConstraintTransform()
{
	return MatrixF::Identity;
}

void Constraint::setAnimClip( const char * clipName , bool locked /*= false*/ )
{

}

void Constraint::onCasterAndTargetSetted()
{

}

DecalInstance* Constraint::addGroundDecal( DecalData * decalData )
{
	return NULL;
}

Constraint::~Constraint()
{

}