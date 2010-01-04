//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/sfxEmitter.h"
#include "renderInstance/renderPassManager.h"
#include "sfx/sfxSystem.h"
#include "sfx/sfxProfile.h"
#include "sfx/sfxSource.h"
//#include "T3D/ambientAudioManager.h"
#include "sceneGraph/sceneState.h"
#include "core/stream/bitStream.h"
#include "sim/netConnection.h"


extern bool gEditingMission;

IMPLEMENT_CO_NETOBJECT_V1(SFXEmitter);


SFXEmitter::SFXEmitter()
   :  SceneObject(),
      mSource( NULL ),
      mProfile( NULL ),
      mLocalProfile( &mDescription ),
      mPlayOnAdd( true )
{
   mTypeMask |= MarkerObjectType;
   mNetFlags.set( Ghostable | ScopeAlways );

   mDescription.mIs3D = true;
   mDescription.mIsLooping = true;
   mDescription.mIsStreaming = false;
   
   mLocalProfile._registerSignals();
}

SFXEmitter::~SFXEmitter()
{
   mLocalProfile._unregisterSignals();
}

void SFXEmitter::initPersistFields()
{
   //[rene 07/04/09]
   //  This entire profile/local profile split thing back from TGE-days is no good and should be removed.
   //  The emitter should link to a single SFXProfile and there should be a separate means of creating/editing/managing
   //  profiles as part of the standard editor toolset (datablock editor?).
   //
   //  The way it is now, it is totally confusing, inconsistent, and difficult to handle in script (example:
   //  what's the "is3D" supposed to mean?  Nothing, if a profile is selected.  So how do I determine whether
   //  a profile is 3D?  Hmmm, check for profile, it set, check it's description, if not, check the emitter...).

   addGroup("Media");
   addField("profile",              TypeSFXProfilePtr,         Offset(mProfile, SFXEmitter));
   addField("fileName",             TypeStringFilename,        Offset(mLocalProfile.mFilename, SFXEmitter));
   endGroup("Media");

   addGroup("Sound");
   addField("playOnAdd",            TypeBool,   Offset(mPlayOnAdd, SFXEmitter));
   addField("isLooping",            TypeBool,   Offset(mDescription.mIsLooping, SFXEmitter));
   addField("isStreaming",          TypeBool,   Offset(mDescription.mIsStreaming, SFXEmitter));
   addField("channel",              TypeS32,    Offset(mDescription.mChannel, SFXEmitter));
   addField("volume",               TypeF32,    Offset(mDescription.mVolume, SFXEmitter));
   addField("pitch",                TypeF32,    Offset(mDescription.mPitch, SFXEmitter));
   addField("fadeInTime",           TypeF32,    Offset(mDescription.mFadeInTime, SFXEmitter));
   addField("fadeOutTime",          TypeF32,    Offset(mDescription.mFadeOutTime, SFXEmitter));
   endGroup("Sound");

   addGroup("3D Sound");
   addField("is3D",                 TypeBool,   Offset(mDescription.mIs3D, SFXEmitter));
   addField("referenceDistance",    TypeF32,    Offset(mDescription.mReferenceDistance, SFXEmitter));
   addField("maxDistance",          TypeF32,    Offset(mDescription.mMaxDistance, SFXEmitter));
   addField("coneInsideAngle",      TypeS32,    Offset(mDescription.mConeInsideAngle, SFXEmitter));
   addField("coneOutsideAngle",     TypeS32,    Offset(mDescription.mConeOutsideAngle, SFXEmitter));
   addField("coneOutsideVolume",    TypeF32,    Offset(mDescription.mConeOutsideVolume, SFXEmitter));
   endGroup("3D Sound");

   Parent::initPersistFields();
}

