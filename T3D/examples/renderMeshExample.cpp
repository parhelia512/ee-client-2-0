//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/examples/renderMeshExample.h"

#include "math/mathIO.h"
#include "sceneGraph/sceneState.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "materials/materialManager.h"
#include "renderInstance/renderPassManager.h"

IMPLEMENT_CO_NETOBJECT_V1(RenderMeshExample);

//-----------------------------------------------------------------------------
// Object setup and teardown
//-----------------------------------------------------------------------------
RenderMeshExample::RenderMeshExample()
{
   // Flag this object so that it will always
   // be sent across the network to clients
   mNetFlags.set( Ghostable | ScopeAlways );

   // Set it as a "static" object that casts shadows
   mTypeMask |= StaticObjectType | ShadowCasterObjectType;

   // Make sure we the Material instance to NULL
   // so we don't try to access it incorrectly
   mMaterialInst = NULL;
}

RenderMeshExample::~RenderMeshExample()
{
   if ( mMaterialInst )
      SAFE_DELETE( mMaterialInst );
}

//-----------------------------------------------------------------------------
// Object Editing
//-----------------------------------------------------------------------------
void RenderMeshExample::initPersistFields()
{
   addGroup( "Rendering" );
   addField( "material",      TypeMaterialName, Offset( mMaterialName, RenderMeshExample ) );
   endGroup( "Rendering" );

   // SceneObject already handles exposing the transform
   Parent::initPersistFields();
}

void RenderMeshExample::inspectPostApply()
{
   Parent::inspectPostApply();

   // Flag the network mask to send the updates
   // to the client object
   setMaskBits( UpdateMask );
}

bool RenderMeshExample::onAdd()
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

void RenderMeshExample::onRemove()
{
   // Remove this object from the scene
   removeFromScene();

   Parent::onRemove();
}

void RenderMeshExample::setTransform(const MatrixF & mat)
{
   // Let SceneObject handle all of the matrix manipulation
   Parent::setTransform( mat );

   // Dirty our network mask so that the new transform gets
   // transmitted to the client object
   setMaskBits( TransformMask );
}

U32 RenderMeshExample::packUpdate( NetConnection *conn, U32 mask, BitStream *stream )
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
      stream->write( mMaterialName );

   return retMask;
}

void RenderMeshExample::unpackUpdate(NetConnection *conn, BitStream *stream)
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
      stream->read( &mMaterialName );

      if ( isProperlyAdded() )
         updateMaterial();
   }
}

//-----------------------------------------------------------------------------
// Object Rendering
//-----------------------------------------------------------------------------
void RenderMeshExample::createGeometry()
{
   static const Point3F cubePoints[8] = 
   {
      Point3F( 1, -1, -1), Point3F( 1, -1,  1), Point3F( 1,  1, -1), Point3F( 1,  1,  1),
      Point3F(-1, -1, -1), Point3F(-1,  1, -1), Point3F(-1, -1,  1), Point3F(-1,  1,  1)
   };

   static const Point3F cubeNormals[6] = 
   {
      Point3F( 1,  0,  0), Point3F(-1,  0,  0), Point3F( 0,  1,  0),
      Point3F( 0, -1,  0), Point3F( 0,  0,  1), Point3F( 0,  0, -1)
   };

   static const Point2F cubeTexCoords[4] = 
   {
      Point2F( 0,  0), Point2F( 0, -1),
      Point2F( 1,  0), Point2F( 1, -1)
   };

   static const U32 cubeFaces[36][3] = 
   {
      { 3, 0, 3 }, { 0, 0, 0 }, { 1, 0, 1 },
      { 2, 0, 2 }, { 0, 0, 0 }, { 3, 0, 3 },
      { 7, 1, 1 }, { 4, 1, 2 }, { 5, 1, 0 },
      { 6, 1, 3 }, { 4, 1, 2 }, { 7, 1, 1 },
      { 3, 2, 1 }, { 5, 2, 2 }, { 2, 2, 0 },
      { 7, 2, 3 }, { 5, 2, 2 }, { 3, 2, 1 },
      { 1, 3, 3 }, { 4, 3, 0 }, { 6, 3, 1 },
      { 0, 3, 2 }, { 4, 3, 0 }, { 1, 3, 3 },
      { 3, 4, 3 }, { 6, 4, 0 }, { 7, 4, 1 },
      { 1, 4, 2 }, { 6, 4, 0 }, { 3, 4, 3 },
      { 2, 5, 1 }, { 4, 5, 2 }, { 0, 5, 0 },
      { 5, 5, 3 }, { 4, 5, 2 }, { 2, 5, 1 }
   };

   // Fill the vertex buffer
   VertexType *pVert = NULL;

   mVertexBuffer.set( GFX, 36, GFXBufferTypeStatic );
   pVert = mVertexBuffer.lock();

   Point3F halfSize = getObjBox().getExtents() * 0.5f;

   for (U32 i = 0; i < 36; i++)
   {
      const U32& vdx = cubeFaces[i][0];
      const U32& ndx = cubeFaces[i][1];
      const U32& tdx = cubeFaces[i][2];

      pVert[i].point    = cubePoints[vdx] * halfSize;
      pVert[i].normal   = cubeNormals[ndx];
      pVert[i].texCoord = cubeTexCoords[tdx];
   }

   mVertexBuffer.unlock();

   // Fill the primitive buffer
   U16 *pIdx = NULL;

   mPrimitiveBuffer.set( GFX, 36, 12, GFXBufferTypeStatic );

   mPrimitiveBuffer.lock(&pIdx);     
   
   for (U16 i = 0; i < 36; i++)
      pIdx[i] = i;

   mPrimitiveBuffer.unlock();
}

