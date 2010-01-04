//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "app/mainLoop.h"
#include "app/badWordFilter.h"
#include "app/game.h"

#include "platform/platformTimer.h"
#include "platform/platformRedBook.h"
#include "platform/platformVolume.h"
#include "platform/platformMemory.h"
#include "platform/platformTimer.h"
#include "platform/nativeDialogs/fileDialog.h"

#include "core/threadStatic.h"
#include "core/iTickable.h"
#include "core/stream/fileStream.h"

#include "windowManager/platformWindowMgr.h"

#include "core/util/journal/process.h"
#include "util/fpsTracker.h"

#include "console/telnetConsole.h"
#include "console/telnetDebugger.h"
#include "console/debugOutputConsumer.h"
#include "console/consoleTypes.h"

#include "gfx/bitmap/gBitmap.h"
#include "gfx/gFont.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/gfxInit.h"

#include "sim/netStringTable.h"
#include "sim/actionMap.h"
#include "sim/netInterface.h"

#include "sfx/sfxSystem.h"

#include "util/sampler.h"
#include "platform/threads/threadPool.h"

#ifdef TORQUE_ENABLE_VFS
#include "platform/platformVFS.h"
#endif

#include "../add/Global/GlobalStatic.h"

DITTS( F32, gTimeScale, 1.0 );
DITTS( U32, gTimeAdvance, 0 );
DITTS( U32, gFrameSkip, 0 );

extern S32 sgBackgroundProcessSleepTime;
extern S32 sgTimeManagerProcessInterval;

extern void netInit();

extern FPSTracker gFPS;

TimeManager* tm = NULL;

static bool gRequiresRestart = false;

#ifdef TORQUE_DEBUG

/// Temporary timer used to time startup times.
static PlatformTimer* gStartupTimer;

#endif

// The following are some tricks to make the memory leak checker run after global
// dtors have executed by placing some code in the termination segments.

#if defined( TORQUE_DEBUG ) && !defined( TORQUE_DISABLE_MEMORY_MANAGER )

   #ifdef TORQUE_COMPILER_VISUALC
   #  pragma data_seg( ".CRT$XTU" )
   
      static void* sCheckMemBeforeTermination = &Memory::ensureAllFreed;
      
   #  pragma data_seg()
   #elif defined( TORQUE_COMPILER_GCC )
   
       __attribute__ ( ( destructor ) ) static void _ensureAllFreed()
      {
         Memory::ensureAllFreed();
      }
      
   #endif

#endif

// Process a time event and update all sub-processes
void processTimeEvent(S32 elapsedTime)
{
   PROFILE_START(ProcessTimeEvent);
   
   // cap the elapsed time to one second
   // if it's more than that we're probably in a bad catch-up situation
   if(elapsedTime > 1024)
      elapsedTime = 1024;
   
   U32 timeDelta;
   if(ATTS(gTimeAdvance))
      timeDelta = ATTS(gTimeAdvance);
   else
      timeDelta = (U32) (elapsedTime * ATTS(gTimeScale));
   
   Platform::advanceTime(elapsedTime);
   
   bool tickPass;
   
   PROFILE_START(ServerProcess);
   tickPass = serverProcess(timeDelta);
   PROFILE_END();
   
   PROFILE_START(ServerNetProcess);
   // only send packets if a tick happened
   if(tickPass)
      GNet->processServer();
   PROFILE_END();
   
   PROFILE_START(SimAdvanceTime);
   Sim::advanceTime(timeDelta);
   PROFILE_END();
   
   PROFILE_START(ClientProcess);
   tickPass = clientProcess(timeDelta);
   PROFILE_END_NAMED(ClientProcess);
   
   PROFILE_START(ClientNetProcess);
   if(tickPass)
      GNet->processClient();
   PROFILE_END();
   
   GNet->checkTimeouts();
   
   gFPS.update();
   
   PROFILE_END();
   
   // Update the console time
   Con::setFloatVariable("Sim::Time",F32(Platform::getVirtualMilliseconds()) / 1000);
}

