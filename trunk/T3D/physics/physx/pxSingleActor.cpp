//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxSingleActor.h"

#include "lighting/lightManager.h"
#include "math/mathIO.h"
#include "ts/tsShapeInstance.h"
#include "console/consoleTypes.h"
#include "sim/netConnection.h"
#include "gfx/gfxTransformSaver.h"
#include "collision/collision.h"
#include "collision/abstractPolyList.h"
#include "T3D/objectTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "core/stream/fileStream.h"
#include "core/stream/bitStream.h"
#include "core/resourceManager.h"
#include "T3D/containerQuery.h"
#include "T3D/physics/physX/px.h"
#include "T3D/physics/physX/pxWorld.h"
#include "T3D/physics/physX/pxUserData.h"
#include "T3D/physics/physX/pxCasts.h"
#include "T3D/physics/physicsPlugin.h"

#include <NXU_helper.h>
#include <nxu_schema.h>
#include <NXU_customcopy.h>


class	PxSingleActor_Notify : public NXU_userNotify
{
protected:

   NxActor *mActor;

   bool mFirstActor;

   const Point3F& mScale;


public:

   void	NXU_notifyActor(NxActor	*actor,	const	char *userProperties)
   {
      if ( mFirstActor )
      {      
         mActor = actor;
         mFirstActor = false;
      }
   };

   bool	NXU_preNotifyActor(NxActorDesc &actor, const char	*userProperties) 
   { 
      // PxSingleActor only cares about one actor,
      // so skip work on anything after the first.
      if ( !mFirstActor )
         return false;
     
      // For every shape, cast to its particular type
      // and apply the scale to size, mass and localPosition.
      for( S32 i = 0; i < actor.shapes.size(); i++ )
      {
         switch( actor.shapes[i]->getType() )
         {
            case NX_SHAPE_BOX:
            {
               NxBoxShapeDesc *boxDesc = (NxBoxShapeDesc*)actor.shapes[i];

               if ( boxDesc->mass > 0.0f )
                  boxDesc->mass *= mScale.x;
               
               boxDesc->dimensions.x *= mScale.x;
               boxDesc->dimensions.y *= mScale.y;
               boxDesc->dimensions.z *= mScale.z;

               boxDesc->localPose.t.x *= mScale.x;
               boxDesc->localPose.t.y *= mScale.y;
               boxDesc->localPose.t.z *= mScale.z;
               break;
            }
            
            case NX_SHAPE_SPHERE:
            {
               NxSphereShapeDesc *sphereDesc = (NxSphereShapeDesc*)actor.shapes[i];
               
               if ( sphereDesc->mass > 0.0f )
                  sphereDesc->mass *= mScale.x;
               
               sphereDesc->radius *= mScale.x;
               
               sphereDesc->localPose.t.x *= mScale.x;
               sphereDesc->localPose.t.y *= mScale.y;
               sphereDesc->localPose.t.z *= mScale.z;
               break;
            }

            case NX_SHAPE_CAPSULE:
            {
               NxCapsuleShapeDesc *capsuleDesc = (NxCapsuleShapeDesc*)actor.shapes[i];
               if ( capsuleDesc->mass > 0.0f )
                  capsuleDesc->mass *= mScale.x;
               capsuleDesc->radius *= mScale.x;
               capsuleDesc->height *= mScale.y;
               
               capsuleDesc->localPose.t.x *= mScale.x;
               capsuleDesc->localPose.t.y *= mScale.y;
               capsuleDesc->localPose.t.z *= mScale.z;
               break;
            }

            default:
            {
               delete actor.shapes[i];
               actor.shapes.erase( actor.shapes.begin() + i );
               --i;
               break;
            }
         }
      }

      NxBodyDesc *body = const_cast<NxBodyDesc*>( actor.body );
      body->mass *= mScale.x;
      body->massLocalPose.t.multiply( mScale.x, body->massLocalPose.t );
      body->massSpaceInertia.multiply( mScale.x, body->massSpaceInertia );

      return	true; 
   };

public:

   PxSingleActor_Notify( const Point3F& scale )
      :  mScale( scale ),
         mFirstActor( true )
   {
   }

   virtual ~PxSingleActor_Notify()
   {
   }

   NxActor* getActor() const { return mActor; };

};

IMPLEMENT_CO_DATABLOCK_V1(PxSingleActorData);

