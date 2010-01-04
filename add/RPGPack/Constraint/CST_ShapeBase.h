#ifndef __CST_SHAPEBASE__
#define __CST_SHAPEBASE__

#include "CST_GameBase.h"

/*
supported constraint string : 
#shapebase.objName									//场景中叫objName的一个sceneObject
*/

class CST_ShapeBase : public CST_GameBase
{
	typedef CST_GameBase Parent;
public:
	CST_ShapeBase();

	DECLARE_CONOBJECT(CST_ShapeBase);
};

#endif