//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/sfxSource.h"
#include "sfx/sfxDevice.h"
#include "sfx/sfxVoice.h"
#include "sfx/sfxSystem.h"
#include "sfx/sfxBuffer.h"
#include "sfx/sfxEffect.h"
#include "sfx/sfxStream.h"
#include "core/util/safeDelete.h"


//#define DEBUG_SPEW


SFXSource::SFXSource()
   : mVoice( NULL )
{
   // NOTE: This should never be used directly 
   // and is only here to satisfy satisfy the
   // construction needs of IMPLEMENT_CONOBJECT.
}

SFXSource::SFXSource( SFXProfile *profile, SFXDescription* desc )
   :  mStatus( SFXStatusStopped ),
      mPitch( 1 ),
      mVelocity( 0, 0, 0 ),
      mTransform( true ),
      mAttenuatedVolume( 0 ),
      mDistToListener( 0 ),
      mModulativeVolume( 1 ),
      mVoice( NULL ),
      mProfile( profile ),
      mStatusCallback( NULL ),
      mPlayStartTick( 0 )
{
   mIs3D = desc->mIs3D;
   mIsLooping = desc->mIsLooping;
   mIsStreaming = desc->mIsStreaming;
   mFadeInTime = desc->mFadeInTime;
   mFadeOutTime = desc->mFadeOutTime;
   mPitch = desc->mPitch;

   setVolume( desc->mVolume ); 

   setMinMaxDistance( desc->mReferenceDistance, desc->mMaxDistance );

   setCone( F32( desc->mConeInsideAngle ),
            F32( desc->mConeOutsideAngle ),
            desc->mConeOutsideVolume );

   mChannel = desc->mChannel;

   // Allow namespace linkage.
   mNSLinkMask = LinkSuperClassName | LinkClassName;
}

SFXSource* SFXSource::_create( SFXDevice *device, SFXProfile *profile )
{
   AssertFatal( profile, "SFXSource::_create() - Got a null profile!" );

   SFXDescription* desc = profile->getDescription();
   if ( !desc )
   {
      Con::errorf( "SFXSource::_create() - Profile has null description!" );
      return NULL;
   }

   // Create the source and init the buffer.
   SFXSource* source = new SFXSource( profile, desc );
   SFXBuffer* buffer = profile->getBuffer();
   if( !buffer )
   {
      delete source;
      Con::errorf( "SFXSource::_create() - Could not create device buffer!" );
      return NULL;
   }
   source->_setBuffer( buffer );
   
   // The source is a console object... register it.
   source->registerObject();

   // All sources are added to 
   // the global source set.
   Sim::getSFXSourceSet()->addObject( source );
   
   #ifdef DEBUG_SPEW
   Platform::outputDebugString( "[SFXSource] new source '%i' for profile '%i'", source->getId(), profile->getId() );
   #endif
   
   // Hook up reloading.
   
   profile->getChangedSignal().notify( source, &SFXSource::_onProfileChanged );

   return source;
}

SFXSource* SFXSource::_create(   SFXDevice* device, 
                                 const ThreadSafeRef< SFXStream >& stream,
                                 SFXDescription* description )
{
   AssertFatal( stream.ptr() != NULL, "SFXSource::_create() - Got a null stream!" );
   AssertFatal( description, "SFXSource::_create() - Got a null description!" );

   // Create the source and init the buffer.
   SFXSource* source = new SFXSource( NULL, description );
   SFXBuffer* buffer = SFX->_createBuffer( stream, description );
   if( !buffer )
   {
      delete source;
      Con::errorf( "SFXSource::_create() - Could not create device buffer!" );
      return NULL;
   }
   source->_setBuffer( buffer );

   // We're all good... register this sucker.
   source->registerObject();

   // All sources are added to the global source set.
   Sim::getSFXSourceSet()->addObject( source );
      
   #ifdef DEBUG_SPEW
   Platform::outputDebugString( "[SFXSource] new source '%i' for stream", source->getId() );
   #endif

   return source;
}

SFXSource::~SFXSource()
{
   // Delete all remaining effects on this source.
   
   for( EffectList::Iterator iter = mEffects.begin();
        iter != mEffects.end(); ++ iter )
      delete *iter;
}

