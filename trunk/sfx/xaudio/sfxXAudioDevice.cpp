//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/xaudio/sfxXAudioDevice.h"
#include "sfx/sfxListener.h"
#include "platform/async/asyncUpdate.h"
#include "core/stringTable.h"
#include "console/console.h"
#include "core/util/safeRelease.h"
#include "core/tAlgorithm.h"
#include "platform/profiler.h"


SFXXAudioDevice::SFXXAudioDevice(   SFXProvider* provider, 
                                    const String& name,
                                    IXAudio2 *xaudio,
                                    U32 deviceIndex,
                                    U32 speakerChannelMask,
                                    U32 maxBuffers )
   :  Parent( name, provider, false, maxBuffers ),
      mXAudio( xaudio ),
      mMasterVoice( NULL )
{
   dMemset( &mListener, 0, sizeof( mListener ) );

   // If mMaxBuffers is negative then use some default value.
   // to decide on a good maximum value... or set 8.
   //
   // TODO: We should change the terminology to voices!
   if ( mMaxBuffers < 0 )
      mMaxBuffers = 64;

   // Create the mastering voice.
   HRESULT hr = mXAudio->CreateMasteringVoice(  &mMasterVoice, 
                                                XAUDIO2_DEFAULT_CHANNELS,
                                                XAUDIO2_DEFAULT_SAMPLERATE, 
                                                0, 
                                                deviceIndex, 
                                                NULL );
   if ( FAILED( hr ) || !mMasterVoice )
   {
      Con::errorf( "SFXXAudioDevice - Failed creating master voice!" );
      return;
   }

   mMasterVoice->GetVoiceDetails( &mMasterVoiceDetails );

   // Init X3DAudio.
   X3DAudioInitialize(  speakerChannelMask, 
                        X3DAUDIO_SPEED_OF_SOUND, 
                        mX3DAudio );

   // Start the update thread.

   if( !Con::getBoolVariable( "$_forceAllMainThread" ) )
   {
      SFXInternal::gUpdateThread = new AsyncUpdateThread
         ( "XAudio Update Thread", SFXInternal::gBufferUpdateList );
      SFXInternal::gUpdateThread->start();
   }
}


SFXXAudioDevice::~SFXXAudioDevice()
{
   _releaseAllResources();

   if ( mMasterVoice )
   {
      mMasterVoice->DestroyVoice();
      mMasterVoice = NULL;
   }

   // Kill the engine.
   SAFE_RELEASE( mXAudio );
}


SFXBuffer* SFXXAudioDevice::createBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description )
{
   SFXXAudioBuffer* buffer = SFXXAudioBuffer::create( stream, description );
   if ( !buffer )
      return NULL;

   _addBuffer( buffer );
   return buffer;
}

SFXVoice* SFXXAudioDevice::createVoice( bool is3D, SFXBuffer *buffer )
{
   // Don't bother going any further if we've 
   // exceeded the maximum voices.
   if ( mVoices.size() >= mMaxBuffers )
      return NULL;

   AssertFatal( buffer, "SFXXAudioDevice::createVoice() - Got null buffer!" );

   SFXXAudioBuffer* xaBuffer = dynamic_cast<SFXXAudioBuffer*>( buffer );
   AssertFatal( xaBuffer, "SFXXAudioDevice::createVoice() - Got bad buffer!" );

   SFXXAudioVoice* voice = SFXXAudioVoice::create( mXAudio, is3D, xaBuffer );
   if ( !voice )
      return NULL;

   voice->mXAudioDevice = this;

   _addVoice( voice );
	return voice;
}

