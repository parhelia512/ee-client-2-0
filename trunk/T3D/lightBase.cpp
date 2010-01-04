//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/lightBase.h"

#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "sim/netConnection.h"
#include "lighting/lightManager.h"
#include "sceneGraph/sceneState.h"
#include "renderInstance/renderPassManager.h"


bool LightBase::smRenderViz = false;

IMPLEMENT_CONOBJECT( LightBase );

LightBase::LightBase()
   :  mIsEnabled( true ),
      mColor( ColorF::WHITE ),
      mBrightness( 1.0f ),
      mCastShadows( false ),
      mPriority( 1.0f ),
      mAnimationData( NULL ),      
      mFlareData( NULL ),
      mFlareScale( 1.0f ),
      mAnimationPeriod( 1.0f ),
      mAnimationPhase( 1.0f )
{
   mNetFlags.set( Ghostable | ScopeAlways );
   mTypeMask = EnvironmentObjectType | LightObjectType;

   mLight = LightManager::createLightInfo();

   mFlareState.clear();
   mAnimState.clear();
   mAnimState.active = true;
}

LightBase::~LightBase()
{
   SAFE_DELETE( mLight );
}

void LightBase::initPersistFields()
{
   // We only add the basic lighting options that all lighting
   // systems would use... the specific lighting system options
   // are injected at runtime by the lighting system itself.

   addGroup( "Light" );
      
      addField( "isEnabled", TypeBool, Offset( mIsEnabled, LightBase ) );
      addField( "color", TypeColorF, Offset( mColor, LightBase ) );
      addField( "brightness", TypeF32, Offset( mBrightness, LightBase ) );      
      addField( "castShadows", TypeBool, Offset( mCastShadows, LightBase ) );
      addField( "priority", TypeF32, Offset( mPriority, LightBase ) );

   endGroup( "Light" );

   addGroup( "Light Animation" );

      addField( "animate", TypeBool, Offset( mAnimState.active, LightBase ) );
      addField( "animationType", TypeLightAnimDataPtr, Offset( mAnimationData, LightBase ) );
      addField( "animationPeriod", TypeF32, Offset( mAnimationPeriod, LightBase ) );
      addField( "animationPhase", TypeF32, Offset( mAnimationPhase, LightBase ) );      

   endGroup( "Light Animation" );

   addGroup( "Misc" );

      addField( "flareType", TypeLightFlareDataPtr, Offset( mFlareData, LightBase ) );
      addField( "flareScale", TypeF32, Offset( mFlareScale, LightBase ) );

   endGroup( "Misc" );

   // Now inject any light manager specific fields.
   LightManager::initLightFields();

   // We do the parent fields at the end so that
   // they show up that way in the inspector.
   Parent::initPersistFields();

   Con::addVariable( "$Light::renderViz", TypeBool, &smRenderViz );
}

bool LightBase::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   // Update the light parameters.
   _conformLights();
	addToScene();

   return true;
}

void LightBase::onRemove()
{
   removeFromScene();
   Parent::onRemove();
}

void LightBase::onDeleteNotify(SimObject* obj)
{
   Parent::onDeleteNotify(obj);
   if (obj == mMount.object)
      unmount();
}

void LightBase::submitLights( LightManager *lm, bool staticLighting )
{
   if ( !mIsEnabled || staticLighting )
      return;

   if ( mAnimState.active && mAnimationData )
   {
      mAnimState.fullBrightness = mBrightness;
      mAnimState.lightInfo = mLight;
      mAnimState.animationPeriod = mAnimationPeriod;
      mAnimState.animationPhase = mAnimationPhase;
      
      mAnimationData->animate( &mAnimState );
   }

   lm->registerGlobalLight( mLight, this );
}

void LightBase::inspectPostApply()
{
   // We do not call the parent here as it 
   // will call setScale() and screw up the 
   // real sizing fields on the light.

   _conformLights();
   setMaskBits( EnabledMask | UpdateMask | TransformMask | DatablockMask );
}