IMPLEMENT_CONOBJECT(SFXSource);

void SFXSource::initPersistFields()
{
   addField( "statusCallback", TypeString, Offset(mStatusCallback, SFXSource) );

   // We don't want any fields that manipluate the playback of the
   // source as they complicate updates.  If we could easily set dirty
   // bits on changes to fields then it would be worth it,
   // so for now just use the console methods.
   
   Parent::initPersistFields();
}

bool SFXSource::processArguments( S32 argc, const char **argv )
{
   // We don't want to allow a source to be constructed
   // via the script... force use of the SFXSystem.
   Con::errorf( ConsoleLogEntry::Script, "Use sfxCreateSource, sfxPlay, or sfxPlayOnce!" );
   return false;
}

template< class T >
void SFXSource::_clearEffects()
{
   for( EffectList::Iterator iter = mEffects.begin();
        iter != mEffects.end(); )
   {
      EffectList::Iterator next = iter;
      next ++;

      if( dynamic_cast< T* >( *iter ) )
      {
         delete *iter;
         mEffects.erase( iter );
      }

      iter = next;
   }
}

void SFXSource::_reloadBuffer()
{
   if( mProfile != NULL && _releaseVoice() )
   {
      SFXBuffer* buffer = mProfile->getBuffer();
      if( !buffer )
      {
         Con::errorf( "SFXSource::_reloadBuffer() - Could not create device buffer!" );
         return;
      }
      
      _setBuffer( buffer );
      
      if( getLastStatus() == SFXStatusPlaying )
         SFX->_assignVoices();
   }
}

void SFXSource::_setBuffer( SFXBuffer* buffer )
{
   mBuffer = buffer;

   // There is no telling when the device will be 
   // destroyed and the buffers deleted.
   //
   // By caching the duration now we can allow sources
   // to continue virtual playback until the device
   // is restored.
   mDuration = mBuffer->getDuration();
}

bool SFXSource::_allocVoice( SFXDevice* device )
{
   // We shouldn't have any existing voice!
   AssertFatal( !mVoice, "SFXSource::_allocVoice() - Already had a voice!" );

   // Must not assign voice to source that isn't playing.
   AssertFatal( getLastStatus() == SFXStatusPlaying,
      "SFXSource::_allocVoice() - Source is not playing!" );

   // The buffer can be lost when the device is reset 
   // or changed, so initialize it if we have to.  If
   // that fails then we cannot create the voice.
   
   if( mBuffer.isNull() )
   {
      if( mProfile != NULL )
         _setBuffer( mProfile->getBuffer() );

      if( mBuffer.isNull() )
         return false;
   }

   // Ask the device for a voice based on this buffer.
   mVoice = device->createVoice( mIs3D, mBuffer );
   if( !mVoice )
      return false;

   setVolume( mVolume );
   if( mPitch != 1.0f )
      setPitch( mPitch );
   
   if( mIs3D )
   {
      setTransform( mTransform );
      setVelocity( mVelocity );
      setMinMaxDistance( mMinDistance, mMaxDistance );
      setCone( mConeInsideAngle, mConeOutsideAngle, mConeOutsideVolume );
   }

   // Update the duration... it shouldn't have changed, but
   // its probably better that we're accurate if it did.
   mDuration = mBuffer->getDuration();

   // If virtualized playback has been started, we transfer its position to the
   // voice and stop virtualization.

   if( mVirtualPlayTimer.isStarted() )
   {
      const U32 playTime = mVirtualPlayTimer.getPosition();
      const U32 pos = mBuffer->getFormat().getSampleCount( playTime );
      mVoice->setPosition( pos);
      mVirtualPlayTimer.stop();
   }

   mVoice->play( mIsLooping );
   
   #ifdef DEBUG_SPEW
   Platform::outputDebugString( "[SFXSource] allocated voice for source '%i'", getId() );
   #endif
   
   return true;
}

