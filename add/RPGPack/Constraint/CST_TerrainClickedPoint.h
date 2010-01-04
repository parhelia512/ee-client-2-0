#ifndef __CST_CLICKED_POINT__
#define __CST_CLICKED_POINT__

#include "CST_Point.h"

/*
supported constraint string : 
"#terrain_clicked"											//鼠标点击地面
*/

class CST_TerrainClicked : public CST_Point
{
	typedef CST_Point Parent;
public:
	static Point3F				smTerrainClicked;
	DECLARE_CONOBJECT(CST_TerrainClicked);
};

#endif