void StandardMainLoop::init()
{
   #ifdef TORQUE_DEBUG
   gStartupTimer = PlatformTimer::create();
   #endif
   
   #ifdef TORQUE_DEBUG_GUARD
      Memory::flagCurrentAllocs( Memory::FLAG_Global );
   #endif

   Platform::setMathControlStateKnown();
   
   // Asserts should be created FIRST
   PlatformAssert::create();

   // Yell if we can't initialize the network.
   if(!Net::init())
   {
      AssertISV(false, "StandardMainLoop::initCore - could not initialize networking!");
   }

   FrameAllocator::init(TORQUE_FRAME_SIZE);      // See comments in torqueConfig.h
   _StringTable::create();

   // Set up the resource manager and get some basic file types in it.
   Con::init();
   Platform::initConsole();
   NetStringTable::create();

   // Use debug output logging on the Xbox and OSX builds
#if defined( _XBOX ) || defined( TORQUE_OS_MAC )
   DebugOutputConsumer::init();
#endif

   TelnetConsole::create();
   TelnetDebugger::create();

   Processor::init();
   Math::init();
   Platform::init();    // platform specific initialization
   RedBook::init();
   Platform::initConsole();
   SFXSystem::init();
   GFXDevice::initConsole();
   GFXTextureManager::init();

   // Initialise ITickable.
#ifdef TORQUE_TGB_ONLY
   ITickable::init( 4 );
#endif

#ifdef TORQUE_ENABLE_VFS
   // [tom, 10/28/2006] Load the VFS here so that it stays loaded
   Zip::ZipArchive *vfs = openEmbeddedVFSArchive();
   gResourceManager->addVFSRoot(vfs);
#endif

   Con::addVariable("timeScale", TypeF32, &ATTS(gTimeScale));
   Con::addVariable("timeAdvance", TypeS32, &ATTS(gTimeAdvance));
   Con::addVariable("frameSkip", TypeS32, &ATTS(gFrameSkip));

   Con::setVariable( "defaultGame", StringTable->insert("scripts") );

   Con::addVariable( "_forceAllMainThread", TypeBool, &ThreadPool::getForceAllMainThread() );

#if !defined( _XBOX ) && !defined( TORQUE_DEDICATED )
   initMessageBoxVars();
#endif

   netInit();
   Sim::init();

   ActionMap* globalMap = new ActionMap;
   globalMap->registerObject("GlobalActionMap");
   Sim::getActiveActionMapSet()->pushObject(globalMap);

   BadWordFilter::create();
   
   // Do this before we init the process so that process notifiees can get the time manager
   tm = new TimeManager;
   tm->timeEvent.notify(&::processTimeEvent);
   
   Process::init();
   Sampler::init();

   // Hook in for UDP notification
   Net::smPacketReceive.notify(GNet, &NetInterface::processPacketReceiveEvent);

   CGlobalStatic::init();

   #ifdef TORQUE_DEBUG_GUARD
      Memory::flagCurrentAllocs( Memory::FLAG_Static );
   #endif
}

void StandardMainLoop::shutdown()
{
   delete tm;
   preShutdown();
   
   CGlobalStatic::shutdown();

   BadWordFilter::destroy();

   // Shut down SFX before SIM so that it clears out any audio handles
   SFXSystem::destroy();

   GFXInit::cleanup();

   // Note: tho the SceneGraphs are created after the Manager, delete them after, rather
   //  than before to make sure that all the objects are removed from the graph.
   Sim::shutdown();
   
   Process::shutdown();

   //necessary for DLL unloading
   ThreadPool::GLOBAL().shutdown();

#ifdef TORQUE_ENABLE_VFS
   closeEmbeddedVFSArchive();
#endif

   RedBook::destroy();

   Platform::shutdown();
   GFXDevice::destroy();

   TelnetDebugger::destroy();
   TelnetConsole::destroy();

#if defined( _XBOX ) || defined( TORQUE_OS_MAC )
   DebugOutputConsumer::destroy();
#endif

   NetStringTable::destroy();
   Con::shutdown();

   _StringTable::destroy();
   FrameAllocator::destroy();
   Net::shutdown();
   Sampler::destroy();

   // asserts should be destroyed LAST
   PlatformAssert::destroy();


#if defined( TORQUE_DEBUG ) && !defined( TORQUE_DISABLE_MEMORY_MANAGER )
   Memory::validate();
#endif
}

