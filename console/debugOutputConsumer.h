//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _DEBUGOUTPUTCONSUMER_H_
#define _DEBUGOUTPUTCONSUMER_H_

#include "platform/platform.h"

//#define TORQUE_LOCBUILD

#if !defined(TORQUE_DEBUG) && defined(TORQUE_OS_XENON) && !defined(TORQUE_LOCBUILD)
#define DISABLE_DEBUG_SPEW
#endif

#include "console/console.h"

namespace DebugOutputConsumer
{
   extern bool debugOutputEnabled;

   void init();
   void destroy();
   void logCallback( ConsoleLogEntry::Level level, const char *consoleLine );

   void enableDebugOutput( bool enable = true );
};

#endif