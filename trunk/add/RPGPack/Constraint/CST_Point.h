#ifndef __CST_POINT__
#define __CST_POINT__

#include "Constraint.h"

/*
supported constraint string : 
"x y z"											//Ä³Ò»¸ö×ø±ê
"x y z ax ay az axi"						//transform
*/

class CST_Point : public Constraint
{
	typedef Constraint Parent;
public:
	Point3F											_mPosition;
	MatrixF											_mTranform;
	CST_Point();
	~CST_Point();

	const Point3F &					getConstraintPos();
	const MatrixF &					getConstraintTransform();
	DecalInstance*						addGroundDecal(DecalData * decalData);

	DECLARE_CONOBJECT(CST_Point);
};

#endif