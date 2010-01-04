//-----------------------------------------------------------------------------
// Torque 3D
//
// Copyright (c) 2001 GarageGames.Com
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "gui/controls/guiBitmapCtrl.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "gfx/gfxDrawUtil.h"


class GuiFadeinBitmapCtrl : public GuiBitmapCtrl
{
   typedef GuiBitmapCtrl Parent;
public:
   DECLARE_CONOBJECT(GuiFadeinBitmapCtrl);
   DECLARE_DESCRIPTION( "A control that shows a bitmap.  It fades the bitmap in a set amount of time,\n"
                        "then waits a set amount of time, and finally fades the bitmap back out in\n"
                        "another set amount of time." );

   U32 wakeTime;
   bool done;
   U32 fadeinTime;
   U32 waitTime;
   U32 fadeoutTime;

   GuiFadeinBitmapCtrl()
   {
      wakeTime    = 0;
      fadeinTime  = 1000;
      waitTime    = 2000;
      fadeoutTime = 1000;
      done        = false;
   }
   void onPreRender()
   {
      Parent::onPreRender();
      setUpdate();
   }
   void onMouseDown(const GuiEvent &)
   {
      Con::executef(this, "click");
   }
   bool onKeyDown(const GuiEvent &)
   {
      Con::executef(this, "click");
      return true;
   }
   bool onWake()
   {
      if(!Parent::onWake())
         return false;
      wakeTime = Platform::getRealMilliseconds();
      return true;
   }
   void onRender(Point2I offset, const RectI &updateRect)
   {
      Parent::onRender(offset, updateRect);
      U32 elapsed = Platform::getRealMilliseconds() - wakeTime;

      U32 alpha;
      if (elapsed < fadeinTime)
      {
         // fade-in
         alpha = (U32)(255.0f * (1.0f - (F32(elapsed) / F32(fadeinTime))));
      }
      else if (elapsed < (fadeinTime+waitTime))
      {
         // wait
         alpha = 0;
      }
      else if (elapsed < (fadeinTime+waitTime+fadeoutTime))
      {
         // fade out
         elapsed -= (fadeinTime+waitTime);
         alpha = (U32)(255.0f * F32(elapsed) / F32(fadeoutTime));
      }
      else
      {
         // done state
         alpha = fadeoutTime ? 255 : 0;
         done = true;

         if (!Con::getBoolVariable("$InGuiEditor"))
            Con::executef(this, "onDone");
      }
      ColorI color(0,0,0,alpha);
      GFX->getDrawUtil()->drawRectFill( offset, getExtent() + offset, color );
   }
   static void initPersistFields()
   {
      addField("fadeinTime", TypeS32, Offset(fadeinTime, GuiFadeinBitmapCtrl));
      addField("waitTime", TypeS32, Offset(waitTime, GuiFadeinBitmapCtrl));
      addField("fadeoutTime", TypeS32, Offset(fadeoutTime, GuiFadeinBitmapCtrl));
      addField("done", TypeBool, Offset(done, GuiFadeinBitmapCtrl));
      Parent::initPersistFields();
   }
};

IMPLEMENT_CONOBJECT(GuiFadeinBitmapCtrl);
