//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "sfx/sfxSystem.h"

#include "sfx/sfxProvider.h"
#include "sfx/sfxDevice.h"
#include "sfx/sfxInternal.h"
#include "sfx/sfxSource.h"
#include "sfx/sfxProfile.h"

#include "console/console.h"
#include "console/consoleTypes.h"
#include "sim/processList.h"
#include "platform/profiler.h"

#include "sfx/media/sfxWavStream.h"
#ifdef TORQUE_OGGVORBIS
   #include "sfx/media/sfxVorbisStream.h"
#endif


SFXSystem* SFXSystem::smSingleton = NULL;


//-----------------------------------------------------------------------------

void SFXSystem::init()
{
   AssertWarn( smSingleton == NULL, "SFX has already been initialized!" );

   SFXProvider::initializeAllProviders();

   // Register the streams and resources.  Note that 
   // the order here does matter!
   SFXFileStream::registerExtension( ".wav", ( SFXFILESTREAM_CREATE_FN ) SFXWavStream::create );
   #ifdef TORQUE_OGGVORBIS
      SFXFileStream::registerExtension( ".ogg", ( SFXFILESTREAM_CREATE_FN ) SFXVorbisStream::create );
   #endif

   // Note: If you have provider specific file types you should
   // register them in the provider initialization.

   // Create the system.
   smSingleton = new SFXSystem();
}


void SFXSystem::destroy()
{
   AssertWarn( smSingleton != NULL, "SFX has not been initialized!" );

   SFXFileStream::unregisterExtension( ".wav" );
   #ifdef TORQUE_OGGVORBIS
      SFXFileStream::unregisterExtension( ".ogg" );
   #endif

   delete smSingleton;
   smSingleton = NULL;

   SFXInternal::THREAD_POOL().shutdown();
}


SFXSystem::SFXSystem()
   :  mDevice( NULL ),
      mLastTime( 0 ),
      mMasterVolume( 1 ),
      mStatNumSources( 0 ),
      mStatNumPlaying( 0 ),
      mStatNumCulled( 0 ),
      mStatNumVoices( 0 ),
      mDistanceModel( SFXDistanceModelLinear ),
      mDopplerFactor( 0.5 ),
      mRolloffFactor( 1.0 )
{
   VECTOR_SET_ASSOCIATION( mSources );
   VECTOR_SET_ASSOCIATION( mPlayOnceSources );

   // Setup the default channel volumes.
   for ( S32 i=0; i < NumChannels; i++ )
      mChannelVolume[i] = 1.0f;

   Con::addVariable( "SFX::numSources", TypeS32, &mStatNumSources );
   Con::addVariable( "SFX::numPlaying", TypeS32, &mStatNumPlaying );
   Con::addVariable( "SFX::numCulled", TypeS32, &mStatNumCulled );
   Con::addVariable( "SFX::numVoices", TypeS32, &mStatNumVoices );
}

SFXSystem::~SFXSystem()
{
   Con::removeVariable( "SFX::numSources" );
   Con::removeVariable( "SFX::numPlaying" );
   Con::removeVariable( "SFX::numCulled" );
   Con::removeVariable( "SFX::numBuffers" );
   
   // Cleanup any remaining sources!
   while ( !mSources.empty() )
      mSources.first()->deleteObject();

   mSources.clear();
   mPlayOnceSources.clear();

   // If we still have a device... delete it.
   deleteDevice();
}

bool SFXSystem::createDevice( const String& providerName, const String& deviceName, bool useHardware, S32 maxBuffers, bool changeDevice )
{
   // Make sure we don't have a device already.
   
   if( mDevice && !changeDevice )
      return false;

   // Lookup the provider.
   
   SFXProvider* provider = SFXProvider::findProvider( providerName );
   if( !provider )
      return false;

   // If we have already created this device and are using it then no need to do anything.
   
   if( mDevice
       && providerName.equal( mDevice->getProvider()->getName(), String::NoCase )
       && deviceName.equal( mDevice->getName(), String::NoCase )
       && useHardware == mDevice->getUseHardware() )
      return true;

   // If we have an existing device remove it.
   
   if( mDevice )
      deleteDevice();

   // Create the new device.
   
   mDevice = provider->createDevice( deviceName, useHardware, maxBuffers );
   if( !mDevice )
   {
      Con::errorf( "SFXSystem::createDevice - failed creating %s device '%s'", providerName.c_str(), deviceName.c_str() );
      return false;
   }
   else
      Con::printf( "SFXSystem::createDevice - created %s device '%s'", providerName.c_str(), deviceName.c_str() );
      
   // Set defaults.
   
   mDevice->setDistanceModel( mDistanceModel );
   mDevice->setDopplerFactor( mDopplerFactor );
   mDevice->setRolloffFactor( mRolloffFactor );
   
   // Signal system.

   getEventSignal().trigger( SFXSystemEvent_CreateDevice );

   return true;
}

