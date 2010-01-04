#include "gui/core/guiControl.h"
#include "console/consoleTypes.h"
#include "T3D/gameConnection.h"
#include "T3D/shapeBase.h"
#include "gfx/gfxDrawUtil.h"


class rpgStatusBar : public GuiControl
{
	typedef GuiControl Parent;

	ColorF            rgba_fill;

	F32               fraction;
	ShapeBase*        shape;
	bool              show_energy;
	bool              monitor_player;

public:
	/*C*/             rpgStatusBar();

	virtual void      onRender(Point2I, const RectI&);

	void              setFraction(F32 frac);
	F32               getFraction() const { return fraction; }

	void              setShape(ShapeBase* s);
	void              clearShape() { setShape(NULL); }

	virtual bool      onWake();
	virtual void      onSleep();
	virtual void      onMouseDown(const GuiEvent &event);
	virtual void      onDeleteNotify(SimObject*);

	static void       initPersistFields();

	DECLARE_CONOBJECT(rpgStatusBar);
};

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

IMPLEMENT_CONOBJECT(rpgStatusBar);

rpgStatusBar::rpgStatusBar()
{
	rgba_fill.set(0.0f, 1.0f, 1.0f, 1.0f);

	fraction = 1.0f;
	shape = 0;
	show_energy = false;
	monitor_player = false;
}

void rpgStatusBar::setFraction(F32 frac)
{
	fraction = mClampF(frac, 0.0f, 1.0f);
}

void rpgStatusBar::setShape(ShapeBase* s) 
{ 
	if (shape)
		clearNotify(shape);
	shape = s;
	if (shape)
		deleteNotify(shape);
}

void rpgStatusBar::onDeleteNotify(SimObject* obj)
{
	if (shape == (ShapeBase*)obj)
	{
		shape = NULL;
		return;
	}

	Parent::onDeleteNotify(obj);
}

bool rpgStatusBar::onWake()
{
	if (!Parent::onWake())
		return false;

	return true;
}

void rpgStatusBar::onSleep()
{
	//clearShape();
	Parent::onSleep();
}

// STATIC 
void rpgStatusBar::initPersistFields()
{
	Parent::initPersistFields();

	addField("fillColor",      TypeColorF, Offset(rgba_fill, rpgStatusBar));
	addField("displayEnergy",  TypeBool,   Offset(show_energy, rpgStatusBar));
	addField("monitorPlayer",  TypeBool,   Offset(monitor_player, rpgStatusBar));
}

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//


void rpgStatusBar::onRender(Point2I offset, const RectI &updateRect)
{
	if (!shape)
		return;

	if (shape->getDamageState() != ShapeBase::Enabled)
		fraction = 0.0f;
	else
		fraction = (show_energy) ? shape->getEnergyValue() : (1.0f - shape->getDamageValue());

	// set alpha value for the fill area
	rgba_fill.alpha = 1.0f;

	// calculate the rectangle dimensions
	RectI rect(updateRect);
	rect.extent.x = (S32)(rect.extent.x*fraction);

	// draw the filled part of bar
	GFX->getDrawUtil()->drawRectFill(rect, rgba_fill);
}

void rpgStatusBar::onMouseDown(const GuiEvent &event)
{
	GuiControl *parent = getParent();
	if (parent)
		parent->onMouseDown(event);
}

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

ConsoleMethod(rpgStatusBar, setProgress, void, 3, 3, "setProgress(percent_done)")
{
	object->setFraction(dAtof(argv[2]));
}

ConsoleMethod(rpgStatusBar, setShape, void, 3, 3, "setShape(shape)")
{
	object->setShape(dynamic_cast<ShapeBase*>(Sim::findObject(argv[2])));
}

ConsoleMethod(rpgStatusBar, clearShape, void, 2, 2, "clearShape()")
{
	object->clearShape();
}


//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//
