//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _GUI_TABBOOKCTRL_H_
#define _GUI_TABBOOKCTRL_H_

#ifndef _GUICONTAINER_H_
#include "gui/containers/guiContainer.h"
#endif

#ifndef _GUITABPAGECTRL_H_
#include "gui/controls/guiTabPageCtrl.h"
#endif


/// Tab Book Control for creation of tabbed dialogs
///
/// @see GUI for an overview of the Torque GUI system.
///
/// @section GuiTabBookCtrl_Intro Introduction
///
/// GuiTabBookCtrl is a container class that holds children of type GuiTabPageCtrl
///
/// GuiTabBookCtrl creates an easy to work with system for creating tabbed dialogs
/// allowing for creation of dialogs that store a lot of information in a small area
/// by separating the information into pages which are changeable by clicking their
/// page title on top or bottom of the control
///
/// tabs may be aligned to be on top or bottom of the book and are changeable while
/// the GUI editor is open for quick switching between pages allowing multi-page dialogs
/// to be edited quickly and easily.
///
/// The control may only contain children of type GuiTabPageCtrl.
/// If a control is added to the Book that is not of type GuiTabPageCtrl, it will be
/// removed and relocated to the currently active page of the control.
/// If there is no active page in the book, the child control will be relocated to the
/// parent of the book.
///
/// @ref GUI has an overview of the GUI system.
///
/// @nosubgrouping
class GuiTabBookCtrl : public GuiContainer
{
public:
	enum TabPosition
	{
		AlignTop,   ///< Align the tabs on the top of the tab book control
		AlignBottom ///< Align the tabs on the bottom of the tab book control
	};

   struct TabHeaderInfo
   {
      GuiTabPageCtrl *Page;
      S32             TextWidth;
      S32             TabRow;
      S32             TabColumn;
      RectI           TabRect;
   };

private:

   typedef GuiContainer Parent;                ///< typedef for parent class access

protected:

   RectI                   mPageRect;        ///< Rectangle of the tab page portion of the control
   RectI                   mTabRect;         ///< Rectangle of the tab portion of the control
   Vector<TabHeaderInfo>   mPages;           ///< Vector of pages contained by the control
   GuiTabPageCtrl*         mActivePage;      ///< Pointer to the active (selected) tab page child control
   GuiTabPageCtrl*         mHoverTab;        ///< Pointer to the tab page that currently has the mouse positioned ontop of its tab
   S32                     mMinTabWidth;     ///< Minimum Width a tab will display as.
   TabPosition             mTabPosition;     ///< Current tab position (see alignment)
   S32                     mTabHeight;       ///< Current tab height
   S32                     mTabWidth;        ///< Current tab width
   S32                     mTabMargin;       ///< Margin left/right of tab text in tab
   S32					   mSelectedPageNum; ///< Current selected tab position
   bool                    mAllowReorder;    ///< Allow the user to reorder tabs by dragging them
   S32                     mFrontTabPadding; ///< Padding to the Left of the first Tab
   bool                    mDraggingTab;
   bool                    mDraggingTabRect;

   enum
   {
	   TabSelected = 0,     ///< Index of selected tab texture
      TabNormal,           ///< Index of normal tab texture
      TabHover,            ///< Index of hover tab texture
      TabEnds,             ///< Index of end lines for horizontal tabs
	   NumBitmaps           ///< Number of bitmaps in this array
   };
   bool  mHasTexture;   ///< Indicates whether we have a texture to render the tabs with
   RectI *mBitmapBounds;///< Array of rectangles identifying textures for tab book

  public:

   GuiTabBookCtrl();
   DECLARE_CONOBJECT(GuiTabBookCtrl);

   static void initPersistFields();

    /// @name Control Events
    /// @{
   bool onAdd();
   void onRemove();
   bool onWake();
   void onSleep();
   void onRender( Point2I offset, const RectI &updateRect );
   /// @}

