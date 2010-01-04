//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "gui/core/guiControl.h"
#include "console/consoleTypes.h"
#include "T3D/shapeBase.h"
#include "gfx/gfxDrawUtil.h"

//-----------------------------------------------------------------------------

/// Vary basic HUD clock.
/// Displays the current simulation time offset from some base. The base time
/// is usually synchronized with the server as mission start time.  This hud
/// currently only displays minutes:seconds.
class GuiClockHud : public GuiControl
{
   typedef GuiControl Parent;

   bool     mShowFrame;
   bool     mShowFill;

   ColorF   mFillColor;
   ColorF   mFrameColor;
   ColorF   mTextColor;

   S32      mTimeOffset;

public:
   GuiClockHud();

   void setTime(F32 newTime);
   F32  getTime();

   void onRender( Point2I, const RectI &);
   static void initPersistFields();
   DECLARE_CONOBJECT( GuiClockHud );
   DECLARE_CATEGORY( "Gui Game" );
};


//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT( GuiClockHud );

GuiClockHud::GuiClockHud()
{
   mShowFrame = mShowFill = true;
   mFillColor.set(0, 0, 0, 0.5);
   mFrameColor.set(0, 1, 0, 1);
   mTextColor.set( 0, 1, 0, 1 );

   mTimeOffset = 0;
}

void GuiClockHud::initPersistFields()
{
   addGroup("Misc");		
   addField( "showFill", TypeBool, Offset( mShowFill, GuiClockHud ) );
   addField( "showFrame", TypeBool, Offset( mShowFrame, GuiClockHud ) );
   addField( "fillColor", TypeColorF, Offset( mFillColor, GuiClockHud ) );
   addField( "frameColor", TypeColorF, Offset( mFrameColor, GuiClockHud ) );
   addField( "textColor", TypeColorF, Offset( mTextColor, GuiClockHud ) );
   endGroup("Misc");

   Parent::initPersistFields();
}


//-----------------------------------------------------------------------------

void GuiClockHud::onRender(Point2I offset, const RectI &updateRect)
{
   // Background first
   if (mShowFill)
      GFX->getDrawUtil()->drawRectFill(updateRect, mFillColor);

   // Convert ms time into hours, minutes and seconds.
   S32 time = S32(getTime());
   S32 secs = time % 60;
   S32 mins = (time % 3600) / 60;

   // Currently only displays min/sec
   char buf[256];
   dSprintf(buf,sizeof(buf), "%02d:%02d",mins,secs);

   // Center the text
   offset.x += (getWidth() - mProfile->mFont->getStrWidth((const UTF8 *)buf)) / 2;
   offset.y += (getHeight() - mProfile->mFont->getHeight()) / 2;
   GFX->getDrawUtil()->setBitmapModulation(mTextColor);
   GFX->getDrawUtil()->drawText(mProfile->mFont, offset, buf);
   GFX->getDrawUtil()->clearBitmapModulation();

   // Border last
   if (mShowFrame)
      GFX->getDrawUtil()->drawRect(updateRect, mFrameColor);
}


//-----------------------------------------------------------------------------

void GuiClockHud::setTime(F32 time)
{
   // Set the current time in seconds.
   mTimeOffset = S32(time * 1000) - Platform::getVirtualMilliseconds();
}

F32 GuiClockHud::getTime()
{
   // Return elapsed time in seconds.
   return F32(mTimeOffset + Platform::getVirtualMilliseconds()) / 1000;
}

ConsoleMethod(GuiClockHud,setTime,void,3, 3,"(time in sec)Sets the current base time for the clock")
{
   object->setTime(dAtof(argv[2]));
}

ConsoleMethod(GuiClockHud,getTime, F32, 2, 2,"()Returns current time in secs.")
{
   return object->getTime();
}