U32 SFXEmitter::packUpdate( NetConnection *con, U32 mask, BitStream *stream )
{
   U32 retMask = Parent::packUpdate( con, mask, stream );

   if ( stream->writeFlag( mask & InitialUpdateMask ) )
   {
      // If this is the initial update then all the source
      // values are dirty and must be transmitted.
      mask |= TransformUpdateMask;
      mDirty = AllDirtyMask;

      // Clear the source masks... they are not
      // used during an initial update!
      mask &= ~AllSourceMasks;
   }

   stream->writeFlag( mPlayOnAdd );

   // transform
   if ( stream->writeFlag( mask & TransformUpdateMask ) )
      stream->writeAffineTransform( mObjToWorld );

   // profile
   if ( stream->writeFlag( mDirty.test( Profile ) ) )
      if ( stream->writeFlag( mProfile ) )
         stream->writeRangedU32( mProfile->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast );

   // filename
   if ( stream->writeFlag( mDirty.test( Filename ) ) )
      stream->writeString( mLocalProfile.mFilename );

   // volume
   if ( stream->writeFlag( mDirty.test( Volume ) ) )
      stream->write( mDescription.mVolume );
      
   // pitch
   if( stream->writeFlag( mDirty.test( Pitch ) ) )
      stream->write( mDescription.mPitch );

   // islooping
   if ( stream->writeFlag( mDirty.test( IsLooping ) ) )
      stream->writeFlag( mDescription.mIsLooping );
      
   // isStreaming
   if( stream->writeFlag( mDirty.test( IsStreaming ) ) )
      stream->writeFlag( mDescription.mIsStreaming );

   // is3d
   if ( stream->writeFlag( mDirty.test( Is3D ) ) )
      stream->writeFlag( mDescription.mIs3D );

   // ReferenceDistance
   if ( stream->writeFlag( mDirty.test( ReferenceDistance ) ) )
      stream->write( mDescription.mReferenceDistance );

   // maxdistance
   if ( stream->writeFlag( mDirty.test( MaxDistance) ) )
      stream->write( mDescription.mMaxDistance );

   // coneinsideangle
   if ( stream->writeFlag( mDirty.test( ConeInsideAngle ) ) )
      stream->write( mDescription.mConeInsideAngle );

   // coneoutsideangle
   if ( stream->writeFlag( mDirty.test( ConeOutsideAngle ) ) )
      stream->write( mDescription.mConeOutsideAngle );

   // coneoutsidevolume
   if ( stream->writeFlag( mDirty.test( ConeOutsideVolume ) ) )
      stream->write( mDescription.mConeOutsideVolume );

   // channel
   if ( stream->writeFlag( mDirty.test( Channel ) ) )
      stream->write( mDescription.mChannel );
      
   // fadein
   if( stream->writeFlag( mDirty.test( FadeInTime ) ) )
      stream->write( mDescription.mFadeInTime );
      
   // fadeout
   if( stream->writeFlag( mDirty.test( FadeOutTime ) ) )
      stream->write( mDescription.mFadeOutTime );

   mDirty.clear();

   // We should never have both source masks 
   // enabled at the same time!
   AssertFatal( ( mask & AllSourceMasks ) != AllSourceMasks, 
      "SFXEmitter::packUpdate() - Bad source mask!" );

   // Write the source playback state.
   stream->writeFlag( mask & SourcePlayMask );
   stream->writeFlag( mask & SourceStopMask );

   return retMask;
}

bool SFXEmitter::_readDirtyFlag( BitStream* stream, U32 mask )
{
   bool flag = stream->readFlag();
   if ( flag )
      mDirty.set( mask );

   return flag;
}

