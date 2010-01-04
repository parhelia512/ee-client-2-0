//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIINPUTCTRL_H_
#define _GUIINPUTCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif
#ifndef _EVENT_H_
#include "platform/event.h"
#endif

class GuiInputCtrl : public GuiControl
{
   private:
      typedef GuiControl Parent;

   public:
      DECLARE_CONOBJECT(GuiInputCtrl);
      DECLARE_CATEGORY( "Gui Other Script" );
      DECLARE_DESCRIPTION( "A control that locks the mouse and reports all input events to script." );

      bool onWake();
      void onSleep();

      bool onInputEvent( const InputEventInfo &event );
};

#endif // _GUI_INPUTCTRL_H
