#include "CST_TerrainRolloverPoint.h"
#include "T3D/decal/decalData.h"
#include "T3D/decal/decalManager.h"

IMPLEMENT_CONOBJECT(CST_TerrainRollover);
Point3F CST_TerrainRollover::smTerrainRollover(Point3F::Zero);
//=========================DESC=====================
class CST_TerrainRollover_Desc : public ConstraintDesc
{
public:
	bool							isMatchDesc(const char * szConstraint);
	Constraint *				createConstraint(const char * szConstraint);
};

bool CST_TerrainRollover_Desc::isMatchDesc( const char * szConstraint )
{
	return dStrstr(szConstraint,"#terrain_rollover") != NULL;
}

Constraint * CST_TerrainRollover_Desc::createConstraint( const char * szConstraint )
{
	CST_TerrainRollover * pInstance =  new CST_TerrainRollover();
	pInstance->_mPosition = CST_TerrainRollover::smTerrainRollover;
	pInstance->_mTranform.setPosition(CST_TerrainRollover::smTerrainRollover);

	return pInstance;
}
IMPLEMENT_CSTDESC(CST_TerrainRollover_Desc);