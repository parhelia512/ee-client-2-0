//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUICHECKBOXCTRL_H_
#define _GUICHECKBOXCTRL_H_

#ifndef _GUIBUTTONBASECTRL_H_
#include "gui/buttons/guiButtonBaseCtrl.h"
#endif

class GuiCheckBoxCtrl : public GuiButtonBaseCtrl
{
   typedef GuiButtonBaseCtrl Parent;

   bool mUseInactiveState;

protected:
public:
   S32 mIndent;
   DECLARE_CONOBJECT(GuiCheckBoxCtrl);
   DECLARE_DESCRIPTION( "A toggle button that displays a text label and an on/off checkbox." );
   GuiCheckBoxCtrl();

   void setStateOn(S32 state);
   virtual const char* getScriptValue();

   virtual void onMouseDown(const GuiEvent& event);
   virtual void onMouseUp(const GuiEvent& event);
   virtual void onAction();
   void onRender(Point2I offset, const RectI &updateRect);
   bool onWake();

   static void initPersistFields();

   void autoSize();
};

#endif //_GUI_CHECKBOX_CTRL_H
