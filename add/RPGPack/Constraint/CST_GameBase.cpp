#include "CST_GameBase.h"
#include "T3D/gameBase.h"

IMPLEMENT_CONOBJECT(CST_GameBase);

CST_GameBase::CST_GameBase()
{

}

//=========================DESC=====================
class CST_GameBase_Desc : public ConstraintDesc
{
public:
	bool							isMatchDesc(const char * szConstraint);
	Constraint *				createConstraint(const char * szConstraint);
};

bool CST_GameBase_Desc::isMatchDesc( const char * szConstraint )
{
	return dStrstr(szConstraint,"#gamebase") != NULL;
}

Constraint * CST_GameBase_Desc::createConstraint( const char * szConstraint )
{
	CST_GameBase * pInstance = NULL;
	const char * pName = dStrchr(szConstraint,'.');
	SceneObject * pObj = dynamic_cast<SceneObject *>( Sim::findObject(pName) );
	if (pName && pObj)
	{
		pInstance = new CST_GameBase();
		pInstance->setSceneObj(pObj);
	}
	return pInstance;
}
IMPLEMENT_CSTDESC(CST_GameBase_Desc);