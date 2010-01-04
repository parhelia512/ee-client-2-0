#ifndef __EFFECTWRAPPER_H__
#define __EFFECTWRAPPER_H__

#include "console/simObject.h"
#include "../RPGDefs.h"

class SimDataBlock;
class Constraint;
class EffectWrapper : public SimObject , public RPGDefs
{
	typedef SimObject Parent;
	U32									_nLastTime;//持续时间
	Constraint *						_pConstraint;
public:
	EffectWrapper();
	~EffectWrapper();
	void									ea_setConstraint(Constraint * pConstraint);
	Constraint *						ea_getConstraint();

	void									ea_setLastingTime(const U32 & time);
	const U32 &						ea_getLastingTime();

	virtual bool						ea_SetData( SimDataBlock * pData , ERRORS & error);
	virtual bool						ea_Start(ERRORS & error);
	virtual void						ea_Update(const U32 & dt);
	virtual void						ea_End();
	DECLARE_CONOBJECT(EffectWrapper);
};

#endif