PxSingleActorData::PxSingleActorData()
{
   shapeName = "";
   physXStream = "";
   waterDragScale = 1.0f;
   forceThreshold = 1.0f;
   clientOnly = false;

   clientOnly = false;

   physicsCollection = NULL;
}

PxSingleActorData::~PxSingleActorData()
{
   if ( physicsCollection )
      NXU::releaseCollection( physicsCollection );
}

void PxSingleActorData::initPersistFields()
{
   Parent::initPersistFields();


   addGroup("Media");
      addField( "shapeName", TypeFilename, Offset( shapeName, PxSingleActorData ) );
   endGroup("Media");

   // PhysX collision properties.
   addGroup( "Physics" );
      addField( "physXStream", TypeFilename, Offset( physXStream, PxSingleActorData ) );
      addField( "waterDragScale", TypeF32, Offset( waterDragScale, PxSingleActorData ) );
      addField( "buoyancyDensity", TypeF32, Offset( buoyancyDensity, PxSingleActorData ) );
      addField( "forceThreshold", TypeF32, Offset( forceThreshold, PxSingleActorData ) );
   endGroup( "Physics" );   

   addField( "clientOnly", TypeBool, Offset( clientOnly, PxSingleActorData ) );
}

//----------------------------------------------------------------------------

void PxSingleActorData::packData(BitStream* stream)
{ 
   Parent::packData(stream);

   stream->writeString( shapeName );
   stream->writeString( physXStream );
   stream->write( forceThreshold );
   stream->write( waterDragScale );
   stream->writeFlag( clientOnly );
}

//----------------------------------------------------------------------------

void PxSingleActorData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

   shapeName = stream->readSTString();
   physXStream = stream->readSTString();
   stream->read( &forceThreshold );
   stream->read( &waterDragScale );
   clientOnly = stream->readFlag();
}

bool PxSingleActorData::preload( bool server, String &errorBuffer )
{
   if ( !Parent::preload( server, errorBuffer ) )
      return false;

   // If the stream is null, exit.
   if ( !physXStream || !physXStream[0] )
   {
      errorBuffer = "PxSingleActorData::preload: physXStream is unset!";
      return false;
   }

   // Set up our buffer for the binary stream filename path.
   UTF8 binPhysXStream[260] = { 0 };
   const UTF8* ext = dStrrchr( physXStream, '.' );

   // Copy the xml stream path except for the extension.
   if ( ext )
      dStrncpy( binPhysXStream, physXStream, getMin( 260, ext - physXStream ) );
   else
      dStrncpy( binPhysXStream, physXStream, 260 );

   // Concatenate the binary extension.
   dStrcat( binPhysXStream, ".nxb" );

   // Get the modified times of the two files.
   FileTime xmlTime = {0}, binTime = {0};
   Platform::getFileTimes( physXStream, NULL, &xmlTime );
   Platform::getFileTimes( binPhysXStream, NULL, &binTime );

   // If the binary is newer... load that.
   if ( Platform::compareFileTimes( binTime, xmlTime ) >= 0 )
      _loadCollection( binPhysXStream, true );

   // If the binary failed... then load the xml.
   if ( !physicsCollection )
   {
      _loadCollection( physXStream, false );

      // If loaded... resave the xml in binary format
      // for quicker subsequent loads.
      if ( physicsCollection )
         NXU::saveCollection( physicsCollection, binPhysXStream, NXU::FT_BINARY );
   }

   // If it still isn't loaded then we've failed!
   if ( !physicsCollection )
   {
      errorBuffer = String::ToString( "PxSingleActorData::preload: could not load '%s'!", physXStream );
      return false;
   }

   if (!shapeName || shapeName == '\0') 
   {
      errorBuffer = "PxSingleActorData::preload: no shape name!";
      return false;
   }

   // Now load the shape.
   shape = ResourceManager::get().load( shapeName );

   if (bool(shape) == false)
   {
      errorBuffer = String::ToString( "PxSingleActorData::preload: unable to load shape: %s", shapeName );
      return false;
   }

   return true;
}

