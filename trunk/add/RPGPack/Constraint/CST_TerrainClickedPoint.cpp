#include "CST_TerrainClickedPoint.h"
#include "T3D/decal/decalData.h"
#include "T3D/decal/decalManager.h"

IMPLEMENT_CONOBJECT(CST_TerrainClicked);
Point3F CST_TerrainClicked::smTerrainClicked(Point3F::Zero);
//=========================DESC=====================
class CST_TerrainClicked_Desc : public ConstraintDesc
{
public:
	bool							isMatchDesc(const char * szConstraint);
	Constraint *				createConstraint(const char * szConstraint);
};

bool CST_TerrainClicked_Desc::isMatchDesc( const char * szConstraint )
{
	return dStrstr(szConstraint,"#terrain_clicked") != NULL;
}

Constraint * CST_TerrainClicked_Desc::createConstraint( const char * szConstraint )
{
	CST_TerrainClicked * pInstance =  new CST_TerrainClicked();
	pInstance->_mPosition = CST_TerrainClicked::smTerrainClicked;
	pInstance->_mTranform.setPosition(CST_TerrainClicked::smTerrainClicked);
	pInstance->_mTranform.setColumn(0,Point3F(1.f,0.f,0.f));
	pInstance->_mTranform.setColumn(1,Point3F(0.f,1.f,0.f));
	pInstance->_mTranform.setColumn(2,Point3F(0.f,0.f,1.f));
	return pInstance;
}
IMPLEMENT_CSTDESC(CST_TerrainClicked_Desc);