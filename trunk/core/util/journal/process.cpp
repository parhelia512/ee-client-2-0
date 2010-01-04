//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/util/journal/process.h"
#include "core/util/journal/journal.h"

static Process* _theOneProcess = NULL; ///< the one instance of the Process class

//-----------------------------------------------------------------------------

void Process::requestShutdown()
{
   Process::get()._RequestShutdown = true;
}

//-----------------------------------------------------------------------------

Process::Process()
:  _RequestShutdown( false )
{
}

Process &Process::get()
{
   struct Cleanup
   {
      ~Cleanup()
      {
         if( _theOneProcess )
            delete _theOneProcess;
      }
   };
   static Cleanup cleanup;

   // NOTE that this function is not thread-safe
   //    To make it thread safe, use the double-checked locking mechanism for singleton objects

   if ( !_theOneProcess )
      _theOneProcess = new Process;

   return *_theOneProcess;
}

bool Process::init()
{
   return Process::get()._signalInit.trigger();
}

void  Process::handleCommandLine(S32 argc, const char **argv)
{
   Process::get()._signalCommandLine.trigger(argc, argv);
}

bool  Process::processEvents()
{
   // Process all the devices. We need to call these even during journal
   // playback to ensure that the OS event queues are serviced.
   Process::get()._signalProcess.trigger();

   if (!Process::get()._RequestShutdown) 
   {
      if (Journal::IsPlaying())
         return Journal::PlayNext();
      return true;
   }

   // Reset the Quit flag so the function can be called again.
   Process::get()._RequestShutdown = false;
   return false;
}

bool  Process::shutdown()
{
   return Process::get()._signalShutdown.trigger();
}