String SFXSystem::getDeviceInfoString()
{
   // Make sure we have a valid device.
   if( !mDevice )
      return String();

   return String::ToString( "%s\t%s\t%s\t%d",
      mDevice->getProvider()->getName().c_str(),
      mDevice->getName().c_str(),
      mDevice->getUseHardware() ? "1" : "0",
      mDevice->getMaxBuffers() );
}

void SFXSystem::deleteDevice()
{
   // Make sure we have a valid device.
   if ( !mDevice )
      return;

   // Put all playing sources into virtualized playback mode.  Where this fails,
   // stop the source.
   
   for( U32 i = 0, numSources = mSources.size(); i < numSources; ++ i )
      if( !mSources[ i ]->_releaseVoice() )
         mSources[ i ]->stop();

   // Signal everyone who cares that the
   // device is being deleted.
   getEventSignal().trigger( SFXSystemEvent_DestroyDevice );

   // Free the device which should delete all
   // the active voices and buffers.
   delete mDevice;
   mDevice = NULL;
}

SFXSource* SFXSystem::createSource( SFXProfile* profile,
                                    const MatrixF* transform, 
                                    const VectorF* velocity )
{
   // We sometimes get null profiles... nothing to play without a profile!
   if ( !profile )
      return NULL;

   // Create the source.
   SFXSource *source = SFXSource::_create( mDevice, profile );
   if ( !source )
   {
      Con::errorf( 
         "SFXSystem::createSource() - Creation failed!\n"
         "  Profile: %s\n"
         "  Filename: %s",
         profile->getName(),
         profile->getFilename() );

      return NULL;
   }

   _addSource( source, transform, velocity );
   return source;
}

SFXSource* SFXSystem::createSourceFromStream( const ThreadSafeRef< SFXStream >& stream,
                                              SFXDescription* description )
{
   // We sometimes get null values from script... fail in that case.
   if ( !stream || !description )
      return NULL;

   // Create the source.
   SFXSource *source = SFXSource::_create( mDevice, stream, description );
   if ( !source )
      return NULL;

   _addSource( source );
   return source;
}

void SFXSystem::_addSource( SFXSource* source, const MatrixF* transform, const VectorF* velocity )
{
   mSources.push_back( source );

   if ( transform )
      source->setTransform( *transform );

   if ( velocity )
      source->setVelocity( *velocity );

   const U32 channel = source->getChannel();
   const F32 volume = getChannelVolume( channel ) * mMasterVolume;
   source->_setModulativeVolume( volume );

   // Update the stats.
   mStatNumSources = mSources.size();
}

void SFXSystem::_onRemoveSource( SFXSource* source )
{
   SFXSourceVector::iterator iter = find( mSources.begin(), mSources.end(), source );
   AssertFatal( iter != mSources.end(), "Got unknown source!" );
   mSources.erase_fast( iter );

   // Check if it was a play once source...
   iter = find( mPlayOnceSources.begin(), mPlayOnceSources.end(), source );
   if ( iter != mPlayOnceSources.end() )
      mPlayOnceSources.erase_fast( iter );

   // Update the stats.
   mStatNumSources = mSources.size();
}

SFXBuffer* SFXSystem::_createBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description )
{
   // The buffers are created by the active
   // device... without one we cannot do anything.
   if ( !mDevice )
      return NULL;

   // Some sanity checking for streaming.  If the stream isn't at least three packets
   // in size given the current settings in "description", then turn off streaming.
   // The device code *will* mess up if the stream input fails to match certain metrics.
   // Just disabling streaming when it doesn't make sense is easier than complicating the
   // device logic to account for bad metrics.

   bool streamFlag = description->mIsStreaming;
   if( description->mIsStreaming
       && stream->getDuration() < description->mStreamPacketSize * 1000 * SFXInternal::SFXAsyncQueue::DEFAULT_STREAM_QUEUE_LENGTH )
      description->mIsStreaming = false;

   SFXBuffer* buffer = mDevice->createBuffer( stream, description );

   description->mIsStreaming = streamFlag; // restore in case we stomped it
   return buffer;
}

