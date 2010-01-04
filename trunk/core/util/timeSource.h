//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TIMESOURCE_H_
#define _TIMESOURCE_H_

#ifndef _PLATFORM_H_
#  include "platform/platform.h"
#endif
#ifndef _TSTREAM_H_
#  include "core/stream/tStream.h"
#endif


/// Timer that queries the real-time ticker.
struct RealMSTimer
{
   static U32 getTick()
   {
      return Platform::getRealMilliseconds();
   }
};

/// Timer that queries the simulation-time ticker.
struct VirtualMSTimer
{
   static U32 getTick()
   {
      return Platform::getVirtualMilliseconds();
   }
};

///
template< class Timer = RealMSTimer, typename Tick = U32 >
class GenericTimeSource : public IPositionable< Tick >,
                          public IProcess,
                          public IResettable
{
   public:

      typedef IPositionable< Tick > Parent;
      typedef Tick TickType;

   protected:

      ///
      TickType mStartTime;

      ///
      TickType mPauseTime;

      ///
      Timer mTimer;

   public:

      GenericTimeSource()
         : mStartTime( TypeTraits< TickType >::MAX ),
           mPauseTime( TypeTraits< TickType >::MAX ) {}

      bool isStarted() const
      {
         return ( mStartTime != TypeTraits< TickType >::MAX );
      }
      bool isPaused() const
      {
         return ( mPauseTime != TypeTraits< TickType >::MAX );
      }

      /// Return the number of ticks since the time source
      /// has been started.
      TickType getPosition() const
      {
         if( !isStarted() )
            return TypeTraits< TickType >::ZERO;
         else if( isPaused() )
            return ( mPauseTime - mStartTime );
         else
            return ( mTimer.getTick() - mStartTime );
      }

      ///
      void setPosition( TickType pos )
      {
         if( !isStarted() )
            mStartTime = pos;
         else
         {
            TickType savedStartTime = mStartTime;

            mStartTime = ( mTimer.getTick() - pos );
            if( isPaused() )
               mPauseTime = ( mStartTime + ( mPauseTime - savedStartTime ) );
         }
      }

      // IResettable.
      virtual void reset()
      {
         mStartTime = TypeTraits< TickType >::MAX;
         mPauseTime = TypeTraits< TickType >::MAX;
      }

      // IProcess.
      virtual void start()
      {
         if( !isStarted() )
         {
            TickType now = mTimer.getTick();

            if( isPaused() )
            {
               mStartTime += now - mPauseTime;
               mPauseTime = TypeTraits< TickType >::MAX;
            }
            else
               mStartTime = now;
         }
      }
      virtual void stop()
      {
         reset();
      }
      virtual void pause()
      {
         if( !isPaused() )
            mPauseTime = mTimer.getTick();
      }
};

#endif // _TIMESOURCE_H_
