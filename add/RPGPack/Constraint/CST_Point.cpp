#include "CST_Point.h"
#include "T3D/decal/decalData.h"
#include "T3D/decal/decalManager.h"

IMPLEMENT_CONOBJECT(CST_Point);
CST_Point::CST_Point():
_mPosition(Point3F::Zero),
_mTranform(MatrixF::Identity)
{

}

const Point3F & CST_Point::getConstraintPos()
{
	return _mPosition;
}

const MatrixF & CST_Point::getConstraintTransform()
{
	return _mTranform;
}

DecalInstance* CST_Point::addGroundDecal( DecalData * decalData )
{
	if (decalData)
	{
		Point3F pos;
		RayInfo rInfo;
		MatrixF mat = _mTranform;
		mat.getColumn(3, &pos);
		if (gClientContainer.castRay(Point3F(pos.x, pos.y, pos.z + 0.01f),
			Point3F(pos.x, pos.y, pos.z - 2.0f ),
			STATIC_COLLISION_MASK | VehicleObjectType, &rInfo))
		{
			Point3F normal;
			Point3F tangent;
			mat = _mTranform;
			mat.getColumn( 0, &tangent );
			mat.getColumn( 2, &normal );
			DecalInstance* pDecal = gDecalManager->addDecal( rInfo.point, normal, tangent, decalData ,1,0,CustomDecal);      
			return pDecal;
		}
	}
	return NULL;
}

CST_Point::~CST_Point()
{

}
//=========================DESC=====================
class CST_Point_Desc : public ConstraintDesc
{
public:
	bool							isMatchDesc(const char * szConstraint);
	Constraint *				createConstraint(const char * szConstraint);
};

bool CST_Point_Desc::isMatchDesc( const char * szConstraint )
{
	F32 x,y,z,ax,ay,az,axis;
	S32 readed = dSscanf(szConstraint,"%g %g %g %g %g %g %g",&x,&y,&z,&ax,&ay,&az,&axis);
	return readed == 3 || readed == 7;
}

Constraint * CST_Point_Desc::createConstraint( const char * szConstraint )
{
	Point3F pos;
	const MatrixF& tmat = MatrixF::Identity;
	tmat.getColumn(3,&pos);
	AngAxisF aa(tmat);

	dSscanf(szConstraint,"%g %g %g %g %g %g %g",
		&pos.x,&pos.y,&pos.z,&aa.axis.x,&aa.axis.y,&aa.axis.z,&aa.angle);

	MatrixF mat;
	aa.setMatrix(&mat);
	mat.setColumn(3,pos);

	CST_Point * pInstance =  new CST_Point();
	pInstance->_mPosition = pos;
	pInstance->_mTranform = mat;

	return pInstance;
}
IMPLEMENT_CSTDESC(CST_Point_Desc);