void RenderMeshExample::updateMaterial()
{
   if ( mMaterialName.isEmpty() )
      return;

   // If the material name matches then don't bother updating it.
   if (  mMaterialInst && mMaterialName.equal( mMaterialInst->getMaterial()->getName(), String::NoCase ) )
      return;

   SAFE_DELETE( mMaterialInst );

   mMaterialInst = MATMGR->createMatInstance( mMaterialName, getGFXVertexFormat< VertexType >() );
   if ( !mMaterialInst )
      Con::errorf( "RenderMeshExample::updateMaterial - no Material called '%s'", mMaterialName.c_str() );
}

bool RenderMeshExample::prepRenderImage( SceneState *state, const U32 stateKey, 
                                         const U32 startZone, const bool modifyBaseZoneState)
{
   // Do a little prep work if needed
   if ( mVertexBuffer.isNull() )
      createGeometry();

   // Make sure we haven't already been processed by this state
   if ( isLastState( state, stateKey ) )
      return false;

   // Update our state
   setLastState(state, stateKey);

   // If we are actually rendered then create and submit our RenderInst
   if ( state->isObjectRendered( this ) ) 
   {
      // Get a handy pointer to our RenderPassmanager
      RenderPassManager *renderPass = state->getRenderPass();

      // Allocate an MeshRenderInst so that we can submit it to the RenderPassManager
      MeshRenderInst *ri = renderPass->allocInst<MeshRenderInst>();

      // Set our RenderInst as a standard mesh render
      ri->type = RenderPassManager::RIT_Mesh;

      // Calculate our sorting point
      if ( state )
      {
         // Calculate our sort point manually.
         const Box3F& rBox = getRenderWorldBox();
         ri->sortDistSq = rBox.getSqDistanceToPoint( state->getCameraPosition() );      
      } 
      else 
         ri->sortDistSq = 0.0f;

      // Set up our transforms
      MatrixF objectToWorld = getRenderTransform();
      objectToWorld.scale( getScale() );

      ri->objectToWorld = renderPass->allocUniqueXform( objectToWorld );
      ri->worldToCamera = renderPass->allocSharedXform(RenderPassManager::View);
      ri->projection    = renderPass->allocSharedXform(RenderPassManager::Projection);

      // Let the light manager fill the RIs light vector with the current best lights
      LightManager* lm = NULL;
      if ( state->getSceneManager() )
      {
         lm = state->getSceneManager()->getLightManager();
         if ( lm && !state->isShadowPass() )
         {
            lm->setupLights( this, getWorldSphere() );

            lm->getBestLights( ri->lights, 8 );
         }
      }

      // Make sure we have an up-to-date backbuffer in case
      // our Material would like to make use of it
      // NOTICE: SFXBB is removed and refraction is disabled!
      //ri->backBuffTex = GFX->getSfxBackBuffer();

      // Set our Material
      if ( mMaterialInst )
         ri->matInst = mMaterialInst;
      else
         ri->matInst = MATMGR->getWarningMatInstance();

      // Set up our vertex buffer and primitive buffer
      ri->vertBuff = &mVertexBuffer;
      ri->primBuff = &mPrimitiveBuffer;

      ri->prim = renderPass->allocPrim();
      ri->prim->type = GFXTriangleList;
      ri->prim->minIndex = 0;
      ri->prim->startIndex = 0;
      ri->prim->numPrimitives = 12;
      ri->prim->startVertex = 0;
      ri->prim->numVertices = 36;

      // We sort by the vertex buffer
      ri->defaultKey = (U32)ri->vertBuff; // Not 64bit safe!

      // Submit our RenderInst to the RenderPassManager
      state->getRenderPass()->addInst( ri );

      // Give the light manager a chance to reset the lights
      if ( lm )
         lm->resetLights();
   }

   return false;
}

ConsoleMethod( RenderMeshExample, postApply, void, 2, 2, "")
{
	object->inspectPostApply();
}