bool PxSingleActorData::_loadCollection( const UTF8 *path, bool isBinary )
{
   if ( physicsCollection )
   {
      NXU::releaseCollection( physicsCollection );
      physicsCollection = NULL;
   }

   FileStream fs;
   if ( !fs.open( path, Torque::FS::File::Read ) )
      return false;

   // Load the data into memory.
   U32 size = fs.getStreamSize();
   FrameTemp<U8> buff( size );
   fs.read( size, buff );

   // If the stream didn't read anything, there's a problem.
   if ( size <= 0 )
      return false;

   // Ok... try to load it.
   physicsCollection = NXU::loadCollection(  path, 
                                             isBinary ? NXU::FT_BINARY : NXU::FT_XML, 
                                             buff, 
                                             size );

   return physicsCollection != NULL;
}

NxActor* PxSingleActorData::createActor( NxScene *scene, const NxMat34 *nxMat, const Point3F& scale )
{
   if ( !scene )
   {
      Con::errorf( "PxSingleActorData::createActor() - returned null NxScene" );
      return NULL;
   }

   PxSingleActor_Notify pxNotify( scale );

   NXU::instantiateCollection( physicsCollection, *gPhysicsSDK, scene, nxMat, &pxNotify );

   NxActor *actor = pxNotify.getActor();

   actor->setGlobalPose( *nxMat );

   if ( !actor )
   {
      Con::errorf( "PxSingleActorData::createActor() - Could not create actor!" );
      return NULL;
   }

   return actor;
}

IMPLEMENT_CO_NETOBJECT_V1(PxSingleActor);

PxSingleActor::PxSingleActor()
   :  mShapeInstance( NULL ),
      mWorld( NULL ),
      mActor( NULL ),
      mSleepingLastTick( false ),
      mStartImpulse( Point3F::Zero ),
      mResetPos( MatrixF::Identity ),
      mBuildScale( Point3F::Zero ),
      mBuildLinDrag( 0.0f ),
      mBuildAngDrag( 0.0f )
{
   mNetFlags.set( Ghostable | ScopeAlways );
   mTypeMask |= StaticObjectType | StaticTSObjectType | StaticRenderedObjectType | ShadowCasterObjectType;

   overrideOptions = false;

   mUserData.setObject( this );
   mUserData.getContactSignal().notify( this, &PxSingleActor::_onContact );
}

void PxSingleActor::initPersistFields()
{
   Parent::initPersistFields();

   addGroup("Lighting");
      addField("receiveSunLight", TypeBool, Offset(receiveSunLight, PxSingleActor));
      addField("receiveLMLighting", TypeBool, Offset(receiveLMLighting, PxSingleActor));
      //addField("useAdaptiveSelfIllumination", TypeBool, Offset(useAdaptiveSelfIllumination, TSStatic));
      addField("useCustomAmbientLighting", TypeBool, Offset(useCustomAmbientLighting, PxSingleActor));
      //addField("customAmbientSelfIllumination", TypeBool, Offset(customAmbientForSelfIllumination, TSStatic));
      addField("customAmbientLighting", TypeColorF, Offset(customAmbientLighting, PxSingleActor));
      addField("lightGroupName", TypeRealString, Offset(lightGroupName, PxSingleActor));
   endGroup("Lighting");

   //addGroup("Collision");
   //endGroup("Collision");
}

bool PxSingleActor::onAdd()
{
   PROFILE_SCOPE(PxSingleActor_onAdd);
   
   // onNewDatablock for the server is called here
   // for the client it is called in unpackUpdate
   if ( !Parent::onAdd() )
      return false;

   // JCFNOTE: we definitely have a datablock here because GameBase::onAdd checks that           

   mNextPos = mLastPos = getPosition();
   mNextRot = mLastRot = getTransform();
   mResetPos = getTransform();
   
   //Con::printf( "Set reset position to %g, %g, %g", mResetPos.getPosition().x, mResetPos.getPosition().y, mResetPos.getPosition().z );
   if ( !mStartImpulse.isZero() )
      applyImpulse( mStartImpulse );

   PhysicsPlugin::getPhysicsResetSignal().notify( this, &PxSingleActor::onPhysicsReset, 1051.0f );   

   addToScene();       

   return true;
}