void SFXSource::onRemove()
{
   stop();

   // Remove it from the set.
   Sim::getSFXSourceSet()->removeObject( this );

   // Let the system know.
   SFX->_onRemoveSource( this );
   
   #ifdef DEBUG_SPEW
   Platform::outputDebugString( "[SFXSource] removed source '%i'", getId() );
   #endif
   
   if( mProfile != NULL )
      mProfile->getChangedSignal().remove( this, &SFXSource::_onProfileChanged );

   Parent::onRemove();
}

bool SFXSource::_releaseVoice()
{
   if( !mVoice )
      return true;
      
   // Refuse to release a voice for a streaming buffer that
   // is not coming from a profile.  For streaming buffer, we will
   // have to release the buffer, too, and without a profile we don't
   // know how to recreate the stream.
   
   if( isStreaming() && !mProfile )
      return false;
      
   // If we're currently playing, transfer our playback position
   // to the playtimer so we can virtualize playback while not
   // having a voice.

   SFXStatus status = getLastStatus();
   if( status == SFXStatusPlaying || status == SFXStatusBlocked )
   {
      mVirtualPlayTimer.setPosition( mVoice->getPosition() );
      mVirtualPlayTimer.start();

      if( status == SFXStatusBlocked )
         status = SFXStatusPlaying;
   }

   mVoice = NULL;
   
   // If this is a streaming source, release our buffer, too.
   // Otherwise the voice will stick around as it is uniquely assigned to
   // the buffer.  When we get reassigned a voice, we will have to do
   // a full stream seek anyway, so it's no real loss here.
   
   if( isStreaming() )
      mBuffer = NULL;
   
   #ifdef DEBUG_SPEW
   Platform::outputDebugString( "[SFXSource] release voice for source '%i' (status: %s)",
      getId(), SFXStatusToString( status ) );
   #endif
   
   return true;
}

void SFXSource::_updateVolume( const MatrixF& listener )
{
   const F32 volume = mVolume * mModulativeVolume;

   if ( !mIs3D ) 
   {
      mDistToListener = 0.0f;
      mAttenuatedVolume = volume; 
      return;
	}

   Point3F pos, lpos;
   mTransform.getColumn( 3, &pos );
   listener.getColumn( 3, &lpos );

   mDistToListener = ( pos - lpos ).len();
   mAttenuatedVolume = SFXDistanceAttenuation(
      SFX->getDistanceModel(),
      mMinDistance,
      mMaxDistance,
      mDistToListener,
      volume,
      SFX->getRolloffFactor() );
}

bool SFXSource::_setStatus( SFXStatus status )
{
   if ( mStatus == status )
      return false;

   mStatus = status;

   // Do the callback if we have it.

   const char* statusString = SFXStatusToString( mStatus );
   if ( mStatusCallback && mStatusCallback[0] )
      Con::executef( mStatusCallback, getIdString(), statusString );
   else if ( getNamespace() )
      Con::executef( this, "onStatusChange", statusString );

   return true;
}

void SFXSource::play( F32 fadeInTime )
{
   // Update our status once.
   _updateStatus();

   if( mStatus == SFXStatusPlaying )
      return;

   if( mStatus != SFXStatusPaused )
      mPlayStartTick = Platform::getVirtualMilliseconds();
         
   // Add fade-out, if requested.
   
   U32 fadeOutStartsAt = getDuration();
   if( mFadeOutTime )
   {
      fadeOutStartsAt = getMax( getPosition(), getDuration() - U32( mFadeOutTime * 1000 ) );
      mEffects.pushBack( new SFXFadeEffect( this,
                                            getMin( mFadeOutTime,
                                                    F32( getDuration() - getPosition() ) / 1000.f ),
                                            0.0f,
                                            fadeOutStartsAt,
                                            SFXFadeEffect::ON_END_Stop,
                                            true ) );
   }
   
   // Add fade-in, if requested.
   
   if( fadeInTime != 0.0f && ( fadeInTime > 0.0f || mFadeInTime > 0.0f ) )
   {
      // Don't fade from full 0.0f to avoid virtualization on this source.
      
      if( fadeInTime == -1.0f )
         fadeInTime = mFadeInTime;
         
      fadeInTime = getMin( fadeInTime, F32( fadeOutStartsAt ) * 1000.f );
      
      mEffects.pushFront( new SFXFadeEffect( this, 
                                             fadeInTime,
                                             mVolume,
                                             getPosition(),
                                             SFXFadeEffect::ON_END_Nop,
                                             true ) );
      setVolume( 0.01f );
   }

   // Start playback.

   _setStatus( SFXStatusPlaying );
   if( mVoice )
   {
      #ifdef DEBUG_SPEW
      Platform::outputDebugString( "[SFXSource] playing source '%i'", getId() );
      #endif
      
      mVoice->play( mIsLooping );
   }
   else
   {
      // To ensure the fastest possible reaction 
      // to this playback let the system reassign
      // voices immediately.
      SFX->_assignVoices();

      // If we did not get assigned a voice, start the
      // playback timer for virtualized playback.

      if( !mVoice )
      {
         #ifdef DEBUG_SPEW
         Platform::outputDebugString( "[SFXSource] virtualizing playback of source '%i'", getId() );
         #endif
         
         mVirtualPlayTimer.start();
      }
   }
}

