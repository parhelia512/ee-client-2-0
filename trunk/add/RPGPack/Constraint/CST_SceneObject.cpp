#include "CST_SceneObject.h"
#include "sceneGraph/sceneObject.h"
#include "T3D/decal/decalData.h"
#include "T3D/decal/decalManager.h"

IMPLEMENT_CONOBJECT(CST_SceneObj);
CST_SceneObj::CST_SceneObj():_mbSceneObj(NULL),
_mPosition(Point3F::Zero)
{

}

const Point3F & CST_SceneObj::getConstraintPos()
{
	if (_mbSceneObj)
	{
		_mPosition = _mbSceneObj->getPosition();
	}
	return _mPosition;
}

const MatrixF & CST_SceneObj::getConstraintTransform()
{
	return _mbSceneObj ? _mbSceneObj->getTransform() : MatrixF::Identity;
}

void CST_SceneObj::setSceneObj( SceneObject * pObj )
{
	if (pObj != _mbSceneObj)
	{
		if(_mbSceneObj)
			clearNotify(_mbSceneObj);
		if(pObj)
			deleteNotify(pObj);
		_mbSceneObj = pObj;
	}
}

void CST_SceneObj::onDeleteNotify( SimObject *object )
{
	if (object == _mbSceneObj)
	{
		_mbSceneObj = NULL;
	}
}

DecalInstance* CST_SceneObj::addGroundDecal( DecalData * decalData )
{
	if (_mbSceneObj && decalData)
	{
		Point3F pos;
		RayInfo rInfo;
		MatrixF mat = _mbSceneObj->getTransform();
		mat.getColumn(3, &pos);
		if (gClientContainer.castRay(Point3F(pos.x, pos.y, pos.z + 0.01f),
			Point3F(pos.x, pos.y, pos.z - 2.0f ),
			STATIC_COLLISION_MASK | VehicleObjectType, &rInfo))
		{
			Point3F normal;
			Point3F tangent;
			mat = _mbSceneObj->getTransform();
			mat.getColumn( 0, &tangent );
			mat.getColumn( 2, &normal );
			DecalInstance* pDecal = gDecalManager->addDecal( rInfo.point, normal, tangent, decalData ,1,0,CustomDecal);      
			return pDecal;
		}
	}
	return NULL;
}

SceneObject * CST_SceneObj::getSceneObj()
{
	return _mbSceneObj;
}

CST_SceneObj::~CST_SceneObj()
{
	setSceneObj(NULL);
}
//=========================DESC=====================
class CST_SceneObj_Desc : public ConstraintDesc
{
public:
	bool							isMatchDesc(const char * szConstraint);
	Constraint *				createConstraint(const char * szConstraint);
};

bool CST_SceneObj_Desc::isMatchDesc( const char * szConstraint )
{
	return dStrstr(szConstraint,"#scene") != NULL;
}

Constraint * CST_SceneObj_Desc::createConstraint( const char * szConstraint )
{
	CST_SceneObj * pInstance = NULL;
	const char * pName = dStrchr(szConstraint,'.');
	SceneObject * pObj = dynamic_cast<SceneObject *>( Sim::findObject(pName) );
	if (pName && pObj)
	{
		pInstance = new CST_SceneObj();
		pInstance->setSceneObj(pObj);
	}
	return pInstance;
}
IMPLEMENT_CSTDESC(CST_SceneObj_Desc);