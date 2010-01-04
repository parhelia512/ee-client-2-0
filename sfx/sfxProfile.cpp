//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "sfx/sfxProfile.h"
#include "sfx/sfxDescription.h"
#include "sfx/sfxSystem.h"
#include "sfx/sfxStream.h"
#include "sim/netConnection.h"
#include "core/stream/bitStream.h"
#include "core/resourceManager.h"


IMPLEMENT_CO_DATABLOCK_V1( SFXProfile );

IMPLEMENT_CONSOLETYPE( SFXProfile )
IMPLEMENT_GETDATATYPE( SFXProfile )
IMPLEMENT_SETDATATYPE( SFXProfile )


//-----------------------------------------------------------------------------

SFXProfile::SFXProfile()
   :  mDescription( NULL ),
      mPreload( false ),
      mDescriptionId( 0 )
{
}

//-----------------------------------------------------------------------------

SFXProfile::SFXProfile( SFXDescription* desc, const String& filename, bool preload )
   :  mFilename( filename ),
      mDescription( desc ),
      mPreload( preload ),
      mDescriptionId( 0 )
{
}

//-----------------------------------------------------------------------------

SFXProfile::~SFXProfile()
{
}

//-----------------------------------------------------------------------------

void SFXProfile::initPersistFields()
{
   addField( "filename",    TypeStringFilename,        Offset(mFilename, SFXProfile));
   addField( "description", TypeSFXDescriptionPtr,     Offset(mDescription, SFXProfile));
   addField( "preload",     TypeBool,                  Offset(mPreload, SFXProfile));

   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------

bool SFXProfile::onAdd()
{
   if( !Parent::onAdd() )
      return false;
      
   // Look up our SFXDescription.

   if( mDescription == NULL &&
       mDescriptionId != 0 )
   {
      if ( !Sim::findObject( mDescriptionId, mDescription ) )
      {
         Con::errorf( 
            "SFXProfile(%s)::onAdd: Invalid packet, bad description id: %d", 
            getName(), mDescriptionId );
         return false;
      }
   }
   
   // If we have no SFXDescription, try to grab a default.

   if( !mDescription )
   {
      if( !Sim::findObject( "AudioSim", mDescription ) )
      {
         Con::errorf( 
            "SFXProfile(%s)::onAdd: The profile is missing a description!", 
            getName() );
         return false;
      }
   }

   // If we're a streaming profile we don't preload
   // or need device events.
   if ( SFX && !mDescription->mIsStreaming )
   {
      // If preload is enabled we load the resource
      // and device buffer now to avoid a delay on
      // first playback.
      if ( mPreload && !_preloadBuffer() )
         Con::errorf( "SFXProfile(%s)::onAdd: The preload failed!", getName() );
   }

   _registerSignals();

   return true;
}

//-----------------------------------------------------------------------------

void SFXProfile::onRemove()
{
   _unregisterSignals();

   Parent::onRemove();
}

//-----------------------------------------------------------------------------

bool SFXProfile::preload( bool server, String &errorStr )
{
   if ( !Parent::preload( server, errorStr ) )
      return false;

   // TODO: Investigate how NetConnection::filesWereDownloaded()
   // effects the system.

   // Validate the datablock... has nothing to do with mPreload.
   if (  !server &&
         NetConnection::filesWereDownloaded() &&
         ( mFilename.isEmpty() || !SFXResource::exists( mFilename ) ) )
      return false;

   return true;
}

//-----------------------------------------------------------------------------

void SFXProfile::packData(BitStream* stream)
{
   Parent::packData( stream );

   // audio description:
   if ( stream->writeFlag( mDescription ) )
   {
      stream->writeRangedU32( mDescription->getId(),  
                              DataBlockObjectIdFirst,
                              DataBlockObjectIdLast );
   }

   //
   char buffer[256];
   if ( mFilename.isEmpty() )
      buffer[0] = 0;
   else
      dStrncpy( buffer, mFilename.c_str(), 256 );
   stream->writeString( buffer );

   stream->writeFlag( mPreload );
}

//-----------------------------------------------------------------------------

void SFXProfile::unpackData(BitStream* stream)
{
   Parent::unpackData( stream );

   // audio datablock:
   if ( stream->readFlag() )
      mDescriptionId = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );

   char buffer[256];
   stream->readString( buffer );
   mFilename = buffer;

   mPreload = stream->readFlag();
}

//-----------------------------------------------------------------------------

void SFXProfile::_registerSignals()
{
   SFX->getEventSignal().notify( this, &SFXProfile::_onDeviceEvent );
   ResourceManager::get().getChangedSignal().notify( this, &SFXProfile::_onResourceChanged );
}