void StandardMainLoop::preShutdown()
{
#ifdef TORQUE_TOOLS
   // Tools are given a chance to do pre-quit processing
   // - This is because for tools we like to do things such
   //   as prompting to save changes before shutting down
   //   and onExit is packaged which means we can't be sure
   //   where in the shutdown namespace chain we are when using
   //   onExit since some components of the tools may already be
   //   destroyed that may be vital to saving changes to avoid
   //   loss of work [1/5/2007 justind]
   if( Con::isFunction("onPreExit") )
      Con::executef( "onPreExit");
#endif

   //exec the script onExit() function
   if ( Con::isFunction( "onExit" ) )
      Con::executef("onExit");
}

bool StandardMainLoop::handleCommandLine( S32 argc, const char **argv )
{
   // Allow the window manager to process command line inputs; this is
   // done to let web plugin functionality happen in a fairly transparent way.
   PlatformWindowManager::get()->processCmdLineArgs(argc, argv);

   Process::handleCommandLine( argc, argv );

   // Set up the command line args for the console scripts...
   Con::setIntVariable("Game::argc", argc);
   U32 i;
   for (i = 0; i < argc; i++)
      Con::setVariable(avar("Game::argv%d", i), argv[i]);

   Platform::FS::InstallFileSystems(); // install all drives for now until we have everything using the volume stuff
   Platform::FS::MountDefaults();

   // Set our working directory.
   Torque::FS::SetCwd( "game:/" );

   // Set our working directory.
   Platform::setCurrentDirectory( Platform::getMainDotCsDir() );

#ifdef TORQUE_PLAYER
   if(argc > 2 && dStricmp(argv[1], "-project") == 0)
   {
      char playerPath[1024];
      Platform::makeFullPathName(argv[2], playerPath, sizeof(playerPath));
      Platform::setCurrentDirectory(playerPath);

      argv += 2;
      argc -= 2;

      // Re-locate the game:/ asset mount.

      Torque::FS::Unmount( "game" );
      Torque::FS::Mount( "game", Platform::FS::createNativeFS( playerPath ) );
   }
#endif

   // Executes an entry script file. This is "main.cs"
   // by default, but any file name (with no whitespace
   // in it) may be run if it is specified as the first
   // command-line parameter. The script used, default
   // or otherwise, is not compiled and is loaded here
   // directly because the resource system restricts
   // access to the "root" directory.

#ifdef TORQUE_ENABLE_VFS
   Zip::ZipArchive *vfs = openEmbeddedVFSArchive();
   bool useVFS = vfs != NULL;
#endif

   Stream *mainCsStream = NULL;

   // The working filestream.
   FileStream str; 

   const char *defaultScriptName = "main.cs";
   bool useDefaultScript = true;

   // Check if any command-line parameters were passed (the first is just the app name).
   if (argc > 1)
   {
      // If so, check if the first parameter is a file to open.
      if ( (str.open(argv[1], Torque::FS::File::Read)) && (dStrcmp(argv[1], "") != 0 ) )
      {
         // If it opens, we assume it is the script to run.
         useDefaultScript = false;
#ifdef TORQUE_ENABLE_VFS
         useVFS = false;
#endif
         mainCsStream = &str;
      }
   }

   if (useDefaultScript)
   {
      bool success = false;

#ifdef TORQUE_ENABLE_VFS
      if(useVFS)
         success = (mainCsStream = vfs->openFile(defaultScriptName, Zip::ZipArchive::Read)) != NULL;
      else
#endif
         success = str.open(defaultScriptName, Torque::FS::File::Read);

#if defined( TORQUE_DEBUG ) && defined (TORQUE_TOOLS) && !defined( _XBOX )
      if (!success)
      {
         OpenFileDialog ofd;
         FileDialogData &fdd = ofd.getData();
         fdd.mFilters = StringTable->insert("Main Entry Script (main.cs)|main.cs|");
         fdd.mTitle   = StringTable->insert("Locate Game Entry Script");

         // Get the user's selection
         if( !ofd.Execute() )
            return false;

         // Process and update CWD so we can run the selected main.cs
         S32 pathLen = dStrlen( fdd.mFile );
         FrameTemp<char> szPathCopy( pathLen + 1);

         dStrcpy( szPathCopy, fdd.mFile );
         //forwardslash( szPathCopy );

         const char *path = dStrrchr(szPathCopy, '/');
         if(path)
         {
            U32 len = path - (const char*)szPathCopy;
            szPathCopy[len+1] = 0;

            Platform::setCurrentDirectory(szPathCopy);

            // Re-locate the game:/ asset mount.

            Torque::FS::Unmount( "game" );
            Torque::FS::Mount( "game", Platform::FS::createNativeFS( ( const char* ) szPathCopy ) );

            success = str.open(fdd.mFile, Torque::FS::File::Read);
            if(success)
               defaultScriptName = fdd.mFile;
         }
      }
#endif
      if( !success )
      {
         char msg[1024];
         dSprintf(msg, sizeof(msg), "Failed to open \"%s\".", defaultScriptName);
         Platform::AlertOK("Error", msg);
#ifdef TORQUE_ENABLE_VFS
         closeEmbeddedVFSArchive();
#endif

         return false;
      }

#ifdef TORQUE_ENABLE_VFS
      if(! useVFS)
#endif
         mainCsStream = &str;
   }

   U32 size = mainCsStream->getStreamSize();
   char *script = new char[size + 1];
   mainCsStream->read(size, script);

#ifdef TORQUE_ENABLE_VFS
   if(useVFS)
      vfs->closeFile(mainCsStream);
   else
#endif
      str.close();

   script[size] = 0;

   char buffer[1024], *ptr;
   Platform::makeFullPathName(useDefaultScript ? defaultScriptName : argv[1], buffer, sizeof(buffer), Platform::getCurrentDirectory());
   ptr = dStrrchr(buffer, '/');
   if(ptr != NULL)
      *ptr = 0;
   Platform::setMainDotCsDir(buffer);
   Platform::setCurrentDirectory(buffer);

   Con::evaluate(script, false, useDefaultScript ? defaultScriptName : argv[1]); 
   delete[] script;

#ifdef TORQUE_ENABLE_VFS
   closeEmbeddedVFSArchive();
#endif

   return true;
}

