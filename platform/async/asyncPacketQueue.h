//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _ASYNCPACKETQUEUE_H_
#define _ASYNCPACKETQUEUE_H_

#ifndef _TFIXEDSIZEQUEUE_H_
#  include "core/util/tFixedSizeDeque.h"
#endif
#ifndef _TSTREAM_H_
#  include "core/stream/tStream.h"
#endif
#ifndef _TYPETRAITS_H_
#  include "platform/typetraits.h"
#endif


//#define DEBUG_SPEW


/// @file
/// Time-based packet streaming.
///
/// The classes contained in this file can be used for any kind
/// of continuous playback that depends on discrete samplings of
/// a source stream (i.e. any kind of digital media streaming).



//--------------------------------------------------------------------------
//    Async packet queue.
//--------------------------------------------------------------------------

/// Time-based packet stream queue.
///
/// Be aware that using single item queues for synchronizing to a timer
/// will usually result in bad timing behavior when packet uploading takes
/// any non-trivial amount of time.
///
/// @note While the queue associates a variable tick count with each
///   individual packet, the queue fill status is measured in number of
///   packets rather than in total tick time.
///
/// @param Packet Value type of packets passed through this queue.
/// @param TimeSource Value type for time tick source to which the queue
///   is synchronized.
/// @param Consumer Value type of stream to which the packets are written.
template< typename Packet, typename TimeSource = IPositionable< U32 >*, typename Consumer = IOutputStream< Packet >*, typename Tick = U32 >
class AsyncPacketQueue
{
   public:

      typedef void Parent;

      /// The type of data packets being streamed through this queue.
      typedef typename TypeTraits< Packet >::BaseType PacketType;

      /// The type of consumer that receives the packets from this queue.
      typedef typename TypeTraits< Consumer >::BaseType ConsumerType;

      ///
      typedef typename TypeTraits< TimeSource >::BaseType TimeSourceType;
      
      ///
      typedef Tick TickType;

   protected:

      /// Information about the time slice covered by an
      /// individual packet currently on the queue.
      struct QueuedPacket
      {
         ///
         TickType mStartTick;

         ///
         TickType mEndTick;

         QueuedPacket( TickType start, TickType end )
            : mStartTick( start ), mEndTick( end ) {}

         ///
         TickType getNumTicks() const
         {
            return ( mEndTick - mStartTick );
         }
      };

      typedef FixedSizeDeque< QueuedPacket > PacketQueue;

      /// If true, packets that have missed their proper queuing timeframe
      /// will be dropped.  If false, they will be queued nonetheless.
      bool mDropPackets;

      /// Total number of ticks spanned by the total queue playback time.
      /// If this is zero, the total queue time is considered to be infinite.
      TickType mTotalTicks;

      ///
      TickType mTotalQueuedTicks;

      ///
      PacketQueue mPacketQueue;

      /// The time source to which we are sync'ing.
      TimeSource mTimeSource;

      /// The output stream that this queue feeds into.
      Consumer mConsumer;

      /// Total number of packets queued so far.
      U32 mTotalQueuedPackets;
      
   public:

      ///
      AsyncPacketQueue(    U32 maxQueuedPackets,
                           TimeSource timeSource,
                           Consumer consumer,
                           TickType totalTicks = 0,
                           bool dropPackets = false )
         : mTotalTicks( totalTicks ),
           mTotalQueuedTicks( 0 ),
           mPacketQueue( maxQueuedPackets ),
           mTimeSource( timeSource ),
           mConsumer( consumer ),
           mDropPackets( dropPackets )
      {
         #ifdef TORQUE_DEBUG
         mTotalQueuedPackets = 0;
         #endif
      }

      /// @return true if there are currently
      bool isEmpty() const { return mPacketQueue.isEmpty(); }

      /// @return true if all packets have been streamed.
      bool isAtEnd() const;

      /// @return true if the queue needs one or more new packets to be submitted.
      bool needPacket();