bool PxSingleActor::onNewDataBlock( GameBaseData *dptr )
{
   // Since onNewDataBlock is actually called before onAdd for client objects
   // we need to initialize this here.
   mWorld = dynamic_cast<PxWorld*>( gPhysicsPlugin->getWorld( isServerObject() ? "server" : "client" ) );
   if ( !mWorld )
      return false;

   mDataBlock = dynamic_cast<PxSingleActorData*>(dptr);

   if ( !mDataBlock || !Parent::onNewDataBlock( dptr ) )
      return false;

   if ( isClientObject() )
   {
      if ( mShapeInstance )   
         SAFE_DELETE(mShapeInstance);      
      mShapeInstance = new TSShapeInstance( mDataBlock->shape, isClientObject() );
   }

   mObjBox = mDataBlock->shape->bounds;
   resetWorldBox();

   // Create the actor.
   _createActor(); 

   // Must be called by the leaf class (of GameBase) once everything is loaded.
   scriptOnNewDataBlock();

   return true;
}

/*
void PxSingleActor::processMessage(const String & msgName, SimObjectMessageData * data)
{
   Parent::processMessage(msgName,data);

   if ( !isServerObject() )
      return;

   static const String sEditorEnable( "EditorEnable" );
   static const String sEditorDisable( "EditorDisable" );
   static const String sInspectPostApply( "InspectPostApply" );

   if ( msgName == sInspectPostApply )
   {
      setMaskBits( LightMask );

      bool forceSleep = mForceSleep;
      mForceSleep = !mForceSleep;
      setForceSleep( forceSleep );
   }
   else if ( msgName == sEditorEnable )
   {
      mForceSleep = false;
      setForceSleep( true );
      applyWarp( mResetPos, true, false );
   }
   else if ( msgName == sEditorDisable )
   {
      mForceSleep = true;
      setForceSleep( false );
      mResetPos = getTransform();
   }
}
*/

void PxSingleActor::onRemove()
{
   removeFromScene();

   delete mShapeInstance;
   mShapeInstance = NULL;

   if ( mActor )
      mWorld->releaseActor( *mActor );

   PhysicsPlugin::getPhysicsResetSignal().remove( this, &PxSingleActor::onPhysicsReset );

   Parent::onRemove();
}

bool PxSingleActor::prepRenderImage(   SceneState *state,
                                          const U32 stateKey,
                                          const U32 startZone, 
                                          const bool modifyBaseState )
{
   if ( !mShapeInstance || !state->isObjectRendered(this) )
      return false;

   Point3F cameraOffset;
   getTransform().getColumn(3,&cameraOffset);
   cameraOffset -= state->getDiffuseCameraPosition();
   F32 dist = cameraOffset.len();
   if ( dist < 0.01f )
      dist = 0.01f;

   F32 invScale = (1.0f/getMax(getMax(getScale().x,getScale().y),getScale().z));
   dist *= invScale;
   S32 dl = mShapeInstance->setDetailFromDistance( state, dist );
   if (dl<0)
      return false;

   renderObject( state );

   return false;
}

void PxSingleActor::renderObject(SceneState* state)
{
   GFXTransformSaver saver;

   // Set up our TS render state here.
   TSRenderState rdata;
   rdata.setSceneState( state );
   //rdata.setObjScale( &getScale() );

   LightManager *lm = gClientSceneGraph->getLightManager();
   if ( !state->isShadowPass() )
      lm->setupLights( this, getWorldSphere() );

   MatrixF mat = getTransform();
   mat.scale( getScale() );
   GFX->setWorldMatrix( mat );

   mShapeInstance->animate();
   mShapeInstance->render( rdata );
   
   lm->resetLights();  
}

U32 PxSingleActor::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   /*
   if ( stream->writeFlag( mask & ForceSleepMask ) )
      stream->writeFlag( mForceSleep );
   

   stream->writeFlag( mask & SleepMask );
   */   
   if ( stream->writeFlag( mask & WarpMask ) )
   {
      stream->writeAffineTransform( getTransform() );
   }

   if ( stream->writeFlag( mask & MoveMask ) )
   {
      stream->writeAffineTransform( getTransform() );

      if ( stream->writeFlag( mActor ) )
      {
         const NxVec3& linVel = mActor->getLinearVelocity();
         stream->write( linVel.x );
         stream->write( linVel.y );
         stream->write( linVel.z );

         const NxVec3& angVel = mActor->getAngularVelocity();
         stream->write( angVel.x );
         stream->write( angVel.y );
         stream->write( angVel.z );
      }
   }

   if ( stream->writeFlag( mask & ImpulseMask ) )
   {
         NxVec3 linVel( 0, 0, 0 );
         if ( mActor )
            linVel = mActor->getLinearVelocity();

         stream->write( linVel.x );
         stream->write( linVel.y );
         stream->write( linVel.z );
   }

   // This internally uses the mask passed to it.
   if ( mLightPlugin )
      retMask |= mLightPlugin->packUpdate( this, LightMask, con, mask, stream );

   return retMask;
}


