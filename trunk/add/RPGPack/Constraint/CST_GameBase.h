#ifndef __CST_GAMEBASE__
#define __CST_GAMEBASE__

#include "CST_SceneObject.h"

/*
supported constraint string : 
#gamebase.objName									//场景中叫objName的一个sceneObject
*/

class CST_GameBase : public CST_SceneObj
{
	typedef CST_SceneObj Parent;
public:
	CST_GameBase();

	DECLARE_CONOBJECT(CST_GameBase);
};

#endif