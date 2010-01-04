//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXSOURCE_H_
#define _SFXSOURCE_H_

#ifndef _SFXDEVICE_H_
   #include "sfx/sfxDevice.h"
#endif
#ifndef _SFXVOICE_H_
   #include "sfx/sfxVoice.h"
#endif
#ifndef _SIMBASE_H_
   #include "console/simBase.h"
#endif
#ifndef _MPOINT3_H_
   #include "math/mPoint3.h"
#endif
#ifndef _MMATRIX_H_
   #include "math/mMatrix.h"
#endif
#ifndef _TSTREAM_H_
   #include "core/stream/tStream.h"
#endif
#ifndef _TIMESOURCE_H_
   #include "core/util/timeSource.h"
#endif
#ifndef _TORQUE_LIST_
   #include "core/util/tList.h"
#endif
#ifndef _SFXPROFILE_H_
   #include "sfx/sfxProfile.h"
#endif


class SFXDescription;
class SFXBuffer;
class SFXDevice;
class SFXEffect;



/// A source is a scriptable controller for all 
/// aspects of sound playback.
class SFXSource : public SimObject,
                  public IPositionable< U32 >
{
   friend class SFXSystem;
   friend class SFXListener;

   typedef SimObject Parent;
   
   protected:

      typedef GenericTimeSource< RealMSTimer > TimeSource;
      typedef Torque::List< SFXEffect* > EffectList;

      /// Used by SFXSystem to create sources.
      static SFXSource* _create( SFXDevice* device, SFXProfile* profile );
      static SFXSource* _create( SFXDevice* device, const ThreadSafeRef< SFXStream >& stream, SFXDescription* description );

      /// Internal constructor used for sources.
      SFXSource( SFXProfile* profile, SFXDescription* description );

      /// You cannot delete a source directly.
      /// @see SFX_DELETE
      virtual ~SFXSource();

      /// This is called only from the device to allow
      /// the source to update it's attenuated volume.
      void _updateVolume( const MatrixF& listener );

      /// Called by the device so that the source can 
      /// update itself and any attached buffer.
      SFXStatus _updateStatus();

      /// The last updated playback status of the source.
      mutable SFXStatus mStatus;

      /// The simulation tick count that playback was started at for this source.
      U32 mPlayStartTick;

      /// Time object used to keep track of playback when running virtualized
      /// (i.e. without connected SFXVoice).  Sync'ed to SFXVoice playback as needed.
      TimeSource mVirtualPlayTimer;

      /// The profile used to create this source.
      /// NULL if the source has been constructed directly from an SFXStream.
      SimObjectPtr<SFXProfile> mProfile;

      /// The device specific voice which is used during
      /// playback.  By making it a SafePtr it will NULL
      /// automatically when the device is deleted.
      StrongWeakRefPtr<SFXVoice> mVoice;

      /// The reference counted device specific buffer used by 
      /// the voice for playback.
      StrongWeakRefPtr<SFXBuffer> mBuffer;

      /// The duration of the sound cached from the buffer in
      /// _initBuffer() used for managing virtual sources.
      U32 mDuration;

      /// This is the volume of a source with respect
      /// to the last listener position.  It is used
      /// for culling sounds.
      F32 mAttenuatedVolume;

      /// The distance of this source to the last 
      /// listener position.
      F32 mDistToListener;

      /// The desired sound volume.
      F32 mVolume;

      /// This 
      F32 mModulativeVolume;

      /// The sound pitch scalar.
      F32 mPitch;

      /// The transform if this is a 3d source.
      MatrixF mTransform;

      /// The last set velocity.
      VectorF mVelocity;

      bool mIs3D;

      bool mIsLooping;

      bool mIsStreaming;

      F32 mMinDistance;

      F32 mMaxDistance;

      /// In radians.
      F32 mConeInsideAngle;

      /// In radians.
      F32 mConeOutsideAngle;

      ///
      F32 mConeOutsideVolume;
      
      ///
      F32 mFadeInTime;
      
      ///
      F32 mFadeOutTime;

      /// Channel number used for playback of this source.
      U32 mChannel;

      ///
      StringTableEntry mStatusCallback;
      
      /// List of effects that are active on this source.
      EffectList mEffects;
      
      /// We overload this to disable creation of 
      /// a source via script 'new'.
      bool processArguments( S32 argc, const char **argv );

      /// Used internally for setting the sound status.
      bool _setStatus( SFXStatus status );

      /// This is called from SFXSystem for setting
      /// the volume scalar generated from the master
      /// and channel volumes.
      void _setModulativeVolume( F32 volume );

      /// Create a new voice for this source.
      bool _allocVoice( SFXDevice* device );

      /// Release the voice if the source has one.
      bool _releaseVoice();
      
      ///
      void _update();

      ///
      void _setBuffer( SFXBuffer* buffer );
      
      /// Reload the sound buffer.  Temporarily goes to virtualized playback when necessary.
      void _reloadBuffer();

      /// Delete all effects of the given type.
      template< class T > void _clearEffects();
      
      ///
      void _onProfileChanged( SFXProfile* profile )
      {
         if( profile == mProfile.getObject() )
            _reloadBuffer();
      }

   public:

      DECLARE_CONOBJECT(SFXSource);

      /// The default constructor is *only* here to satisfy the
      /// construction needs of IMPLEMENT_CONOBJECT.  It does not
      /// create a valid source!
      explicit SFXSource();

      /// Also needed by IMPLEMENT_CONOBJECT, but we don't use it.
      static void initPersistFields();

      /// This is normally called from the system to 
      /// detect if this source has been assigned a
      /// voice for playback.
      bool hasVoice() const { return mVoice != NULL; }

      /// Starts the sound from the current playback position.
      void play( F32 fadeInTime = -1.0f );

      /// Stops playback and resets the playback position.
      void stop( F32 fadeOutTime = -1.0f );

      /// Pauses the sound playback.
      void pause( F32 fadeOutTime = -1.0f );

      /// Return the current playback position in milliseconds.
      /// @note For looping sources, this returns the total playback time so far.
      U32 getPosition() const;

      /// Set the current playback position in milliseconds.
      void setPosition( U32 ms );

      /// Sets the position and orientation for a 3d buffer.
      void setTransform( const MatrixF& transform );

      /// Sets the velocity for a 3d buffer.
      void setVelocity( const VectorF& velocity );

      /// Sets the minimum and maximum distances for 3d falloff.
      void setMinMaxDistance( F32 min, F32 max );

      /// Set sound cone of a 3D sound.
      ///
      /// @param innerAngle Inner cone angle in degrees.
      /// @param outerAngle Outer cone angle in degrees.
      /// @param outerVolume Outer volume factor.
      void setCone(  F32 innerAngle, 
                     F32 outerAngle,
                     F32 outerVolume );

      /// Sets the source volume which will still be
      /// scaled by the master and channel volumes.
      void setVolume( F32 volume );

      /// Sets the source pitch scale.
      void setPitch( F32 pitch );

      /// Returns the last set velocity.
      const VectorF& getVelocity() const { return mVelocity; }

      /// Returns the last set transform.
      const MatrixF& getTransform() const { return mTransform; }

      /// Returns the source's total playback time in milliseconds.
      U32 getDuration() const { return mDuration; }

      /// Returns the source volume.
      F32 getVolume() const { return mVolume; }

      /// Returns the volume with respect to the master 
      /// and channel volumes and the listener.
      F32 getAttenuatedVolume() const { return mAttenuatedVolume; }

      /// Returns the source pitch scale.
      F32 getPitch() const { return mPitch; }

      /// Returns the last known status without checking
      /// the voice or doing the virtual calculation.
      SFXStatus getLastStatus() const { return mStatus; }

      /// Returns the sound status.
      SFXStatus getStatus() const { return const_cast<SFXSource*>( this )->_updateStatus(); }

      /// Returns true if the source is playing.
      bool isPlaying() const { return getStatus() == SFXStatusPlaying; }

      /// Returns true if the source is stopped.
      bool isStopped() const { return getStatus() == SFXStatusStopped; }

      /// Returns true if the source has been paused.
      bool isPaused() const { return getStatus() == SFXStatusPaused; }
      
      ///
      bool isBlocked() const { return ( mVoice && mVoice->getStatus() == SFXStatusBlocked ); }
      
      ///
      bool isVirtualized() const { return ( mVoice == NULL && mVirtualPlayTimer.isStarted() ); }

      /// Returns true if this is a 3D source.
      bool is3d() const { return mIs3D; }

      /// Returns true if this is a looping source.
      bool isLooping() const { return mIsLooping; }

      /// Returns true if this is a continuously streaming source.
      bool isStreaming() const { return mIsStreaming; }

      /// Returns true if the source's associated data is ready for playback.
      bool isReady() const;

      /// Returns the volume channel this source is assigned to.
      U32 getChannel() const { return mChannel; }

      /// Returns the last distance to the listener.
      F32 getDistToListener() const { return mDistToListener; }
      
      ///
      void addMarker( const String& name, U32 pos );

      ///
      SFXProfile* getProfile() const;

      // SimObject.
      void onRemove();
};

#endif // _SFXSOURCE_H_