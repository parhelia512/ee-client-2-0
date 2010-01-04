#include "CST_Caster.h"
#include "T3D/player.h"

IMPLEMENT_CONOBJECT(CST_Caster);
CST_Caster::CST_Caster()
{

}

void CST_Caster::onCasterAndTargetSetted()
{
	setSceneObj(dynamic_cast<SceneObject*>(Sim::findObject(getCasterSimID())));
}
//=========================DESC=====================
class CST_Caster_Desc : public ConstraintDesc
{
public:
	bool							isMatchDesc(const char * szConstraint);
	Constraint *				createConstraint(const char * szConstraint);
};

bool CST_Caster_Desc::isMatchDesc( const char * szConstraint )
{
	return dStrstr(szConstraint,"#caster") != NULL;
}

Constraint * CST_Caster_Desc::createConstraint( const char * szConstraint )
{
	CST_Caster * pInstance = NULL;
	pInstance = new CST_Caster();

	return pInstance;
}
IMPLEMENT_CSTDESC(CST_Caster_Desc);