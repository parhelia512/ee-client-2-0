#include "GuiRPGStatuLabel.h"

IMPLEMENT_CONOBJECT(rpgStatusLabel);

rpgStatusLabel::rpgStatusLabel()
{
}

void rpgStatusLabel::onMouseDown(const GuiEvent &event)
{
	GuiControl *parent = getParent();
	if (parent)
		parent->onMouseDown(event);
}