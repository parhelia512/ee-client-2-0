//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (c) 2003 GarageGames.Com
//-----------------------------------------------------------------------------

#include "gfx/gfxInit.h"
#include "console/console.h"
#include "windowManager/platformWindowMgr.h"

Vector<GFXAdapter*> GFXInit::smAdapters( __FILE__, __LINE__ );
GFXInit::RegisterDeviceSignal* GFXInit::smRegisterDeviceSignal;

//-----------------------------------------------------------------------------

inline static void _GFXInitReportAdapters(Vector<GFXAdapter*> &adapters)
{
   for (U32 i = 0; i < adapters.size(); i++)
   {
      switch (adapters[i]->mType)
      {
      case Direct3D9:
         Con::printf("Direct 3D (version 9.x) device found");
         break;
      case OpenGL:
         Con::printf("OpenGL device found");
         break;
      case NullDevice:
         Con::printf("Null device found");
         break;
      case Direct3D8:
         Con::printf("Direct 3D (version 8.1) device found");
         break;
      default :
         Con::printf("Unknown device found");
         break;
      }
   }
}

inline static void _GFXInitGetInitialRes(GFXVideoMode &vm, const Point2I &initialSize)
{
   const U32 kDefaultWindowSizeX = 800;
   const U32 kDefaultWindowSizeY = 600;
   const bool kDefaultFullscreen = false;
   const U32 kDefaultBitDepth = 32;
   const U32 kDefaultRefreshRate = 60;

   // cache the desktop size of the main screen
   GFXVideoMode desktopVm = GFXInit::getDesktopResolution();

   // load pref variables, properly choose windowed / fullscreen  
   const String resString = Con::getVariable("$pref::Video::mode");

   // Set defaults into the video mode, then have it parse the user string.
   vm.resolution.x = kDefaultWindowSizeX;
   vm.resolution.y = kDefaultWindowSizeY;
   vm.fullScreen   = kDefaultFullscreen;
   vm.bitDepth     = kDefaultBitDepth;
   vm.refreshRate  = kDefaultRefreshRate;
   vm.wideScreen = false;

   vm.parseFromString(resString);
}

GFXInit::RegisterDeviceSignal& GFXInit::getRegisterDeviceSignal()
{
   if (smRegisterDeviceSignal)
      return *smRegisterDeviceSignal;
   smRegisterDeviceSignal = new RegisterDeviceSignal();
   return *smRegisterDeviceSignal;
}

void GFXInit::init()
{
   // init only once.
   static bool doneOnce = false;
   if(doneOnce)
      return;
   doneOnce = true;
   
   Con::printf( "GFX Init:" );

   //find our adapters
   Vector<GFXAdapter*> adapters( __FILE__, __LINE__ );
   GFXInit::enumerateAdapters();
   GFXInit::getAdapters(&adapters);

   if(!adapters.size())
       Con::errorf("Could not find a display adapter");

   //loop through and tell the user what kind of adapters we found
   _GFXInitReportAdapters(adapters);
   Con::printf( "" );
}

void GFXInit::cleanup()
{
   while( smAdapters.size() )
   {
      GFXAdapter* adapter = smAdapters.last();
      smAdapters.decrement();
      delete adapter;
   }

   if( smRegisterDeviceSignal )
      SAFE_DELETE( smRegisterDeviceSignal );
}

GFXAdapter* GFXInit::getAdapterOfType( GFXAdapterType type )
{
   GFXAdapter* adapter = NULL;
   for( U32 i = 0; i < smAdapters.size(); i++ )
   {
      if( smAdapters[i]->mType == type )
      {
         adapter = smAdapters[i];
         break;
      }
   }
   return adapter;
}