SFXBuffer* SFXSystem::_createBuffer( const String& filename, SFXDescription* description )
{
   if( !mDevice )
      return NULL;
      
   return mDevice->createBuffer( filename, description );
}

SFXSource* SFXSystem::playOnce(  SFXProfile *profile, 
                                 const MatrixF *transform,
                                 const VectorF *velocity )
{
   // We sometimes get null profiles... nothing to play without a profile!
   if ( !profile )
      return NULL;

   SFXSource *source = createSource( profile, transform, velocity );
   if ( source )
   {
      mPlayOnceSources.push_back( source ); 
      source->play();
   }

   return source;
}

void SFXSystem::stopAll( S32 channel )
{
   AssertFatal( channel < 0 || channel < NumChannels, "Got an invalid channel!" );

   // Go thru the sources and stop them.
   SFXSourceVector::iterator iter = mSources.begin();
   for ( ; iter != mSources.end(); iter++ )
   {
      SFXSource* source = *iter;
      if ( channel < 0 || source->getChannel() == channel )
         source->stop();
   }
}

void SFXSystem::setMasterVolume( F32 volume )
{
   mMasterVolume = mClampF( volume, 0, 1 );

   // Go thru the sources and update the modulative volume.
   SFXSourceVector::iterator iter = mSources.begin();
   for ( ; iter != mSources.end(); iter++ )
   {
      SFXSource* source = *iter;
      U32 channel = source->getChannel();
      F32 volume = getChannelVolume( channel ) * mMasterVolume;
      source->_setModulativeVolume( volume );
   }
}

F32 SFXSystem::getChannelVolume( U32 channel ) const
{
   AssertFatal( channel < NumChannels, "Got an invalid channel!" );
   return mChannelVolume[ channel ];
}

void SFXSystem::setChannelVolume( U32 channel, F32 volume )
{
   AssertFatal( channel < NumChannels, "Got an invalid channel!" );

   volume = mClampF( volume, 0, 1 );
   mChannelVolume[ channel ] = volume;

   // Scale it by the master volume.
   volume *= mMasterVolume;

   // Go thru the sources and update the modulative volume.
   SFXSourceVector::iterator iter = mSources.begin();
   for ( ; iter != mSources.end(); iter++ )
   {
      SFXSource* source = *iter;
      if ( source->getChannel() == channel )
         source->_setModulativeVolume( volume );
   }
}

void SFXSystem::_update()
{
   PROFILE_SCOPE( SFXSystem_Update );
   
   getEventSignal().trigger( SFXSystemEvent_Update );
   
   // Every four system ticks.
   const U32 SOURCE_UPDATE_MS = TickMs * 4;

   // The update of the sources can be a bit expensive
   // and it does not need to be updated every frame.
   const U32 time = Platform::getVirtualMilliseconds();
   const U32 elapsed = time - mLastTime;
   if ( elapsed >= SOURCE_UPDATE_MS )
   {
      _updateSources();
      mLastTime = time;
   }

   // If we have a device then update it.
   if ( mDevice )
      mDevice->update( mListener );

   // Update some stats.
   mStatNumSources = mSources.size();
}

void SFXSystem::_updateSources()
{
   PROFILE_SCOPE( SFXSystem_UpdateSources );

   // Check the status of the sources here once.
   mStatNumPlaying = 0;
   SFXSourceVector::iterator iter = mSources.begin();
   for ( ; iter != mSources.end(); ++iter )
   {
      ( *iter )->_update();
      if ( (*iter)->getStatus() == SFXStatusPlaying )
         ++mStatNumPlaying;
   }

   // First check to see if any play once sources have
   // finished playback... delete them.
   iter = mPlayOnceSources.begin();
   for ( ; iter != mPlayOnceSources.end();  )
   {
      SFXSource* source = *iter;

      if ( source->getLastStatus() == SFXStatusStopped )
      {
         int index = iter - mPlayOnceSources.begin();

         // Erase it from the vector first, so that onRemoveSource
         // doesn't do it during cleanup and screw up our loop here!
         mPlayOnceSources.erase_fast( iter );
         source->deleteObject();

         iter = mPlayOnceSources.begin() + index;
         continue;
      }

      iter++;
   }

   // Reassign buffers to the sources.
   _assignVoices();
}

