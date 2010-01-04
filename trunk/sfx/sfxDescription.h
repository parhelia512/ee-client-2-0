//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXDESCRIPTION_H_
#define _SFXDESCRIPTION_H_


#ifndef _CONSOLETYPES_H_
   #include "console/consoleTypes.h"
#endif
#ifndef _SIMDATABLOCK_H_
   #include "console/simDataBlock.h"
#endif
#ifndef _MPOINT3_H_
   #include "math/mPoint3.h"
#endif


/// The SFXDescription defines how a sound should be played.
///
/// If mConeInsideAngle and mConeOutsideAngle are not both
/// 360 then the sound will be directional and facing out
/// the Y axis.
///
/// A few tips:
///
/// Make sure that server SFXDescription are defined with the 
/// datablock keyword, and that client SFXDescription are defined
/// with the 'new' keyword.
///
class SFXDescription : public SimDataBlock
{
      typedef SimDataBlock Parent;

   public:

      /// The 0 to 1 volume scale.
      F32 mVolume;
      
      /// The pitch scale.
      F32 mPitch;

      /// If true the sound will loop.
      bool mIsLooping;

      /// If true the sound data will be streamed from
      /// disk and not loaded completely into memory.
      bool mIsStreaming;

      /// If true the sound will be 3D positional.
      bool mIs3D;

      /// The distance from the emitter at which the
      /// sound volume is unchanged.  Beyond this distance
      /// the volume begins to falloff.
      ///
      /// This is only valid for 3D sounds.
      F32 mReferenceDistance;

      /// The distance from the emitter at which the
      /// sound volume becomes zero.
      ///
      /// This is only valid for 3D sounds.
      F32 mMaxDistance;

      /// The angle in degrees of the inner part of
      /// the cone.  It must be within 0 to 360.
      ///
      /// This is only valid for 3D sounds.
      U32 mConeInsideAngle;

      /// The angle in degrees of the outer part of
      /// the cone.  It must be greater than mConeInsideAngle
      /// and less than to 360.
      ///
      /// This is only valid for 3D sounds.
      U32 mConeOutsideAngle;

      /// The volume scalar for on/beyond the outside angle.
      ///
      /// This is only valid for 3D sounds.
      F32 mConeOutsideVolume;

      /// The sound channel for this sound.
      /// @see SFXSystem::getChannelVolume, SFXSystem::setChannelVolume
      U32 mChannel;
      
      /// Number of seconds until playback reaches full volume after starting/resuming.
      /// Zero to deactivate (default).
      F32 mFadeInTime;
      
      /// Number of seconds to fade out fading before stopping/pausing.
      /// Zero to deactivate (default).
      F32 mFadeOutTime;

      /// The number of seconds of sound data to read per streaming
      /// packet.  Only relevant if "isStreaming" is true.
      U32 mStreamPacketSize;

      /// The number of streaming packets to read and buffer in advance.
      /// Only relevant if "isStreaming" is true.
      U32 mStreamReadAhead;

      SFXDescription();
      SFXDescription( const SFXDescription& desc );
      DECLARE_CONOBJECT( SFXDescription );
      static void initPersistFields();

      // SimObject
      virtual bool onAdd();
      virtual void packData( BitStream* stream );
      virtual void unpackData( BitStream* stream );

      /// Validates the description fixing any
      /// parameters that are out of range.
      void validate();
};

DECLARE_CONSOLETYPE( SFXDescription )


#endif // _SFXDESCRIPTION_H_