void SFXSource::stop( F32 fadeOutTime )
{
   _updateStatus();
   
   if( mStatus != SFXStatusPlaying
       && mStatus != SFXStatusPaused )
      return;
      
   if( fadeOutTime != 0.0f && ( fadeOutTime > 0.0f || mFadeOutTime > 0.0f ) )
   {
      // Do a fade-out and then stop.
      
      _clearEffects< SFXFadeEffect >();
      
      if( fadeOutTime == -1.0f )
         fadeOutTime = mFadeOutTime;
         
      mEffects.pushFront( new SFXFadeEffect( this,
                                             getMin( fadeOutTime,
                                                     F32( getDuration() - getPosition() ) / 1000.f ),
                                             0.0f,
                                             getPosition(),
                                             SFXFadeEffect::ON_END_Stop,
                                             true ) );
   }
   else
   {
      // Stop immediately.
      
      _setStatus( SFXStatusStopped );
   
      if ( mVoice )
         mVoice->stop();
      else
         mVirtualPlayTimer.stop();
         
      #ifdef DEBUG_SPEW
      Platform::outputDebugString( "[SFXSource] stopped playback of source '%i'", getId() );
      #endif
   }      
}

void SFXSource::pause( F32 fadeOutTime )
{
   _updateStatus();
   
   if( mStatus != SFXStatusPlaying )
      return;
      
   if( fadeOutTime != 0.0f && ( fadeOutTime > 0.0f || mFadeOutTime > 0.0f ) )
   {
      // Do a fade-out and then pause.
      
      _clearEffects< SFXFadeEffect >();
      if( fadeOutTime == -1.0f )
         fadeOutTime = mFadeOutTime;

      mEffects.pushFront( new SFXFadeEffect( this,
                                             getMin( fadeOutTime,
                                                     F32( getDuration() - getPosition() ) / 1000.f ),
                                             0.0f,
                                             getPosition(),
                                             SFXFadeEffect::ON_END_Pause,
                                             true ) );
   }
   else
   {
      // Pause immediately.
      
      _setStatus( SFXStatusPaused );

      if ( mVoice )
         mVoice->pause();
      else
         mVirtualPlayTimer.pause();

      #ifdef DEBUG_SPEW
      Platform::outputDebugString( "[SFXSource] paused playback of source '%i'", getId() );
      #endif
   }
}

void SFXSource::_update()
{
   // Update our effects, if any.
   
   for( EffectList::Iterator iter = mEffects.begin();
        iter != mEffects.end(); )
   {
      EffectList::Iterator next = iter;
      next ++;

      if( !( *iter )->update() )
      {
         delete *iter;
         mEffects.erase( iter );
      }

      iter = next;
   }
}

SFXStatus SFXSource::_updateStatus()
{
   // If we have a voice, use its status.
   
   if( mVoice )
   {
      SFXStatus voiceStatus = mVoice->getStatus();
      
      // Filter out SFXStatusBlocked.
      
      if( voiceStatus == SFXStatusBlocked )
         _setStatus( SFXStatusPlaying );
      else
         _setStatus( voiceStatus );
         
      return mStatus;
   }

   // If we're not in a playing state or we're a looping
   // sound then we don't need to calculate the status.
   
   if( mIsLooping || mStatus != SFXStatusPlaying )
      return mStatus;

   // If we're playing and don't have a voice we
   // need to decide if the sound is done playing
   // to ensure proper virtualization of the sound.

   if( mVirtualPlayTimer.getPosition() > mDuration )
      _setStatus( SFXStatusStopped );

   return mStatus;
}

