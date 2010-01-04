//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/buttons/guiSwatchButtonCtrl.h"

#include "console/console.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "gui/buttons/guiSwatchButtonCtrl.h"
#include "gui/core/guiDefaultControlRender.h"


GuiSwatchButtonCtrl::GuiSwatchButtonCtrl()
 : mSwatchColor( 1, 1, 1, 1 )
{
   mButtonText = StringTable->insert( "" );   
   setExtent(140, 30);
}

IMPLEMENT_CONOBJECT( GuiSwatchButtonCtrl );


void GuiSwatchButtonCtrl::initPersistFields()
{
   addField( "color", TypeColorF, Offset( mSwatchColor, GuiSwatchButtonCtrl), "Foreground color" );
   Parent::initPersistFields();
}

bool GuiSwatchButtonCtrl::onWake()
{      
   if ( !Parent::onWake() )
      return false;

   if ( mGrid.isNull() )
      mGrid.set( "core/art/gui/images/transp_grid", &GFXDefaultGUIProfile, avar("%s() - mGrid (line %d)", __FUNCTION__, __LINE__) );

   return true;
}

void GuiSwatchButtonCtrl::onRender( Point2I offset, const RectI &updateRect )
{
   bool highlight = mMouseOver;
   //bool depressed = mDepressed;

   //ColorI fontColor   = mActive ? (highlight ? mProfile->mFontColorHL : mProfile->mFontColor) : mProfile->mFontColorNA;
   ColorI backColor   = mSwatchColor;
   ColorI borderColor = mActive ? ( highlight ? mProfile->mBorderColorHL : mProfile->mBorderColor ) : mProfile->mBorderColorNA;

   RectI renderRect( offset, getExtent() );
   if ( !highlight )
      renderRect.inset( 1, 1 );      

   GFXDrawUtil *drawer = GFX->getDrawUtil();
   drawer->clearBitmapModulation();

   // Draw background transparency grid texture...
   if ( mGrid.isValid() )
      drawer->drawBitmapStretch( mGrid, renderRect );

   // Draw swatch color as fill...
   drawer->drawRectFill( renderRect, mSwatchColor );

   // Draw any borders...
   drawer->drawRect( renderRect, borderColor );
   //renderBorder( boundsRect, mProfile );   

   // Draw any text...
//    Point2I textPos = offset + mProfile->mTextOffset;
//    if ( depressed )
//       textPos += Point2I(1,1);

//    drawer->setBitmapModulation( fontColor );
//    renderJustifiedText(textPos, getExtent(), mButtonText);
//    drawer->clearBitmapModulation();

   // render the children...
   //renderChildControls( offset, updateRect);
}

