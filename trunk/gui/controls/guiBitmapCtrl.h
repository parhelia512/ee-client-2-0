//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIBITMAPCTRL_H_
#define _GUIBITMAPCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

/// Renders a bitmap.
class GuiBitmapCtrl : public GuiControl
{
private:
   typedef GuiControl Parent;

protected:
   static bool setBitmapName( void *obj, const char *data );
   static const char *getBitmapName( void *obj, const char *data );

   String mBitmapName;
   GFXTexHandle mTextureObject;
   Point2I mStartPoint;
   bool mWrap;

public:
   //creation methods
   DECLARE_CONOBJECT(GuiBitmapCtrl);
   DECLARE_CATEGORY( "Gui Images" );
   DECLARE_DESCRIPTION( "A control that displays a single, static image from a file.\n"
                        "The bitmap can either be tiled or stretched inside the control." );
   
   GuiBitmapCtrl();
   static void initPersistFields();

   //Parental methods
   bool onWake();
   void onSleep();
   void inspectPostApply();

   void setBitmap(const char *name,bool resize = false);
   void setBitmapHandle(GFXTexHandle handle, bool resize = false);


   void updateSizing();

   void onRender(Point2I offset, const RectI &updateRect);
   void setValue(S32 x, S32 y);
};

#endif
