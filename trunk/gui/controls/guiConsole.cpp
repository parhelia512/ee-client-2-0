//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "gfx/gfxDrawUtil.h"
#include "gui/core/guiTypes.h"
#include "gui/core/guiControl.h"
#include "gui/controls/guiConsole.h"
#include "gui/containers/guiScrollCtrl.h"

IMPLEMENT_CONOBJECT(GuiConsole);

GuiConsole::GuiConsole()
{
   setExtent(64, 64);
   mCellSize.set(1, 1);
   mSize.set(1, 0);
}

bool GuiConsole::onWake()
{
   if (! Parent::onWake())
      return false;

   //get the font
   mFont = mProfile->mFont;

   return true;
}

S32 GuiConsole::getMaxWidth(S32 startIndex, S32 endIndex)
{
   //sanity check
   U32 size;
   ConsoleLogEntry *log;

   Con::getLockLog(log, size);

   if(startIndex < 0 || (U32)endIndex >= size || startIndex > endIndex)
      return 0;

   S32 result = 0;
   for(S32 i = startIndex; i <= endIndex; i++)
      result = getMax(result, (S32)(mFont->getStrWidth((const UTF8 *)log[i].mString)));
   
   Con::unlockLog();
   
   return(result + 6);
}

void GuiConsole::onPreRender()
{
   //see if the size has changed
   U32 prevSize = getHeight() / mCellSize.y;
   U32 size;
   ConsoleLogEntry *log;

   Con::getLockLog(log, size);
   Con::unlockLog(); // we unlock immediately because we only use size here, not log.
   
   if(size != prevSize)
   {
      //first, find out if the console was scrolled up
      bool scrolled = false;
      GuiScrollCtrl *parent = dynamic_cast<GuiScrollCtrl*>(getParent());

      if(parent)
         scrolled = parent->isScrolledToBottom();

      //find the max cell width for the new entries
      S32 newMax = getMaxWidth(prevSize, size - 1);
      if(newMax > mCellSize.x)
         mCellSize.set(newMax, mFont->getHeight());

      //set the array size
      mSize.set(1, size);

      //resize the control
      setExtent( Point2I(mCellSize.x, mCellSize.y * size));

      //if the console was not scrolled, make the last entry visible
      if (scrolled)
         scrollCellVisible(Point2I(0,mSize.y - 1));
   }
}

void GuiConsole::onRenderCell(Point2I offset, Point2I cell, bool /*selected*/, bool /*mouseOver*/)
{
   U32 size;
   ConsoleLogEntry *log;

   Con::getLockLog(log, size);

   ConsoleLogEntry &entry = log[cell.y];
   switch (entry.mLevel)
   {
      case ConsoleLogEntry::Normal:   GFX->getDrawUtil()->setBitmapModulation(mProfile->mFontColor); break;
      case ConsoleLogEntry::Warning:  GFX->getDrawUtil()->setBitmapModulation(mProfile->mFontColorHL); break;
      case ConsoleLogEntry::Error:    GFX->getDrawUtil()->setBitmapModulation(mProfile->mFontColorNA); break;
      default: AssertFatal(false, "GuiConsole::onRenderCell - Unrecognized ConsoleLogEntry type, update this.");
   }
   GFX->getDrawUtil()->drawText(mFont, Point2I(offset.x + 3, offset.y), entry.mString, mProfile->mFontColors);
   
   Con::unlockLog();
}