void PxSingleActor::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);
   
   /*
   if ( stream->readFlag() ) // ForceSleepMask
   {
      bool forceSleep = stream->readFlag();

      if ( isProperlyAdded() )
         setForceSleep( forceSleep );
      else
         mForceSleep = forceSleep;
   }


   if ( stream->readFlag() ) // SleepMask
   {
      if ( mActor )
         mActor->putToSleep();

      // TODO: The sleep should start packing its own
      // position warp data and not rely on the WarpMask
      // below.  This will allow us to place the actor
      // in a way that is less prone to explosions.
   }
   */
   if ( stream->readFlag() ) // WarpMask
   {
      // If we set a warp mask,
      // we need to instantly move
      // the actor to the new position
      // without applying any corrections.
      MatrixF mat;
      stream->readAffineTransform( &mat );
      
      applyWarp( mat, true, false );
   }  

   if ( stream->readFlag() ) // MoveMask
   {
      MatrixF mat;
      stream->readAffineTransform( &mat );

      NxVec3 linVel, angVel;
      if ( stream->readFlag() )
      {
         stream->read( &linVel.x );
         stream->read( &linVel.y );
         stream->read( &linVel.z );

         stream->read( &angVel.x );
         stream->read( &angVel.y );
         stream->read( &angVel.z );

         if ( !mDataBlock->clientOnly )
            applyCorrection( mat, linVel, angVel );
      }
   }

   if ( stream->readFlag() ) // ImpulseMask
   {
      NxVec3 linVel;
      stream->read( &linVel.x );
      stream->read( &linVel.y );
      stream->read( &linVel.z );

      if ( mActor )
      {
         mWorld->releaseWriteLock();
         mActor->setLinearVelocity( linVel );
         mStartImpulse.zero();
      }
      else
         mStartImpulse.set( linVel.x, linVel.y, linVel.z );
   }

   if ( mLightPlugin )
      mLightPlugin->unpackUpdate( this, con, stream );
}

void PxSingleActor::setScale( const VectorF& scale )
{
   // This is so that the level
   // designer can change the scale
   // of a PxSingleActor in the editor
   // and have the PhysX representation updated properly.

   // First we call the parent's setScale
   // so that the ScaleMask can be set.
   Parent::setScale( scale );

   if ( !mActor || mBuildScale.equal( scale ) )
      return;

   _createActor();
}

void PxSingleActor::applyWarp( const MatrixF& mat, bool interpRender, bool sweep )
{
   MatrixF newMat( mat );

   if ( mActor )
   {
      mWorld->releaseWriteLock();

      mActor->wakeUp();    
      
      NxMat34 nxMat;
      nxMat.setRowMajor44( newMat );

      {
         mActor->setAngularVelocity( NxVec3( 0.0f ) );
         mActor->setLinearVelocity( NxVec3( 0.0f ) );
         mActor->setGlobalOrientation( nxMat.M );
      }

      /*
      if ( sweep )
      {
         mResetPos = mat;
         sweepTest( &newMat );
      }
      */

      nxMat.setRowMajor44( newMat );

      mActor->setGlobalPose( nxMat );
   }

   Parent::setTransform( newMat );

   mNextPos = newMat.getPosition();
   mNextRot = newMat;

   if ( !interpRender )
   {
      mLastPos = mNextPos;
      mLastRot = mNextRot;
   }
}

void PxSingleActor::setTransform( const MatrixF& mat )
{
   applyWarp( mat, false, true );
   setMaskBits( WarpMask );
}

F32 PxSingleActor::getMass() const
{
   if ( !mActor )
      return 0.0f;

    return mActor->getMass();
}

Point3F PxSingleActor::getVelocity() const
{
   if ( !mActor )
      return Point3F::Zero;

   return pxCast<Point3F>(mActor->getLinearVelocity());
}