bool StandardMainLoop::doMainLoop()
{
   #ifdef TORQUE_DEBUG
   if( gStartupTimer )
   {
      Con::printf( "Started up in %.2f seconds...",
         F32( gStartupTimer->getElapsedMs() ) / 1000.f );
      SAFE_DELETE( gStartupTimer );
   }
   #endif
   
   bool keepRunning = true;
//   while(keepRunning)
   {
      tm->setBackgroundThreshold(mClamp(sgBackgroundProcessSleepTime, 1, 200));
      tm->setForegroundThreshold(mClamp(sgTimeManagerProcessInterval, 1, 200));
      // update foreground/background status
      if(WindowManager->getFirstWindow())
      {
         static bool lastFocus = false;
         bool newFocus = ( WindowManager->getFocusedWindow() != NULL );
         if(lastFocus != newFocus)
         {
#ifndef TORQUE_SHIPPING
            Con::printf("Window focus status changed: focus: %d", newFocus);
            if (!newFocus)
               Con::printf("  Using background sleep time: %u", Platform::getBackgroundSleepTime());
#endif

#ifdef TORQUE_OS_MAC
            if (newFocus)
               WindowManager->getFirstWindow()->show();
               
#endif
            lastFocus = newFocus;
         }
         
#ifndef TORQUE_OS_MAC         
         // under the web plugin do not sleep the process when the child window loses focus as this will cripple the browser perfomance
         if (!Platform::getWebDeployment())
            tm->setBackground(!newFocus);
         else
            tm->setBackground(false);
#else
         tm->setBackground(false);
#endif
      }
      else
      {
         tm->setBackground(false);
      }
      
      PROFILE_START(MainLoop);
      Sampler::beginFrame();

      if(!Process::processEvents())
         keepRunning = false;

      ThreadPool::processMainThreadWorkItems();
      Sampler::endFrame();
      PROFILE_END_NAMED(MainLoop);
   }
   
   return keepRunning;
}

void StandardMainLoop::setRestart(bool restart )
{
   gRequiresRestart = restart;
}

bool StandardMainLoop::requiresRestart()
{
   return gRequiresRestart;
}
