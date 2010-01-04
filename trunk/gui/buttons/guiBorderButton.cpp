//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"
#include "gui/core/guiCanvas.h"
#include "gui/buttons/guiButtonBaseCtrl.h"
#include "gui/core/guiDefaultControlRender.h"


class GuiBorderButtonCtrl : public GuiButtonBaseCtrl
{
   typedef GuiButtonBaseCtrl Parent;

protected:
public:
   DECLARE_CONOBJECT(GuiBorderButtonCtrl);

   void onRender(Point2I offset, const RectI &updateRect);
};

IMPLEMENT_CONOBJECT(GuiBorderButtonCtrl);

void GuiBorderButtonCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   if ( mProfile->mBorder > 0 )
   {
      RectI bounds( offset, getExtent() );
      for ( S32 i=0; i < mProfile->mBorderThickness; i++ )
      {
         GFX->getDrawUtil()->drawRect( bounds, mProfile->mBorderColor );
         bounds.inset( 1, 1 );
      }      
   }

   if ( mActive )
   {
      if ( mStateOn || mDepressed )
      {
         RectI bounds( offset, getExtent() );
         for ( S32 i=0; i < mProfile->mBorderThickness; i++ )
         {
            GFX->getDrawUtil()->drawRect( bounds, mProfile->mFontColorSEL );
            bounds.inset( 1, 1 );
         }
      }

      if ( mMouseOver )
      {
         RectI bounds( offset, getExtent() );
         for ( S32 i=0; i < mProfile->mBorderThickness; i++ )
         {
            GFX->getDrawUtil()->drawRect( bounds, mProfile->mFontColorHL );
            bounds.inset( 1, 1 );
         }
      }
   }

   renderChildControls( offset, updateRect );
}