void SFXSystem::_assignVoices()
{
   PROFILE_SCOPE( SFXSystem_AssignVoices );

   mStatNumVoices = 0;
   mStatNumCulled = 0;

   // If we have no device then we have 
   // nothing more to do.
   if ( !mDevice )
      return;

   // Now let the listener prioritize the sounds for us 
   // before we go off and assign buffers.
   mListener.sortSources( mSources );

   // We now make sure that the sources closest to the 
   // listener, the ones at the top of the source list,
   // have a device buffer to play thru.
   mStatNumCulled = 0;
   SFXSourceVector::iterator iter = mSources.begin(); 
   for ( ; iter != mSources.end(); ++iter )
   {
      SFXSource* source = *iter;

      // Non playing sources (paused or stopped) are at the
      // end of the vector, so when i encounter one i know 
      // that nothing else in the vector needs buffer assignment.
		if( !source->isPlaying() )
         break;

      // If the source is outside it's max range we can
      // skip it as well, so that we don't waste cycles
      // setting up a buffer for something we won't hear.
      if( source->getAttenuatedVolume() <= 0.0f )
      {
         mStatNumCulled++;
         continue;
      }

      // If the source has a voice then we can skip it.
      if( source->hasVoice() )
         continue;

      // Ok let the device try to assign a new voice for 
      // this source... this may fail if we're out of voices.
      if( source->_allocVoice( mDevice ) )
         continue;

      // The device couldn't assign a new voice, so we look for
      // the last source in the list with a voice and free it.
      SFXSourceVector::iterator hijack = mSources.end() - 1;
      for( ; hijack != iter; hijack-- )
      {
         if( ( *hijack )->hasVoice()
             && ( *hijack )->_releaseVoice() )
            break;
		}

      // Ok try to assign a voice once again!
      if( source->_allocVoice( mDevice ) )
         continue;

      // If the source still doesn't have a buffer... well
      // tough cookies.  It just cannot be heard yet, maybe
      // it can in the next update.
      mStatNumCulled++;
	}

   // Update the buffer count stat.
   mStatNumVoices = mDevice->getVoiceCount();
}

void SFXSystem::setDistanceModel( SFXDistanceModel model )
{
   mDistanceModel = model;
   if( mDevice )
      mDevice->setDistanceModel( model );
}

void SFXSystem::setDopplerFactor( F32 factor )
{
   mDopplerFactor = factor;
   if( mDevice )
      mDevice->setDopplerFactor( factor );
}

void SFXSystem::setRolloffFactor( F32 factor )
{
   mRolloffFactor = factor;
   if( mDevice )
      mDevice->setRolloffFactor( factor );
}

void SFXSystem::dumpSources()
{
   for( U32 i = 0; i < mSources.size(); ++ i )
   {
      SFXSource* source = mSources[ i ];

      bool isPlayOnce = false;
      for( U32 j = 0; j < mPlayOnceSources.size(); ++ j )
         if( mPlayOnceSources[ j ] == source )
         {
            isPlayOnce = true;
            break;
         }

      String fileName;
      SFXProfile* profile = source->getProfile();
      if( profile )
         fileName = profile->getFileName();

      Con::printf( "%5i: status=%s, blocked=%s, virtual=%s, looping=%s, 3d=%s, channel=%i, position=%i, playOnce=%s, streaming=%s, hasVoice=%s, file='%s'",
         source->getId(),
         source->isPlaying()
         ? "playing"
         : source->isPaused()
         ? "paused"
         : source->isStopped()
         ? "stopped"
         : "unknown",
         source->isBlocked() ? "1" : "0",
         source->isVirtualized() ? "1" : "0",
         source->isLooping() ? "1" : "0",
         source->is3d() ? "1" : "0",
         source->getChannel(),
         source->getPosition(),
         isPlayOnce ? "1" : "0",
         source->isStreaming() ? "1" : "0",
         source->hasVoice() ? "1" : "0",
         fileName.c_str()
         );
   }
}

