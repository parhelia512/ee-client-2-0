//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/sfxVoice.h"
#include "sfx/sfxBuffer.h"
#include "sfx/sfxInternal.h"
#include "console/console.h"


Signal< void( SFXVoice* voice ) > SFXVoice::smVoiceDestroyedSignal;


//-----------------------------------------------------------------------------

SFXVoice::SFXVoice( SFXBuffer* buffer )
   : mBuffer( buffer ),
     mStatus( SFXStatusNull ),
     mOffset( 0 )
{
}

//-----------------------------------------------------------------------------

SFXVoice::~SFXVoice()
{
   if( mBuffer )
      mBuffer->mOnStatusChange.remove( this, &SFXVoice::_onBufferStatusChange );

   smVoiceDestroyedSignal.trigger( this );
}

//-----------------------------------------------------------------------------

void SFXVoice::_attachToBuffer()
{
   using namespace SFXInternal;

   // If the buffer is unique, attach us as its unique voice.

   if( mBuffer->isUnique() )
   {
      AssertFatal( !mBuffer->mUniqueVoice,
         "SFXVoice::SFXVoice - streaming buffer already is assigned a voice" );

      mBuffer->mUniqueVoice = this;
   }

   mBuffer->mOnStatusChange.notify( this, &SFXVoice::_onBufferStatusChange );
}

//-----------------------------------------------------------------------------

void SFXVoice::_onBufferStatusChange( SFXBuffer* buffer, SFXBuffer::EStatus newStatus )
{
   AssertFatal( buffer == mBuffer, "SFXVoice::_onBufferStatusChange() - got an invalid buffer" );

   switch( newStatus )
   {
   case SFXBuffer::STATUS_Loading:
      if( mStatus != SFXStatusNull )
         _stop();
      mStatus = SFXStatusBlocked;
      break;

   case SFXBuffer::STATUS_AtEnd:
      _stop();
      mStatus = SFXStatusStopped;
      mOffset = 0;
      break;

   case SFXBuffer::STATUS_Blocked:
      _pause();
      mStatus = SFXStatusBlocked;
      break;

   case SFXBuffer::STATUS_Ready:
      if( mStatus == SFXStatusBlocked )
      {
         // Get the playback going again.

         _play();
         mStatus = SFXStatusPlaying;
      }
      break;
   
   case SFXBuffer::STATUS_Null:
      AssertFatal(false, "Buffer changed to invalid NULL status");
      break;
   }
}

//-----------------------------------------------------------------------------

SFXStatus SFXVoice::getStatus() const
{
   if( mStatus == SFXStatusPlaying
       && !mBuffer->isReady() )
      return SFXStatusBlocked;

   // Detect when the device has finished playback.
   if( mStatus == SFXStatusPlaying
       && !mBuffer->isStreaming()
       && _status() == SFXStatusStopped )
      mStatus = SFXStatusStopped;

   return mStatus;
}

//-----------------------------------------------------------------------------

void SFXVoice::play( bool looping )
{
   AssertFatal( mBuffer != NULL, "SFXVoice::play() - no buffer" );
   using namespace SFXInternal;

   // For streaming, check whether we have played previously.
   // If so, reset the buffer's stream.

   if( mStatus == SFXStatusStopped
       && mBuffer->isStreaming() )
      setPosition( 0 );

   if( mBuffer->isReady() )
   {
      _play();
      mStatus = SFXStatusPlaying;
   }
   else
      mStatus = SFXStatusBlocked;
}

//-----------------------------------------------------------------------------

void SFXVoice::pause()
{
   _pause();
   mStatus = SFXStatusPaused;
}

//-----------------------------------------------------------------------------

void SFXVoice::stop()
{
   _stop();
   mStatus = SFXStatusStopped;
}

//-----------------------------------------------------------------------------

U32 SFXVoice::getPosition() const
{
   // It depends on the device if and when it will return a count of the total samples
   // played so far.  With streaming buffers, all devices will do that.  With non-streaming
   // buffers, some may do for looping voices thus returning a number that exceeds the actual
   // source stream size.  So, clamp things into range here and also take care of any offsetting
   // resulting from a setPosition() call

   U32 pos = _tell() + mOffset;
   const U32 numStreamSamples = mBuffer->getFormat().getSampleCount( mBuffer->getDuration() );

   if( mBuffer->mIsLooping )
      pos %= numStreamSamples;
   else if( pos > numStreamSamples )
   {
      // Ensure we never report out-of-range positions even if the device does.

      pos = numStreamSamples;
   }

   return pos;
}

//-----------------------------------------------------------------------------

void SFXVoice::setPosition( U32 inSample )
{
   const U32 sample = getMin( inSample, mBuffer->getFormat().getSampleCount( mBuffer->getDuration() ) - 1 );

   if( !mBuffer->isStreaming() )
   {
      // Non-streaming sound.  Just seek in the device buffer.

      _seek( sample );
   }
   else
   {
      ThreadSafeRef< SFXBuffer::AsyncState > oldState = mBuffer->mAsyncState;
      AssertFatal( oldState != NULL,
         "SFXVoice::setPosition() - streaming buffer must have valid async state" );

      // Rather than messing up the async code by adding repositioning (which
      // further complicates synchronizing the various parts), just construct
      // a complete new AsyncState and discard the old one.  The only problem
      // here is the stateful sound streams.  We can't issue a new packet as long
      // as we aren't sure there's no request pending, so we just clone the stream
      // and leave the old one to the old AsyncState.

      ThreadSafeRef< SFXStream > sfxStream = oldState->mStream->getSourceStream()->clone();
      if( sfxStream != NULL )
      {
         IPositionable< U32 >* sfxPositionable = dynamic_cast< IPositionable< U32 >* >( sfxStream.ptr() );
         if( sfxPositionable )
         {
            sfxPositionable->setPosition( sample * sfxStream->getFormat().getBytesPerSample() );
            
            ThreadSafeRef< SFXInternal::SFXAsyncStream > newStream =
               new SFXInternal::SFXAsyncStream
                  ( sfxStream,
                    true,
                    oldState->mStream->getPacketDuration() / 1000,
                    oldState->mStream->getReadAhead(),
                    oldState->mStream->isLooping() );
            newStream->setReadSilenceAtEnd( oldState->mStream->getReadSilenceAtEnd() );

            AssertFatal( newStream->getPacketSize() == oldState->mStream->getPacketSize(),
               "SFXVoice::setPosition() - packet size mismatch with new stream" );

            ThreadSafeRef< SFXBuffer::AsyncState > newState =
               new SFXBuffer::AsyncState( newStream );
            newStream->start();

            // Switch the states.

            mOffset = sample;
            mBuffer->mAsyncState = newState;

            // Stop the old state from reading more data.

            oldState->mStream->stop();

            // Trigger update.

            SFXInternal::TriggerUpdate();
         }
         else
            Con::errorf( "SFXVoice::setPosition - could not seek in SFXStream" );
      }
      else
         Con::errorf( "SFXVoice::setPosition - could not clone SFXStream" );
   }
}
