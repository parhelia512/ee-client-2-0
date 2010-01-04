//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/examples/renderShapeExample.h"

#include "math/mathIO.h"
#include "sim/netConnection.h"
#include "sceneGraph/sceneState.h"
#include "console/consoleTypes.h"
#include "core/resourceManager.h"
#include "core/stream/bitStream.h"
#include "gfx/gfxTransformSaver.h"
#include "renderInstance/renderPassManager.h"

IMPLEMENT_CO_NETOBJECT_V1(RenderShapeExample);

//-----------------------------------------------------------------------------
// Object setup and teardown
//-----------------------------------------------------------------------------
RenderShapeExample::RenderShapeExample()
{
   // Flag this object so that it will always
   // be sent across the network to clients
   mNetFlags.set( Ghostable | ScopeAlways );

   // Set it as a "static" object that casts shadows
   mTypeMask |= StaticObjectType | ShadowCasterObjectType;

   // Make sure to initialize our TSShapeInstance to NULL
   mShapeInstance = NULL;
}

RenderShapeExample::~RenderShapeExample()
{
}

//-----------------------------------------------------------------------------
// Object Editing
//-----------------------------------------------------------------------------
void RenderShapeExample::initPersistFields()
{
   addGroup( "Rendering" );
   addField( "shapeFile",      TypeStringFilename, Offset( mShapeFile, RenderShapeExample ) );
   endGroup( "Rendering" );

   // SceneObject already handles exposing the transform
   Parent::initPersistFields();
}

void RenderShapeExample::inspectPostApply()
{
   Parent::inspectPostApply();

   // Flag the network mask to send the updates
   // to the client object
   setMaskBits( UpdateMask );
}

bool RenderShapeExample::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   // Set up a 1x1x1 bounding box
   mObjBox.set( Point3F( -0.5f, -0.5f, -0.5f ),
                Point3F(  0.5f,  0.5f,  0.5f ) );

   resetWorldBox();

   // Add this object to the scene
   addToScene();

   return true;
}

void RenderShapeExample::onRemove()
{
   // Remove this object from the scene
   removeFromScene();

   // Remove our TSShapeInstance
   if ( mShapeInstance )
      SAFE_DELETE( mShapeInstance );

   Parent::onRemove();
}

void RenderShapeExample::setTransform(const MatrixF & mat)
{
   // Let SceneObject handle all of the matrix manipulation
   Parent::setTransform( mat );

   // Dirty our network mask so that the new transform gets
   // transmitted to the client object
   setMaskBits( TransformMask );
}

U32 RenderShapeExample::packUpdate( NetConnection *conn, U32 mask, BitStream *stream )
{
   // Allow the Parent to get a crack at writing its info
   U32 retMask = Parent::packUpdate( conn, mask, stream );

   // Write our transform information
   if ( stream->writeFlag( mask & TransformMask ) )
   {
      mathWrite(*stream, getTransform());
      mathWrite(*stream, getScale());
   }

   // Write out any of the updated editable properties
   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      stream->write( mShapeFile );

      // Allow the server object a chance to handle a new shape
      createShape();
   }

   return retMask;
}

void RenderShapeExample::unpackUpdate(NetConnection *conn, BitStream *stream)
{
   // Let the Parent read any info it sent
   Parent::unpackUpdate(conn, stream);

   if ( stream->readFlag() )  // TransformMask
   {
      mathRead(*stream, &mObjToWorld);
      mathRead(*stream, &mObjScale);

      setTransform( mObjToWorld );
   }

   if ( stream->readFlag() )  // UpdateMask
   {
      stream->read( &mShapeFile );

      if ( isProperlyAdded() )
         createShape();
   }
}

//-----------------------------------------------------------------------------
// Object Rendering
//-----------------------------------------------------------------------------
void RenderShapeExample::createShape()
{
   if ( mShapeFile.isEmpty() )
      return;

   // If this is the same shape then no reason to update it
   if ( mShapeInstance && mShapeFile.equal( mShape.getPath().getFullPath(), String::NoCase ) )
      return;

   // Clean up our previous shape
   if ( mShapeInstance )
      SAFE_DELETE( mShapeInstance );
   mShape = NULL;

   // Attempt to get the resource from the ResourceManager
   mShape = ResourceManager::get().load( mShapeFile );

   if ( !mShape )
   {
      Con::errorf( "RenderShapeExample::createShape() - Unable to load shape: %s", mShapeFile.c_str() );
      return;
   }

   // Attempt to preload the Materials for this shape
   if ( isClientObject() && 
        !mShape->preloadMaterialList( mShape.getPath() ) && 
        NetConnection::filesWereDownloaded() )
   {
      mShape = NULL;
      return;
   }

   // Update the bounding box
   mObjBox = mShape->bounds;
   resetWorldBox();

   // Create the TSShapeInstance
   mShapeInstance = new TSShapeInstance( mShape, isClientObject() );
}

bool RenderShapeExample::prepRenderImage( SceneState *state, const U32 stateKey, 
                                         const U32 startZone, const bool modifyBaseZoneState)
{
   // Make sure we have a TSShapeInstance
   if ( !mShapeInstance )
      return false;

   // Make sure we haven't already been processed by this state
   if ( isLastState( state, stateKey ) )
      return false;

   // Update our state
   setLastState(state, stateKey);

   // If we are actually rendered then create and submit our RenderInst
   if ( state->isObjectRendered( this ) ) 
   {
      // Calculate the distance of this object from the camera
      Point3F cameraOffset;
      getRenderTransform().getColumn( 3, &cameraOffset );
      cameraOffset -= state->getDiffuseCameraPosition();
      F32 dist = cameraOffset.len();
      if ( dist < 0.01f )
         dist = 0.01f;

      // Set up the LOD for the shape
      F32 invScale = ( 1.0f / getMax( getMax( mObjScale.x, mObjScale.y ), mObjScale.z ) );

      mShapeInstance->setDetailFromDistance( state, dist * invScale );

      // Make sure we have a valid level of detail
      if ( mShapeInstance->getCurrentDetail() < 0 )
         return false;

      // GFXTransformSaver is a handy helper class that restores
      // the current GFX matrices to their original values when
      // it goes out of scope at the end of the function
      GFXTransformSaver saver;

      // Set up our TS render state      
      TSRenderState rdata;
      rdata.setSceneState( state );
      rdata.setFadeOverride( 1.0f );

      // Allow the light manager to set up any lights it needs
      LightManager* lm = NULL;
      if ( state->getSceneManager() )
      {
         lm = state->getSceneManager()->getLightManager();
         if ( lm && !state->isShadowPass() )
            lm->setupLights( this, getWorldSphere() );
      }

      // Set the world matrix to the objects render transform
      MatrixF mat = getRenderTransform();
      mat.scale( mObjScale );
      GFX->setWorldMatrix( mat );

      // Animate the the shape
      mShapeInstance->animate();

      // Allow the shape to submit the RenderInst(s) for itself
      mShapeInstance->render( rdata );

      // Give the light manager a chance to reset the lights
      if ( lm )
         lm->resetLights();
   }

   return false;
}