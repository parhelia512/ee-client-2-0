#ifndef __CST_ROLLOVER_POINT__
#define __CST_ROLLOVER_POINT__

#include "CST_Point.h"

/*
supported constraint string : 
"#terrain_rollover"											// Û±Í“∆∂Ø
*/

class CST_TerrainRollover : public CST_Point
{
	typedef CST_Point Parent;
public:
	static Point3F				smTerrainRollover;
	DECLARE_CONOBJECT(CST_TerrainRollover);
};

#endif