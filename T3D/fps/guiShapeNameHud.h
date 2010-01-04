//-----------------------------------------------------------------------------
// Element Engine

//-----------------------------------------------------------------------------

#include "gui/core/guiControl.h"
#include "gui/3d/guiTSControl.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "T3D/gameConnection.h"
#include "T3D/shapeBase.h"

//----------------------------------------------------------------------------
/// Displays name & damage above shape objects.
///
/// This control displays the name and damage value of all named
/// ShapeBase objects on the client.  The name and damage of objects
/// within the control's display area are overlayed above the object.
///
/// This GUI control must be a child of a TSControl, and a server connection
/// and control object must be present.
///
/// This is a stand-alone control and relies only on the standard base GuiControl.
class GuiShapeNameHud : public GuiControl {
	typedef GuiControl Parent;

	// field data
	ColorF   mFillColor;
	ColorF   mFrameColor;
	ColorF   mTextColor;

	F32      mVerticalOffset;
	F32      mDistanceFade;
	bool     mShowFrame;
	bool     mShowFill;

	StringTableEntry _m_str_MarkBmp1; // ÎÊºÅµÄÍ¼Æ¬Ãû
	StringTableEntry _m_str_MarkBmp2; //¸ÐÌ¾ºÅµÄÍ¼Æ¬Ãû
	GFXTexHandle    _m_tex_MarkBmp1;//ÎÊºÅµÄÍ¼Æ¬¾ä±ú
	GFXTexHandle    _m_tex_MarkBmp2;//¸ÐÌ¾ºÅµÄÍ¼Æ¬¾ä±ú
public:
	enum MARKS
	{
		MARK_QUESTION,
		MARK_CANRETURN,
	};
protected:
	void drawName( Point2I offset, const char *buf, F32 opacity);
	void drawMark(Point2I offset , MARKS mark , F32 distance);
public:

	static Vector<SceneObject*>	PlayersInScene;

	GuiShapeNameHud();

	// GuiControl
	virtual void onRender(Point2I offset, const RectI &updateRect);
	static void initPersistFields();

	static bool setMarkBmp1Name( void *obj, const char *data );
	void setMarkBmp1(const char * bmpName);
	static bool setMarkBmp2Name( void *obj, const char *data );
	void setMarkBmp2(const char * bmpName);
	DECLARE_CONOBJECT( GuiShapeNameHud );
};