//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "windowManager/dedicated/dedicatedWindowStub.h"


PlatformWindowManager *CreatePlatformWindowManager()
{
   return new DedicatedWindowMgr;
}
