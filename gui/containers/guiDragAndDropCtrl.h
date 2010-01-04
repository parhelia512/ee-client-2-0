//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIDRAGANDDROPCTRL_H_
#define _GUIDRAGANDDROPCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

#include "gfx/gfxDevice.h"
#include "console/console.h"
#include "console/consoleTypes.h"

class GuiDragAndDropControl : public GuiControl
{
private:
   typedef GuiControl Parent;

   // The mouse down offset from the upper left of the control.
   Point2I mOffset;
   bool mDeleteOnMouseUp;

   // Controls may want to react when they are dragged over, entered or exited.
   GuiControl* mLastTarget;
   
   // Convenience methods for sending events
   void sendDragEvent(GuiControl* target, const char* event);
   GuiControl* findDragTarget(Point2I mousePoint, const char* method);

public:
   GuiDragAndDropControl() { }

   void startDragging(Point2I offset = Point2I(0, 0));

   virtual void onMouseDown(const GuiEvent& event);
   virtual void onMouseDragged(const GuiEvent& event);
   virtual void onMouseUp(const GuiEvent& event);

   static void initPersistFields();

   DECLARE_CONOBJECT( GuiDragAndDropControl );
   DECLARE_CATEGORY( "Gui Other" );
   DECLARE_DESCRIPTION( "A special control that implements drag&drop behavior.\n"
                        "The control will notify other controls as it moves across the canvas.\n"
                        "Content can be attached through dynamic fields or child objects." );
};

#endif