void SFXEmitter::unpackUpdate( NetConnection *conn, BitStream *stream )
{
   Parent::unpackUpdate( conn, stream );

   // initial update?
   bool initialUpdate = stream->readFlag();

   mPlayOnAdd = stream->readFlag();

   // transform
   if ( _readDirtyFlag( stream, Transform ) )
   {
      MatrixF mat;
      stream->readAffineTransform(&mat);
      Parent::setTransform(mat);
   }

   // profile
   if ( _readDirtyFlag( stream, Profile ) )
   {
      if ( stream->readFlag() )
      {
         S32 profileId = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );
         mProfile = dynamic_cast<SFXProfile*>( Sim::findObject( profileId ) );
      }
      else
         mProfile = NULL;
   }

   // filename
   if ( _readDirtyFlag( stream, Filename ) )
      mLocalProfile.mFilename = stream->readSTString();

   // volume
   if ( _readDirtyFlag( stream, Volume ) )
      stream->read( &mDescription.mVolume );
      
   // pitch
   if( _readDirtyFlag( stream, Pitch ) )
      stream->read( &mDescription.mPitch );

   // islooping
   if ( _readDirtyFlag( stream, IsLooping ) )
      mDescription.mIsLooping = stream->readFlag();
      
   if( _readDirtyFlag( stream, IsStreaming ) )
      mDescription.mIsStreaming = stream->readFlag();

   // is3d
   if ( _readDirtyFlag( stream, Is3D ) )
      mDescription.mIs3D = stream->readFlag();

   // ReferenceDistance
   if ( _readDirtyFlag( stream, ReferenceDistance ) )
      stream->read( &mDescription.mReferenceDistance );

   // maxdistance
   if ( _readDirtyFlag( stream, MaxDistance ) )
      stream->read( &mDescription.mMaxDistance );

   // coneinsideangle
   if ( _readDirtyFlag( stream, ConeInsideAngle ) )
      stream->read( &mDescription.mConeInsideAngle );

   // coneoutsideangle
   if ( _readDirtyFlag( stream, ConeOutsideAngle ) )
      stream->read( &mDescription.mConeOutsideAngle );

   // coneoutsidevolume
   if ( _readDirtyFlag( stream, ConeOutsideVolume ) )
      stream->read( &mDescription.mConeOutsideVolume );

   // channel
   if ( _readDirtyFlag( stream, Channel ) )
      stream->read( &mDescription.mChannel );
      
   // fadein
   if ( _readDirtyFlag( stream, FadeInTime ) )
      stream->read( &mDescription.mFadeInTime );
      
   // fadeout
   if( _readDirtyFlag( stream, FadeOutTime ) )
      stream->read( &mDescription.mFadeOutTime );

   // update the emitter now?
   if ( !initialUpdate )
      _update();

   // Check the source playback masks.
   if ( stream->readFlag() ) // SourcePlayMask
      play();
   if ( stream->readFlag() ) // SourceStopMask
      stop();
}

void SFXEmitter::onStaticModified( const char* slotName, const char* newValue )
{
   // NOTE: The signature for this function is very 
   // misleading... slotName is a StringTableEntry.

   // We don't check for changes on the client side.
   if ( isClientObject() )
      return;

   // Lookup and store the property names once here
   // and we can then just do pointer compares. 
   static StringTableEntry slotPosition   = StringTable->lookup( "position" );
   static StringTableEntry slotRotation   = StringTable->lookup( "rotation" );
   static StringTableEntry slotScale      = StringTable->lookup( "scale" );
   static StringTableEntry slotProfile    = StringTable->lookup( "profile" );
   static StringTableEntry slotFilename   = StringTable->lookup( "fileName" );
   static StringTableEntry slotVolume     = StringTable->lookup( "volume" );
   static StringTableEntry slotPitch      = StringTable->lookup( "pitch" );
   static StringTableEntry slotIsLooping  = StringTable->lookup( "isLooping" );
   static StringTableEntry slotIsStreaming= StringTable->lookup( "isStreaming" );
   static StringTableEntry slotIs3D       = StringTable->lookup( "is3D" );
   static StringTableEntry slotRefDist    = StringTable->lookup( "referenceDistance" );
   static StringTableEntry slotMaxDist    = StringTable->lookup( "maxDistance" );
   static StringTableEntry slotConeInAng  = StringTable->lookup( "coneInsideAngle" );
   static StringTableEntry slotConeOutAng = StringTable->lookup( "coneOutsideAngle" );
   static StringTableEntry slotConeOutVol = StringTable->lookup( "coneOutsideVolume" );
   static StringTableEntry slotFadeInTime = StringTable->lookup( "fadeInTime" );
   static StringTableEntry slotFadeOutTime= StringTable->lookup( "fadeOutTime" );

   // Set the dirty flags.
   mDirty.clear();
   if (  slotName == slotPosition ||
         slotName == slotRotation ||
         slotName == slotScale )
      mDirty.set( Transform );

   else if ( slotName == slotProfile )
      mDirty.set( Profile );

   else if ( slotName == slotFilename )
      mDirty.set( Filename );

   else if ( slotName == slotVolume )
      mDirty.set( Volume );
      
   else if( slotName == slotPitch )
      mDirty.set( Pitch );

   else if ( slotName == slotIsLooping )
      mDirty.set( IsLooping );
      
   else if ( slotName == slotIsStreaming )
      mDirty.set( IsStreaming );

   else if ( slotName == slotIs3D )
      mDirty.set( Is3D );

   else if ( slotName == slotRefDist )
      mDirty.set( ReferenceDistance );

   else if ( slotName == slotMaxDist )
      mDirty.set( MaxDistance );

   else if ( slotName == slotConeInAng )
      mDirty.set( ConeInsideAngle );

   else if ( slotName == slotConeOutAng )
      mDirty.set( ConeOutsideAngle );

   else if ( slotName == slotConeOutVol )
      mDirty.set( ConeOutsideVolume );
      
   else if( slotName == slotFadeInTime )
      mDirty.set( FadeInTime );
      
   else if( slotName == slotFadeOutTime )
      mDirty.set( FadeOutTime );

   if ( mDirty )
      setMaskBits( DirtyUpdateMask );
}

