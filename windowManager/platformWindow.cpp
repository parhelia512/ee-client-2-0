//-----------------------------------------------------------------------------
// Torque Game Engine 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "windowManager/platformWindow.h"


void PlatformWindow::setFullscreen( const bool fullscreen )
{
   // Notify listeners that we will acquire the screen
   if(fullscreen && !Journal::IsDispatching())
      appEvent.trigger(getWindowId(),GainScreen);

   // Do platform specific fullscreen code
   _setFullscreen(fullscreen);

   // Notify listeners that we released the screen
   if(!fullscreen && !Journal::IsDispatching())
      appEvent.trigger(getWindowId(),LoseScreen);
}