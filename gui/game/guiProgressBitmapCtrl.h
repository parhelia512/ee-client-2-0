//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GuiProgressBitmapCtrl_H_
#define _GuiProgressBitmapCtrl_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

#ifndef _GUITEXTCTRL_H_
#include "gui/controls/guiTextCtrl.h"
#endif


class GuiProgressBitmapCtrl : public GuiTextCtrl
{
private:
   typedef GuiTextCtrl Parent;

   F32 mProgress;
	StringTableEntry mBitmapName;
   bool  mUseVariable;
   bool  mTile;

public:
   //creation methods
   DECLARE_CONOBJECT(GuiProgressBitmapCtrl);
   GuiProgressBitmapCtrl();

	static void initPersistFields();
	void setBitmap(const char *name);
   
	//console related methods
   virtual const char *getScriptValue();
   virtual void setScriptValue(const char *value);

   void onPreRender();
	void onRender(Point2I offset, const RectI &updateRect);
	bool onWake();
   void onSleep();

	S32 mNumberOfBitmaps;
	S32 mDim;
};

#endif