GFXAdapter* GFXInit::chooseAdapter( GFXAdapterType type)
{
   GFXAdapter* adapter = GFXInit::getAdapterOfType(type);
   
   if(!adapter && type != OpenGL)
   {
      Con::errorf("The requested renderer, %s, doesn't seem to be available."
                  " Trying the default, OpenGL.", getAdapterNameFromType(type));
      adapter = GFXInit::getAdapterOfType(OpenGL);         
   }
   
   if(!adapter)
   {
      Con::errorf("The OpenGL renderer doesn't seem to be available. Trying the GFXNulDevice.");
      adapter = GFXInit::getAdapterOfType(NullDevice);
   }
   
   AssertFatal( adapter, "There is no rendering device available whatsoever.");
   return adapter;
}

const char* GFXInit::getAdapterNameFromType(GFXAdapterType type)
{
   static const char* _names[] = { "OpenGL", "D3D9", "D3D8", "NullDevice", "Xenon" };
   
   if( type < 0 || type >= GFXAdapterType_Count )
   {
      Con::errorf( "GFXInit::getAdapterNameFromType - Invalid renderer type, defaulting to OpenGL" );
      return _names[OpenGL];
   }
      
   return _names[type];
}

GFXAdapterType GFXInit::getAdapterTypeFromName(const char* name)
{
   S32 ret = -1;
   for(S32 i = 0; i < GFXAdapterType_Count; i++)
   {
      if( !dStricmp( getAdapterNameFromType((GFXAdapterType)i), name ) )
         ret = i;
   }
   
   if( ret == -1 )
   {
      Con::errorf( "GFXInit::getAdapterTypeFromName - Invalid renderer name, defaulting to D3D9" );
      ret = Direct3D9;
   }
   
   return (GFXAdapterType)ret;
}

GFXAdapter *GFXInit::getBestAdapterChoice()
{
   // Get the user's preference for device...
   const String   renderer   = Con::getVariable("$pref::Video::displayDevice");
   GFXAdapterType adapterType = getAdapterTypeFromName(renderer);
   GFXAdapter     *adapter    = chooseAdapter(adapterType);

   // Did they have one? Return it.
   if(adapter)
      return adapter;

   // Didn't have one. So make it up. Find the highest SM available. Prefer
   // D3D to GL because if we have a D3D device at all we're on windows,
   // and in an unknown situation on Windows D3D is probably the safest bet.
   //
   // If D3D is unavailable, we're not on windows, so GL is de facto the
   // best choice!
   F32 highestSM9 = 0.f, highestSMGL = 0.f;
   GFXAdapter  *foundAdapter8 = NULL, *foundAdapter9 = NULL, 
               *foundAdapterGL = NULL;

   for(S32 i=0; i<smAdapters.size(); i++)
   {
      GFXAdapter *currAdapter = smAdapters[i];
      switch(currAdapter->mType)
      {
      case Direct3D9:
         if(currAdapter->mShaderModel > highestSM9)
         {
            highestSM9 = currAdapter->mShaderModel;
            foundAdapter9 = currAdapter;
         }
         break;

      case OpenGL:
         if(currAdapter->mShaderModel > highestSMGL)
         {
            highestSMGL = currAdapter->mShaderModel;
            foundAdapterGL = currAdapter;
         }
         break;

      case Direct3D8:
         if(!foundAdapter8)
            foundAdapter8 = currAdapter;
         break;

      default:
         break;
      }
   }

   // Return best found in order DX9, GL, DX8.
   if(foundAdapter9)
      return foundAdapter9;

   if(foundAdapterGL)
      return foundAdapterGL;

   if(foundAdapter8)
      return foundAdapter8;

   // Uh oh - we didn't find anything. Grab whatever we can that's not Null...
   for(S32 i=0; i<smAdapters.size(); i++)
      if(smAdapters[i]->mType != NullDevice)
         return smAdapters[i];

   // Dare we return a null device? No. Just return NULL.
   return NULL;
}

GFXVideoMode GFXInit::getInitialVideoMode()
{
   GFXVideoMode vm;
   _GFXInitGetInitialRes(vm, Point2I(800,600));
   return vm;
}

S32 GFXInit::getAdapterCount()
{
   return smAdapters.size();
}

