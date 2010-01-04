// Revision History:
// January 3, 2004		David Wyand		Added onMouseEnter() and onMouseLeave() methods to
//										GuiPopUpMenuCtrl class.  Also added the bool mMouseOver
//										to the same class.
// January 3, 2004		David Wyand		Added the bool mBackgroundCancel to the GuiPopUpMenuCtrl
//										class.
// May 19, 2004			David Wyand		Added the bool usesColorBox and ColorI colorbox to the
//										Entry structure of the GuiPopUpMenuCtrl class.  These
//										are used to draw a coloured rectangle beside the text in
//										the list.
// November 16, 2005	David Wyand		Added the method setNoneSelected() to set none of the
//										items as selected.  Use this over setSelected(-1); when
//										there are negative IDs in the item list.
// January 11, 2006		David Wyand		Added the method setFirstSelected() to set the first
//										item to be the selected one.  Handy when you don't know
//										the ID of the first item.
//
//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIPOPUPCTRLEX_H_
#define _GUIPOPUPCTRLEX_H_

#ifndef _GUITEXTCTRL_H_
#include "gui/controls/guiTextCtrl.h"
#endif
#ifndef _GUITEXTLISTCTRL_H_
#include "gui/controls/guiTextListCtrl.h"
#endif
#ifndef _GUIBUTTONCTRL_H_
#include "gui/buttons/guiButtonCtrl.h"
#endif
#ifndef _GUIBACKGROUNDCTRL_H_
#include "gui/controls/guiBackgroundCtrl.h"
#endif
#ifndef _GUISCROLLCTRL_H_
#include "gui/containers/guiScrollCtrl.h"
#endif
class GuiPopUpMenuCtrlEx;
class GuiPopupTextListCtrlEx;

class GuiPopUpBackgroundCtrlEx : public GuiControl
{
protected:
   GuiPopUpMenuCtrlEx *mPopUpCtrl;
   GuiPopupTextListCtrlEx *mTextList; 
public:
   GuiPopUpBackgroundCtrlEx(GuiPopUpMenuCtrlEx *ctrl, GuiPopupTextListCtrlEx* textList);
   void onMouseDown(const GuiEvent &event);
};

class GuiPopupTextListCtrlEx : public GuiTextListCtrl
{
   private:
      typedef GuiTextListCtrl Parent;

      bool hasCategories();

   protected:
      GuiPopUpMenuCtrlEx *mPopUpCtrl;

   public:
      GuiPopupTextListCtrlEx(); // for inheritance
      GuiPopupTextListCtrlEx(GuiPopUpMenuCtrlEx *ctrl);

      // GuiArrayCtrl overload:
      void onCellSelected(Point2I cell);

      // GuiControl overloads:
      bool onKeyDown(const GuiEvent &event);
      void onMouseDown(const GuiEvent &event);
      void onMouseUp(const GuiEvent &event);
      void onMouseMove(const GuiEvent &event);
      void onRenderCell(Point2I offset, Point2I cell, bool selected, bool mouseOver);
};

class GuiPopUpMenuCtrlEx : public GuiTextCtrl
{
   typedef GuiTextCtrl Parent;

  public:
   struct Entry
   {
      char buf[256];
      S32 id;
      U16 ascii;
      U16 scheme;
	  bool usesColorBox;	//  Added
	  ColorI colorbox;		//  Added
   };

   struct Scheme
   {
      U32      id;
      ColorI   fontColor;
      ColorI   fontColorHL;
      ColorI   fontColorSEL;
   };

   bool mBackgroundCancel; //  Added

  protected:
   GuiPopupTextListCtrlEx *mTl;
   GuiScrollCtrl *mSc;
   GuiPopUpBackgroundCtrlEx *mBackground;
   Vector<Entry> mEntries;
   Vector<Scheme> mSchemes;
   S32 mSelIndex;
   S32 mMaxPopupHeight;
   F32 mIncValue;
   F32 mScrollCount;
   S32 mLastYvalue;
   GuiEvent mEventSave;
   S32 mRevNum;
   bool mInAction;
   bool mReplaceText;
   bool mMouseOver; //  Added
   bool mRenderScrollInNA; //  Added
   bool mReverseTextList;	//  Added - Should we reverse the text list if we display up?
   bool mHotTrackItems;
   StringTableEntry mBitmapName; //  Added
   Point2I mBitmapBounds; //  Added
   GFXTexHandle mTextureNormal; //  Added
   GFXTexHandle mTextureDepressed; //  Added
	S32 mIdMax;

   virtual void addChildren();
   virtual void repositionPopup();

  public:
   GuiPopUpMenuCtrlEx(void);
   ~GuiPopUpMenuCtrlEx();   
   GuiScrollCtrl::Region mScrollDir;
   bool onWake(); //  Added
   bool onAdd();
   void onSleep();
   void setBitmap(const char *name); //  Added
   void sort();
   void sortID(); //  Added
	void addEntry(const char *buf, S32 id = -1, U32 scheme = 0);
   void addScheme(U32 id, ColorI fontColor, ColorI fontColorHL, ColorI fontColorSEL);
   void onRender(Point2I offset, const RectI &updateRect);
   void onAction();
   virtual void closePopUp();
   void clear();
	void clearEntry( S32 entry ); //  Added
   void onMouseDown(const GuiEvent &event);
   void onMouseUp(const GuiEvent &event);
   void onMouseEnter(const GuiEvent &event); //  Added
   void onMouseLeave(const GuiEvent &); //  Added
   void setupAutoScroll(const GuiEvent &event);
   void autoScroll();
   bool onKeyDown(const GuiEvent &event);
   void reverseTextList();
   bool getFontColor(ColorI &fontColor, S32 id, bool selected, bool mouseOver);
   bool getColoredBox(ColorI &boxColor, S32 id); //  Added

   S32 getSelected();
   void setSelected(S32 id, bool bNotifyScript = true);
   void setFirstSelected(bool bNotifyScript = true); //  Added
   void setNoneSelected();	//  Added
   const char *getScriptValue();
   const char *getTextById(S32 id);
   S32 findText( const char* text );
   S32 getNumEntries()   { return( mEntries.size() ); }
   void replaceText(S32);
   
   DECLARE_CONOBJECT(GuiPopUpMenuCtrlEx);
   static void initPersistFields(void);

};

#endif //_GUIIMPROVEDPOPUPCTRL_H_