bool SFXEmitter::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   if ( isServerObject() )
   {
      // Validate the data we'll be passing across
      // the network to the client.
      mDescription.validate();
   }
   else
   {
      _update();

      // Do we need to start playback?
      if ( mPlayOnAdd && mSource )
         mSource->play();
   }

   // Setup the bounds.
   mObjBox.maxExtents = mObjScale;
   mObjBox.minExtents = mObjScale;
   mObjBox.minExtents.neg();
   resetWorldBox();
   addToScene();

   return true;
}

void SFXEmitter::onRemove()
{
   SFX_DELETE( mSource );

   removeFromScene();
   Parent::onRemove();
}

void SFXEmitter::_update()
{
   AssertFatal( isClientObject(), "SFXEmitter::_update() - This shouldn't happen on the server!" );

   // Store the playback status so we
   // we can restore it.
   SFXStatus prevState = mSource ? mSource->getStatus() : SFXStatusNull;

   // Make sure all the settings are valid.
   mDescription.validate();

   const MatrixF &transform   = getTransform();
   const VectorF &velocity    = getVelocity();

   // Did we change the source?
   if( mDirty.test( Profile | Filename | Is3D | IsLooping | IsStreaming | FadeInTime | FadeOutTime | Channel ) )
   {
      SFX_DELETE( mSource );

      // Do we have a profile?
      if( mProfile )
      {
         mSource = SFX->createSource( mProfile, &transform, &velocity );
         AssertFatal( mSource != NULL, "SFXEmitter::_update() - failed to create source!" );

         // If we're supposed to play when the emitter is 
         // added to the scene then also restart playback 
         // when the profile changes.
         prevState = mPlayOnAdd ? SFXStatusPlaying : prevState;
         
         // Force an update of properties set on the local description.
         
         mDirty.set( AllDirtyMask );
      }
      
      // Else take the local profile
      else
      {
         // Clear the resource and buffer to force a
         // reload if the filename changed.
         if( mDirty.test( Filename ) )
         {
            mLocalProfile.mResource = NULL;
            mLocalProfile.mBuffer = NULL;
         }

         if( !mLocalProfile.mFilename.isEmpty() )
         {
            mSource = SFX->createSource( &mLocalProfile, &transform, &velocity );
            AssertFatal( mSource != NULL, "SFXEmitter::_update() - failed to create source!" );
            
            prevState = mPlayOnAdd ? SFXStatusPlaying : prevState;
         }
      }
      
      mDirty.clear( Profile | Filename | Is3D | IsLooping | IsStreaming | FadeInTime | FadeOutTime | Channel );
   }

   // Cheat if the editor is open and the looping state
   // is toggled on a local profile sound.  It makes the
   // editor feel responsive and that things are working.
   if (  gEditingMission &&
         !mProfile && 
         mPlayOnAdd && 
         mDirty.test( IsLooping ) )
      prevState = SFXStatusPlaying;

   // The rest only applies if we have a source.
   if( mSource )
   {
      // Set the volume irrespective of the profile.
      if( mDirty.test( Volume ) )
         mSource->setVolume( mDescription.mVolume );
         
      if( mDirty.test( Pitch ) )
         mSource->setPitch( mDescription.mPitch );

      // Skip these 3d only settings.
      if( mDescription.mIs3D )
      {
         if( mDirty.test( Transform ) )
         {
            mSource->setTransform( transform );
            mSource->setVelocity( velocity );
         }
         
         if( mDirty.test( ReferenceDistance | MaxDistance ) )
         {
            mSource->setMinMaxDistance(   mDescription.mReferenceDistance,
                                          mDescription.mMaxDistance );
         }

         if( mDirty.test( ConeInsideAngle | ConeOutsideAngle | ConeOutsideVolume ) )
         {
            mSource->setCone( F32( mDescription.mConeInsideAngle ),
                              F32( mDescription.mConeOutsideAngle ),
                              mDescription.mConeOutsideVolume );
         }
         
         mDirty.clear( Transform | ReferenceDistance | MaxDistance | ConeInsideAngle | ConeOutsideAngle | ConeOutsideVolume );
      }     

      // Restore the pre-update playback state.
      if ( prevState == SFXStatusPlaying )
         mSource->play();
         
      mDirty.clear( Volume | Pitch | Transform );
   }
}

