//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIWINDOWCOLLAPSECTRL_H_
#define _GUIWINDOWCOLLAPSECTRL_H_

#ifndef _GUIWINDOWCTRL_H_
#include "gui/containers/guiWindowCtrl.h"
#endif

#ifndef _GUICONTAINER_H_
#include "gui/containers/guiContainer.h"
#endif

/// @addtogroup gui_container_group Containers
///
/// @ingroup gui_group Gui System
/// @{
class GuiWindowCollapseCtrl : public GuiWindowCtrl
{
   private:
		typedef GuiWindowCtrl Parent;

		
		S32 mCollapseGroup;
		S32 mCollapseGroupNum;
		S32 mPreCollapsedYExtent;
		S32 mPreCollapsedYMinExtent;

	public:
      GuiWindowCollapseCtrl();
      DECLARE_CONOBJECT(GuiWindowCollapseCtrl);
      static void initPersistFields();
		
		bool mIsCollapsed;
		bool mIsMouseResizing;

		// Parent Methods
      void getSnappableWindows( Vector<EdgeRectI> &outVector, Vector<GuiWindowCollapseCtrl*> &windowOutVector);
		virtual void parentResized(const RectI &oldParentRect, const RectI &newParentRect);
		void onMouseDown(const GuiEvent &event);
      void onMouseDragged(const GuiEvent &event);
      void onMouseUp(const GuiEvent &event);
		S32 getCollapseGroupNum() { return mCollapseGroupNum; }

		void moveFromCollapseGroup();
		void moveToCollapseGroup(GuiWindowCollapseCtrl* hitWindow, bool orientation);
		void moveWithCollapseGroup(Point2I deltaMousePosition);
	   
      void setCollapseGroup(bool state);
		void toggleCollapseGroup();
		bool resizeCollapseGroup(bool resizeX, bool resizeY, Point2I resizePos, Point2I resizeWidth);
		void refreshCollapseGroups();

protected:
      void handleCollapseGroup();
};
/// @}

#endif //_GUI_WINDOWCOLLAPSE_CTRL_H