ConsoleFunctionGroupBegin( SFX, 
   "Functions dealing with the SFX audio layer." );

ConsoleFunction( sfxGetAvailableDevices, const char*, 1, 1, 
   "Returns a list of available devices in the form:\n\n"
   "provider1 [tab] device1 [tab] hasHardware1 [tab] maxBuffers1 [nl] provider2 ... etc." )
{
   char* deviceList = Con::getReturnBuffer( 2048 );
   deviceList[0] = 0;

   SFXProvider* provider = SFXProvider::getFirstProvider();
   while ( provider )
   {
      // List the devices in this provider.
      const SFXDeviceInfoVector& deviceInfo = provider->getDeviceInfo();
      for ( S32 d=0; d < deviceInfo.size(); d++ )
      {
         const SFXDeviceInfo* info = deviceInfo[d];
         dStrcat( deviceList, provider->getName() );
         dStrcat( deviceList, "\t" );
         dStrcat( deviceList, info->name );
         dStrcat( deviceList, "\t" );
         dStrcat( deviceList, info->hasHardware ? "1" : "0" );
         dStrcat( deviceList, "\t" );
         dStrcat( deviceList, Con::getIntArg( info->maxBuffers ) );         
         dStrcat( deviceList, "\n" );
      }

      provider = provider->getNextProvider();
   }

   return deviceList;
}

ConsoleFunction( sfxCreateDevice, bool, 5, 5,  
                  "sfxCreateDevice( string provider, string device, bool useHardware, S32 maxBuffers )\n"
                  "Initializes the requested device.  This must be called successfully before any sounds will be heard.\n"
                  "@param provider The provider name.\n"
                  "@param device The device name.\n"
                  "@param useHardware A boolean which toggles the use of hardware processing when available.\n"
                  "@param maxBuffers The maximum buffers for this device to use or -1 for the device to pick its own reasonable default.")
{
   return SFX->createDevice( argv[1], argv[2], dAtob( argv[3] ), dAtoi( argv[4] ), true );
}

ConsoleFunction( sfxDeleteDevice, void, 1, 1, 
   "Destroys the currently initialized device.  Sounds will still play, but not be heard.")
{
   SFX->deleteDevice();
}

ConsoleFunction( sfxGetDeviceInfo, const char*, 1, 1,  
      "Returns a tab delimited string containing information on the current device." )
{
   String deviceInfo = SFX->getDeviceInfoString();
   if( deviceInfo.isEmpty() )
      return NULL;

   U32 size = deviceInfo.size();
   char* buffer = Con::getReturnBuffer( size );
   dMemcpy( buffer, deviceInfo.c_str(), size );

   return buffer;
}

ConsoleFunction( sfxPlay, S32, 2, 5, "sfxPlay( source )\n"
                                     "sfxPlay( profile, <x, y, z> )\n" )
{
   if ( argc == 2 )
   {
      SFXSource* source = dynamic_cast<SFXSource*>( Sim::findObject( argv[1] ) );
      if ( source )
      {
         source->play();
         return source->getId();
      }
   }

   SFXProfile *profile = dynamic_cast<SFXProfile*>( Sim::findObject( argv[1] ) );
   if ( !profile )
   {
      Con::printf( "Unable to locate sfx profile '%s'", argv[1] );
      return 0;
   }

   Point3F pos(0.f, 0.f, 0.f);
   if ( argc == 3 )
      dSscanf( argv[2], "%g %g %g", &pos.x, &pos.y, &pos.z );
   else if(argc == 5)
      pos.set( dAtof(argv[2]), dAtof(argv[3]), dAtof(argv[4]) );

   MatrixF transform;
   transform.set( EulerF(0,0,0), pos );

   SFXSource* source = SFX->playOnce( profile, &transform );
   if ( source )
      return source->getId();

   return 0;
}


