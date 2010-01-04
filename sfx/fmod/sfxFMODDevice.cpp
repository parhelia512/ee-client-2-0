//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/threads/mutex.h"
#include "sfx/fmod/sfxFMODDevice.h"
#include "sfx/fmod/sfxFMODBuffer.h"
#include "sfx/sfxListener.h"
#include "sfx/sfxSystem.h"
#include "platform/async/asyncUpdate.h"


FMOD_SYSTEM *SFXFMODDevice::smSystem;
FModFNTable *SFXFMODDevice::smFunc;
Mutex* FModFNTable::mutex;


SFXFMODDevice::SFXFMODDevice( SFXProvider* provider, 
                              FModFNTable *fmodFnTbl, 
                              int deviceIdx, 
                              String name )
   :  SFXDevice( name, provider, false, 32 ),
      m3drolloffmode( FMOD_3D_LOGROLLOFF ),
      mDeviceIndex( deviceIdx )
{
	// Store off the function pointers for later use.
	smFunc = fmodFnTbl;
}

bool SFXFMODDevice::_init()
{
   #define FMOD_CHECK( message )                               \
      if( result != FMOD_OK )                                  \
      {                                                        \
         Con::errorf( "SFXFMODDevice::_init() - %s (%s)",      \
            message,                                           \
            FMOD_ErrorString( result ) );                      \
         return false;                                         \
      }

	AssertISV(smSystem, 
      "SFXFMODDevice::_init() - can't init w/o an existing FMOD system handle!");

   FMOD_RESULT result;

	// Initialize everything from fmod.
	FMOD_SPEAKERMODE speakermode;
	FMOD_CAPS        caps;
	result = smFunc->FMOD_System_GetDriverCaps(smSystem, 0, &caps, ( int* ) 0, ( int* ) 0, &speakermode);
   FMOD_CHECK( "SFXFMODDevice::SFXFMODDevice - Failed to get driver caps" );

	result = smFunc->FMOD_System_SetDriver(smSystem, mDeviceIndex);
   FMOD_CHECK( "SFXFMODDevice::SFXFMODDevice - Failed to set driver" );

	result = smFunc->FMOD_System_SetSpeakerMode(smSystem, speakermode);
   FMOD_CHECK( "SFXFMODDevice::SFXFMODDevice - Failed to set the user selected speaker mode" );

	if (caps & FMOD_CAPS_HARDWARE_EMULATED)             /* The user has the 'Acceleration' slider set to off!  This is really bad for latency!. */
	{                                                   /* You might want to warn the user about this. */
		result = smFunc->FMOD_System_SetDSPBufferSize(smSystem, 1024, 10);
      FMOD_CHECK( "SFXFMODDevice::SFXFMODDevice - Failed to set DSP buffer size" );
	}

	result = smFunc->FMOD_System_Init(smSystem, 100, FMOD_INIT_NORMAL, ( void* ) 0 );
	if( result == FMOD_ERR_OUTPUT_CREATEBUFFER )         /* Ok, the speaker mode selected isn't supported by this soundcard.  Switch it back to stereo... */
	{
		result = smFunc->FMOD_System_SetSpeakerMode(smSystem, FMOD_SPEAKERMODE_STEREO);
      FMOD_CHECK( "SFXFMODDevice::SFXFMODDevice - failed on fallback speaker mode setup" );
		result = smFunc->FMOD_System_Init(smSystem, 100, FMOD_INIT_NORMAL, ( void* ) 0);
	}
   FMOD_CHECK( "SFXFMODDevice::SFXFMODDevice - failed to init system" );   

   // we let FMod handle this stuff, instead of having sfx do it
   //mCullInaudible = false;

   // Start the update thread.
   
   if( !Con::getBoolVariable( "$_forceAllMainThread" ) )
   {
      SFXInternal::gUpdateThread = new AsyncPeriodicUpdateThread
         ( "FMOD Update Thread", SFXInternal::gBufferUpdateList,
           Con::getIntVariable( "$pref::SFX::updateInterval", SFXInternal::DEFAULT_UPDATE_INTERVAL ) );
      SFXInternal::gUpdateThread->start();
   }
   
   return true;
}

SFXFMODDevice::~SFXFMODDevice()
{
   _releaseAllResources();
   
   smFunc->FMOD_System_Close( smSystem );
}