U32 SFXSource::getPosition() const
{
   if( mVoice )
      return mVoice->getFormat().getDuration( mVoice->getPosition() );
   else
      return mVirtualPlayTimer.getPosition();
}

void SFXSource::setPosition( U32 ms )
{
   AssertFatal( ms < getDuration(), "SFXSource::setPosition() - position out of range" );
   if( mVoice )
      mVoice->setPosition( mVoice->getFormat().getSampleCount( ms ) );
   else
      mVirtualPlayTimer.setPosition( ms );
}

void SFXSource::setVelocity( const VectorF& velocity )
{
   mVelocity = velocity;

   if ( mVoice && mIs3D )
      mVoice->setVelocity( velocity );      
}

void SFXSource::setTransform( const MatrixF& transform )
{
   mTransform = transform;

   if ( mVoice && mIs3D )
      mVoice->setTransform( mTransform );      
}

void SFXSource::setVolume( F32 volume )
{
   mVolume = mClampF( volume, 0, 1 );

   if ( mVoice )
      mVoice->setVolume( mVolume * mModulativeVolume );      
}

void SFXSource::_setModulativeVolume( F32 volume )
{
   mModulativeVolume = volume;
   setVolume( mVolume );
}

void SFXSource::setPitch( F32 pitch )
{
   AssertFatal( pitch > 0.0f, "Got bad pitch!" );
   mPitch = pitch;

   if ( mVoice )
      mVoice->setPitch( mPitch );
}

void SFXSource::setMinMaxDistance( F32 min, F32 max )
{
   min = getMax( 0.0f, min );
   max = getMax( 0.0f, max );

   mMinDistance = min;
   mMaxDistance = max;

   if ( mVoice && mIs3D )
      mVoice->setMinMaxDistance( mMinDistance, mMaxDistance );
}

void SFXSource::setCone(   F32 innerAngle,
                           F32 outerAngle,
                           F32 outerVolume )
{
   mConeInsideAngle     = mClampF( innerAngle, 0.0f, 360.0 );
   mConeOutsideAngle    = mClampF( outerAngle, mConeInsideAngle, 360.0 );
   mConeOutsideVolume   = mClampF( outerVolume, 0.0f, 1.0f );

   if ( mVoice && mIs3D )
      mVoice->setCone(  mConeInsideAngle,
                        mConeOutsideAngle,
                        mConeOutsideVolume );
}

bool SFXSource::isReady() const
{
   return ( mBuffer != NULL && mBuffer->isReady() );
}

void SFXSource::addMarker( const String& name, U32 pos )
{
   mEffects.pushBack( new SFXMarkerEffect( this, name, pos ) );
}

SFXProfile* SFXSource::getProfile() const
{
   return mProfile;
}

//-----------------------------------------------------------------------------

ConsoleMethod( SFXSource, addMarker, void, 4, 4, "( string name, float pos ) - Add a notification marker called 'name' at 'pos' seconds of playback." )
{
   String name = argv[ 2 ];
   U32 pos = U32( dAtof( argv[ 3 ] ) * 1000.f );
   object->addMarker( name, pos );
}

ConsoleMethod( SFXSource, play, void, 2, 3,  "( [float fadeIn] ) - Starts playback of the source." )
{
   F32 fadeInTime = -1.0f;
   if( argc > 2 )
      fadeInTime = dAtof( argv[ 2 ] );
      
   object->play( fadeInTime );
}

ConsoleMethod( SFXSource, stop, void, 2, 3,  "( [float fadeOut] ) - Ends playback of the source." )
{
   F32 fadeOutTime = -1.0f;
   if( argc > 2 )
      fadeOutTime = dAtof( argv[ 2 ] );
      
   object->stop( fadeOutTime );
}

