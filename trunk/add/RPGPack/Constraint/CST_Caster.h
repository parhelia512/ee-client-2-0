#ifndef __CST_CASTER__
#define __CST_CASTER__

#include "CST_Player.h"

/*
supported constraint string : 
#caster										// Õ∑≈’ﬂ
*/

class CST_Caster : public CST_Player
{
	typedef CST_Player Parent;
public:
	CST_Caster();

	void										onCasterAndTargetSetted();

	DECLARE_CONOBJECT(CST_Caster);
};

#endif