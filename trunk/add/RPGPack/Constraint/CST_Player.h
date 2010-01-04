#ifndef __CST_PLAYER__
#define __CST_PLAYER__

#include "CST_ShapeBase.h"

/*
supported constraint string : 
#player.objName										//一个指定的player
*/

class Player;
class CST_Player : public CST_ShapeBase
{
	typedef CST_ShapeBase Parent;
public:
	CST_Player();

	void								setAnimClip(const char * clipName , bool locked = false);

	DECLARE_CONOBJECT(CST_Player);
};

#endif