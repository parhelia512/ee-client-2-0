//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUI_TREEVIEWCTRL_H
#define _GUI_TREEVIEWCTRL_H

#include "core/bitSet.h"
#include "math/mRect.h"
#include "gfx/gFont.h"
#include "gui/core/guiControl.h"
#include "gui/core/guiArrayCtrl.h"

//------------------------------------------------------------------------------

class GuiTreeViewCtrl : public GuiArrayCtrl
{
   private:
      typedef GuiArrayCtrl Parent;

   public:
      /// @section GuiControl_Intro Introduction
      /// @nosubgrouping

      ///
      struct Item
      {

         enum ItemState
         {
            Selected       = BIT(0),
            Expanded       = BIT(1),
            Marked         = BIT(2), ///< Marked items are drawn with a border around them. This is
                                     ///  different than "Selected" because it can only be set by script.
            MouseOverBmp   = BIT(3),
            MouseOverText  = BIT(4),
            InspectorData  = BIT(5), ///< Set if we're representing some inspector
                                     /// info (ie, use mInspectorInfo, not mScriptInfo)

            VirtualParent  = BIT(6), ///< This indicates that we should be rendered as
                                     ///  a parent even though we don't have any children.
                                     ///  This is useful for preventing scenarios where
                                     ///  we might want to create thousands of
                                     ///  Items that might never be shown (for instance
                                     ///  if we're browsing the object hierarchy in
                                     ///  Torque, which might have thousands of objects).

            InternalNameOnly = BIT(7), ///< Only use the SimObject internal names for the item text.
				ObjectNameOnly   = BIT(8), ///< Only use the SimObject name for the item text
         };

         BitSet32                mState;
         SimObjectPtr<GuiControlProfile> mProfile;
         S16                     mId;
         U16                     mTabLevel;
         Item *                  mParent;
         Item *                  mChild;
         Item *                  mNext;
         Item *                  mPrevious;
         String                  mTooltip;
         S32                     mIcon; //stores the icon that will represent the item in the tree
         S32                     mDataRenderWidth; /// this stores the pixel width needed
                                                   /// to render the item's data in the 
                                                   /// onRenderCell function to optimize
                                                   /// for speed.

         Item( GuiControlProfile *pProfile );
         ~Item();

         struct ScriptTag
         {
            S8                      mNormalImage;
            S8                      mExpandedImage;
            StringTableEntry        mText;
            StringTableEntry        mValue;
         } mScriptInfo;
         struct InspectorTag
         {
            SimObjectPtr<SimObject> mObject;
         } mInspectorInfo;

         /// @name Get Methods
         /// @{

         ///
         const S8 getNormalImage() const;
         const S8 getExpandedImage() const;
         StringTableEntry getText();
         StringTableEntry getValue();
         inline const S16 getID() const { return mId; };
         SimObject *getObject();
         const U32 getDisplayTextLength();
         const S32 getDisplayTextWidth(GFont *font);
         void getDisplayText(U32 bufLen, char *buf);
         /// @}


         /// @name Set Methods
         /// @{

         /// Set whether an item is expanded or not (showing children or having them hidden)
         void setExpanded(const bool f);
         /// Set the image to display when an item IS expanded
         void setExpandedImage(const S8 id);
         /// Set the image to display when an item is NOT expanded
         void setNormalImage(const S8 id);
         /// Assign a SimObject pointer to an inspector data item
         void setObject(SimObject *obj);
         /// Set the items displayable text (caption)
         void setText(StringTableEntry txt);
         /// Set the items script value (data)
         void setValue(StringTableEntry val);
         /// Set the items virtual parent flag
         void setVirtualParent( bool value );
         /// @}


         /// @name State Retrieval
         /// @{

         /// Returns true if this item is expanded. For
         /// inspector objects, the expansion is stored
         /// on the SimObject, for other things we use our
         /// bit vector.
         const bool isExpanded() const;

         /// Returns true if an item is inspector data
         /// or false if it's just an item.
         inline const bool isInspectorData() const { return mState.test(InspectorData); };

         /// Returns true if we should show the expand art
         /// and make the item interact with the mouse as if
         /// it were a parent.
         const bool isParent() const;
         /// @}

         /// @name Searching Methods
         /// @{
         
         /// Find a regular data item by it's script name.
         Item* findChildByName( const char* name );

         /// Find an inspector data item by it's SimObject pointer
         Item *findChildByValue(const SimObject *obj);

         /// Find a regular data item by it's script value
         Item *findChildByValue(StringTableEntry Value);
         /// @}
         
