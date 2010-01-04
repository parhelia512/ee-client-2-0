//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/xaudio/sfxXAudioBuffer.h"
#include "sfx/xaudio/sfxXAudioVoice.h"


SFXXAudioBuffer* SFXXAudioBuffer::create( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description )
{
   SFXXAudioBuffer *buffer = new SFXXAudioBuffer( stream, description );
   return buffer;
}

SFXXAudioBuffer::SFXXAudioBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description )
   : Parent( stream, description ),
     mBufferQueue( isStreaming() ? QUEUE_LENGTH : 1 )
{
}

SFXXAudioBuffer::~SFXXAudioBuffer()
{
   if( _getUniqueVoice() != NULL )
      _getUniqueVoice()->_stop();

   while( !mBufferQueue.isEmpty() )
   {
      Buffer buffer = mBufferQueue.popFront();
      destructSingle< SFXInternal::SFXStreamPacket* >( buffer.mPacket );
   }
}

void SFXXAudioBuffer::write( SFXInternal::SFXStreamPacket* const* packets, U32 num )
{
   AssertFatal( SFXInternal::isSFXThread(), "SFXXAudioBuffer::write() - not on SFX thread" );

   using namespace SFXInternal;

   // Unqueue processed packets.

   if( isStreaming() )
   {
      EnterCriticalSection( &_getUniqueVoice()->mLock );

      XAUDIO2_VOICE_STATE state;
      _getUniqueVoice()->mXAudioVoice->GetState( &state );

      U32 numProcessed = mBufferQueue.size() - state.BuffersQueued;
      for( U32 i = 0; i < numProcessed; ++ i )
      {
         Buffer buffer = mBufferQueue.popFront();
         destructSingle< SFXStreamPacket* >( buffer.mPacket );
      }

      LeaveCriticalSection( &_getUniqueVoice()->mLock );
   }

   // Queue new packets.

   for( U32 i = 0; i < num; ++ i )
   {
      SFXStreamPacket* packet = packets[ i ];
      Buffer buffer;

      if( packet->mIsLast )
         buffer.mData.Flags = XAUDIO2_END_OF_STREAM;

      buffer.mPacket = packet;
      buffer.mData.AudioBytes = packet->mSizeActual;
      buffer.mData.pAudioData = packet->data;

      mBufferQueue.pushBack( buffer );

      if( isStreaming() )
      {
         EnterCriticalSection( &_getUniqueVoice()->mLock );
         _getUniqueVoice()->mXAudioVoice->SubmitSourceBuffer( &buffer.mData );
         LeaveCriticalSection( &_getUniqueVoice()->mLock );
      }
   }
}

void SFXXAudioBuffer::_flush()
{
   AssertFatal( isStreaming(), "SFXXAudioBuffer::_flush() - not a streaming buffer" );
   AssertFatal( SFXInternal::isSFXThread(), "SFXXAudioBuffer::_flush() - not on SFX thread" );

   _getUniqueVoice()->_stop();

   while( !mBufferQueue.isEmpty() )
   {
      Buffer buffer = mBufferQueue.popFront();
      destructSingle( buffer.mPacket );
   }
}
