//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/sfxProvider.h"
#include "sfx/null/sfxNullDevice.h"
#include "core/strings/stringFunctions.h"


class SFXNullProvider : public SFXProvider
{
public:

   SFXNullProvider()
      : SFXProvider( "Null" ) {}
   virtual ~SFXNullProvider();

protected:
   void addDeviceDesc( const String& name, const String& desc );
   void init();

public:

   SFXDevice* createDevice( const String& deviceName, bool useHardware, S32 maxBuffers );

};

SFX_INIT_PROVIDER( SFXNullProvider );

void SFXNullProvider::init()
{
   regProvider( this );
   addDeviceDesc( "SFX Null Device", "SFX baseline device" );
}

SFXNullProvider::~SFXNullProvider()
{
}


void SFXNullProvider::addDeviceDesc( const String& name, const String& desc )
{
   SFXDeviceInfo* info = new SFXDeviceInfo;
   info->name = desc;
   info->driver = name;
   info->hasHardware = false;
   info->maxBuffers = 8;

   mDeviceInfo.push_back( info );
}

SFXDevice* SFXNullProvider::createDevice( const String& deviceName, bool useHardware, S32 maxBuffers )
{
   SFXDeviceInfo* info = _findDeviceInfo( deviceName );

   // Do we find one to create?
   if ( info )
      return new SFXNullDevice( this, info->name, useHardware, maxBuffers );

   return NULL;
}