void PxSingleActor::processTick( const Move *move )
{
   PROFILE_SCOPE( PxSingleActor_ProcessTick );

   if ( !mActor )
      return;

   // Set the last pos/rot to the
   // values of the previous tick for interpolateTick.
   mLastPos = mNextPos;
   mLastRot = mNextRot;

   if ( mActor->isSleeping() || mActor->readBodyFlag( NX_BF_FROZEN ) )
   {
      /*
      if ( !mSleepingLastTick )
      {
         // TODO: We cannot use a warp when we sleep.  The warp
         // can cause interpenetration with other actors which
         // then cause explosions on the next simulation tick.
         //
         // Instead lets have the sleep mask itself send the sleep
         // position and let it set the actor in a safe manner
         // that is specific to sleeping.

         setMaskBits( WarpMask | SleepMask );
      }
      */
      mSleepingLastTick = true;
      
      return;
   }

   mSleepingLastTick = false;

   // Container buoyancy & drag
   _updateContainerForces();      

   // Set the Torque transform to the dynamic actor's transform.
   MatrixF mat;
   mActor->getGlobalPose().getRowMajor44( mat );
   Parent::setTransform( mat );

   // Set the next pos/rot to the current values.
   mNextPos = mat.getPosition();
   mNextRot.set( mat );

   // TODO: We removed passing move updates to the client
   // on every tick because it was a waste of bandwidth.  The
   // client is correct or within limits the vast majority of 
   // the time.
   //
   // Still it can get out of sync.  To correct this when the
   // actor sleeps can cause large ugly jumps in position.  One
   // thought here was to hide the correction by waiting until
   // the object and its new position are offscreen.  Short of
   // that we have to apply corrections gradually over the time
   // of movement if we want corrections to work well.
   //
   // Lets consider some techniques for future improvement.
   //
   // First lets send updates to all non-sleeping actors in some
   // sort of round robin fashion, so that the bandwidth used is
   // limited by the number of actors moving.  If 1 actor is moving
   // it sends updates every tick... if 10 actors are moving each
   // actor gets an update every 10 ticks.  Lets say a really ugly
   // case... 100 moving actors... every actor is updated every 
   // 3.2 seconds... thats probably acceptable.
   //
   // Second lets also consider the idea of sending a velocity only 
   // correction more regularly.  If the actors are in the same start
   // position, a correct velocity would do as much good as a position.
   // Plus the linear velocity can me compressed really well.. less 
   // than 64 bits.  Maybe sending velocity regularly and positions
   // less regular would work?
   //
   // A more radical idea would be to have a special scene object that
   // manages all the packUpdates for all actors.  This object would
   // ensure that the right actors are updated and break up the updates
   // in a way to limit bandwidth usage.

   // Set the MoveMask so this will be updated to the client.
   //setMaskBits( MoveMask );
}

void PxSingleActor::interpolateTick( F32 delta )
{
   Point3F interpPos;
   QuatF interpRot;

   // Interpolate the position based on the delta.
   interpPos.interpolate( mNextPos, mLastPos, delta );

   // Interpolate the rotation based on the delta.
   interpRot.interpolate( mNextRot, mLastRot, delta );

   // Set up the interpolated transform.
   MatrixF interpMat;

   // Set the interpolated position and rotation.
   interpRot.setMatrix( &interpMat );
   interpMat.setPosition( interpPos );

   // Set the transform to the interpolated transform.
   Parent::setTransform( interpMat );
}

void PxSingleActor::sweepTest( MatrixF *mat )
{
   NxVec3 nxCurrPos = getPosition();

   // If the position is zero, 
   // the parent hasn't been updated yet
   // and we don't even need to do the sweep test.
   // This is a fix for a problem that was happening
   // where on the add of the PxSingleActor, it would
   // set the position to a very small value because it would be getting a hit
   // even though the current position was 0.
   if ( nxCurrPos.isZero() )
      return;
   
   // Set up the flags and the query structure.
   NxU32 flags = NX_SF_STATICS | NX_SF_DYNAMICS;

   NxSweepQueryHit sweepResult;
   dMemset( &sweepResult, 0, sizeof( sweepResult ) );

   NxVec3 nxNewPos = mat->getPosition();

   // Get the velocity which will be our sweep direction and distance.
   NxVec3 nxDir = nxNewPos - nxCurrPos;
   if ( nxDir.isZero() )
      return;

   // Get the scene and do the sweep.
   mActor->wakeUp();
   mActor->linearSweep( nxDir, flags, NULL, 1, &sweepResult, NULL );

   if ( sweepResult.hitShape && sweepResult.t < nxDir.magnitude() )
   {
      nxDir.normalize();
      nxDir *= sweepResult.t;
      nxCurrPos += nxDir;

      mat->setPosition( Point3F( nxCurrPos.x, nxCurrPos.y, nxCurrPos.z ) );
   }
}