ConsoleFunction( sfxCreateSource, S32, 2, 6,
                     "sfxCreateSource(profile)\n"
                     "sfxCreateSource(profile, x,y,z)\n"
                     "sfxCreateSource(description, filename)\n"
                     "sfxCreateSource(description, filename, x,y,z)\n"
                     "\n"
                     "Creates a new paused sound source using a profile or a description "
                     "and filename.  The return value is the source which must be "
                     "released by delete()." )
{
   SFXDescription* description = NULL;
   SFXProfile* profile = dynamic_cast<SFXProfile*>( Sim::findObject( argv[1] ) );
   if ( !profile )
   {
      description = dynamic_cast<SFXDescription*>( Sim::findObject( argv[1] ) );
      if ( !description )
      {
         Con::printf( "Unable to locate sound profile/description '%s'", argv[1] );
         return 0;
      }
   }

   SFXSource* source = NULL;

   if ( profile )
   {
      if ( argc == 2 )
      {
         source = SFX->createSource( profile );
      }
      else
      {
         MatrixF transform;
         transform.set( EulerF(0,0,0), Point3F( dAtof(argv[2]), dAtof(argv[3]), dAtof(argv[4])) );
         source = SFX->createSource( profile, &transform );
      }
   }
   else if ( description )
   {
      SFXProfile* tempProfile = new SFXProfile( description, StringTable->insert( argv[2] ), true );
      if( !tempProfile->registerObject() )
      {
         Con::errorf( "sfxCreateSource - unable to create profile" );
         delete tempProfile;
      }
      else
      {
         if ( argc == 3 )
         {
            source = SFX->createSource( tempProfile );
         }
         else
         {
            MatrixF transform;
            transform.set(EulerF(0,0,0), Point3F( dAtof(argv[3]),dAtof(argv[4]),dAtof(argv[5]) ));
            source = SFX->createSource( tempProfile, &transform );
         }

         tempProfile->setAutoDelete( true );
      }
   }

   if ( source )
      return source->getId();

   return 0;
}

ConsoleFunction( sfxPlayOnce, S32, 2, 6,
                     "sfxPlayOnce(profile)\n"
                     "sfxPlayOnce(profile, x,y,z)\n"
                     "sfxPlayOnce(description, filename)\n"
                     "sfxPlayOnce(description, filename, x,y,z)\n"
                     "\n"
                     "Creates a new sound source using a profile or a description "
                     "and filename and plays it once.  Once playback is finished the "
                     "source is deleted.  The return value is the temporary source id." )
{
   SFXDescription* description = NULL;
   SFXProfile* profile = dynamic_cast<SFXProfile*>( Sim::findObject( argv[1] ) );
   if ( !profile )
   {
      description = dynamic_cast<SFXDescription*>( Sim::findObject( argv[1] ) );
      if ( !description )
      {
         Con::errorf( "sfxPlayOnce - Unable to locate sound profile/description '%s'", argv[1] );
         return 0;
      }
   }

   SFXSource* source = NULL;

   if ( profile )
   {
      if ( argc == 2 )
         source = SFX->playOnce( profile );
      else
      {
         MatrixF transform;
         transform.set(EulerF(0,0,0), Point3F( dAtof(argv[2]),dAtof(argv[3]),dAtof(argv[4]) ));
         source = SFX->playOnce( profile, &transform );
      }
   }

   else if ( description )
   {
      SFXProfile* tempProfile = new SFXProfile( description, StringTable->insert( argv[2] ), true );
      if( !tempProfile->registerObject() )
      {
         Con::errorf( "sfxPlayOnce - unable to create profile" );
         delete tempProfile;
      }
      else
      {
         if ( argc == 3 )
            source = SFX->playOnce( tempProfile );
         else
         {
            MatrixF transform;
            transform.set(EulerF(0,0,0), Point3F( dAtof(argv[3]),dAtof(argv[4]),dAtof(argv[5]) ));
            source = SFX->playOnce( tempProfile, &transform );
         }
         
         // Set profile to auto-delete when SFXSource releases its reference.
         // Also add to root group so the profile will get deleted when the
         // Sim system is shut down before the SFXSource has played out.

         tempProfile->setAutoDelete( true );
         Sim::getRootGroup()->addObject( tempProfile );
      }
   }

   if ( source )
      return source->getId();

   return 0;
}

ConsoleFunction(sfxStop, void, 2, 2, "(S32 id): stop a source, equivalent to id.stop()")
{
   S32 id = dAtoi(argv[1]);
   SFXSource * obj;
   if (Sim::findObject<SFXSource>(id, obj))
      obj->stop();
}