void SFXEmitter::play()
{
   if ( mSource )
      mSource->play();
   else
   {
      // By clearing the playback masks first we
      // ensure the last playback command called 
      // within a single tick is the one obeyed.
      clearMaskBits( AllSourceMasks );

      setMaskBits( SourcePlayMask );
   }
}

void SFXEmitter::stop()
{
   if ( mSource )
      mSource->stop();
   else
   {
      // By clearing the playback masks first we
      // ensure the last playback command called 
      // within a single tick is the one obeyed.
      clearMaskBits( AllSourceMasks );

      setMaskBits( SourceStopMask );
   }
}

SFXStatus SFXEmitter::getPlaybackStatus() const
{
   const SFXEmitter* emitter = this;

   // We only have a source playing on client objects, so if this is a server
   // object, we want to know the playback status on the local client connection's
   // version of this emitter.
   
   if( isServerObject() )
   {
      S32 index = NetConnection::getLocalClientConnection()->getGhostIndex( ( NetObject* ) this );
      if( index != -1 )
         emitter = dynamic_cast< SFXEmitter* >( NetConnection::getConnectionToServer()->resolveGhost( index ) );
      else
         emitter = NULL;
   }
   
   if( emitter && emitter->mSource )
      return emitter->mSource->getStatus();
      
   return SFXStatusNull;
}

bool SFXEmitter::isInRange() const
{
   if( !is3D() )
      return true;
      
   SFXListener& listener = SFX->getListener();
   
   const Point3F listenerPos = listener.getTransform().getPosition();
   const Point3F emitterPos = getPosition();
   const F32 dist = getSFXDescription().mMaxDistance;
   
   return ( ( emitterPos - listenerPos ).len() <= dist );
}

void SFXEmitter::setTransform( const MatrixF &mat )
{
   // Set the transform directly from the 
   // matrix created by inspector.
   Parent::setTransform( mat );
   setMaskBits( TransformUpdateMask );
}

void SFXEmitter::setScale( const VectorF &scale )
{
   // We ignore scale... it doesn't effect us.
}

ConsoleMethod( SFXEmitter, play, void, 2, 2,   
   "SFXEmitter.play()\n"
   "Sends network event to start playback if "
   "the emitter source is not already playing." )
{
   object->play();
}

ConsoleMethod( SFXEmitter, stop, void, 2, 2,   
   "SFXEmitter.stop()\n"
   "Sends network event to stop emitter "
   "playback on all ghosted clients." )
{
   object->stop();
}

ConsoleMethod( SFXEmitter, getPlaybackStatus, const char*, 2, 2, "() - Return the playback status of the emitter's sound." )
{
   return SFXStatusToString( object->getPlaybackStatus() );
} 

ConsoleMethod( SFXEmitter, isInRange, bool, 2, 2, "( vector pos ) - Return true if the emitter is currently in range of the listener." )
{
   return object->isInRange();
}