void SFXXAudioDevice::_setOutputMatrix( SFXXAudioVoice *voice )
{
   X3DAUDIO_DSP_SETTINGS dspSettings = {0};
   FLOAT32 matrix[12] = { 0 };
   dspSettings.DstChannelCount = mMasterVoiceDetails.InputChannels;
   dspSettings.pMatrixCoefficients = matrix;

   const X3DAUDIO_EMITTER &emitter = voice->getEmitter();
   dspSettings.SrcChannelCount = emitter.ChannelCount;

   // Calculate the output volumes and doppler.
   X3DAudioCalculate(   mX3DAudio, 
                        &mListener, 
                        &emitter, 
                        X3DAUDIO_CALCULATE_MATRIX | 
                        X3DAUDIO_CALCULATE_DOPPLER,
                        &dspSettings );

   voice->mXAudioVoice->SetOutputMatrix(   mMasterVoice, 
                                           dspSettings.SrcChannelCount, 
                                           dspSettings.DstChannelCount, 
                                           dspSettings.pMatrixCoefficients,
                                           4321 );

   voice->mXAudioVoice->SetFrequencyRatio(   dspSettings.DopplerFactor * voice->mPitch, 
                                             4321 );

   // Commit the changes.
   mXAudio->CommitChanges( 4321 );
}

void SFXXAudioDevice::update( const SFXListener& listener )
{
   PROFILE_SCOPE( SFXXAudioDevice_Update );

   Parent::update( listener );

   // Get the transform from the listener.
   const MatrixF& transform = listener.getTransform();
   transform.getColumn( 3, (Point3F*)&mListener.Position );
   transform.getColumn( 1, (Point3F*)&mListener.OrientFront );
   transform.getColumn( 2, (Point3F*)&mListener.OrientTop );

   // And the velocity...
   const VectorF& velocity = listener.getVelocity();
   mListener.Velocity.x = velocity.x;
   mListener.Velocity.y = velocity.y;
   mListener.Velocity.z = velocity.z;

   // XAudio and Torque use opposite handedness, so
   // flip the z coord to account for that.
   mListener.Position.z    *= -1.0f;
   mListener.OrientFront.z *= -1.0f;
   mListener.OrientTop.z   *= -1.0f;
   mListener.Velocity.z    *= -1.0f;

   X3DAUDIO_DSP_SETTINGS dspSettings = {0};
   FLOAT32 matrix[12] = { 0 };
   dspSettings.DstChannelCount = mMasterVoiceDetails.InputChannels;
   dspSettings.pMatrixCoefficients = matrix;

   dspSettings.DopplerFactor = mDopplerFactor;

   // Now update the volume and frequency of 
   // all the active 3D voices.
   VoiceVector::iterator voice = mVoices.begin();
   for ( ; voice != mVoices.end(); voice++ )
   {
      SFXXAudioVoice* xaVoice = ( SFXXAudioVoice* ) *voice;

      // Skip 2D or stopped voices.
      if (  !xaVoice->is3D() ||
            xaVoice->getStatus() != SFXStatusPlaying )
         continue;

      const X3DAUDIO_EMITTER &emitter = xaVoice->getEmitter();
      dspSettings.SrcChannelCount = emitter.ChannelCount;

      // Calculate the output volumes and doppler.
      X3DAudioCalculate(   mX3DAudio, 
                           &mListener, 
                           &emitter, 
                           X3DAUDIO_CALCULATE_MATRIX | 
                           X3DAUDIO_CALCULATE_DOPPLER, 
                           &dspSettings );

      xaVoice->mXAudioVoice->SetOutputMatrix(   mMasterVoice, 
                                                dspSettings.SrcChannelCount, 
                                                dspSettings.DstChannelCount, 
                                                dspSettings.pMatrixCoefficients,
                                                4321 ) ;

      xaVoice->mXAudioVoice->SetFrequencyRatio( dspSettings.DopplerFactor * xaVoice->mPitch, 
                                                4321 );
   }

   // Commit the changes.
   mXAudio->CommitChanges( 4321 );
}

void SFXXAudioDevice::setDistanceModel( SFXDistanceModel model )
{
   mDistanceModel = model;
}

void SFXXAudioDevice::setDopplerFactor( F32 factor )
{
   mDopplerFactor = factor;
}

void SFXXAudioDevice::setRolloffFactor( F32 factor )
{
   mRolloffFactor = factor;
}
