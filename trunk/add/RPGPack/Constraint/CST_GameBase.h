#ifndef __CST_GAMEBASE__
#define __CST_GAMEBASE__

#include "CST_SceneObject.h"

/*
supported constraint string : 
#gamebase.objName									//�����н�objName��һ��sceneObject
*/

class CST_GameBase : public CST_SceneObj
{
	typedef CST_SceneObj Parent;
public:
	CST_GameBase();

	DECLARE_CONOBJECT(CST_GameBase);
};

#endif