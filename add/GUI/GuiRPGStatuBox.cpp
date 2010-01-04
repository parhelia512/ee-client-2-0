#include "console/consoleTypes.h"
#include "GuiRPGStatuBox.h"

IMPLEMENT_CONOBJECT(rpgStatusBox);

rpgStatusBox::rpgStatusBox()
{
}

void rpgStatusBox::onMouseDown(const GuiEvent &event)
{
	Parent::onMouseDown(event);
	Con::executef(this, "onMouseDown");
}

void rpgStatusBox::onSleep()
{
	Parent::onSleep();
}

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//