//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIFORMCTRL_H_
#define _GUIFORMCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

#ifndef _GUI_PANEL_H_
#include "gui/containers/guiPanel.h"
#endif

#ifndef _GUIMENUBAR_H_
#include "gui/editor/guiMenuBar.h"
#endif

#ifndef _GUICANVAS_H_
#include "gui/core/guiCanvas.h"
#endif

#include "console/console.h"
#include "console/consoleTypes.h"

class GuiMenuBar;


/// @addtogroup gui_container_group Containers
///
/// @ingroup gui_group Gui System
/// @{
class GuiFormCtrl : public GuiPanel
{
private:
   typedef GuiPanel Parent;

   Resource<GFont>  mFont;
   StringTableEntry mCaption;
   bool             mCanMove;
   bool             mUseSmallCaption;
   StringTableEntry mSmallCaption;
   StringTableEntry mContentLibrary;
   StringTableEntry mContent;

   Point2I          mThumbSize;

   bool             mHasMenu;
   GuiMenuBar*      mMenuBar;

   bool mMouseMovingWin;

   Point2I mMouseDownPosition;
   RectI mOrigBounds;
   RectI mStandardBounds;

   RectI mCloseButton;
   RectI mMinimizeButton;
   RectI mMaximizeButton;

   bool mMouseOver;
   bool mDepressed;

public:
   GuiFormCtrl();
   virtual ~GuiFormCtrl();

   void setCaption(const char* caption);

   bool resize(const Point2I &newPosition, const Point2I &newExtent);
   void onRender(Point2I offset, const RectI &updateRect);

   bool onAdd();
   bool onWake();
   void onSleep();

   virtual void addObject(SimObject *newObj );

   void onMouseDown(const GuiEvent &event);
   void onMouseUp(const GuiEvent &event);
   void onMouseMove(const GuiEvent &event);
   void onMouseLeave(const GuiEvent &event);
   void onMouseEnter(const GuiEvent &event);

   U32  getMenuBarID();

   static void initPersistFields();
   DECLARE_CONOBJECT(GuiFormCtrl);
};
/// @}

#endif