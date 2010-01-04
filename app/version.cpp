//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "app/version.h"
#include "console/console.h"

static const U32 csgVersionNumber = TORQUE_GAME_ENGINE;

U32 getVersionNumber()
{
   return csgVersionNumber;
}

const char* getVersionString()
{
   return TORQUE_GAME_ENGINE_VERSION_STRING;
}

/// TGE       0001
/// TGEA      0002
/// TGB       0003
/// TGEA 360  0004
/// TGE  WII  0005
/// Torque 3D 0006

const char* getEngineProductString()
{
	return "Element";
#ifndef TORQUE_ENGINE_PRODUCT
   return "Torque Engine";
#else
   switch (TORQUE_ENGINE_PRODUCT)
   {
      case 0001:
         return "Torque Game Engine";
      case 0002:
         return "Torque Game Engine Advanced";
      case 0003:
         return "Torque 2D";
      case 0004:
         return "Torque 360";
      case 0005:
         return "Torque for Wii";
      case 0006:
         return "Torque 3D";

      default:
         return "Torque Engine";
   };
#endif
}

const char* getCompileTimeString()
{
   return __DATE__ " at " __TIME__;
}
//----------------------------------------------------------------

ConsoleFunctionGroupBegin( CompileInformation, "Functions to get version information about the current executable." );

ConsoleFunction( getVersionNumber, S32, 1, 1, "Get the version of the build, as a string.")
{
   return getVersionNumber();
}

ConsoleFunction( getVersionString, const char*, 1, 1, "Get the version of the build, as a string.")
{
   return getVersionString();
}

ConsoleFunction( getEngineName, const char*, 1, 1, "Get the name of the engine product that this is running from, as a string.")
{
   return getEngineProductString();
}

ConsoleFunction( getCompileTimeString, const char*, 1, 1, "Get the time of compilation.")
{
   return getCompileTimeString();
}

ConsoleFunction( getBuildString, const char*, 1, 1, "Get the type of build, \"Debug\" or \"Release\".")
{
#ifdef TORQUE_DEBUG
   return "Debug";
#else
   return "Release";
#endif
}

ConsoleFunctionGroupEnd( CompileInformation );

ConsoleFunction(isDemo, bool, 1, 1, "")
{
#ifdef TORQUE_DEMO
   return true;
#else
   return false;
#endif
}

ConsoleFunction(isWebDemo, bool, 1, 1, "")
{
#ifdef TORQUE_DEMO
   return Platform::getWebDeployment();
#else
   return false;
#endif
}