ConsoleFunction(sfxStopAll, void, 1, 2, "(S32 channel = -1)\n\n"
                "@param channel The optional channel index of which sources to stop.\n"
                "@return The volume of the channel.")
{
   U32 channel = -1;
   
   if ( argc > 1 )
   {
      channel = dAtoi( argv[1] );

      if ( channel >= SFXSystem::NumChannels )
      {
         Con::errorf( ConsoleLogEntry::General, "sfxStopAll: invalid channel '%d'", dAtoi( argv[1] ) );
         return;
      }
   }

   SFX->stopAll( channel );
}

ConsoleFunction(sfxGetChannelVolume, F32, 2, 2, "(S32 channel)\n\n"
                "@param channel The channel index to fetch the volume from.\n"
                "@return The volume of the channel.")
{
   U32 channel = dAtoi( argv[1] );

   if ( channel >= SFXSystem::NumChannels )
   {
      Con::errorf( ConsoleLogEntry::General, "sfxGetChannelVolume: invalid channel '%d'", dAtoi( argv[1] ) );
      return 0.0f;
   }

   return SFX->getChannelVolume( channel );
}

ConsoleFunction(sfxSetChannelVolume, bool, 3, 3, "(S32 channel, F32 volume)\n\n"
                "@param channel The channel index to set volume on.\n"
                "@param volume New 0 to 1 channel volume."
                )
{
   U32 channel = dAtoi( argv[1] );

   F32 volume = mClampF( dAtof( argv[2] ), 0, 1 );

   if ( channel >= SFXSystem::NumChannels )
   {
      Con::errorf( ConsoleLogEntry::General, "sfxSetChannelVolume: invalid channel '%d'", dAtoi( argv[1] ) );
      return false;
   }

   SFX->setChannelVolume( channel, volume );
   return true;
}

ConsoleFunction(sfxGetMasterVolume, F32, 1, 1, "()\n\n"
                "@return The sound system master volume." )
{
   return SFX->getMasterVolume();
}

ConsoleFunction(sfxSetMasterVolume, void, 2, 2, "(F32 volume)\n\n"
                "@param volume The new 0 to 1 sound system master volume." )
{
   F32 volume = mClampF( dAtof( argv[1] ), 0, 1 );
   SFX->setMasterVolume( volume );
}

ConsoleFunction( sfxGetDistanceModel, const char*, 1, 1, "()" )
{
   switch( SFX->getDistanceModel() )
   {
      case SFXDistanceModelLinear:
         return "linear";
         
      case SFXDistanceModelLogarithmic:
         return "logarithmic";
   }
   
   return "";
}

ConsoleFunction( sfxSetDistanceModel, void, 2, 2, "( string model ) - Set the curve model used for distance attenuation of 3D sounds." )
{
   if( !argv[ 1 ] )
      return;
      
   if( dStricmp( argv[ 1 ], "linear" ) == 0 )
      SFX->setDistanceModel( SFXDistanceModelLinear );
   else if( dStricmp( argv[ 1 ], "logarithmic" ) == 0 )
      SFX->setDistanceModel( SFXDistanceModelLogarithmic );
   else
      Con::errorf( "sfxSetDistanceModel - unknown model '%s'", argv[ 1 ] );
}

ConsoleFunction( sfxGetDopplerFactor, F32, 1, 1, "()" )
{
   return SFX->getDopplerFactor();
}

ConsoleFunction( sfxSetDopplerFactor, void, 2, 2, "( F32 value )" )
{
   if( !argv[ 1 ] )
      return;
      
   F32 val = dAtof( argv[ 1 ] );
   if( val < 0.0f )
   {
      Con::errorf( "sfxSetDopplerFactor - factor must be >0" );
      return;
   }

   SFX->setDopplerFactor( val );
}

ConsoleFunction( sfxGetRolloffFactor, F32, 1, 1, "()" )
{
   return SFX->getRolloffFactor();
}

ConsoleFunction( sfxSetRolloffFactor, void, 2, 2, "( F32 value )" )
{
   if( !argv[ 1 ] )
      return;
      
   F32 val = dAtof( argv[ 1 ] );
   if( val < 0.0f )
   {
      Con::errorf( "sfxSetRolloffFactor - factor must be >0" );
      return;
   }

   SFX->setRolloffFactor( val );
}

ConsoleFunction( sfxDumpSources, void, 1, 1, "() - Dump information about all current SFXSource instances" )
{
   SFX->dumpSources();
}

ConsoleFunctionGroupEnd( SFX );

