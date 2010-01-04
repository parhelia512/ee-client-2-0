//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUISLIDERCTRL_H_
#define _GUISLIDERCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

class GuiSliderCtrl : public GuiControl
{
private:
   typedef GuiControl Parent;

protected:
   Point2F mRange;
   U32  mTicks;
   F32  mValue;
   RectI   mThumb;
   Point2I mThumbSize;
   void updateThumb( F32 value, bool snap = true, bool onWake = false, bool doCallback = true );
   S32 mShiftPoint;
   S32 mShiftExtent;
   F32 mIncAmount;
   bool mDisplayValue;
   bool mDepressed;
   bool mMouseOver;
   bool mHasTexture;

   enum
   {
	   SliderLineLeft = 0,
	   SliderLineCenter,
	   SliderLineRight,
	   SliderButtonNormal,
	   SliderButtonHighlight,
	   NumBitmaps
   };
   	RectI *mBitmapBounds;

public:
   //creation methods
   DECLARE_CONOBJECT(GuiSliderCtrl);
   DECLARE_CATEGORY( "Gui Values" );
   DECLARE_DESCRIPTION( "A control that implements a horizontal or vertical slider to\n"
                        "select/represent values in a certain range." )
   
   GuiSliderCtrl();
   static void initPersistFields();

   //Parental methods
   bool onWake();

   void onMouseDown(const GuiEvent &event);
   void onMouseDragged(const GuiEvent &event);
   void onMouseUp(const GuiEvent &);
   void onMouseLeave(const GuiEvent &);
   void onMouseEnter(const GuiEvent &);
   bool onMouseWheelUp(const GuiEvent &event);
   bool onMouseWheelDown(const GuiEvent &event);
   
   void setActive( bool value );

   const Point2F& getRange() const { return mRange; }
   F32 getValue() const { return mValue; }
   void setScriptValue(const char *val) { setValue(dAtof(val)); }
   void setValue(F32 val);

   void onRender(Point2I offset, const RectI &updateRect);
};

#endif
