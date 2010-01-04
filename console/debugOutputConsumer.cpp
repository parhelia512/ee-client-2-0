//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/debugOutputConsumer.h"

namespace DebugOutputConsumer
{
#ifndef DISABLE_DEBUG_SPEW
bool debugOutputEnabled = true;
#else
bool debugOutputEnabled = false;
#endif


void init()
{
   Con::addConsumer( DebugOutputConsumer::logCallback );
}

void destroy()
{
   Con::removeConsumer( DebugOutputConsumer::logCallback );
}

void logCallback( ConsoleLogEntry::Level level, const char *consoleLine )
{
   if( debugOutputEnabled )
   {
      Platform::outputDebugString( "%s", consoleLine );
   }
}

}