//-----------------------------------------------------------------------------

void SFXProfile::_unregisterSignals()
{
   ResourceManager::get().getChangedSignal().remove( this, &SFXProfile::_onResourceChanged );
   if( SFX )
      SFX->getEventSignal().remove( this, &SFXProfile::_onDeviceEvent );
}

//-----------------------------------------------------------------------------

void SFXProfile::_onDeviceEvent( SFXSystemEventType evt )
{
   switch( evt )
   {
      case SFXSystemEvent_CreateDevice:
      {
         if( mPreload && !mDescription->mIsStreaming && !_preloadBuffer() )
            Con::errorf( "SFXProfile::_onDeviceEvent: The preload failed! %s", getName() );
         break;
      }
      
      default:
         break;
   }
}

//-----------------------------------------------------------------------------

void SFXProfile::_onResourceChanged( ResourceBase::Signature sig, const Torque::Path& path )
{
   if( sig == Resource< SFXResource >::signature()
       && path == Path( mFilename ) )
   {
      // Let go of the old resource and buffer.
            
      mResource = NULL;
      mBuffer = NULL;
      
      // Load the new resource.
      
      getResource();
      
      if( mPreload && !mDescription->mIsStreaming )
      {
         if( !_preloadBuffer() )
            Con::errorf( "SFXProfile::_onResourceChanged() - failed to preload '%s'", mFilename.c_str() );
      }
      
      mChangedSignal.trigger( this );
   }
}

//-----------------------------------------------------------------------------

bool SFXProfile::_preloadBuffer()
{
   AssertFatal( !mDescription->mIsStreaming, "SFXProfile::_preloadBuffer() - must not be called for streaming profiles" );

   mBuffer = _createBuffer();
   return ( !mBuffer.isNull() );
}

//-----------------------------------------------------------------------------

Resource<SFXResource>& SFXProfile::getResource()
{
   if( !mResource && !mFilename.isEmpty() )
      mResource = SFXResource::load( mFilename );

   return mResource;
}

//-----------------------------------------------------------------------------

SFXBuffer* SFXProfile::getBuffer()
{
   if ( mDescription->mIsStreaming )
   {
      // Streaming requires unique buffers per 
      // source, so this creates a new buffer.
      if ( SFX )
         return _createBuffer();

      return NULL;
   }

   if ( mBuffer.isNull() )
      _preloadBuffer();

   return mBuffer;
}

//-----------------------------------------------------------------------------

SFXBuffer* SFXProfile::_createBuffer()
{
   SFXBuffer* buffer = 0;
   
   // Try to create through SFXDevie.
   
   if( !mFilename.isEmpty() && SFX )
   {
      buffer = SFX->_createBuffer( mFilename, mDescription );
      if( buffer )
      {
         #ifdef TORQUE_DEBUG
         const SFXFormat& format = buffer->getFormat();
         Con::printf( "%s SFX: %s (%i channels, %i kHz, %.02f sec, %i kb)",
            mDescription->mIsStreaming ? "Streaming" : "Loaded", mFilename.c_str(),
            format.getChannels(),
            format.getSamplesPerSecond() / 1000,
            F32( buffer->getDuration() ) / 1000.0f,
            format.getDataLength( buffer->getDuration() ) / 1024 );
         #endif
      }
   }
   
   // If that failed, load through SFXResource.
   
   if( !buffer )
   {
      Resource< SFXResource >& resource = getResource();
      if( resource != NULL && SFX )
      {
         #ifdef TORQUE_DEBUG
         const SFXFormat& format = resource->getFormat();
         Con::printf( "%s SFX: %s (%i channels, %i kHz, %.02f sec, %i kb)",
            mDescription->mIsStreaming ? "Streaming" : "Loading", resource->getFileName().c_str(),
            format.getChannels(),
            format.getSamplesPerSecond() / 1000,
            F32( resource->getDuration() ) / 1000.0f,
            format.getDataLength( resource->getDuration() ) / 1024 );
         #endif

         ThreadSafeRef< SFXStream > sfxStream = resource->openStream();
         buffer = SFX->_createBuffer( sfxStream, mDescription );
      }
   }

   return buffer;
}

//-----------------------------------------------------------------------------

U32 SFXProfile::getSoundDuration()
{
   Resource< SFXResource  >& resource = getResource();
   if( resource != NULL )
      return mResource->getDuration();
   else
      return 0;
}

//-----------------------------------------------------------------------------

ConsoleMethod(SFXProfile, getSoundDuration, F32, 2, 2,
   "()\n"
   "@return Returns the length of the sound in seconds." )
{
   return (F32)object->getSoundDuration() * 0.001f;
}