void LightBase::setTransform( const MatrixF &mat )
{
   setMaskBits( TransformMask );
   Parent::setTransform( mat );
}

bool LightBase::prepRenderImage( SceneState *state, const U32 stateKey, const U32, const bool )
{
   if ( isLastState( state, stateKey ) )
      return false;

   setLastState( state, stateKey );

   if ( !state->isObjectRendered( this ) )
      return false;
   
   if ( mIsEnabled && mFlareData )
   {
      mFlareState.fullBrightness = mBrightness;
      mFlareState.scale = mFlareScale;
      mFlareState.lightInfo = mLight;
      mFlareState.lightMat = getRenderTransform();

      mFlareData->prepRender( state, &mFlareState );
   }

   if ( !state->isDiffusePass() )
      return false;

   // If the light is selected or light visualization
   // is enabled then register the callback.
   if ( smRenderViz || isSelected() || ( getServerObject() && getServerObject()->isSelected() ) )
   {      
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &LightBase::_onRenderViz );
      ri->type = RenderPassManager::RIT_Object;
      state->getRenderPass()->addInst( ri );
   }

   return false;
}

void LightBase::_onRenderViz( ObjectRenderInst *ri, 
                              SceneState *state, 
                              BaseMatInstance *overrideMat )
{
   if ( !overrideMat )
      _renderViz( state );
}

void LightBase::onMount( SceneObject *obj, S32 node )
{
   deleteNotify(obj);

   if (!isGhost()) 
   {   
      setMaskBits(MountedMask);
   }
}

void LightBase::onUnmount( SceneObject *obj, S32 node )
{
   clearNotify(obj);

   if (!isGhost()) 
   {           
      setMaskBits(MountedMask);
   }
}

void LightBase::unmount()
{
   if (mMount.object)
      mMount.object->unmountObject(this);
}

void LightBase::interpolateTick( F32 delta )
{
}

void LightBase::processTick()
{
}

void LightBase::advanceTime( F32 timeDelta )
{
   if ( mMount.object )
   {
      MatrixF mat;
      mMount.object->getRenderMountTransform( mMount.node, &mat );
      mLight->setTransform( mat );
      Parent::setTransform( mat );
   }
}

U32 LightBase::packUpdate( NetConnection *conn, U32 mask, BitStream *stream )
{
   U32 retMask = Parent::packUpdate( conn, mask, stream );

   stream->writeFlag( mIsEnabled );

   if ( stream->writeFlag( mask & TransformMask ) )
      stream->writeAffineTransform( mObjToWorld );

   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      stream->write( mColor );
      stream->write( mBrightness );

      stream->writeFlag( mCastShadows );

      stream->write( mPriority );      

      mLight->packExtended( stream ); 

      stream->writeFlag( mAnimState.active );
      stream->write( mAnimationPeriod );
      stream->write( mAnimationPhase );
      stream->write( mFlareScale );
   }

   if ( stream->writeFlag( mask & DatablockMask ) )
   {
      if ( stream->writeFlag( mAnimationData ) )
      {
         stream->writeRangedU32( mAnimationData->getId(),
                                 DataBlockObjectIdFirst, 
                                 DataBlockObjectIdLast );
      }

      if ( stream->writeFlag( mFlareData ) )
      {
         stream->writeRangedU32( mFlareData->getId(),
                                 DataBlockObjectIdFirst, 
                                 DataBlockObjectIdLast );
      }
   }

   if (mask & MountedMask) 
   {
      if (mMount.object) 
      {
         S32 gIndex = conn->getGhostIndex(mMount.object);
         if (stream->writeFlag(gIndex != -1)) 
         {
            stream->writeFlag(true);
            stream->writeInt(gIndex,NetConnection::GhostIdBitSize);
            stream->write(mMount.node);
         }
         else
            // Will have to try again later
            retMask |= MountedMask;
      }
      else
         // Unmount if this isn't the initial packet
         if (stream->writeFlag(!(mask & InitialUpdateMask)))
            stream->writeFlag(false);
   }
   else
      stream->writeFlag(false);

   return retMask;
}

