//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXEFFECT_H_
#define _SFXEFFECT_H_

#ifndef _SFXSOURCE_H_
   #include "sfx/sfxSource.h"
#endif
#ifndef _TSTREAM_H_
   #include "core/stream/tStream.h"
#endif


/// An SFXEffect modifies the playback of an SFXSource while it is running.
class SFXEffect : public IPolled
{
   protected:
      
      /// The source that this effect works on.
      SFXSource* mSource;
      
      /// If true, the effect is removed from the effects stack
      bool mRemoveWhenDone;
            
   public:
   
      /// Create an effect that operates on "source".
      SFXEffect( SFXSource* source, bool removeWhenDone = false )
         : mSource( source ) {}
   
      virtual ~SFXEffect() {}
};

/// An SFXEffect that is triggered once after passing a certain playback position.
class SFXOneShotEffect : public SFXEffect
{
   public:
   
      typedef SFXEffect Parent;
      
   protected:
   
      /// Playback position that triggers the effect.
      U32 mTriggerPos;
      
      ///
      virtual void _onTrigger() = 0;
      
   public:
   
      /// Create an effect that triggers when playback of "source" passes "triggerPos".
      SFXOneShotEffect( SFXSource* source, U32 triggerPos, bool removeWhenDone = false );
   
      // IPolled.
      virtual bool update();
};

/// An SFXEffect that is spans a certain range of playback time.
class SFXRangeEffect : public SFXEffect
{
   public:

      typedef SFXEffect Parent;
      
   protected:
   
      /// If true, the effect is currently being applied to the source.
      bool mIsActive;
         
      /// Playback position in milliseconds when this effect becomes active.
      U32 mStartTime;
      
      /// Playback position in milliseconds when this effect becomes inactive.
      U32 mEndTime;
      
      /// Called when the play cursor passes mStartTime.
      /// @note There may be latency between the cursor actually passing mStartTime
      ///   and this method being called.
      virtual void _onStart() {}
      
      /// Called on each update() while the play cursor is in range.
      virtual void _onUpdate() {}
      
      /// Called when the play cursor passes mEndTime.
      /// @note There may be latency between the cursor actually passing mEndTime
      ///   and this method being called.
      virtual void _onEnd() {}
      
   public:
   
      /// Create an effect that operates on "source" between "startTime" milliseconds
      /// (inclusive) and "endTime" milliseconds (exclusive).
      SFXRangeEffect( SFXSource* source, U32 startTime, U32 endTime, bool removeWhenDone = false );
         
      ///
      bool isActive() const { return mIsActive; }
      
      // IPolled.
      virtual bool update();
};

/// A volume fade effect (fade-in or fade-out).
class SFXFadeEffect : public SFXRangeEffect
{
   public:
   
      typedef SFXRangeEffect Parent;
      
      enum EOnEnd
      {
         ON_END_Nop,       ///< Do nothing with source when fade is complete.
         ON_END_Stop,      ///< Stop source when fade is complete.
         ON_END_Pause,     ///< Pause source when fade is complete.
      };
      
   protected:
      
      /// Volume when beginning fade.  Set when effect is activated.
      F32 mStartVolume;
      
      /// Volume when ending fade.
      F32 mEndVolume;
      
      /// Current volume level.
      F32 mCurrentVolume;
      
      /// Action to perform when the fade has been completed.  Defaults to no action.
      EOnEnd mOnEnd;
      
      // SFXEffect.
      virtual void _onStart();
      virtual void _onUpdate();
      virtual void _onEnd();
      
   public:
   
      /// Create an effect that fades the volume of "source" to "endVolume" over the
      /// period of  "time" seconds.  The fade will start at "referenceTime" using the
      /// source's current volume at the time as a start.
      SFXFadeEffect( SFXSource* source, F32 time, F32 endVolume, U32 startTime, EOnEnd onEndDo = ON_END_Nop, bool removeWhenDone = false );
      
      virtual ~SFXFadeEffect();
};

/// An effect that calls a method on the SFXSource when a particular playback position
/// is passed.
///
/// @note At the moment, doing a setPosition() on a source will not cause markers that have
///   been jumped over in the operation to be ignored.  Instead they will trigger on the
///   next update.
class SFXMarkerEffect : public SFXOneShotEffect
{
   public:
   
      typedef SFXOneShotEffect Parent;
      
   protected:
   
      /// Symbolic marker name that is passed to the "onMarkerPassed" callback.
      String mMarkerName;
      
      // SFXOneShotEffect
      virtual void _onTrigger();
   
   public:
   
      SFXMarkerEffect( SFXSource* source, const String& name, U32 pos, bool removeWhenDone = false );
};

#endif // !_SFXEFFECT_H_
