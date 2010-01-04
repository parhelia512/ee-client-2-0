//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/openal/sfxALDevice.h"
#include "sfx/openal/sfxALBuffer.h"
#include "sfx/sfxListener.h"
#include "platform/async/asyncUpdate.h"


SFXALDevice::SFXALDevice(  SFXProvider *provider, 
                           const OPENALFNTABLE &openal, 
                           String name, 
                           bool useHardware, 
                           S32 maxBuffers )
   :  Parent( name, provider, useHardware, maxBuffers ),
      mOpenAL( openal ), 
      mDevice( NULL ), 
      mContext( NULL )
{
   mMaxBuffers = getMax( maxBuffers, 8 );

   // TODO: The OpenAL device doesn't set the primary buffer
   // $pref::SFX::frequency or $pref::SFX::bitrate!

   mDevice = mOpenAL.alcOpenDevice( name );
   mOpenAL.alcGetError( mDevice );
   if( mDevice ) 
   {
      mContext = mOpenAL.alcCreateContext( mDevice, NULL );

      if( mContext ) 
         mOpenAL.alcMakeContextCurrent( mContext );

      U32 err = mOpenAL.alcGetError( mDevice );
      
      if( err != ALC_NO_ERROR )
         Con::errorf( "SFXALDevice - Initialization Error: %s", mOpenAL.alcGetString( mDevice, err ) );
   }

   AssertFatal( mDevice != NULL && mContext != NULL, "Failed to create OpenAL device and/or context!" );

   // Start the update thread.
   
   if( !Con::getBoolVariable( "$_forceAllMainThread" ) )
   {
      SFXInternal::gUpdateThread = new AsyncPeriodicUpdateThread
         ( "OpenAL Update Thread", SFXInternal::gBufferUpdateList,
           Con::getIntVariable( "$pref::SFX::updateInterval", SFXInternal::DEFAULT_UPDATE_INTERVAL ) );
      SFXInternal::gUpdateThread->start();
   }
}


SFXALDevice::~SFXALDevice()
{
   _releaseAllResources();

   mOpenAL.alcMakeContextCurrent( NULL );
	mOpenAL.alcDestroyContext( mContext );
	mOpenAL.alcCloseDevice( mDevice );
}

SFXBuffer* SFXALDevice::createBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description )
{
   AssertFatal( stream, "SFXALDevice::createBuffer() - Got null stream!" );
   AssertFatal( description, "SFXALDevice::createBuffer() - Got null description!" );

   SFXALBuffer* buffer = SFXALBuffer::create(   mOpenAL, 
                                                stream,
                                                description, 
                                                mUseHardware );
   if ( !buffer )
      return NULL;

   _addBuffer( buffer );
   return buffer;
}

SFXVoice* SFXALDevice::createVoice( bool is3D, SFXBuffer *buffer )
{
   // Don't bother going any further if we've 
   // exceeded the maximum voices.
   if ( mVoices.size() >= mMaxBuffers )
      return NULL;

   AssertFatal( buffer, "SFXALDevice::createVoice() - Got null buffer!" );

   SFXALBuffer* alBuffer = dynamic_cast<SFXALBuffer*>( buffer );
   AssertFatal( alBuffer, "SFXALDevice::createVoice() - Got bad buffer!" );

   SFXALVoice* voice = SFXALVoice::create( this, alBuffer );
   if ( !voice )
      return NULL;

   _addVoice( voice );
	return voice;
}

void SFXALDevice::update( const SFXListener &listener )
{
   Parent::update( listener );

   // Torque and OpenAL are both right handed 
   // systems, so no coordinate flipping is needed.

   const MatrixF &transform = listener.getTransform();
   Point3F pos, tupple[2];
   transform.getColumn( 3, &pos );
   transform.getColumn( 1, &tupple[0] );
   transform.getColumn( 2, &tupple[1] );

   const VectorF &velocity = listener.getVelocity();

   mOpenAL.alListenerfv( AL_POSITION, pos );
   mOpenAL.alListenerfv( AL_VELOCITY, velocity );
   mOpenAL.alListenerfv( AL_ORIENTATION, (const F32 *)&tupple[0] );
}

void SFXALDevice::setDistanceModel( SFXDistanceModel model )
{
   switch( model )
   {
      case SFXDistanceModelLinear:
         mOpenAL.alDistanceModel( AL_LINEAR_DISTANCE_CLAMPED );
         if( mRolloffFactor != 1.0f )
            _setRolloffFactor( 1.0f ); // No rolloff on linear.
         break;
         
      case SFXDistanceModelLogarithmic:
         mOpenAL.alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );
         if( mUserRolloffFactor != mRolloffFactor )
            _setRolloffFactor( mUserRolloffFactor );
         break;
         
      default:
         AssertWarn( false, "SFXALDevice::setDistanceModel - distance model not implemented" );
   }
   
   mDistanceModel = model;
}

void SFXALDevice::setDopplerFactor( F32 factor )
{
   mOpenAL.alDopplerFactor( factor );
}

void SFXALDevice::_setRolloffFactor( F32 factor )
{
   mRolloffFactor = factor;
   
   for( U32 i = 0, num = mVoices.size(); i < num; ++ i )
      mOpenAL.alSourcef( ( ( SFXALVoice* ) mVoices[ i ] )->mSourceName, AL_ROLLOFF_FACTOR, factor );
}

void SFXALDevice::setRolloffFactor( F32 factor )
{
   if( mDistanceModel == SFXDistanceModelLinear && factor != 1.0f )
      Con::errorf( "SFXALDevice::setRolloffFactor - rolloff factor <> 1.0f ignored in linear distance model" );
   else
      _setRolloffFactor( factor );
      
   mUserRolloffFactor = factor;
}
