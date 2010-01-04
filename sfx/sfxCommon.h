//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXCOMMON_H_
#define _SFXCOMMON_H_

#ifndef _PLATFORM_H_
   #include "platform/platform.h"
#endif



//-----------------------------------------------------------------------------
//    SFXStatus.
//-----------------------------------------------------------------------------


/// The sound playback state.
enum SFXStatus 
{
   SFXStatusNull,                   ///< Initial state; no operation yet performed on sound.
   SFXStatusPlaying,                ///< Sound is playing.
   SFXStatusStopped,                ///< Sound has been stopped.
   SFXStatusPaused,                 ///< Sound is paused.
   SFXStatusBlocked,                ///< Sound stream is starved and playback blocked.
};


inline const char* SFXStatusToString( SFXStatus status )
{
   switch ( status )
   {
      case SFXStatusPlaying:     return "playing";
      case SFXStatusStopped:     return "stopped";
      case SFXStatusPaused:      return "paused";
      case SFXStatusBlocked:     return "blocked";
      
      case SFXStatusNull:
      default: ;
   }
   
   return "null";
}


//-----------------------------------------------------------------------------
//    SFXDistanceModel.
//-----------------------------------------------------------------------------


/// Rolloff curve used for distance volume attenuation of 3D sounds.
enum SFXDistanceModel
{
   SFXDistanceModelLinear,             ///< Volume decreases linearly from min to max where it reaches zero.
   SFXDistanceModelLogarithmic,        ///< Volume halves every min distance steps starting from min distance; attenuation stops at max distance.
};


/// Compute the distance attenuation based on the given distance model.
///
/// @param minDistance Reference distance; attenuation starts here.
/// @param maxDistance
/// @param distance Actual distance of sound from listener.
/// @param volume Unattenuated volume.
/// @param rolloffFactor Rolloff curve scale factor.
///
/// @return The attenuated volume.
inline F32 SFXDistanceAttenuation( SFXDistanceModel model, F32 minDistance, F32 maxDistance, F32 distance, F32 volume, F32 rolloffFactor )
{
   F32 gain = 1.0f;
      
   switch( model )
   {
      case SFXDistanceModelLinear:
      
         distance = getMax( distance, minDistance );
         distance = getMin( distance, maxDistance );
         
         gain = ( 1 - ( distance - minDistance ) / ( maxDistance - minDistance ) );
         break;
                  
      case SFXDistanceModelLogarithmic:
      
         distance = getMax( distance, minDistance );
         distance = getMin( distance, maxDistance );
         
         gain = minDistance / ( minDistance + rolloffFactor * ( distance - minDistance ) );
         break;
         
   }
   
   return ( volume * gain );
}


//-----------------------------------------------------------------------------
//    SFXFormat.
//-----------------------------------------------------------------------------


/// This class defines the various types of sound data that may be
/// used in the sound system.
///
/// Unlike with most sound APIs, we consider each sample point to comprise
/// all channels in a sound stream rather than only one value for a single
/// channel.
class SFXFormat
{
   protected:

      /// The number of sound channels in the data.
      U8 mChannels;

      /// The number of bits per sound sample.
      U8 mBitsPerSample;

      /// The frequency in samples per second.
      U32 mSamplesPerSecond;

   public:

      SFXFormat(  U8 channels = 0,                  
                  U8 bitsPerSample = 0,
                  U32 samplesPerSecond = 0 )
         :  mChannels( channels ),
            mSamplesPerSecond( samplesPerSecond ), 
            mBitsPerSample( bitsPerSample )
      {}

      /// Copy constructor.
      SFXFormat( const SFXFormat &format )
         :  mChannels( format.mChannels ),
            mBitsPerSample( format.mBitsPerSample ),
            mSamplesPerSecond( format.mSamplesPerSecond )
      {}

   public:

      /// Sets the format.
      void set(   U8 channels,                  
                  U8 bitsPerSample,
                  U32 samplesPerSecond )
      {
         mChannels = channels;
         mBitsPerSample = bitsPerSample;
         mSamplesPerSecond = samplesPerSecond;
      }

      /// Comparision between formats.
      bool operator == ( const SFXFormat& format ) const 
      { 
         return   mChannels == format.mChannels && 
                  mBitsPerSample == format.mBitsPerSample &&
                  mSamplesPerSecond == format.mSamplesPerSecond;
      }

      /// Returns the number of sound channels.
      U8 getChannels() const { return mChannels; }

      /// Returns true if there is a single sound channel.
      bool isMono() const { return mChannels == 1; }

      /// Is true if there are two sound channels.
      bool isStereo() const { return mChannels == 2; }

      /// Is true if there are more than two sound channels.
      bool isMultiChannel() const { return mChannels > 2; }

      /// 
      U32 getSamplesPerSecond() const { return mSamplesPerSecond; }

      /// The bits of data per channel.
      U8 getBitsPerChannel() const { return mBitsPerSample / mChannels; }

      /// The number of bytes of data per channel.
      U8 getBytesPerChannel() const { return getBitsPerChannel() / 8; }

      /// The number of bits per sound sample.
      U8 getBitsPerSample() const { return mBitsPerSample; }

      /// The number of bytes of data per sample.
      /// @note Be aware that this comprises all channels.
      U8 getBytesPerSample() const { return mBitsPerSample / 8; }

      /// Returns the duration from the sample count.
      U32 getDuration( U32 samples ) const
      {
         // Use 64bit types to avoid overflow during division. 
         return ( (U64)samples * (U64)1000 ) / (U64)mSamplesPerSecond;
      }

      ///
      U32 getSampleCount( U32 ms ) const
      {
         return U64( mSamplesPerSecond ) * U64( ms ) / U64( 1000 );
      }

      /// Returns the data length in bytes.
      U32 getDataLength( U32 ms ) const
      {
         U32 bytes = ( ( (U64)ms * (U64)mSamplesPerSecond ) * (U64)getBytesPerSample() ) / (U64)1000;
         return bytes;
      }
};

#endif // _SFXCOMMON_H_
