//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXVOICE_H_
#define _SFXVOICE_H_

#ifndef _REFBASE_H_
#  include "core/util/refBase.h"
#endif
#ifndef _TSTREAM_H_
#  include "core/stream/tStream.h"
#endif
#ifndef _MPOINT3_H_
#  include "math/mPoint3.h"
#endif
#ifndef _MMATRIX_H_
#  include "math/mMatrix.h"
#endif
#ifndef _SFXBUFFER_H_
#  include "sfx/sfxBuffer.h"
#endif


namespace SFXInternal {
   class SFXVoiceTimeSource;
   class SFXAynscQueue;
}


/// The voice interface provides for playback of
/// sound buffers and positioning of 3D sounds.
class SFXVoice :  public StrongRefBase,
                  public IPositionable< U32 >
{
   public:

      typedef void Parent;

      friend class SFXDevice; // _attachToBuffer
      friend class SFXInternal::SFXVoiceTimeSource; // _tell
      friend class SFXInternal::SFXAsyncQueue; // mOffset

   protected:

      ///
      mutable SFXStatus mStatus;

      ///
      WeakRefPtr< SFXBuffer > mBuffer;

      /// For streaming voices, this keeps track of play start offset
      /// after seeking.
      U32 mOffset;

      explicit SFXVoice( SFXBuffer* buffer );

      ///
      virtual SFXStatus _status() const = 0;

      /// Stop playback on the device.
      /// @note Called from both the SFX update thread and the main thread.
      virtual void _stop() = 0;

      /// Start playback on the device.
      /// @note Called from both the SFX update thread and the main thread.
      virtual void _play() = 0;

      /// Pause playback on the device.
      /// @note Called from both the SFX update thread and the main thread.
      virtual void _pause() = 0;

      /// Set the playback cursor on the device.
      /// Only used for non-streaming voices.
      virtual void _seek( U32 sample ) = 0;

      /// Get the playback cursor on the device.
      virtual U32 _tell() const = 0;

      ///
      virtual void _onBufferStatusChange( SFXBuffer* buffer, SFXBuffer::EStatus newStatus );

      ///
      void _attachToBuffer();

   public:

      static Signal< void( SFXVoice* ) > smVoiceDestroyedSignal;

      /// The destructor.
      virtual ~SFXVoice();

      ///
      const SFXFormat& getFormat() const { return mBuffer->getFormat(); }

      /// Return the current playback position (in number of samples).
      ///
      /// @note This value this method returns equals the total number of
      ///   samples played so far.  For looping streams, this will exceed
      ///   the source stream's duration after the first loop.
      virtual U32 getPosition() const;

      /// Sets the playback position to the given sample count.
      virtual void setPosition( U32 sample );

      /// @return the current playback status.
      virtual SFXStatus getStatus() const;

      /// Starts playback from the current position.
      virtual void play( bool looping );

      /// Stops playback and moves the position to the start.
      virtual void stop();

      /// Pauses playback.
      virtual void pause();

      /// Sets the position and orientation for a 3d voice.
      virtual void setTransform( const MatrixF &transform ) = 0;

      /// Sets the velocity for a 3d voice.
      virtual void setVelocity( const VectorF &velocity ) = 0;

      /// Sets the minimum and maximum distances for 3d falloff.
      virtual void setMinMaxDistance( F32 min, F32 max ) = 0;

      /// Sets the volume.
      virtual void setVolume( F32 volume ) = 0;

      /// Sets the pitch scale.
      virtual void setPitch( F32 pitch ) = 0;

      /// Set sound cone of a 3D sound.
      ///
      /// @param innerAngle Inner cone angle in degrees.
      /// @param outerAngle Outer cone angle in degrees.
      /// @param outerVolume Outer volume factor.
      virtual void setCone(   F32 innerAngle, 
                              F32 outerAngle,
                              F32 outerVolume ) = 0;
};

#endif // _SFXVOICE_H_
