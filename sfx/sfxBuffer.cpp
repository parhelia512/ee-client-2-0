//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/sfxBuffer.h"
#include "sfx/sfxVoice.h"
#include "sfx/sfxDescription.h"
#include "sfx/sfxInternal.h"


Signal< void( SFXBuffer* ) > SFXBuffer::smBufferDestroyedSignal;


SFXBuffer::SFXBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description, bool createAsyncState )
   : mStatus( STATUS_Null ),
     mIsStreaming( description->mIsStreaming ),
     mFormat( stream->getFormat() ),
     mDuration( stream->getDuration() ),
     mUniqueVoice( NULL ),
     mIsDead( false ),
     mIsLooping( description->mIsLooping ),
     mIsUnique( description->mIsStreaming )
{
   using namespace SFXInternal;
   
   if( createAsyncState )
   {
      U32 packetLength = description->mStreamPacketSize;
      U32 readAhead = description->mStreamReadAhead;

      ThreadSafeRef< SFXStream > streamRef( stream );
      mAsyncState = new AsyncState(
         new SFXAsyncStream
         ( streamRef, mIsStreaming, packetLength, readAhead,
           mIsStreaming ? description->mIsLooping : false ) );
   }
}

SFXBuffer::SFXBuffer( SFXDescription* description )
   : mStatus( STATUS_Ready ),
     mIsStreaming( false ), // Not streaming through our system.
     mDuration( 0 ), // Must be set by subclass.
     mUniqueVoice( NULL ),
     mIsDead( false ),
     mIsLooping( description->mIsLooping ),
     mIsUnique( false ) // Must be set by subclass.
{
}

SFXBuffer::~SFXBuffer()
{
   smBufferDestroyedSignal.trigger( this );
}

void SFXBuffer::load()
{
   if( getStatus() == STATUS_Null )
   {
      AssertFatal( mAsyncState != NULL, "SFXBuffer::load() - no async state!" );

      _setStatus( STATUS_Loading );
      SFXInternal::UPDATE_LIST().add( this );
      mAsyncState->mStream->start();
   }
}

bool SFXBuffer::update()
{
   using namespace SFXInternal;

   if( isDead() )
   {
      mAsyncState->mStream->stop();
      mAsyncState = NULL;
      gDeadBufferList.pushFront( this );
      return false;
   }
   else if( isAtEnd() && isStreaming() )
      return true;

   AssertFatal( mAsyncState != NULL, "SFXBuffer::update() - async state has already been released" );

   bool needFurtherUpdates = true;
   if( !isStreaming() )
   {
      // Not a streaming buffer.  If the async stream has its data
      // ready, we load it and finish up on our async stuff.

      SFXStreamPacket* packet;
      while( mAsyncState->mStream->read( &packet, 1 ) )
      {
         bool isLast = packet->mIsLast;

         write( &packet, 1 );
         packet = NULL;

         _setStatus( STATUS_Ready );
         
         if( isLast )
         {
            mAsyncState = NULL;
            needFurtherUpdates = false;
            break;
         }
      }
   }
   else
   {
      // A streaming buffer.
      //
      // If we don't have a queue, construct one now.  Note that when doing
      // a stream seek on us, SFXVoice will drop our async stream and queue.
      // Work on local copies of the pointers to allow having the async state
      // be switched in parallel.
      //
      // Note that despite us being a streamed buffer, our unique voice may
      // not yet have been assigned to us.

      AsyncStatePtr state = mAsyncState;
      if( !state->mQueue && !mUniqueVoice.isNull() )
      {
         // Make sure we have no data currently submitted to the device.
         // This will stop and discard an outdated feed if we've been
         // switching streams.
         
         _setStatus( STATUS_Loading );
         _flush();

         // Create a new queue.

         state->mQueue = new SFXAsyncQueue( mUniqueVoice, this, mIsLooping );
      }
      
      // Check the queue.

      if( state->mQueue != NULL )
      {
         // Feed the queue, if necessary and possible.

         while( state->mQueue->needPacket() )
         {
            SFXStreamPacket* packet;
            if( state->mStream->read( &packet, 1 ) )
            {
               state->mQueue->submitPacket( packet, packet->getSampleCount() );
               _setStatus( STATUS_Ready );
            }
            else
               break;
         }

         // Detect buffer underrun and end-of-stream.

         if( isReady() && state->mQueue->isEmpty() )
            _setStatus( STATUS_Blocked );
         else if( state->mQueue->isAtEnd() )
            _setStatus( STATUS_AtEnd );
      }
   }

   return needFurtherUpdates;
}

void SFXBuffer::destroySelf()
{
   AssertFatal( !isDead(), "SFXBuffer::destroySelf() - buffer already dead" );

   mIsDead = true;
   if( !mAsyncState )
   {
      // Easy way.  This buffer has finished all its async
      // processing, so we can just kill it.

      delete this;
   }
   else
   {
      // Hard way.  We will have to make the buffer finish
      // all its concurrent stuff, so we mark it dead, make sure
      // to see an update, and then wait for the buffer to surface
      // on the dead buffer list.

      SFXInternal::TriggerUpdate();
   }
}

void SFXBuffer::_setStatus( EStatus status )
{
   if( mStatus != status )
   {
      mOnStatusChange.trigger( this, status );
      mStatus = status;
   }
}

SFXBuffer::AsyncState::AsyncState()
   : mQueue( NULL )
{
}

SFXBuffer::AsyncState::AsyncState( SFXInternal::SFXAsyncStream* stream )
   : mStream( stream ), mQueue( NULL )
{
}

SFXBuffer::AsyncState::~AsyncState()
{
   if( mQueue )
      SAFE_DELETE( mQueue );
}