   /// @name Child events
   /// @{
   void onChildRemoved( GuiControl* child );
   void onChildAdded( GuiControl *child );
   bool reOrder(SimObject* obj, SimObject* target);
   /// @}

   /// @name Rendering methods
   /// @{

   /// Tab rendering routine, iterates through all tabs rendering one at a time
   /// @param   offset   the render offset to accomodate global coords
   /// @param   tabRect  the Rectangle in which these tabs are to be rendered
   void renderTabs( const Point2I &offset, const RectI &tabRect );

   /// Tab rendering subroutine, renders one tab with specified options
   /// @param   tabRect   the rectangle to render the tab into
   /// @param   tab   pointer to the tab page control for which to render the tab
   void renderTab( RectI tabRect, GuiTabPageCtrl* tab );

   /// @}

   /// @name Page Management
   /// @{

   /// Create a new tab page child in the book
   ///
   /// Pages created are not titled and appear with no text on their tab when created.
   /// This may change in the future.
   void addNewPage();

   /// Select a tab page based on an index
   /// @param   index   The index in the list that specifies which page to select
   void selectPage( S32 index );

   /// Select a tab page by a pointer to that page
   /// @param   page   A pointer to the GuiTabPageCtrl to select.  This page must be a child of the tab book.
   void selectPage( GuiTabPageCtrl *page );

   /// Get the current selected tab number
   S32 getSelectedPageNum(){ return mSelectedPageNum; };

   /// Select the Next page in the tab book
   void selectNextPage();

   /// Select the Previous page in the tab book
   void selectPrevPage();

   /// @}

   /// @name Internal Utility Functions
   /// @{

   /// Update ourselves by hooking common GuiControl functionality.
   void setUpdate();

   /// Balance a top/bottom tab row
   void balanceRow( S32 row, S32 totalTabWidth );

   /// Balance a left/right tab column
   void balanceColumn( S32 row, S32 totalTabWidth );

   /// Calculate the tab width of a page, given it's caption
   S32 calculatePageTabWidth( GuiTabPageCtrl *page );

   /// Calculate Page Header Information
   void calculatePageTabs();

   /// Get client area of tab book
   virtual const RectI getClientRect();

   /// Find the tab that was hit by the current event, if any
   /// @param   event   The GuiEvent that caused this function call
   GuiTabPageCtrl *findHitTab( const GuiEvent &event );

   /// Find the tab that was hit, based on a point
   /// @param    hitPoint   A Point2I that specifies where to search for a tab hit
   GuiTabPageCtrl *findHitTab( Point2I hitPoint );

   /// @}

   /// @name Sizing
   /// @{

   /// Rezize our control
   /// This method is overridden so that we may handle resizing of our child tab
   /// pages when we are resized.
   /// This ensures we keep our sizing in sync when we are moved or sized.
   ///
   /// @param   newPosition   The new position of the control
   /// @param   newExtent   The new extent of the control
   bool resize(const Point2I &newPosition, const Point2I &newExtent);

   /// Called when a child page is resized
   /// This method is overridden so that we may handle resizing of our child tab
   /// pages when one of them is resized.
   /// This ensures we keep our sizing in sync when we our children are sized or moved.
   ///
   /// @param   child   A pointer to the child control that has been resized
   void childResized(GuiControl *child);

   /// @}


   virtual bool onKeyDown(const GuiEvent &event);


   /// @name Mouse Events
   /// @{
   virtual void onMouseDown(const GuiEvent &event);
   virtual void onMouseUp(const GuiEvent &event);
   virtual void onMouseDragged(const GuiEvent &event);
   virtual void onMouseMove(const GuiEvent &event);
   virtual void onMouseLeave(const GuiEvent &event);
   virtual bool onMouseDownEditor(const GuiEvent &event, Point2I offset);
   /// @}
};

#endif //_GUI_TABBOOKCTRL_H_
