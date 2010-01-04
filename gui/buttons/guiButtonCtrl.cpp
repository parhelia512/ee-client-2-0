//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/buttons/guiButtonCtrl.h"

#include "console/console.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "gui/core/guiDefaultControlRender.h"


IMPLEMENT_CONOBJECT(GuiButtonCtrl);

GuiButtonCtrl::GuiButtonCtrl()
{
   setExtent(140, 30);
   mButtonText = StringTable->insert("");
}

bool GuiButtonCtrl::onWake()
{
   if( !Parent::onWake() )
      return false;

   // Button Theme?
   if( mProfile->constructBitmapArray() >= 36 )
      mHasTheme = true;
   else
      mHasTheme = false;

   return true;

}
//--------------------------------------------------------------------------

void GuiButtonCtrl::onRender(Point2I      offset,
                             const RectI& updateRect)
{
   bool highlight = mMouseOver;
   bool depressed = mDepressed;

   ColorI fontColor   = mActive ? (highlight ? mProfile->mFontColorHL : mProfile->mFontColor) : mProfile->mFontColorNA;
   ColorI backColor   = mActive ? mProfile->mFillColor : mProfile->mFillColorNA;
   ColorI borderColor = mActive ? mProfile->mBorderColor : mProfile->mBorderColorNA;

   RectI boundsRect(offset, getExtent());

   if( mProfile->mBorder != 0 && !mHasTheme )
   {
      if (mDepressed || mStateOn)
         renderFilledBorder( boundsRect, mProfile->mBorderColorHL, mProfile->mFillColorHL );
      else
         renderFilledBorder( boundsRect, mProfile->mBorderColor, mProfile->mFillColor );
   }
   else if( mHasTheme )
   {
      S32 indexMultiplier = 1;
      
      if( !mActive )
         indexMultiplier = 4;
      else if ( mMouseOver ) 
         indexMultiplier = 3;
      else if ( mDepressed || mStateOn )
         indexMultiplier = 2;

      renderSizableBitmapBordersFilled( boundsRect, indexMultiplier, mProfile );
   }

   Point2I textPos = offset;
   if(depressed)
      textPos += Point2I(1,1);

   GFX->getDrawUtil()->setBitmapModulation( fontColor );
   renderJustifiedText(textPos, getExtent(), mButtonText);

   //render the children
   renderChildControls( offset, updateRect);
}