ConsoleMethod( SFXSource, pause, void, 2, 3,  "( [float fadeOut] ) - Pauses playback of the source." )
{
   F32 fadeOutTime = -1.0f;
   if( argc > 2 )
      fadeOutTime = dAtof( argv[ 2 ] );

   object->pause( fadeOutTime );
}

ConsoleMethod( SFXSource, isReady, bool, 2, 2, "() - Returns true if the sound data associated with the source has been loaded." )
{
   return object->isReady();
}

ConsoleMethod( SFXSource, isPlaying, bool, 2, 2,  "() - Returns true if the source is playing or false if not." )
{
   return object->isPlaying();
}

ConsoleMethod( SFXSource, isPaused, bool, 2, 2,  "() - Returns true if the source is paused or false if not." )
{
   return object->isPaused();
}

ConsoleMethod( SFXSource, isStopped, bool, 2, 2, "() - Returns true if the source is stopped or false if not." )
{
   return object->isStopped();
}

ConsoleMethod( SFXSource, setVolume, void, 3, 3,  "( float volume ) - Sets the playback volume of the source." )
{
   object->setVolume( dAtof( argv[2] ) );
}

ConsoleMethod( SFXSource, setPitch, void, 3, 3,  "( float pitch ) - Scales the pitch of the source." )
{
   object->setPitch( dAtof( argv[2] ) );
}

ConsoleMethod( SFXSource, getChannel, S32, 2, 2,   "() - Returns the volume channel." )
{
   return object->getChannel();
}

ConsoleMethod( SFXSource, setTransform, void, 3, 4,  "( vector pos [, vector direction ] ) - Set the position and orientation of a 3D SFXSource." )
{
   MatrixF mat = object->getTransform();

   Point3F pos;
   dSscanf( argv[2], "%g %g %g", &pos.x, &pos.y, &pos.z );
   mat.setPosition( pos );
   
   if( argc > 3 )
   {
      Point3F dir;
      dSscanf( argv[ 3 ], "%g %g %g", &dir.x, &dir.y, &dir.z );
      mat.setColumn( 1, dir );
   }
   
   object->setTransform( mat );
}

ConsoleMethod( SFXSource, getStatus, const char*, 2, 2, "() - Returns the playback status of the source." )
{
   return SFXStatusToString( object->getStatus() );
}

ConsoleMethod( SFXSource, getPosition, F32, 2, 2, "() - Returns the current playback position in seconds." )
{
   return F32( object->getPosition() ) * 0.001f;
}

ConsoleMethod( SFXSource, setPosition, void, 3, 3, "( float ) - Set the current playback position in seconds." )
{
   S32 pos = dAtoi( argv[ 2 ] );
   if( pos >= 0 && pos <= object->getDuration() )
      object->setPosition( U32( dAtof( argv[ 2 ] ) ) * 1000.0f );
}

ConsoleMethod( SFXSource, setCone, void, 5, 5, "( float innerAngle, float outerAngle, float outsideVolume ) - Set the 3D volume cone for the sound." )
{
   F32 innerAngle = dAtof( argv[ 2 ] );
   F32 outerAngle = dAtof( argv[ 3 ] );
   F32 outsideVolume = dAtof( argv[ 4 ] );
   
   bool isValid = true;
   
   if( innerAngle < 0.0 || innerAngle > 360.0 )
   {
      Con::errorf( "SFXSource.setCone() - 'innerAngle' must be between 0 and 360" );
      isValid = false;
   }
   if( outerAngle < 0.0 || outerAngle > 360.0 )
   {
      Con::errorf( "SFXSource.setCone() - 'outerAngle' must be between 0 and 360" );
      isValid = false;
   }
   if( outsideVolume < 0.0 || outsideVolume > 1.0 )
   {
      Con::errorf( "SFXSource.setCone() - 'outsideVolume' must be between 0 and 1" );
      isValid = false;
   }
   
   if( !isValid )
      return;
      
   object->setCone( innerAngle, outerAngle, outsideVolume );
}

ConsoleMethod( SFXSource, getDuration, F32, 2, 2, "() - Get the total playback time in seconds." )
{
   return F32( object->getDuration() ) * 0.001f;
}
