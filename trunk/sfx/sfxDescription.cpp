//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "sfx/sfxDescription.h"
#include "sfx/sfxSystem.h"
#include "sim/netConnection.h"
#include "core/stream/bitStream.h"
#include "sfx/sfxInternal.h"


IMPLEMENT_CO_DATABLOCK_V1( SFXDescription );


SFXDescription::SFXDescription()
   :  SimDataBlock(),
      mVolume( 1 ),
      mPitch( 1 ),
      mIsLooping( false ),
      mIsStreaming( false ),
      mIs3D( false ),
      mReferenceDistance( 1 ),
      mMaxDistance( 100 ),
      mConeInsideAngle( 360 ),
      mConeOutsideAngle( 360 ),
      mConeOutsideVolume( 1 ),
      mChannel( 0 ),
      mFadeInTime( 0.0f ),
      mFadeOutTime( 0.0f ),
      mStreamPacketSize( SFXInternal::SFXAsyncStream::DEFAULT_STREAM_PACKET_LENGTH ),
      mStreamReadAhead( SFXInternal::SFXAsyncStream::DEFAULT_STREAM_LOOKAHEAD )
{
}

SFXDescription::SFXDescription( const SFXDescription& desc )
   :  SimDataBlock(),
      mVolume( desc.mVolume ),
      mPitch( desc.mPitch ),
      mIsLooping( desc.mIsLooping ),
      mIsStreaming( desc.mIsStreaming ),
      mIs3D( desc.mIs3D ),
      mReferenceDistance( desc.mReferenceDistance ),
      mMaxDistance( desc.mMaxDistance ),
      mConeInsideAngle( desc.mConeInsideAngle ),
      mConeOutsideAngle( desc.mConeOutsideAngle ),
      mConeOutsideVolume( desc.mConeOutsideVolume ),
      mChannel( desc.mChannel ),
      mFadeInTime( desc.mFadeInTime ),
      mFadeOutTime( desc.mFadeOutTime ),
      mStreamPacketSize( desc.mStreamPacketSize ),
      mStreamReadAhead( desc.mStreamReadAhead )
{
}

IMPLEMENT_CONSOLETYPE( SFXDescription )
IMPLEMENT_GETDATATYPE( SFXDescription )
IMPLEMENT_SETDATATYPE( SFXDescription )


void SFXDescription::initPersistFields()
{
   addField( "volume",            TypeF32,     Offset(mVolume, SFXDescription));
   addField( "pitch",             TypeF32,     Offset(mPitch, SFXDescription));
   addField( "isLooping",         TypeBool,    Offset(mIsLooping, SFXDescription));
   addField( "isStreaming",       TypeBool,    Offset(mIsStreaming, SFXDescription));
   addField( "is3D",              TypeBool,    Offset(mIs3D, SFXDescription));
   addField( "referenceDistance", TypeF32,     Offset(mReferenceDistance, SFXDescription));
   addField( "maxDistance",       TypeF32,     Offset(mMaxDistance, SFXDescription));
   addField( "coneInsideAngle",   TypeS32,     Offset(mConeInsideAngle, SFXDescription));
   addField( "coneOutsideAngle",  TypeS32,     Offset(mConeOutsideAngle, SFXDescription));
   addField( "coneOutsideVolume", TypeF32,     Offset(mConeOutsideVolume, SFXDescription));
   addField( "channel",           TypeS32,     Offset(mChannel, SFXDescription));
   addField( "fadeInTime",        TypeF32,     Offset(mFadeInTime, SFXDescription));
   addField( "fadeOutTime",       TypeF32,     Offset(mFadeOutTime, SFXDescription));
   addField( "streamPacketSize",  TypeS32,     Offset(mStreamPacketSize, SFXDescription));
   addField( "streamReadAhead",   TypeS32,     Offset(mStreamReadAhead, SFXDescription));

   Parent::initPersistFields();
}


bool SFXDescription::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   // Validate the data we'll be passing to 
   // the audio layer.
   validate();

   return true;
}

void SFXDescription::validate()
{
   // Validate the data we'll be passing to the audio layer.
   mVolume = mClampF( mVolume, 0, 1 );
   
   if( mPitch <= 0.0f )
      mPitch = 1.0f;
   if( mFadeInTime < 0.0f )
      mFadeInTime = 0.0f;
   if( mFadeOutTime < 0.0f )
      mFadeOutTime = 0.0f;

   mReferenceDistance = mClampF( mReferenceDistance, 0, mReferenceDistance );

   if ( mMaxDistance <= mReferenceDistance )
      mMaxDistance = mReferenceDistance + 0.01f;

   mConeInsideAngle     = mClamp( mConeInsideAngle, 0, 360 );
   mConeOutsideAngle    = mClamp( mConeOutsideAngle, mConeInsideAngle, 360 );
   mConeOutsideVolume   = mClampF( mConeOutsideVolume, 0, 1 );

   mChannel = mClamp( mChannel, 0, SFXSystem::NumChannels - 1 );
}

void SFXDescription::packData( BitStream *stream )
{
   Parent::packData( stream );

   stream->writeFloat( mVolume, 6 );
   stream->writeFloat( mPitch, 6 );

   stream->writeFlag( mIsLooping );

   stream->writeFlag( mIsStreaming );
   stream->writeFlag( mIs3D );

   if ( mIs3D )
   {
      stream->write( mReferenceDistance );
      stream->write( mMaxDistance );

      stream->writeInt( mConeInsideAngle, 9 );
      stream->writeInt( mConeOutsideAngle, 9 );

      stream->writeFloat( mConeOutsideVolume, 6 );
   }

   stream->writeInt( mChannel, SFXSystem::NumChannelBits );
   stream->writeFloat( mFadeInTime, 6 );
   stream->writeFloat( mFadeOutTime, 6 );
   stream->writeInt( mStreamPacketSize, 8 );
   stream->writeInt( mStreamReadAhead, 8 );
}


void SFXDescription::unpackData( BitStream *stream )
{
   Parent::unpackData( stream );

   mVolume    = stream->readFloat( 6 );
   mPitch     = stream->readFloat( 6 );
   mIsLooping = stream->readFlag();

   mIsStreaming   = stream->readFlag();
   mIs3D          = stream->readFlag();

   if ( mIs3D )
   {
      stream->read( &mReferenceDistance );
      stream->read( &mMaxDistance );

      mConeInsideAngle     = stream->readInt( 9 );
      mConeOutsideAngle    = stream->readInt( 9 );

      mConeOutsideVolume   = stream->readFloat( 6 );
   }

   mChannel = stream->readInt( SFXSystem::NumChannelBits );
   mFadeInTime = stream->readFloat( 6 );
   mFadeOutTime = stream->readFloat( 6 );
   mStreamPacketSize = stream->readInt( 8 );
   mStreamReadAhead = stream->readInt( 8 );
}





