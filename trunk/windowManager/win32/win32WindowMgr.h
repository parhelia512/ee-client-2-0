//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef  _WINDOWMANAGER_WIN32_WIN32WINDOWMANAGER_
#define  _WINDOWMANAGER_WIN32_WIN32WINDOWMANAGER_

#include <windows.h>

#include "math/mMath.h"
#include "gfx/gfxStructs.h"
#include "windowManager/win32/win32Window.h"
#include "core/util/tVector.h"

/// Win32 implementation of the window manager interface.
class Win32WindowManager : public PlatformWindowManager
{
   friend class Win32Window;

   virtual void _processCmdLineArgs(const S32 argc, const char **argv);

   /// Link the specified window into the window list.
   void linkWindow(Win32Window *w);

   /// Remove specified window from the window list.
   void unlinkWindow(Win32Window *w);

   /// Callback for the process list.
   void _process();

   /// List of allocated windows.
   Win32Window *mWindowListHead;

   /// Parent window, used in window setup in web plugin scenarios.
   HWND mParentWindow;

   /// Callback to receive information about available monitors.
   static BOOL CALLBACK MonitorEnumProc(
      HMONITOR hMonitor,  // handle to display monitor
      HDC hdcMonitor,     // handle to monitor DC
      LPRECT lprcMonitor, // monitor intersection rectangle
      LPARAM dwData       // data
      );

   /// If a curtain window is present, then its HWND will be stored here.
   HWND mCurtainWindow;

public:
   Win32WindowManager();
   ~Win32WindowManager();

   virtual RectI getPrimaryDesktopArea();
   virtual S32       getDesktopBitDepth();
   virtual Point2I   getDesktopResolution();

   virtual void getMonitorRegions(Vector<RectI> &regions);
   virtual PlatformWindow *createWindow(GFXDevice *device, const GFXVideoMode &mode);
   virtual void getWindows(VectorPtr<PlatformWindow*> &windows);

   virtual void setParentWindow(void* newParent);
   virtual void* getParentWindow();

   virtual PlatformWindow *getWindowById(WindowId id);
   virtual PlatformWindow *getFirstWindow();
   virtual PlatformWindow* getFocusedWindow();

   virtual void lowerCurtain();
   virtual void raiseCurtain();
};

#endif