//-----------------------------------------------------------------------------
// Torque Game Engine Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "platform/platform.h"

#include "sfx/sfxProvider.h"
#include "sfx/openal/sfxALDevice.h"
#include "sfx/openal/aldlist.h"

#include "core/strings/stringFunctions.h"
#include "console/console.h"

class SFXALProvider : public SFXProvider
{
public:

   SFXALProvider()
      : SFXProvider( "OpenAL" ) {}
   virtual ~SFXALProvider();

protected:
   OPENALFNTABLE mOpenAL;
   ALDeviceList *mALDL;

   struct ALDeviceInfo : SFXDeviceInfo
   {
      
   };

   void init();

public:
   SFXDevice *createDevice( const String& deviceName, bool useHardware, S32 maxBuffers );

};

SFX_INIT_PROVIDER( SFXALProvider );

void SFXALProvider::init()
{
   if( LoadOAL10Library( NULL, &mOpenAL ) != AL_TRUE )
   {
      Con::printf( "SFXALProvider - OpenAL not available." );
      return;
   }
   mALDL = new ALDeviceList( mOpenAL );

   // Did we get any devices?
   if ( mALDL->GetNumDevices() < 1 )
   {
      Con::printf( "SFXALProvider - No valid devices found!" );
      return;
   }

   // Cool, loop through them, and caps em
   const char *deviceFormat = "OpenAL v%d.%d %s";

   char temp[256];
   for( int i = 0; i < mALDL->GetNumDevices(); i++ )
   {
      ALDeviceInfo* info = new ALDeviceInfo;
      
      info->name = String( mALDL->GetDeviceName( i ) );

      int major, minor, eax = 0;

      mALDL->GetDeviceVersion( i, &major, &minor );

      // Apologies for the blatent enum hack -patw
      for( int j = SFXALEAX2; j < SFXALEAXRAM; j++ )
         eax += (int)mALDL->IsExtensionSupported( i, (SFXALCaps)j );

      if( eax > 0 )
      {
         eax += 2; // EAX support starts at 2.0
         dSprintf( temp, sizeof( temp ), "[EAX %d.0] %s", eax, ( mALDL->IsExtensionSupported( i, SFXALEAXRAM ) ? "EAX-RAM" : "" ) );
      }
      else
         dStrcpy( temp, "" );

      info->driver = String::ToString( deviceFormat, major, minor, temp );
      info->hasHardware = eax > 0;
      info->maxBuffers = mALDL->GetMaxNumSources( i );

      mDeviceInfo.push_back( info );
   }

   regProvider( this );
}

SFXALProvider::~SFXALProvider()
{
   UnloadOAL10Library();
   delete mALDL;
}

SFXDevice *SFXALProvider::createDevice( const String& deviceName, bool useHardware, S32 maxBuffers )
{
   ALDeviceInfo *info = dynamic_cast< ALDeviceInfo* >
      ( _findDeviceInfo( deviceName) );

   // Do we find one to create?
   if ( info )
      return new SFXALDevice( this, mOpenAL, info->name, useHardware, maxBuffers );

   return NULL;
}