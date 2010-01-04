//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIBITMAPBUTTON_H_
#define _GUIBITMAPBUTTON_H_

#ifndef _GUIBUTTONCTRL_H_
#include "gui/buttons/guiButtonCtrl.h"
#endif
#ifndef GFX_Texture_Manager_H_
#include "gfx/gfxTextureManager.h"
#endif

///-------------------------------------
/// Bitmap Button Control
/// Set 'bitmap' console field to base name of bitmaps to use.  This control will
/// append '_n' for normal
/// append '_h' for highlighted
/// append '_d' for depressed
///
/// if bitmap cannot be found it will use the default bitmap to render.
///
/// if the extent is set to (0,0) in the gui editor and apply hit, this control will
/// set it's extent to be exactly the size of the normal bitmap (if present)
///
class GuiBitmapButtonCtrl : public GuiButtonCtrl
{
private:
   typedef GuiButtonCtrl Parent;

protected:
   StringTableEntry mBitmapName;
   GFXTexHandle mTextureNormal;
   GFXTexHandle mTextureHilight;
   GFXTexHandle mTextureDepressed;
   GFXTexHandle mTextureInactive;

   void renderButton(GFXTexHandle &texture, Point2I &offset, const RectI& updateRect);

public:

   DECLARE_CONOBJECT(GuiBitmapButtonCtrl);
   DECLARE_DESCRIPTION( "A button control rendered entirely from bitmaps.\n"
                        "The individual button states are represented with separate bitmaps." );
                        
   GuiBitmapButtonCtrl();

   static void initPersistFields();

   //Parent methods
   bool onWake();
   void onSleep();
   void inspectPostApply();

   void setBitmap(const char *name);
   void setBitmapHandles(GFXTexHandle normal, GFXTexHandle highlighted, GFXTexHandle depressed, GFXTexHandle inactive);

   void onRender(Point2I offset, const RectI &updateRect);
};

class GuiBitmapButtonTextCtrl : public GuiBitmapButtonCtrl
{
   typedef GuiBitmapButtonCtrl Parent;

public:

   DECLARE_CONOBJECT( GuiBitmapButtonTextCtrl );
   DECLARE_DESCRIPTION( "An extension of GuiBitmapButtonCtrl that also renders a text\n"
                        "label on the button." );

   void onRender(Point2I offset, const RectI &updateRect);
};

#endif //_GUI_BITMAP_BUTTON_CTRL_H