void GFXInit::getAdapters(Vector<GFXAdapter*> *adapters)
{
   adapters->clear();
   for (U32 k = 0; k < smAdapters.size(); k++)
      adapters->push_back(smAdapters[k]);
}

GFXVideoMode GFXInit::getDesktopResolution()
{
   GFXVideoMode resVm;

   // Retrieve Resolution Information.
   resVm.bitDepth    = WindowManager->getDesktopBitDepth();
   resVm.resolution  = WindowManager->getDesktopResolution();
   resVm.fullScreen  = false;
   resVm.refreshRate = 60;

   // Return results
   return resVm;
}

void GFXInit::enumerateAdapters() 
{
   // Call each device class and have it report any adapters it supports.
   if(smAdapters.size())
   {
      // CodeReview Seems like this is ok to just ignore? [bjg, 5/19/07]
      //Con::warnf("GFXInit::enumerateAdapters - already have a populated adapter list, aborting re-analysis.");
      return;
   }

   getRegisterDeviceSignal().trigger(GFXInit::smAdapters);     
}

GFXDevice *GFXInit::createDevice( GFXAdapter *adapter ) 
{
   Con::printf("Attempting to create GFX device: %s", adapter->getName());

   GFXDevice* temp = adapter->mCreateDeviceInstanceDelegate(adapter->mIndex);
   if (temp)
   {
      Con::printf("Device created, setting adapter and enumerating modes");
      temp->setAdapter(*adapter);
      temp->enumerateVideoModes();
      temp->getVideoModeList();
   }
   else
      Con::errorf("Failed to create GFX device");

   GFXDevice::getDeviceEventSignal().trigger(GFXDevice::deCreate);

   return temp;
}

//-----------------------------------------------------------------------------
ConsoleFunction( getDesktopResolution, const char*, 1, 1, "Get the width, height, and bitdepth of the screen.")
{
   GFXVideoMode res = GFXInit::getDesktopResolution();

   String temp = String::ToString("%d %d %d", res.resolution.x, res.resolution.y, res.bitDepth );
   U32 tempSize = temp.size();
   
   char* retBuffer = Con::getReturnBuffer( tempSize );
   dMemcpy( retBuffer, temp, tempSize );
   return retBuffer;
}

ConsoleStaticMethod( GFXInit, getAdapterCount, S32, 1, 1, "() Return the number of adapters available.")
{
   return GFXInit::getAdapterCount();
}

ConsoleStaticMethod( GFXInit, getAdapterName, const char*, 2, 2, "(int id) Returns the name of a given adapter.")
{
   Vector<GFXAdapter*> adapters( __FILE__, __LINE__ );
   GFXInit::getAdapters(&adapters);

   S32 idx = dAtoi(argv[1]);
   if(idx >= 0 && idx < adapters.size())
      return adapters[idx]->mName;

   Con::errorf("GFXInit::getAdapterName - out of range adapter index.");
   return NULL;
}

ConsoleStaticMethod( GFXInit, getAdapterType, const char*, 2, 2, "(int id) Returns the type (D3D9, D3D8, GL, Null) of a given adapter.")
{
   Vector<GFXAdapter*> adapters( __FILE__, __LINE__ );
   GFXInit::getAdapters(&adapters);

   S32 idx = dAtoi(argv[1]);
   if(idx >= 0 && idx < adapters.size())
      return GFXInit::getAdapterNameFromType(adapters[idx]->mType);

   Con::errorf("GFXInit::getAdapterType - out of range adapter index.");
   return NULL;
}

ConsoleStaticMethod( GFXInit, getAdapterShaderModel, F32, 2, 2, "(int id) Returns the SM supported by a given adapter.")
{
   Vector<GFXAdapter*> adapters( __FILE__, __LINE__ );
   GFXInit::getAdapters(&adapters);

   S32 idx = dAtoi(argv[1]);

   if(idx < 0 || idx >= adapters.size())
   {
      Con::errorf("GFXInit::getAdapterShaderModel - out of range adapter index.");
      return -1.f;
   }

   return adapters[idx]->mShaderModel;
}

