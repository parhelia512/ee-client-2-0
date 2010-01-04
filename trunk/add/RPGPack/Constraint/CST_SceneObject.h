#ifndef __CST_SCENEOBJ__
#define __CST_SCENEOBJ__

#include "Constraint.h"

/*
supported constraint string : 
#scene.objName									//场景中叫objName的一个sceneObject
*/
class DecalInstance;
class SceneObject;
class CST_SceneObj : public Constraint
{
	typedef Constraint Parent;
	SceneObject *								_mbSceneObj;
	Point3F											_mPosition;
public:
	CST_SceneObj();
	~CST_SceneObj();
	void										setSceneObj(SceneObject * pObj);
	SceneObject *						getSceneObj();
	void										onDeleteNotify(SimObject *object);

	const Point3F &					getConstraintPos();
	const MatrixF &					getConstraintTransform();

	DecalInstance*						addGroundDecal(DecalData * decalData);

	DECLARE_CONOBJECT(CST_SceneObj);
};

#endif