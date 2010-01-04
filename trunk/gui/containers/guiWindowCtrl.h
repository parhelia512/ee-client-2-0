//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIWINDOWCTRL_H_
#define _GUIWINDOWCTRL_H_

#ifndef _GUICONTAINER_H_
#include "gui/containers/guiContainer.h"
#endif

/// @addtogroup gui_container_group Containers
///
/// @ingroup gui_group Gui System
/// @{
class GuiWindowCtrl : public GuiContainer
{
	protected:
		typedef GuiContainer Parent;

      bool mResizeWidth;
      bool mResizeHeight;
      bool mCanMove;
      bool mCanClose;
      bool mCanMinimize;
      bool mCanMaximize;
      bool mCanDock; ///< Show a docking button on the title bar?
      bool mEdgeSnap; ///< Should this window snap to other windows edges?
      bool mPressClose;
      bool mPressMinimize;
      bool mPressMaximize;

		bool mRepositionWindow;
		bool mResizeWindow;
		bool mSnapSignal;

      Point2I mMinSize;

      StringTableEntry mCloseCommand;
      StringTableEntry mText;
      S32 mResizeEdge; ///< Resizing Edges Mask (See Edges Enumeration)

      S32 mTitleHeight;

      F32 mResizeMargin;

      bool mMouseMovingWin;
      bool mMouseResizeWidth;
      bool mMouseResizeHeight;
      bool mMinimized;
      bool mMaximized;

      Point2I mMouseDownPosition;
      RectI mOrigBounds;
      RectI mStandardBounds;

      RectI mCloseButton;
      RectI mMinimizeButton;
      RectI mMaximizeButton;
      S32 mMinimizeIndex;
      S32 mTabIndex;

      void PositionButtons(void);

   protected:
      enum BitmapIndices
      {
         BmpClose,
         BmpMaximize,
         BmpNormal,
         BmpMinimize,

         BmpCount
      };
      enum {
         BorderTopLeftKey = 12,
         BorderTopRightKey,
         BorderTopKey,
         BorderTopLeftNoKey,
         BorderTopRightNoKey,
         BorderTopNoKey,
         BorderLeft,
         BorderRight,
         BorderBottomLeft,
         BorderBottom,
         BorderBottomRight,
         NumBitmaps
      };

      enum BitmapStates
      {
         BmpDefault = 0,
         BmpHilite,
         BmpDisabled,

         BmpStates
      };
      RectI *mBitmapBounds;  //bmp is [3*n], bmpHL is [3*n + 1], bmpNA is [3*n + 2]
      GFXTexHandle mTextureObject;


      /// Window Edge Bit Masks
      ///
      /// Edges can be combined to create a mask of multiple edges.  
      /// This is used for hit detection throughout this class.
      enum Edges
      {
         edgeNone   = 0,      ///< No Edge
         edgeTop    = BIT(1), ///< Top Edge
         edgeLeft   = BIT(2), ///< Left Edge
         edgeRight  = BIT(3), ///< Right Edge
         edgeBottom = BIT(4)  ///< Bottom Edge
      };

   public:
      GuiWindowCtrl();
      DECLARE_CONOBJECT(GuiWindowCtrl);
      static void initPersistFields();

      bool onWake();
      void onSleep();
		virtual void parentResized(const RectI &oldParentRect, const RectI &newParentRect);

      bool isMinimized(S32 &index);

      virtual void getCursor(GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent);

      void setFont(S32 fntTag);

      void setCloseCommand(const char *newCmd);

      GuiControl* findHitControl(const Point2I &pt, S32 initialLayer = -1);
      S32 findHitEdges( const Point2I &globalPoint );
      void getSnappableWindows( Vector<EdgeRectI> &outVector, Vector<GuiWindowCtrl*> &windowOutVector);
      bool resize(const Point2I &newPosition, const Point2I &newExtent);

      virtual void onMouseDown(const GuiEvent &event);
      virtual void onMouseDragged(const GuiEvent &event);
      virtual void onMouseUp(const GuiEvent &event);

      //only cycle tabs through the current window, so overwrite the method
      GuiControl* findNextTabable(GuiControl *curResponder, bool firstCall = true);
      GuiControl* findPrevTabable(GuiControl *curResponder, bool firstCall = true);

      bool onKeyDown(const GuiEvent &event);

      S32 getTabIndex(void) { return mTabIndex; }
      void selectWindow(void);

      void onRender(Point2I offset, const RectI &updateRect);

      ////
      const RectI getClientRect();

      /// Mutators for window properties from code.
      /// Using setDataField is a bit overkill.
      void setMobility( bool canMove, bool canClose, bool canMinimize, bool canMaximize, bool canDock, bool edgeSnap )
      {
         mCanMove = canMove;
         mCanClose = canClose;
         mCanMinimize = canMinimize;
         mCanMaximize = canMaximize;
         mCanDock = canDock;
         mEdgeSnap = edgeSnap;
      }

      /// Mutators for window properties from code.
      /// Using setDataField is a bit overkill.
      void setCanResize( bool canResizeWidth, bool canResizeHeight )
      {
         mResizeWidth = canResizeWidth;
         mResizeHeight = canResizeHeight;
      }

      // Allows the text to be set
      void setText( const char* text )
      {
         mText = StringTable->insert(text);
      }
};
/// @}

#endif //_GUI_WINDOW_CTRL_H
