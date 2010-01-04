//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUICONSOLE_H_
#define _GUICONSOLE_H_

#ifndef _GUIARRAYCTRL_H_
#include "gui/core/guiArrayCtrl.h"
#endif

class GuiConsole : public GuiArrayCtrl
{
   private:
      typedef GuiArrayCtrl Parent;

      Resource<GFont> mFont;

      S32 getMaxWidth(S32 startIndex, S32 endIndex);

   public:
      GuiConsole();
      DECLARE_CONOBJECT(GuiConsole);
      DECLARE_CATEGORY( "Gui Editor" );
      DECLARE_DESCRIPTION( "Control that displays the console log text." );

      bool onWake();

      void onPreRender();
      void onRenderCell(Point2I offset, Point2I cell, bool selected, bool mouseOver);
};

#endif