ConsoleStaticMethod( GFXInit, getDefaultAdapterIndex, S32, 1, 1, "() Returns the index of the adapter we'll be starting up with.")
{
   // Get the chosen adapter, and locate it in the list. (Kind of silly.)
   GFXAdapter *a = GFXInit::getBestAdapterChoice();

   Vector<GFXAdapter*> adapters( __FILE__, __LINE__ );
   for(S32 i=0; i<adapters.size(); i++)
      if(adapters[i]->mIndex == a->mIndex && adapters[i]->mType == a->mType)
         return i;

   Con::warnf("GFXInit::getDefaultAdapterIndex - didn't find the chosen adapter in the adapter list!");
   return -1;
}

ConsoleStaticMethod(GFXInit, getAdapterModeCount, S32, 2, 2,
                    "(int id)\n"
                    "Gets the number of modes available on the specified adapter.\n\n"
                    "\\param id Index of the adapter to get data from.\n"
                    "\\return (int) The number of video modes supported by the adapter, or -1 if the given adapter was not found.")
{
   Vector<GFXAdapter*> adapters( __FILE__, __LINE__ );
   GFXInit::getAdapters(&adapters);

   S32 idx = dAtoi(argv[1]);

   if(idx < 0 || idx >= adapters.size())
   {
      Con::errorf("GFXInit::getAdapterModeCount - You specified an out of range adapter index of %d. Please specify an index in the range [0, %d).", idx, idx >= adapters.size());
      return -1;
   }

   return adapters[idx]->mAvailableModes.size();
}

ConsoleStaticMethod(GFXInit, getAdapterMode, const char*, 3, 3,
                    "(int id, int modeId)\n"
                    "Gets information on the specified adapter and mode.\n\n"
                    "\\param id Index of the adapter to get data from.\n"
                    "\\param modeId Index of the mode to get data from.\n"
                    "\\return (string) A video mode string given an adapter and mode index. See GuiCanvas.getVideoMode()")
{
   Vector<GFXAdapter*> adapters( __FILE__, __LINE__ );
   GFXInit::getAdapters(&adapters);

   S32 adapIdx = dAtoi(argv[1]);
   if((adapIdx < 0) || (adapIdx >= adapters.size()))
   {
      Con::errorf("GFXInit::getAdapterMode - You specified an out of range adapter index of %d. Please specify an index in the range [0, %d).", adapIdx, adapIdx >= adapters.size());
      return NULL;
   }

   S32 modeIdx = dAtoi(argv[2]);
   if((modeIdx < 0) || (modeIdx >= adapters[adapIdx]->mAvailableModes.size()))
   {
      Con::errorf("GFXInit::getAdapterMode - You requested an out of range mode index of %d. Please specify an index in the range [0, %d).", modeIdx, adapters[adapIdx]->mAvailableModes.size());
      return NULL;
   }

   GFXVideoMode vm = adapters[adapIdx]->mAvailableModes[modeIdx];

   // Format and return to console.
   
   String vmString = vm.toString();
   U32 bufferSize = vmString.size();
   char* buffer = Con::getReturnBuffer( bufferSize );
   dMemcpy( buffer, vmString, bufferSize );

   return buffer;
}

ConsoleStaticMethod( GFXInit, createNullDevice, void, 1, 1, "() Create a NULL device")
{
   // Enumerate things for GFX before we have an active device.
   GFXInit::enumerateAdapters();
 
   // Create a device.
   GFXAdapter *a = GFXInit::chooseAdapter(NullDevice);
 
   GFXDevice *newDevice = GFX;
 
   // Do we have a global device already? (This is the site if you want
   // to start rendering to multiple devices simultaneously)
   if(newDevice == NULL)
      newDevice = GFXInit::createDevice(a);
 
   newDevice->setAllowRender( false );
 
   return;
}
