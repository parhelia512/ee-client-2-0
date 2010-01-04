//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXXAUDIOVOICE_H_
#define _SFXXAUDIOVOICE_H_

#include <xaudio2.h>
#include <x3daudio.h>

#include "sfx/sfxVoice.h"


class SFXXAudioBuffer;


class SFXXAudioVoice :  public SFXVoice,
                        public IXAudio2VoiceCallback
{
   public:

      typedef SFXVoice Parent;

      friend class SFXXAudioDevice;
      friend class SFXXAudioBuffer;

   protected:

      /// This constructor does not create a valid voice.
      /// @see SFXXAudioVoice::create
      SFXXAudioVoice( SFXXAudioBuffer* buffer );

      /// The device that created us.
      SFXXAudioDevice *mXAudioDevice;

      /// The XAudio voice.
      IXAudio2SourceVoice *mXAudioVoice;

      ///
      XAUDIO2_BUFFER mNonStreamBuffer;

      ///
      U32 mNonStreamBufferOffset;

      ///
      CRITICAL_SECTION mLock;

      /// Used to know what sounds need 
      /// positional updates.
      bool mIs3D;

      ///
      mutable bool mHasStopped;

      ///
      bool mHasStarted;

      ///
      bool mIsPlaying;

      ///
      bool mIsLooping;

      /// Since 3D sounds are pitch shifted for doppler
      /// effect we need to track the users base pitch.
      F32 mPitch;

      /// The cached X3DAudio emitter data.
      X3DAUDIO_EMITTER mEmitter;

      ///
      U32 mSamplesPlayedOffset;

      // IXAudio2VoiceCallback
      void STDMETHODCALLTYPE OnStreamEnd();      
      void STDMETHODCALLTYPE OnVoiceProcessingPassStart( UINT32 BytesRequired ) {}
      void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() {}
      void STDMETHODCALLTYPE OnBufferEnd( void *bufferContext );
      void STDMETHODCALLTYPE OnBufferStart( void *bufferContext ) {}
      void STDMETHODCALLTYPE OnLoopEnd( void *bufferContext ) {}
      void STDMETHODCALLTYPE OnVoiceError( void * bufferContext, HRESULT error ) {}

      /// @deprecated This is only here for compatibility with
      /// the March 2008 SDK version of IXAudio2VoiceCallback.
      void STDMETHODCALLTYPE OnVoiceProcessingPassStart() {} 

      void _flush();
      void _loadNonStreamed();

      // SFXVoice.
      virtual SFXStatus _status() const;
      virtual void _play();
      virtual void _pause();
      virtual void _stop();
      virtual void _seek( U32 sample );
      virtual U32 _tell() const;

      SFXXAudioBuffer* _getBuffer() const { return ( SFXXAudioBuffer* ) mBuffer.getPointer(); }

   public:

      ///
      static SFXXAudioVoice* create(   IXAudio2 *xaudio,
                                       bool is3D,
                                       SFXXAudioBuffer *buffer,
                                       SFXXAudioVoice* inVoice = NULL );

      ///
      virtual ~SFXXAudioVoice();

      // SFXVoice
      void setMinMaxDistance( F32 min, F32 max );
      void play( bool looping );
      void setVelocity( const VectorF& velocity );
      void setTransform( const MatrixF& transform );
      void setVolume( F32 volume );
      void setPitch( F32 pitch );
      void setCone( F32 innerAngle, F32 outerAngle, F32 outerVolume );

      /// Is this a 3D positional voice.
      bool is3D() const { return mIs3D; }

      ///
      const X3DAUDIO_EMITTER& getEmitter() const { return mEmitter; }
};

#endif // _SFXXAUDIOVOICE_H_