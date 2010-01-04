//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/sfxProvider.h"
#include "sfx/fmod/sfxFMODDevice.h"
#include "core/util/safeRelease.h"
#include "console/console.h"
#include "core/util/safeDelete.h"


class SFXFMODProvider : public SFXProvider
{
public:

   SFXFMODProvider()
      : SFXProvider( "FMOD" ) {}
   virtual ~SFXFMODProvider();

protected:
   FModFNTable mFMod;

   struct FModDeviceInfo : SFXDeviceInfo
   {
   };

   void init();

public:

   SFXDevice* createDevice( const String& deviceName, bool useHardware, S32 maxBuffers );

};

SFX_INIT_PROVIDER( SFXFMODProvider );

//------------------------------------------------------------------------------
// Helper

bool fmodBindFunction( DLibrary *dll, void *&fnAddress, const char* name )
{
   fnAddress = dll->bind( name );

   if (!fnAddress)
      Con::warnf( "FMod Loader: DLL bind failed for %s", name );

   return fnAddress != 0;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

void SFXFMODProvider::init()
{
   const char* dllName;
#ifdef TORQUE_OS_WIN32
   dllName = "fmodex.dll";
#elif defined( TORQUE_OS_MAC )
   dllName = "libfmodex.dylib";
#elif defined( TORQUE_OS_XENON ) || defined( TORQUE_OS_PS3 )
   dllName = "FMOD static library";
#else
#  warning Need to set FMOD DLL filename for platform.
   return;
#endif

#if !defined(TORQUE_OS_XENON) && !defined(TORQUE_OS_PS3)
   // Grab the functions we'll want from the fmod DLL.
   mFMod.dllRef = OsLoadLibrary( dllName );

   if(!mFMod.dllRef)
   {
      Con::warnf( "SFXFMODProvider - Could not locate %s - FMod  not available.", dllName );
      return;
   }
#define FMOD_FUNCTION(fn_name, fn_args) \
   mFMod.isLoaded &= fmodBindFunction(mFMod.dllRef, *(void**)&mFMod.fn_name.fn, #fn_name);
#else
#define FMOD_FUNCTION(fn_name, fn_args) \
   (*(void**)&mFMod.fn_name.fn) = &fn_name;
#endif

   mFMod.isLoaded = true;

#include "sfx/fmod/fmodFunctions.h"
#undef FMOD_FUNCTION

   if(mFMod.isLoaded == false)
   {
      Con::warnf("SFXFMODProvider - Could not locate %s - FMod  not available.", dllName);
      return;
   }

   // Allocate the FMod system.
   FMOD_RESULT res = mFMod.FMOD_System_Create(&SFXFMODDevice::smSystem);

   if(res != FMOD_OK)
   {
      // Failed - deal with it!
      Con::warnf("SFXFMODProvider - Could not create the FMod system - FMod  not available.");
      return;
   }

   // Check that the version is OK.
   unsigned int version;
   res = mFMod.FMOD_System_GetVersion(SFXFMODDevice::smSystem, &version);
   FModAssert(res, "SFXFMODProvider - Failed to get fmod version!");

   if(version < FMOD_VERSION)
   {
      Con::warnf("SFXFMODProvider - FMod version in DLL is too old - FMod  not available.");
      return;
   }

   int majorVersion = ( version & 0xffff0000 ) >> 16;
   int minorVersion = ( version & 0x0000ff00 ) >> 8;
   int revision = ( version & 0x000000ff );
   Con::printf( "SFXFMODProvider - FMOD version: %i.%i.%i", majorVersion, minorVersion, revision );

   // Now, enumerate our devices.
   int numDrivers;
   res = mFMod.FMOD_System_GetNumDrivers(SFXFMODDevice::smSystem, &numDrivers);
   FModAssert(res, "SFXFMODProvider - Failed to get driver count - FMod  not available.");

   char nameBuff[256];

   for(S32 i=0; i<numDrivers; i++)
   {
      res = mFMod.FMOD_System_GetDriverInfo(SFXFMODDevice::smSystem, i, nameBuff, 256, ( FMOD_GUID* ) NULL);
      if( res != FMOD_OK )
      {
         Con::warnf( "SFXFMODProvider - Failed to get driver name - FMod  not available." );
         return;
      }
      nameBuff[ 255 ] = '\0';

      // Great - got something - so add it to the list of options.
      FModDeviceInfo *fmodInfo = new FModDeviceInfo();
      fmodInfo->name = String( nameBuff );
#ifdef TORQUE_OS_WIN32 //RDFIXME: why do we have this?
      fmodInfo->hasHardware = false;
#else
      fmodInfo->hasHardware = true;
#endif
      fmodInfo->maxBuffers = 32;
      fmodInfo->driver = String();

      mDeviceInfo.push_back(fmodInfo);
   }

   // Did we get any devices?
   if ( mDeviceInfo.empty() )
   {
      Con::warnf( "SFXFMODProvider - No valid devices found - FMod  not available." );
      return;
   }

   // TODO: FMOD_Memory_Initialize
#ifdef TORQUE_OS_XENON
   const dsize_t memSz = 5 * 1024 * 1024;
   void *memBuffer = XPhysicalAlloc( memSz, MAXULONG_PTR, 0, PAGE_READWRITE );
   mFMod.FMOD_Memory_Initialize( memBuffer, memSz, FMOD_MEMORY_ALLOCCALLBACK(NULL), FMOD_MEMORY_REALLOCCALLBACK(NULL), FMOD_MEMORY_FREECALLBACK(NULL) );
#endif

   // Wow, we made it - register the provider.
   regProvider( this );
}

SFXFMODProvider::~SFXFMODProvider()
{
   // Release the fmod system.
   if ( mFMod.isLoaded )
   {
      mFMod.FMOD_System_Release(SFXFMODDevice::smSystem);
      SFXFMODDevice::smSystem = NULL;
   }
}

SFXDevice* SFXFMODProvider::createDevice( const String& deviceName, bool useHardware, S32 maxBuffers )
{
   FModDeviceInfo* info = dynamic_cast< FModDeviceInfo* >
      ( _findDeviceInfo( deviceName ) );

   if( !info )
      return NULL;

   SFXFMODDevice* device = new SFXFMODDevice(this, &mFMod, 0, info->name );
   if( !device->_init() )
      SAFE_DELETE( device );

   return device;
}
