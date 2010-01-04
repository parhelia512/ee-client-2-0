//-----------------------------------------------------------------------------
// Torque Shader Engine 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/stream/fileStream.h"
#include "core/strings/unicode.h"
#include "console/console.h"
#include "screenshot.h"
#include "core/volume.h"


// This must be initialized by the device
ScreenShot * gScreenShot = NULL;


//**************************************************************************
// Console function
//**************************************************************************
ConsoleFunction(screenShot, void, 3, 3, "(string file, string format)"
                "Take a screenshot.\n\n"
                "@param format One of JPEG or PNG.")
{
   if( !gScreenShot )
   {
      Con::errorf( "Screenshot module not initialized by device" );
      return;
   }

   
   Torque::Path ssPath(argv[1]);
   Torque::FS::CreatePath(ssPath);
   Torque::FS::FileSystemRef fs = Torque::FS::GetFileSystem(ssPath);
   Torque::Path newPath = fs->mapTo(ssPath);

   gScreenShot->mPending = true;
   gScreenShot->mFilename = newPath.getFullPath();
}

//**************************************************************************
// ScreenShot class
//**************************************************************************
ScreenShot::ScreenShot()
{
   mSurfWidth = 0;
   mSurfHeight = 0;
   mPending = false;
}



