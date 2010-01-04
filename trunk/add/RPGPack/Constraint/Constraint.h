#ifndef __CONSTRAINT_H__
#define __CONSTRAINT_H__

#include <vector>
#include "math/mMath.h"
#include "console/simObject.h"

class GameConnection;
class DecalData;
class DecalInstance;

class Constraint : public SimObject
{
	typedef SimObject Parent;
	U32							_casterSimID;
	U32							_targetSimID;
public:
	virtual						~Constraint();
	void							setCasterSimID(const U32 & id);
	void							setTargetSimID(const U32 & id);
	const U32 &				getCasterSimID();
	const U32 &				getTargetSimID();

	//commen
	virtual void										onCasterAndTargetSetted();
	virtual const Point3F &					getConstraintPos();											//Position
	virtual const MatrixF &					getConstraintTransform();								//transform
	virtual DecalInstance*						addGroundDecal(DecalData * decalData);		//decal
	//sceneobject
	//gamebase
	//shapebase
	//player
	virtual void										setAnimClip(const char * clipName , bool locked = false);
	//caster & target

	DECLARE_CONOBJECT(Constraint);
};

#define IMPLEMENT_CSTDESC(T) T g_##T
class ConstraintDesc
{
public:
	ConstraintDesc();
	static std::vector< ConstraintDesc * >				smDescs;
	//����szConstraint��4�����̶��Ĳ������һ�������Constraint����
	static Constraint *				getConstraint(	const char * szConstraint );
	//����desc�ַ����ж��Ƿ��Ӧĳ��constraint
	virtual bool							isMatchDesc(const char * szConstraint) = 0;
	//����һ��constraint
	virtual Constraint *				createConstraint(const char * szConstraint) = 0;
};
#endif