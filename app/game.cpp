//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/platformInput.h"

#include "app/game.h"
#include "math/mMath.h"
#include "core/dnet.h"
#include "core/stream/fileStream.h"
#include "core/frameAllocator.h"
#include "core/iTickable.h"
#include "core/strings/findMatch.h"
#include "console/simBase.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "gui/controls/guiMLTextCtrl.h"
#ifdef TORQUE_TGB_ONLY
#include "T2D/networking/t2dGameConnection.h"
#include "T2D/networking/t2dNetworkServerSceneProcess.h"
#include "T2D/networking/t2dNetworkClientSceneProcess.h"
#else
#include "T3D/gameConnection.h"
#include "T3D/gameFunctions.h"
#include "T3D/gameProcess.h"
#endif
#include "platform/profiler.h"
#include "gfx/gfxCubemap.h"
#include "gfx/gfxTextureManager.h"
#include "sfx/sfxSystem.h"

#ifdef TORQUE_PLAYER
// See matching #ifdef in editor/editor.cpp
bool gEditingMission = false;
#endif

//--------------------------------------------------------------------------

ConsoleFunctionGroupBegin( InputManagement, "Functions that let you deal with input from scripts" );

ConsoleFunction( deactivateDirectInput, void, 1, 1, "Deactivate input. (ie, ungrab the mouse so the user can do other things." )
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   if ( Input::isActive() )
      Input::deactivate();
}

ConsoleFunction( activateDirectInput, void, 1, 1, "Activate input. (ie, grab the mouse again so the user can play our game." )
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   if ( !Input::isActive() )
      Input::activate();
}
ConsoleFunctionGroupEnd( InputManagement );

//--------------------------------------------------------------------------

static const U32 MaxPlayerNameLength = 16;
ConsoleFunction( strToPlayerName, const char*, 2, 2, "strToPlayerName( string )" )
{
   TORQUE_UNUSED(argc);

   const char* ptr = argv[1];

	// Strip leading spaces and underscores:
   while ( *ptr == ' ' || *ptr == '_' )
      ptr++;

   U32 len = dStrlen( ptr );
   if ( len )
   {
      char* ret = Con::getReturnBuffer( MaxPlayerNameLength + 1 );
      char* rptr = ret;
      ret[MaxPlayerNameLength - 1] = '\0';
      ret[MaxPlayerNameLength] = '\0';
      bool space = false;

      U8 ch;
      while ( *ptr && dStrlen( ret ) < MaxPlayerNameLength )
      {
         ch = (U8) *ptr;

         // Strip all illegal characters:
         if ( ch < 32 || ch == ',' || ch == '.' || ch == '\'' || ch == '`' )
         {
            ptr++;
            continue;
         }

         // Don't allow double spaces or space-underline combinations:
         if ( ch == ' ' || ch == '_' )
         {
            if ( space )
            {
               ptr++;
               continue;
            }
            else
               space = true;
         }
         else
            space = false;

         *rptr++ = *ptr;
         ptr++;
      }
      *rptr = '\0';

		//finally, strip out the ML text control chars...
		return GuiMLTextCtrl::stripControlChars(ret);
   }

	return( "" );
}

ConsoleFunctionGroupBegin( Platform , "General platform functions.");

ConsoleFunction( lockMouse, void, 2, 2, "(bool isLocked)"
                "Lock the mouse (or not, depending on the argument's value) to the window.")
{
   Platform::setWindowLocked(dAtob(argv[1]));
}


ConsoleFunction( setNetPort, bool, 2, 3, "(int port, bool bind=true)"
                "Set the network port for the game to use.  If bind is true, bind()"
				"will be called on the port.  This will trigger a windows firewall prompt."
				"If you don't have firewall tunneling tech you can set this to false to avoid the prompt.")
{
   bool bind = true;
   if (argc == 3)
      bind = dAtob(argv[2]);
   return Net::openPort(dAtoi(argv[1]), bind);
}

ConsoleFunction( closeNetPort, void, 1, 1, "()")
{
   Net::closePort();
}

ConsoleFunction( saveJournal, void, 2, 2, "(string filename)"
                "Save the journal to the specified file.")
{
   Journal::Record(argv[1]);
}

ConsoleFunction( playJournal, void, 2, 3, "(string filename, bool break=false)"
                "Begin playback of a journal from a specified field, optionally breaking at the start.")
{
   // CodeReview - BJG 4/24/2007 - The break flag needs to be wired back in.
   // bool jBreak = (argc > 2)? dAtob(argv[2]): false;
   Journal::Play(argv[1]);
}

ConsoleFunction( getSimTime, S32, 1, 1, "Return the current sim time in milliseconds.\n\n"
                "Sim time is time since the game started.")
{
   return Sim::getCurrentTime();
}

ConsoleFunction( getRealTime, S32, 1, 1, "Return the current real time in milliseconds.\n\n"
                "Real time is platform defined; typically time since the computer booted.")
{
   return Platform::getRealMilliseconds();
}

ConsoleFunctionGroupEnd(Platform);

//-----------------------------------------------------------------------------

bool clientProcess(U32 timeDelta)
{
   bool ret = true;

#ifndef TORQUE_TGB_ONLY
   ret = gClientProcessList.advanceTime(timeDelta);
#else
	ret = gt2dNetworkClientProcess.advanceTime( timeDelta );
#endif

   ITickable::advanceTime(timeDelta);

#ifndef TORQUE_TGB_ONLY
   // Run the collision test and update the Audio system
   // by checking the controlObject
   MatrixF mat;
   Point3F velocity;

   if (GameGetCameraTransform(&mat, &velocity))
   {
      SFX->getListener().setTransform(mat);
      SFX->getListener().setVelocity(velocity);
   }

   // Determine if we're lagging
   GameConnection* connection = GameConnection::getConnectionToServer();
   if(connection)
	{
      connection->detectLag();
	}
#else
   // Determine if we're lagging
   t2dGameConnection* connection = t2dGameConnection::getConnectionToServer();
   if(connection)
	{
      connection->detectLag();
	}
#endif

   // Let SFX process.
   SFX->_update();

   return ret;
}

bool serverProcess(U32 timeDelta)
{
   bool ret = true;
#ifndef TORQUE_TGB_ONLY
   ret =  gServerProcessList.advanceTime(timeDelta);
#else
   ret =  gt2dNetworkServerProcess.advanceTime( timeDelta );
#endif
   return ret;
}