         /// Sort the childs of the item by their text.
         ///
         /// @param caseSensitive If true, sorting is case-sensitive.
         /// @param traverseHierarchy If true, also triggers a sort() on all child items.
         /// @param parentsFirst If true, parents are grouped before children in the resulting sort.
         void sort( bool caseSensitive = true, bool traverseHierarchy = false, bool parentsFirst = false );

      };

      /// @name Enums
      /// @{

      ///
      enum TreeState
      {
         RebuildVisible    = BIT(0), ///< Temporary flag, we have to rebuild the tree.
         IsInspector       = BIT(1), ///< We are mapping a SimObject hierarchy.
         IsEditable        = BIT(2), ///< We allow items to be moved around.
         ShowTreeLines     = BIT(3), ///< Should we render tree lines or just icons?
         BuildingVisTree   = BIT(4), ///< We are currently building the visible tree (prevent recursion)
      };

protected:
  		enum
		{
         MaxIcons = 32,
		};

      enum Icons
      {
         Default1 = 0,
         SimGroup1,
         SimGroup2,
         SimGroup3,
         SimGroup4,         
         Hidden,         
         Lock1,
         Lock2,         
         Default,
         Icon31,
         Icon32
      };

      enum mDragMidPointFlags
      {
            NomDragMidPoint,
            AbovemDragMidPoint,
            BelowmDragMidPoint
      };

      ///
      enum HitFlags
      {
         OnIndent       = BIT(0),
         OnImage        = BIT(1),
         OnText         = BIT(2),
         OnRow          = BIT(3),
      };

      ///
      enum BmpIndices
      {
         BmpDunno,
         BmpLastChild,
         BmpChild,
         BmpExp,
         BmpExpN,
         BmpExpP,
         BmpExpPN,
         BmpCon,
         BmpConN,
         BmpConP,
         BmpConPN,
         BmpLine,
         BmpGlow,
      };


      /// @}
public:

      ///
      Vector<Item*>           mItems;
      Vector<Item*>           mVisibleItems;
      Vector<Item*>           mSelectedItems;
      Vector<S32>             mSelected;     ///< Used for tracking stuff that was
                                             ///  selected, but may not have been
                                             ///  created at time of selection
      S32                     mItemCount;
      Item *                  mItemFreeList; ///< We do our own free list, as we
                                             ///  we want to be able to recycle
                                             ///  item ids and do some other clever
                                             ///  things.
      Item *                  mRoot;
      S32                     mMaxWidth;
      S32                     mSelectedItem;
      S32                     mDraggedToItem;
      S32                     mTempItem;
      S32                     mStart;
      BitSet32                mFlags;

protected:
      GFXTexHandle mIconTable[MaxIcons];

      // for debugging
      bool mDebug;

      S32               mTabSize;
      S32               mTextOffset;
      bool              mFullRowSelect;
      S32               mItemHeight;
      bool              mDestroyOnSleep;
      bool              mSupportMouseDragging;
      bool              mMultipleSelections;
      bool              mDeleteObjectAllowed;
      bool              mDragToItemAllowed;
      bool              mClearAllOnSingleSelection;   ///< When clicking on an already selected item, clear all other selections
      bool              mCompareToObjectID;

      /// Used to hide the root tree 
      /// element, defaults to true.
      bool mShowRoot;

      /// If true InspectorData items only show 
      /// the SimObject internal names.
      bool mInternalNamesOnly;

		/// If true InspectorData items only show 
      /// the SimObject names.
		bool mObjectNamesOnly;

      /// If true then tooltips will be automatically
      /// generated for all Inspector items
      bool mUseInspectorTooltips;

      /// If true then only render item tooltips if the item
      /// extends past the displayable width
      bool mTooltipOnWidthOnly;

      S32               mOldDragY;
      S32               mCurrentDragCell;
      S32               mPreviousDragCell;
      S32               mDragMidPoint;
      bool              mMouseDragged;
      bool              mDragStartInSelection;

      StringTableEntry  mBitmapBase;
      GFXTexHandle	   mTexRollover;
      GFXTexHandle	   mTexSelected;

      ColorI   mAltFontColor;
      ColorI   mAltFontColorHL;
      ColorI   mAltFontColorSE;

      SimObjectPtr<SimObject> mRootObject;

      void destroyChildren(Item * item, Item * parent);
      void destroyItem(Item * item);
      void destroyTree();

      void deleteItem(Item *item);

      void buildItem(Item * item, U32 tabLevel, bool bForceFullUpdate = false);

      bool hitTest(const Point2I & pnt, Item* & item, BitSet32 & flags);

      S32 getInspectorItemIconsWidth(Item* & item);

      virtual bool onVirtualParentBuild(Item *item, bool bForceFullUpdate = false);
      virtual bool onVirtualParentExpand(Item *item);
      virtual bool onVirtualParentCollapse(Item *item);
      virtual void onItemSelected( Item *item );
      virtual void onAddSelection( Item *item );
      virtual void onRemoveSelection( Item *item );
      virtual void onClearSelection() {};