      ///
      bool submitPacket(   Packet packet,
                           TickType packetTicks,
                           bool isLast = false,
                           TickType packetPos = TypeTraits< TickType >::MAX );

      ///
      TickType getCurrentTick() const { return Deref( mTimeSource ).getPosition(); }

      ///
      TickType getTotalQueuedTicks() const { return mTotalQueuedTicks; }
      
      ///
      U32 getTotalQueuedPackets() const { return mTotalQueuedPackets; }
};

template< typename Packet, typename TimeSource, typename Consumer, typename Tick >
inline bool AsyncPacketQueue< Packet, TimeSource, Consumer, Tick >::isAtEnd() const
{
   if( !mTotalTicks )
      return false;
   else
      return ( getCurrentTick() >= mTotalTicks
               && ( mDropPackets || mTotalQueuedTicks >= mTotalTicks ) );
}

template< typename Packet, typename TimeSource, typename Consumer, typename Tick >
bool AsyncPacketQueue< Packet, TimeSource, Consumer, Tick >::needPacket()
{
   if( mPacketQueue.capacity() != 0 )
      return true;
   else
   {
      // Unqueue packets that have expired their playtime.

      TickType currentTick = getCurrentTick();
      while( mPacketQueue.size() && currentTick >= mPacketQueue.front().mEndTick )
      {
         #ifdef DEBUG_SPEW
         Platform::outputDebugString( "[AsyncPacketQueue] expired packet #%i: %i-%i (tick: %i; queue: %i)",
            mTotalQueuedPackets - mPacketQueue.size(),
            U32( mPacketQueue.front().mStartTick ),
            U32( mPacketQueue.front().mEndTick ),
            U32( currentTick ),
            mPacketQueue.size() );
         #endif
         
         mPacketQueue.popFront();
      }

      // Need more packets if the queue isn't full anymore.

      return ( mPacketQueue.capacity() != 0 );
   }
}

template< typename Packet, typename TimeSource, typename Consumer, typename Tick >
bool AsyncPacketQueue< Packet, TimeSource, Consumer, Tick >::submitPacket( Packet packet, TickType packetTicks, bool isLast, TickType packetPos )
{
   AssertFatal( mPacketQueue.capacity() != 0,
      "AsyncPacketQueue::submitPacket() - Queue is full!" );

   TickType packetStartPos;
   TickType packetEndPos;
   
   if( packetPos != TypeTraits< TickType >::MAX )
   {
      packetStartPos = packetPos;
      packetEndPos = packetPos + packetTicks;
   }
   else
   {
      packetStartPos = mTotalQueuedTicks;
      packetEndPos = mTotalQueuedTicks + packetTicks;
   }

   // Check whether the packet is outdated, if enabled.

   bool dropPacket = false;
   if( mDropPackets )
   {
      TickType currentTick = getCurrentTick();
      if( currentTick >= packetEndPos )
         dropPacket = true;
   }

   #ifdef DEBUG_SPEW
   Platform::outputDebugString( "[AsyncPacketQueue] new packet #%i: %i-%i (ticks: %i, current: %i, queue: %i)%s",
      mTotalQueuedPackets,
      U32( mTotalQueuedTicks ),
      U32( packetEndPos ),
      U32( packetTicks ),
      U32( getCurrentTick() ),
      mPacketQueue.size(),
      dropPacket ? " !! DROPPED !!" : "" );
   #endif

   // Queue the packet.

   if( !dropPacket )
   {
      mPacketQueue.pushBack( QueuedPacket( packetStartPos, packetEndPos ) );
      Deref( mConsumer ).write( &packet, 1 );
   }

   mTotalQueuedTicks = packetEndPos;
   if( isLast && !mTotalTicks )
      mTotalTicks = mTotalQueuedTicks;
      
   mTotalQueuedPackets ++;
   
   return !dropPacket;
}

#undef DEBUG_SPEW
#endif // _ASYNCPACKETQUEUE_H_
