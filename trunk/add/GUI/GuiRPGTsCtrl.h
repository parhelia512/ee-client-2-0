#ifndef __GUIRPGTS_H__
#define __GUIRPGTS_H__

#include "T3D/gameTSCtrl.h"

class RPGTSCtrl : public GameTSCtrl
{
private:
	typedef GameTSCtrl   Parent;

	Point3F             mMouse3DVec;
	Point3F             mMouse3DPos;

	U32                 mouse_dn_timestamp;

public:
	/*C*/               RPGTSCtrl();

	virtual bool        processCameraQuery(CameraQuery *query);
	virtual void        renderWorld(const RectI &updateRect);
	virtual void        onRender(Point2I offset, const RectI &updateRect);

	virtual void        onMouseUp(const GuiEvent&);
	virtual void        onMouseDown(const GuiEvent&);
	virtual void        onMouseMove(const GuiEvent&);
	virtual void        onMouseDragged(const GuiEvent&);
	virtual void        onMouseEnter(const GuiEvent&);
	virtual void        onMouseLeave(const GuiEvent&);

	virtual bool        onMouseWheelUp(const GuiEvent&);
	virtual bool        onMouseWheelDown(const GuiEvent&);

	virtual void        onRightMouseDown(const GuiEvent&);
	virtual void        onRightMouseUp(const GuiEvent&);
	virtual void        onRightMouseDragged(const GuiEvent&);

	Point3F             getMouse3DVec() {return mMouse3DVec;};   
	Point3F             getMouse3DPos() {return mMouse3DPos;};


	DECLARE_CONOBJECT(RPGTSCtrl);
};

#endif