      void addInspectorDataItem(Item *parent, SimObject *obj);

   public:
      GuiTreeViewCtrl();
      virtual ~GuiTreeViewCtrl();

      /// Used for syncing the mSelected and mSelectedItems lists.
      void syncSelection();

      void lockSelection(bool lock);
      void hideSelection(bool hide);
      virtual bool canAddSelection( Item *item ) { return true; };
      void addSelection(S32 itemId, bool update = true );

      bool isSelected(S32 itemId);
      bool isSelected(Item *item);

      /// Should use addSelection and removeSelection when calling from script
      /// instead of setItemSelected. Use setItemSelected when you want to select
      /// something in the treeview as it has script call backs.
      void removeSelection(S32 itemId);

      /// Sets the flag of the item with the matching itemId.
      bool setItemSelected(S32 itemId, bool select);
      bool setItemExpanded(S32 itemId, bool expand);
      bool setItemValue(S32 itemId, StringTableEntry Value);

      const char * getItemText(S32 itemId);
      const char * getItemValue(S32 itemId);
      StringTableEntry getTextToRoot(S32 itemId, const char *delimiter = "");

      Item * getItem(S32 itemId);
      Item * createItem(S32 icon);
      bool editItem( S32 itemId, const char* newText, const char* newValue );

      bool markItem( S32 itemId, bool mark );
      
      bool isItemSelected( S32 itemId );

      // insertion/removal
      void unlinkItem(Item * item);
      S32 insertItem(S32 parentId, const char * text, const char * value = "", const char * iconString = "", S16 normalImage = 0, S16 expandedImage = 1);
      bool removeItem(S32 itemId);
      void removeAllChildren(S32 itemId); // Remove all children of the given item

      bool buildIconTable(const char * icons);

      bool setAddGroup(SimObject * obj);

      S32 getIcon(const char * iconString);

      // tree items
      const S32 getFirstRootItem() const;
      S32 getChildItem(S32 itemId);
      S32 getParentItem(S32 itemId);
      S32 getNextSiblingItem(S32 itemId);
      S32 getPrevSiblingItem(S32 itemId);
      S32 getItemCount();
      S32 getSelectedItem();
      S32 getSelectedItem(S32 index); // Given an item's index in the selection list, return its itemId
      S32 getSelectedItemsCount() {return mSelectedItems.size();} // Returns the number of selected items
      void moveItemUp( S32 itemId );
      void moveItemDown( S32 itemId );

      // misc.
      bool scrollVisible( Item *item );
      bool scrollVisible( S32 itemId );
      bool scrollVisibleByObjectId( S32 objID );
      
      void deleteSelection();
      void clearSelection();

      S32 findItemByName(const char *name);
      S32 findItemByObjectId(S32 iObjId);

      // GuiControl
      bool onWake();
      void onSleep();
      void onPreRender();
      bool onKeyDown( const GuiEvent &event );
		void onMouseDown(const GuiEvent &event);
      void onMiddleMouseDown(const GuiEvent &event);
      void onMouseMove(const GuiEvent &event);
      void onMouseEnter(const GuiEvent &event);
      void onMouseLeave(const GuiEvent &event);
      void onRightMouseDown(const GuiEvent &event);
      void onRightMouseUp(const GuiEvent &event);
      void onMouseDragged(const GuiEvent &event);
      virtual void onMouseUp(const GuiEvent &event);

      /// Returns false if the object is a child of one of the inner items.
      bool childSearch(Item * item, SimObject *obj, bool yourBaby);

      /// Find immediately available inspector items (eg ones that aren't children of other inspector items)
      /// and then update their sets
      void inspectorSearch(Item * item, Item * parent, SimSet * parentSet, SimSet * newParentSet);

		/// Find the Item associated with a sceneObject, returns true if it found one
		bool objectSearch( const SimObject *object, Item **item );

      // GuiArrayCtrl
      void onRenderCell(Point2I offset, Point2I cell, bool, bool);
      void onRender(Point2I offset, const RectI &updateRect);
      
      bool renderTooltip( const Point2I &hoverPos, const Point2I& cursorPos, const char* tipText );

      static void initPersistFields();

      void inspectObject(SimObject * obj, bool okToEdit);
      void buildVisibleTree(bool bForceFullUpdate = false);

      DECLARE_CONOBJECT(GuiTreeViewCtrl);
      DECLARE_CATEGORY( "Gui Lists" );
      DECLARE_DESCRIPTION( "Hierarchical list of text items with optional icons.\nCan also be used to inspect SimObject hierarchies." );
};

#endif