SFXBuffer* SFXFMODDevice::createBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description )
{
   AssertFatal( stream, "SFXFMODDevice::createBuffer() - Got a null stream!" );
   AssertFatal( description, "SFXFMODDevice::createBuffer() - Got null description!" );

   SFXFMODBuffer *buffer = SFXFMODBuffer::create( stream, description );
   if ( buffer )
      _addBuffer( buffer );

   return buffer;
}

SFXBuffer* SFXFMODDevice::createBuffer( const String& filename, SFXDescription* description )
{
   AssertFatal( filename.isNotEmpty(), "SFXFMODDevice::createBuffer() - Got an empty filename!" );
   AssertFatal( description, "SFXFMODDevice::createBuffer() - Got null description!" );

   SFXFMODBuffer* buffer = SFXFMODBuffer::create( filename, description );
   if( buffer )
      _addBuffer( buffer );
      
   return buffer;
}

SFXVoice* SFXFMODDevice::createVoice( bool is3D, SFXBuffer* buffer )
{
   AssertFatal( buffer, "SFXFMODDevice::createVoice() - Got null buffer!" );

   SFXFMODBuffer* fmodBuffer = dynamic_cast<SFXFMODBuffer*>( buffer );
   AssertFatal( fmodBuffer, "SFXFMODDevice::createVoice() - Got bad buffer!" );

   SFXFMODVoice* voice = SFXFMODVoice::create( this, fmodBuffer );
   if ( !voice )
      return NULL;

   _addVoice( voice );
	return voice;
}

void SFXFMODDevice::update( const SFXListener& listener )
{
   Parent::update( listener );

	// Set the listener state on fmod!
	Point3F position, vel, fwd, up;
	vel = listener.getVelocity();
	listener.getTransform().getColumn( 3, &position );
	listener.getTransform().getColumn( 1, &fwd );
	listener.getTransform().getColumn( 2, &up );

	// We have to convert to FMOD_VECTOR, thus this cacophony.
	// NOTE: we correct for handedness here. We model off of the d3d device.
	//       Basically, XYZ => XZY.
	FMOD_VECTOR fposition, fvel, ffwd, fup;
#define COPY_FMOD_VECTOR(a) f##a.x = a.x; f##a.y = a.z; f##a.z = a.y;
	COPY_FMOD_VECTOR(position)
	COPY_FMOD_VECTOR(vel)
	COPY_FMOD_VECTOR(fwd)
	COPY_FMOD_VECTOR(up)
#undef COPY_FMOD_VECTOR

	// Do the listener state update, then update!
	FModAssert(smFunc->FMOD_System_Set3DListenerAttributes(smSystem, 0, &fposition, &fvel, &ffwd, &fup), "Failed to set 3d listener attribs!");
	FModAssert(smFunc->FMOD_System_Update(smSystem), "Failed to update system!");
}

void SFXFMODDevice::setDistanceModel( SFXDistanceModel model )
{
   switch( model )
   {
      case SFXDistanceModelLinear:
         m3drolloffmode = FMOD_3D_LINEARROLLOFF;
         break;
         
      case SFXDistanceModelLogarithmic:
         m3drolloffmode = FMOD_3D_LOGROLLOFF;
         break;
         
      default:
         AssertWarn( false, "SFXFMODDevice::setDistanceModel - model not implemented" );
   }
}

void SFXFMODDevice::setDopplerFactor( F32 factor )
{
   F32 dopplerFactor;
   F32 distanceFactor;
   F32 rolloffFactor;
   
   smFunc->FMOD_System_Get3DSettings( smSystem, &dopplerFactor, &distanceFactor, &rolloffFactor );
   dopplerFactor = factor;
   smFunc->FMOD_System_Set3DSettings( smSystem, dopplerFactor, distanceFactor, rolloffFactor );
}

void SFXFMODDevice::setRolloffFactor( F32 factor )
{
   F32 dopplerFactor;
   F32 distanceFactor;
   F32 rolloffFactor;
   
   smFunc->FMOD_System_Get3DSettings( smSystem, &dopplerFactor, &distanceFactor, &rolloffFactor );
   rolloffFactor = factor;
   smFunc->FMOD_System_Set3DSettings( smSystem, dopplerFactor, distanceFactor, rolloffFactor );
}

ConsoleFunction(fmodDumpMemoryStats, void, 1, 1, "()")
{
   int current = 0;
   int max = 0;

   if (SFXFMODDevice::smFunc && SFXFMODDevice::smFunc->FMOD_Memory_GetStats.fn)
         SFXFMODDevice::smFunc->FMOD_Memory_GetStats(&current, &max);
   Con::printf("Fmod current: %d, max: %d", current, max);
}