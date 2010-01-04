//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIPOPUPCTRL_H_
#define _GUIPOPUPCTRL_H_

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
class GuiPopUpMenuCtrl;
class GuiPopupTextListCtrl;

class GuiPopUpBackgroundCtrl : public GuiControl
{
protected:
   GuiPopUpMenuCtrl *mPopUpCtrl;
   GuiPopupTextListCtrl *mTextList; 
public:
   GuiPopUpBackgroundCtrl(GuiPopUpMenuCtrl *ctrl, GuiPopupTextListCtrl* textList);
   void onMouseDown(const GuiEvent &event);
};

class GuiPopupTextListCtrl : public GuiTextListCtrl
{
   private:
   typedef GuiTextListCtrl Parent;

protected:
   GuiPopUpMenuCtrl *mPopUpCtrl;

public:
   GuiPopupTextListCtrl(); // for inheritance
   GuiPopupTextListCtrl(GuiPopUpMenuCtrl *ctrl);

   // GuiArrayCtrl overload:
   void onCellSelected(Point2I cell);

   // GuiControl overloads:
   bool onKeyDown(const GuiEvent &event);
   void onMouseDown(const GuiEvent &event);
   void onMouseUp(const GuiEvent &event);
   void onRenderCell(Point2I offset, Point2I cell, bool selected, bool mouseOver);
};

class GuiPopUpMenuCtrl : public GuiTextCtrl
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
   GuiPopupTextListCtrl *mTl;
   GuiScrollCtrl *mSc;
   GuiPopUpBackgroundCtrl *mBackground;
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
   StringTableEntry mBitmapName; //  Added
   Point2I mBitmapBounds; //  Added
   GFXTexHandle mTextureNormal; //  Added
   GFXTexHandle mTextureDepressed; //  Added
	S32 mIdMax;

   virtual void addChildren();
   virtual void repositionPopup();

public:
   GuiPopUpMenuCtrl(void);
   ~GuiPopUpMenuCtrl();   
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

   DECLARE_CONOBJECT(GuiPopUpMenuCtrl);
   static void initPersistFields(void);

};

#endif //_GUI_POPUPMENU_CTRL_H