void PxSingleActor::applyCorrection( const MatrixF& mat, const NxVec3& linVel, const NxVec3& angVel )
{
   // Sometimes the actor hasn't been
   // created yet during the call from unpackUpdate.
   if ( !mActor || !mWorld )
      return;

   mWorld->releaseWriteLock();

   NxVec3 newPos = mat.getPosition();
   NxVec3 currPos = getPosition();

   NxVec3 offset = newPos - currPos;

   // If the difference isn't large enough,
   // just set the new transform, no correction.
   if ( offset.magnitude() > 0.3f )
   {
      // If we're going to set the linear or angular velocity,
      // we do it before we add a corrective force, since it would be overwritten otherwise.
      NxVec3 currLinVel, currAngVel;
      currLinVel = mActor->getLinearVelocity();
      currAngVel = mActor->getAngularVelocity();

      // Scale the corrective force by half,
      // otherwise it will over correct and oscillate.
      NxVec3 massCent = mActor->getCMassGlobalPosition();
      mActor->addForceAtPos( offset, massCent, NX_VELOCITY_CHANGE );

      // If the linear velocity is divergent enough, change to server linear velocity.
      if ( (linVel - currLinVel).magnitude() > 0.3f )
         mActor->setLinearVelocity( linVel );
      // Same for angular.
      if ( (angVel - currAngVel).magnitude() > 0.3f )
         mActor->setAngularVelocity( angVel );   
   }

   Parent::setTransform( mat );
}

void PxSingleActor::applyImpulse( const VectorF& vel )
{
   if ( !mWorld || !mActor )
      return;

   mWorld->releaseWriteLock();

   NxVec3 linVel = mActor->getLinearVelocity();
   NxVec3 nxVel( vel.x, vel.y, vel.z );

   mActor->setLinearVelocity(linVel + nxVel);
   
   setMaskBits( ImpulseMask );
}

void PxSingleActor::onPhysicsReset( PhysicsResetEvent reset )
{
   if ( !mActor )
      return;

   // Store the reset transform for later use.
   if ( reset == PhysicsResetEvent_Store )
      mActor->getGlobalPose().getRowMajor44( mResetPos ); 
   else if ( reset == PhysicsResetEvent_Restore )
   {
      mActor->wakeUp();
      setTransform( mResetPos );
   }
}

void PxSingleActor::_updateContainerForces()
{   
   if ( !mWorld->getEnabled() )
      return;

   PROFILE_SCOPE( PxSingleActor_updateContainerForces );

   // Update container drag and buoyancy properties      

   ContainerQueryInfo info;
   info.box = getWorldBox();
   info.mass = getMass();

   // Find and retreive physics info from intersecting WaterObject(s)
   mContainer->findObjects( getWorldBox(), WaterObjectType|PhysicalZoneObjectType, findRouter, &info );
   
   // Calculate buoyancy and drag
   F32 angDrag = mBuildAngDrag;
   F32 linDrag = mBuildLinDrag;
   F32 buoyancy = 0.0f;

   if ( true ) //info.waterCoverage >= 0.1f) 
   {
      F32 waterDragScale = info.waterViscosity * mDataBlock->waterDragScale;
      F32 powCoverage = mPow( info.waterCoverage, 0.25f );

      if ( info.waterCoverage > 0.0f )
      {
         //angDrag = mBuildAngDrag * waterDragScale;
         //linDrag = mBuildLinDrag * waterDragScale;
         angDrag = mLerp( mBuildAngDrag, mBuildAngDrag * waterDragScale, powCoverage );
         linDrag = mLerp( mBuildLinDrag, mBuildLinDrag * waterDragScale, powCoverage );
      }

      buoyancy = ( info.waterDensity / mDataBlock->buoyancyDensity ) * mPow( info.waterCoverage, 2.0f );
   }

   // Apply drag (dampening)
   mActor->setLinearDamping( linDrag );
   mActor->setAngularDamping( angDrag );   

   // Apply buoyancy force
   if ( buoyancy != 0 )
   {     
      // A little hackery to prevent oscillation
      // Based on this blog post (http://reinot.blogspot.com/2005/11/oh-yes-they-float-georgie-they-all.html)
      // JCF: DISABLED
      NxVec3 gravity;
      mWorld->getScene()->getGravity(gravity);
      //NxVec3 velocity = mActor->getLinearVelocity();

      NxVec3 buoyancyForce = buoyancy * -gravity * TickSec;
      //F32 currHeight = getPosition().z;
      //const F32 C = 2.0f;
      //const F32 M = 0.1f;

      //if ( currHeight + velocity.z * TickSec * C > info.waterHeight )
      //   buoyancyForce *= M;

      mActor->addForceAtPos( buoyancyForce, mActor->getCMassGlobalPosition(), NX_IMPULSE );
   }

   // Apply physical zone forces
   if ( info.appliedForce.len() > 0.001f )
      mActor->addForceAtPos( pxCast<NxVec3>(info.appliedForce), mActor->getCMassGlobalPosition(), NX_IMPULSE );
}