void LightBase::unpackUpdate( NetConnection *conn, BitStream *stream )
{
   Parent::unpackUpdate( conn, stream );

   mIsEnabled = stream->readFlag();

   if ( stream->readFlag() ) // TransformMask
      stream->readAffineTransform( &mObjToWorld );

   if ( stream->readFlag() ) // UpdateMask
   {   
      stream->read( &mColor );
      stream->read( &mBrightness );      
      mCastShadows = stream->readFlag();

      stream->read( &mPriority );      
      
      mLight->unpackExtended( stream );

      mAnimState.active = stream->readFlag();
      stream->read( &mAnimationPeriod );
      stream->read( &mAnimationPhase );
      stream->read( &mFlareScale );
   }

   if ( stream->readFlag() ) // DatablockMask
   {
      if ( stream->readFlag() )
      {
         SimObjectId id = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );  
         LightAnimData *datablock = NULL;
         
         if ( Sim::findObject( id, datablock ) )
            mAnimationData = datablock;
         else
         {
            conn->setLastError( "Light::unpackUpdate() - invalid LightAnimData!" );
            mAnimationData = NULL;
         }
      }
      else
         mAnimationData = NULL;

      if ( stream->readFlag() )
      {
         SimObjectId id = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );  
         LightFlareData *datablock = NULL;

         if ( Sim::findObject( id, datablock ) )
            mFlareData = datablock;
         else
         {
            conn->setLastError( "Light::unpackUpdate() - invalid LightCoronaData!" );
            mFlareData = NULL;
         }
      }
      else
         mFlareData = NULL;
   }

   if ( stream->readFlag() ) 
   {
      if ( stream->readFlag() ) 
      {
         S32 gIndex = stream->readInt(NetConnection::GhostIdBitSize);
         SceneObject* obj = dynamic_cast<SceneObject*>(conn->resolveGhost(gIndex));
         S32 node;
         stream->read(&node);
         if(!obj)
         {
            conn->setLastError("Invalid packet from server.");
            return;
         }
         obj->mountObject(this,node);
      }
      else
         unmount();
   }
   
   if ( isProperlyAdded() )
      _conformLights();
}

void LightBase::setLightEnabled( bool enabled )
{
   if ( mIsEnabled != enabled )
   {
      mIsEnabled = enabled;
      setMaskBits( EnabledMask );
   }
}

ConsoleMethod( LightBase, setLightEnabled, void, 3, 3, "( bool enabled )\t"
   "Toggles the light on and off." )
{
   object->setLightEnabled( dAtob( argv[2] ) );
}

ConsoleMethod( LightBase, playAnimation, void, 2, 3, "( [LightAnimData anim] )\t"
   "Plays a light animation on the light.  If no LightAnimData is passed the "
   "existing one is played." )
{
    if ( argc == 2 )
    {
        object->playAnimation();
        return;
    }

    LightAnimData *animData;
    if ( !Sim::findObject( argv[2], animData ) )
    {
        Con::errorf( "LightBase::playAnimation() - Invalid LightAnimData '%s'.", argv[2] );
        return;
    }

    // Play Animation.
    object->playAnimation( animData );
}

void LightBase::playAnimation( void )
{
    if ( !mAnimState.active )
    {
        mAnimState.active = true;
        setMaskBits( UpdateMask );
    }
}

void LightBase::playAnimation( LightAnimData *animData )
{
    // Play Animation.
    playAnimation();

    // Update Datablock?
    if ( mAnimationData != animData )
    {
        mAnimationData = animData;
        setMaskBits( DatablockMask );
    }
}

ConsoleMethod( LightBase, pauseAnimation, void, 2, 2, "Stops the light animation." )
{
    object->pauseAnimation();
}

void LightBase::pauseAnimation( void )
{
    if ( mAnimState.active )
    {
        mAnimState.active = false;
        setMaskBits( UpdateMask );
    }
}