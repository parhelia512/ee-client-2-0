//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXXAUDIOBUFFER_H_
#define _SFXXAUDIOBUFFER_H_

#include <xaudio2.h>
#ifndef _SFXINTERNAL_H_
#  include "sfx/sfxInternal.h"
#endif
#ifndef _TFIXEDSIZEDEQUE_H_
#  include "core/util/tFixedSizeDeque.h"
#endif


class SFXXAudioBuffer : public SFXBuffer
{
   public:

      typedef SFXBuffer Parent;
      
      friend class SFXXAudioDevice;
      friend class SFXXAudioVoice;

   protected:

      enum { QUEUE_LENGTH = SFXInternal::SFXAsyncQueue::DEFAULT_STREAM_QUEUE_LENGTH + 1 };

      struct Buffer
      {
         XAUDIO2_BUFFER mData;
         SFXInternal::SFXStreamPacket* mPacket;

         Buffer()
            : mPacket( 0 )
         {
            dMemset( &mData, 0, sizeof( mData ) );
         }
      };

      typedef FixedSizeDeque< Buffer > QueueType;

      QueueType mBufferQueue;

      SFXXAudioVoice* _getUniqueVoice() { return ( SFXXAudioVoice* ) mUniqueVoice.getPointer(); }

      ///
      SFXXAudioBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description );
      virtual ~SFXXAudioBuffer();

      // SFXBuffer.
      virtual void write( SFXInternal::SFXStreamPacket* const* packets, U32 num );
      void _flush();

   public:

      ///
      static SFXXAudioBuffer* create( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description );
};

#endif // _SFXXAUDIOBUFFER_H_