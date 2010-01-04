#ifndef __RADAR_MAP__
#define __RADAR_MAP__

#include "gui/controls/guiBitmapCtrl.h"

class guiRadarMap : public GuiBitmapCtrl
{
private:
	typedef GuiBitmapCtrl Parent;
public:
	DECLARE_CONOBJECT(guiRadarMap);
	guiRadarMap();
	static void initPersistFields();
	static bool setPlayerBmpName( void *obj, const char *data );
	void onRender(Point2I offset, const RectI &updateRect);
	void setPlayerBmp(const char * bmpName);
protected:
	S32	_m_n_RadaRaius;
	StringTableEntry _m_str_PlayerBmp;
	GFXTexHandle _m_tex_PlayerBmp;

	void renderPlayer(Point2I offset,Point3F selfPos,Point3F otherPos,int type = 0);
	RectI getMapRect(Point3F selfPos);
};

#endif