void PxSingleActor::onCollision( GameBase *hitObject, const VectorF &impactForce )
{
   if ( isGhost() )  
      return;

   PROFILE_SCOPE( PxSingleActor_OnCollision );

   // TODO: Why do we not get a hit position?
   Point3F hitPos = getPosition();

   String strHitPos = String::ToString( "%g %g %g", hitPos.x, hitPos.y, hitPos.z );
   String strImpactVec = String::ToString( "%g %g %g", impactForce.x, impactForce.y, impactForce.z );
   String strImpactForce = String::ToString( "%g", impactForce.len() );
      
   Con::executef( mDataBlock, "onCollision", scriptThis(), 
      hitObject ? hitObject->scriptThis() : NULL, 
      strHitPos.c_str(), strImpactVec.c_str(), strImpactForce.c_str() );
}

void PxSingleActor::_onContact(  NxActor *ourActor, 
                                    NxActor *hitActor, 
                                    SceneObject *hitObject,
                                    const Point3F &hitPoint,
                                    const Point3F &impactForce )
{
   if ( isGhost() )
      return;

   String strHitPos = String::ToString( "%g %g %g", hitPoint.x, hitPoint.y, hitPoint.z );
   String strImpactVec = String::ToString( "%g %g %g", impactForce.x, impactForce.y, impactForce.z );
   String strImpactForce = String::ToString( "%g", impactForce.len() );
      
   Con::executef( mDataBlock, "onCollision", getIdString(), 
      hitObject ? hitObject->scriptThis() : "", 
      strHitPos.c_str(), strImpactVec.c_str(), strImpactForce.c_str() );
}

void PxSingleActor::_createActor()
{
   // NXU::instantiateCollection sometimes calls methods that need
   // to have write access.
   mWorld->releaseWriteLock();

   if ( mActor )
   {
      mWorld->releaseActor( *mActor );      
      mActor = NULL;
   }
   
   NxScene *scene = mWorld->getScene();

   NxMat34 nxMat;
   nxMat.setRowMajor44( getTransform() );

   Point3F scale = getScale();

   // Look for a zero scale in any component.
   if ( mIsZero( scale.least() ) )
      return;

   bool createActors = false;
   if ( !mDataBlock->clientOnly || (mDataBlock->clientOnly && isClientObject()) )
      createActors = true;

   mActor = createActors ? mDataBlock->createActor( scene, &nxMat, scale ) : NULL;

   if ( !mActor )
   {
      mBuildScale = getScale();
      mBuildAngDrag = 0;
      mBuildLinDrag = 0;
      return;
   }

   U32 group = mDataBlock->clientOnly ? PxGroup_ClientOnly : PxGroup_Default;
   mActor->setGroup( group );

   mActor->userData = &mUserData;

   mActor->setContactReportFlags( NX_NOTIFY_ON_START_TOUCH_FORCE_THRESHOLD | NX_NOTIFY_FORCES );
   mActor->setContactReportThreshold( mDataBlock->forceThreshold );

   mBuildScale = getScale();
   mBuildAngDrag = mActor->getAngularDamping();
   mBuildLinDrag = mActor->getLinearDamping();
}

ConsoleMethod( PxSingleActor, applyImpulse, void, 3, 3, "applyImpulse - takes a velocity vector to apply")
{
   VectorF vec;
   dSscanf( argv[2],"%g %g %g",
           &vec.x,&vec.y,&vec.z );
   
   object->applyImpulse( vec );
}

