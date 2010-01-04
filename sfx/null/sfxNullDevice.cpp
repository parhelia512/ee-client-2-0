//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/null/sfxNullDevice.h"
#include "sfx/null/sfxNullBuffer.h"
#include "sfx/sfxListener.h"
#include "sfx/sfxInternal.h"


SFXNullDevice::SFXNullDevice( SFXProvider* provider, 
                              String name, 
                              bool useHardware, 
                              S32 maxBuffers )

   :  SFXDevice( name, provider, useHardware, maxBuffers )
{
   mMaxBuffers = getMax( maxBuffers, 8 );
}

SFXNullDevice::~SFXNullDevice()
{
}

SFXBuffer* SFXNullDevice::createBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description )
{
   SFXNullBuffer* buffer = new SFXNullBuffer( stream, description );
   _addBuffer( buffer );

   return buffer;
}

SFXVoice* SFXNullDevice::createVoice( bool is3D, SFXBuffer *buffer )
{
   // Don't bother going any further if we've 
   // exceeded the maximum voices.
   if ( mVoices.size() >= mMaxBuffers )
      return NULL;

   AssertFatal( buffer, "SFXNullDevice::createVoice() - Got null buffer!" );

   SFXNullBuffer* nullBuffer = dynamic_cast<SFXNullBuffer*>( buffer );
   AssertFatal( nullBuffer, "SFXNullDevice::createVoice() - Got bad buffer!" );

   SFXNullVoice* voice = new SFXNullVoice( nullBuffer );
   if ( !voice )
      return NULL;

   _addVoice( voice );
	return voice;
}
