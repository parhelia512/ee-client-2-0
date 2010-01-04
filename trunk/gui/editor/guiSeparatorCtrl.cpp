//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/editor/guiSeparatorCtrl.h"

#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "gui/core/guiDefaultControlRender.h"

IMPLEMENT_CONOBJECT(GuiSeparatorCtrl);

static const EnumTable::Enums separatorTypeEnum[] =
{
   { GuiSeparatorCtrl::separatorTypeVertical, "Vertical"  },
   { GuiSeparatorCtrl::separatorTypeHorizontal,"Horizontal" }
};
static const EnumTable gSeparatorTypeTable(2, &separatorTypeEnum[0]);


//--------------------------------------------------------------------------
GuiSeparatorCtrl::GuiSeparatorCtrl() : GuiControl()
{
   mInvisible = false;
   mText = StringTable->insert("");
   mTextLeftMargin = 0;
   mMargin = 2;
   setExtent( 12, 35 );
   mSeparatorType = GuiSeparatorCtrl::separatorTypeVertical;
}

//--------------------------------------------------------------------------
void GuiSeparatorCtrl::initPersistFields()
{
   addField("Caption",         TypeString, Offset(mText,           GuiSeparatorCtrl));
   addField("Type",            TypeEnum,   Offset(mSeparatorType,  GuiSeparatorCtrl), 1, &gSeparatorTypeTable);
   addField("BorderMargin",      TypeS32,    Offset(mMargin, GuiSeparatorCtrl));
   addField("Invisible",      TypeBool,    Offset(mInvisible, GuiSeparatorCtrl));
   addField("LeftMargin",      TypeS32,    Offset(mTextLeftMargin, GuiSeparatorCtrl));

   Parent::initPersistFields();
}

//--------------------------------------------------------------------------
void GuiSeparatorCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   Parent::onRender( offset, updateRect );

   if( mInvisible )
      return;

   if(mText != StringTable->lookup("") && !( mSeparatorType == separatorTypeVertical ) )
   {
      // If text is present and we have a left margin, then draw some separator, then the
      // text, and then the rest of the separator
      S32 posx = offset.x + mMargin;
      S32 fontheight = mProfile->mFont->getHeight();
      S32 seppos = (fontheight - 2) / 2 + offset.y;
      if(mTextLeftMargin > 0)
      {
         RectI rect(Point2I(posx,seppos),Point2I(mTextLeftMargin,2));
         renderSlightlyLoweredBox(rect, mProfile);
         posx += mTextLeftMargin;
      }

      GFX->getDrawUtil()->setBitmapModulation( mProfile->mFontColor );
      posx += GFX->getDrawUtil()->drawText(mProfile->mFont, Point2I(posx,offset.y), mText, mProfile->mFontColors);
      //posx += mProfile->mFont->getStrWidth(mText);

      RectI rect(Point2I(posx,seppos),Point2I(getWidth() - posx + offset.x,2));
      renderSlightlyLoweredBox(rect, mProfile);

   } else
   {
      if( mSeparatorType == separatorTypeHorizontal )
      {
         S32 seppos = getHeight() / 2 + offset.y; 
         RectI rect(Point2I(offset.x + mMargin ,seppos),Point2I(getWidth() - (mMargin * 2),2));
         renderSlightlyLoweredBox(rect, mProfile);
      }
      else
      {
         S32 seppos = getWidth() / 2 + offset.x; 
         RectI rect(Point2I(seppos, offset.y + mMargin),Point2I(2, getHeight() - (mMargin * 2)));
         renderSlightlyLoweredBox(rect, mProfile);
      }
   }

   renderChildControls(offset, updateRect);
}


