#include "CST_ShapeBase.h"
#include "T3D/shapeBase.h"

IMPLEMENT_CONOBJECT(CST_ShapeBase);

CST_ShapeBase::CST_ShapeBase()
{

}

//=========================DESC=====================
class CST_ShapeBase_Desc : public ConstraintDesc
{
public:
	bool							isMatchDesc(const char * szConstraint);
	Constraint *				createConstraint(const char * szConstraint);
};

bool CST_ShapeBase_Desc::isMatchDesc( const char * szConstraint )
{
	return dStrstr(szConstraint,"#shapebase") != NULL;
}

Constraint * CST_ShapeBase_Desc::createConstraint( const char * szConstraint )
{
	CST_ShapeBase * pInstance = NULL;
	const char * pName = dStrchr(szConstraint,'.');
	SceneObject * pObj = dynamic_cast<SceneObject *>( Sim::findObject(pName) );
	if (pName && pObj)
	{
		pInstance = new CST_ShapeBase();
		pInstance->setSceneObj(pObj);
	}
	return pInstance;
}
IMPLEMENT_CSTDESC(CST_ShapeBase_Desc);