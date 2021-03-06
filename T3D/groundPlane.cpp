//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/groundPlane.h"
#include "renderInstance/renderPassManager.h"
#include "sceneGraph/sceneState.h"
#include "materials/sceneData.h"
#include "materials/materialDefinition.h"
#include "materials/materialManager.h"
#include "math/util/frustum.h"
#include "math/mPlane.h"
#include "math/mathIO.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "collision/boxConvex.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsStatic.h"


/// Minimum square size allowed.  This is a cheap way to limit the amount
/// of geometry possibly generated by the GroundPlane (vertex buffers have a
/// limit, too).  Dynamically clipping extents into range is a problem since the
/// location of the horizon depends on the camera orientation.  Just shifting
/// squareSize as needed also doesn't work as that causes different geometry to
/// be generated depending on the viewpoint and orientation which affects the
/// texturing.
static const F32 sMIN_SQUARE_SIZE = 16;


IMPLEMENT_CO_NETOBJECT_V1( GroundPlane );

GroundPlane::GroundPlane()
   : mSquareSize( 128.0f ),
     mScaleU( 1.0f ),
     mScaleV( 1.0f ),
     mMaterial( NULL ),
     mMin( 0.0f, 0.0f ),
     mMax( 0.0f, 0.0f ),
     mPhysicsRep( NULL )
{
   mTypeMask |= StaticObjectType | StaticRenderedObjectType | StaticShapeObjectType;
   mNetFlags.set( Ghostable | ScopeAlways );

   mConvexList = new Convex;
}

GroundPlane::~GroundPlane()
{
   if( mMaterial )
      SAFE_DELETE( mMaterial );

   mConvexList->nukeList();
   SAFE_DELETE( mConvexList );
}

void GroundPlane::initPersistFields()
{
   addGroup( "Plane" );
   addField( "squareSize",    TypeF32,          Offset( mSquareSize, GroundPlane ) );
   addField( "scaleU",        TypeF32,          Offset( mScaleU, GroundPlane ) );
   addField( "scaleV",        TypeF32,          Offset( mScaleV, GroundPlane ) );
   addField( "material",      TypeMaterialName, Offset( mMaterialName, GroundPlane ) );
   endGroup( "Plane" );
   
   Parent::initPersistFields();

   removeField( "scale" );
}

bool GroundPlane::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   if( isClientObject() )
      _updateMaterial();
      
   if( mSquareSize < sMIN_SQUARE_SIZE )
   {
      Con::errorf( "GroundPlane - squareSize below threshold; re-setting to %.02f", sMIN_SQUARE_SIZE );
      mSquareSize = sMIN_SQUARE_SIZE;
   }

   setScale( VectorF( 1.0f, 1.0f, 1.0f ) );
   setGlobalBounds();
   resetWorldBox();

   addToScene();

   if( gPhysicsPlugin )
      mPhysicsRep = gPhysicsPlugin->createStatic( this );

   return true;
}

void GroundPlane::onRemove()
{
   if ( mPhysicsRep )
      SAFE_DELETE( mPhysicsRep );

   removeFromScene();
   Parent::onRemove();
}

void GroundPlane::inspectPostApply()
{
   Parent::inspectPostApply();
   setMaskBits( UpdateMask );

   if( mSquareSize < sMIN_SQUARE_SIZE )
   {
      Con::errorf( "GroundPlane - squareSize below threshold; re-setting to %.02f", sMIN_SQUARE_SIZE );
      mSquareSize = sMIN_SQUARE_SIZE;
   }

   setScale( VectorF( 1.0f, 1.0f, 1.0f ) );
   resetWorldBox();
}

void GroundPlane::setTransform( const MatrixF &mat )
{
   Parent::setTransform( mat );

   // Parent::setTransforms ends up setting our worldBox to something other than
   // global, so we have to set it back... but we can't actually call setGlobalBounds
   // again because it does extra work adding and removing us from the container.

   mGlobalBounds = true;
   mObjBox.minExtents.set(-1e10, -1e10, -1e10);
   mObjBox.maxExtents.set( 1e10,  1e10,  1e10);
   resetWorldBox();

   mPlaneBox = getPlaneBox();
}

U32 GroundPlane::packUpdate( NetConnection* connection, U32 mask, BitStream* stream )
{
   U32 retMask = Parent::packUpdate( connection, mask, stream );

   stream->write( mSquareSize );
   stream->write( mScaleU );
   stream->write( mScaleV );
   stream->write( mMaterialName );

   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      mathWrite( *stream, getTransform() );
   }

   return retMask;
}

void GroundPlane::unpackUpdate( NetConnection* connection, BitStream* stream )
{
   Parent::unpackUpdate( connection, stream );

   stream->read( &mSquareSize );
   stream->read( &mScaleU );
   stream->read( &mScaleV );
   stream->read( &mMaterialName );

   if( stream->readFlag() ) // UpdateMask
   {
      MatrixF mat;
      mathRead( *stream, &mat );
      setTransform( mat );
   }

   // If we're added then something possibly changed in 
   // the editor... do an update of the material and the
   // geometry.
   if ( isProperlyAdded() )
   {
      _updateMaterial();
      mVertexBuffer = NULL;
   }
}

void GroundPlane::_updateMaterial()
{
   if( mMaterialName.isEmpty() )
   {
      Con::warnf( "GroundPlane::_updateMaterial - no material set; defaulting to 'WarningMaterial'" );
      mMaterialName = "WarningMaterial";
   }

   // If the material name matches then don't 
   // bother updating it.
   if (  mMaterial && 
         mMaterialName.compare( mMaterial->getMaterial()->getName() ) == 0 )
      return;

   SAFE_DELETE( mMaterial );

   mMaterial = MATMGR->createMatInstance( mMaterialName, getGFXVertexFormat< VertexType >() );
   if ( !mMaterial )
      Con::errorf( "GroundPlane::_updateMaterial - no material called '%s'", mMaterialName.c_str() );
}

bool GroundPlane::castRay( const Point3F& start, const Point3F& end, RayInfo* info )
{
   PlaneF plane( Point3F( 0.0f, 0.0f, 0.0f ), Point3F( 0.0f, 0.0f, 1.0f ) );

   F32 t = plane.intersect( start, end );
   if( t >= 0.0 && t <= 1.0 )
   {
      info->t = t;
      info->setContactPoint( start, end );
      info->normal.set( 0, 0, 1 );
      info->material = mMaterial;
      info->object = this;
      info->distance = 0;
      info->faceDot = 0;
      info->texCoord.set( 0, 0 );
      return true;
   }

   return false;
}

void GroundPlane::buildConvex( const Box3F& box, Convex* convex )
{
   mConvexList->collectGarbage();

   if ( !box.isOverlapped( mPlaneBox ) )
      return;

   // See if we already have a convex in the working set.
   BoxConvex *boxConvex = NULL;
   CollisionWorkingList &wl = convex->getWorkingList();
   CollisionWorkingList *itr = wl.wLink.mNext;
   for ( ; itr != &wl; itr = itr->wLink.mNext )
   {
      if (  itr->mConvex->getType() == BoxConvexType &&
            itr->mConvex->getObject() == this )
      {
         boxConvex = (BoxConvex*)itr->mConvex;
         break;
      }
   }

   if ( !boxConvex )
   {
      boxConvex = new BoxConvex;
      mConvexList->registerObject( boxConvex );
      boxConvex->init( this );

      convex->addToWorkingList( boxConvex );
   }

   // Update our convex to best match the queried box
   if ( boxConvex )
   {
      Point3F queryCenter = box.getCenter();

      boxConvex->mCenter      = Point3F( queryCenter.x, queryCenter.y, -GROUND_PLANE_BOX_HEIGHT_HALF );
      boxConvex->mSize        = Point3F( box.getExtents().x,
                                         box.getExtents().y,
                                         GROUND_PLANE_BOX_HEIGHT_HALF );
   }
}

bool GroundPlane::buildPolyList( AbstractPolyList* polyList, const Box3F&, const SphereF& )
{
   polyList->setObject( this );
   polyList->setTransform( &MatrixF::Identity, Point3F( 1.0f, 1.0f, 1.0f ) );

   polyList->addBox( mPlaneBox, mMaterial );

   return true;
}

bool GroundPlane::prepRenderImage( SceneState* state, const U32 stateKey, const U32, const bool )
{
   PROFILE_SCOPE( GroundPlane_prepRenderImage );
   
   if( isLastState( state, stateKey ) )
      return false;

   setLastState( state, stateKey );

   if( !state->isObjectRendered( this )
       || !mMaterial )
      return false;

   PROFILE_SCOPE( GroundPlane_prepRender );

   // Update the geometry.
   createGeometry( state->getFrustum() );
   if( mVertexBuffer.isNull() )
      return false;

   // Add a render instance.

   RenderPassManager*   pass  = state->getRenderPass();
   MeshRenderInst*      ri    = pass->allocInst< MeshRenderInst >();

   ri->type                   = RenderPassManager::RIT_Mesh;
   ri->vertBuff               = &mVertexBuffer;
   ri->primBuff               = &mPrimitiveBuffer;
   ri->prim                   = &mPrimitive;
   ri->matInst                = mMaterial;
   ri->objectToWorld          = pass->allocUniqueXform( mRenderObjToWorld );
   ri->worldToCamera          = pass->allocSharedXform(RenderPassManager::View);
   ri->projection             = pass->allocSharedXform(RenderPassManager::Projection);
   ri->visibility             = 1.0f;
   ri->translucentSort        = ( ri->matInst->getMaterial()->isTranslucent() );
   // NOTICE: SFXBB is removed and refraction is disabled!
   //ri->backBuffTex            = GFX->getSfxBackBuffer();
   ri->defaultKey             = ( U32 ) mMaterial;

   // TODO: Get the best lights for the plane in a better
   // way.... maybe the same way as we do for terrain?
   ri->lights[0] = state->getLightManager()->getDefaultLight();

   if( ri->translucentSort )
      ri->type = RenderPassManager::RIT_Translucent;

   pass->addInst( ri );

   return true;
}

/// Generate a subset of the ground plane matching the given frustum.

void GroundPlane::createGeometry( const Frustum& frustum )
{
   PROFILE_SCOPE( GroundPlane_createGeometry );
   
   enum { MAX_WIDTH = 256, MAX_HEIGHT = 256 };
   
   // Project the frustum onto the XY grid.

   Point2F min;
   Point2F max;

   projectFrustum( frustum, mSquareSize, min, max );
   
   // Early out if the grid projection hasn't changed.

   if(   mVertexBuffer.isValid() && 
         min == mMin && 
         max == mMax )
      return;

   mMin = min;
   mMax = max;

   // Determine the grid extents and allocate the buffers.
   // Adjust square size permanently if with the given frustum,
   // we end up producing more than a certain limit of geometry.
   // This is to prevent this code from causing trouble with
   // long viewing distances.
   // This only affects the client object, of course, and thus
   // has no permanent effect.
   
   U32 width = U32( ( max.x - min.x ) / mSquareSize );
   if( width > MAX_WIDTH )
   {
      mSquareSize = U32( max.x - min.x ) / MAX_WIDTH;
      width = MAX_WIDTH;
   }
   
   U32 height = U32( ( max.y - min.y ) / mSquareSize );
   if( height > MAX_HEIGHT )
   {
      mSquareSize = U32( max.y - min.y ) / MAX_HEIGHT;
      height = MAX_HEIGHT;
   }

   const U32 numVertices   = ( width + 1 ) * ( height + 1 );
   const U32 numTriangles  = width * height * 2;

   // Only reallocate if the vertex buffer is null or too small.
   if ( mVertexBuffer.isNull() || numVertices > mVertexBuffer->mNumVerts )
   {
      mVertexBuffer.set( GFX, numVertices, GFXBufferTypeDynamic );
      mPrimitiveBuffer.set( GFX, numTriangles * 3, numTriangles, GFXBufferTypeDynamic );
   }

   // Generate the grid.

   generateGrid( width, height, mSquareSize, min, max, mVertexBuffer, mPrimitiveBuffer );

   // Set up GFX primitive.

   mPrimitive.type            = GFXTriangleList;
   mPrimitive.numPrimitives   = numTriangles;
   mPrimitive.numVertices     = numVertices;
}

/// Project the given frustum onto the ground plane and return the XY bounds in world space.

void GroundPlane::projectFrustum( const Frustum& _frustum, F32 squareSize, Point2F& outMin, Point2F& outMax )
{
   // Go through all the frustum's corner points and mark
   // the min and max XY coordinates.

   // transform the frustum to plane object space
   Frustum frustum = _frustum;
   frustum.mulL( mWorldToObj );

   Point2F minPt( F32_MAX, F32_MAX );
   Point2F maxPt( F32_MIN, F32_MIN );

   for( U32 i = 0; i < Frustum::CornerPointCount; ++ i )
   {
      const Point3F& point = frustum.getPoint( i );
      
      if( point.x < minPt.x )
         minPt.x = point.x;
      if( point.y < minPt.y )
         minPt.y = point.y;

      if( point.x > maxPt.x )
         maxPt.x = point.x;
      if( point.y > maxPt.y )
         maxPt.y = point.y;
   }

   // Round the min and max coordinates so they align on the grid.

   minPt.x -= mFmod( minPt.x, squareSize );
   minPt.y -= mFmod( minPt.y, squareSize );

   F32 maxDeltaX = mFmod( maxPt.x, squareSize );
   F32 maxDeltaY = mFmod( maxPt.y, squareSize );

   if( maxDeltaX != 0.0f )
      maxPt.x += ( squareSize - maxDeltaX );
   if( maxDeltaY != 0.0f )
      maxPt.y += ( squareSize - maxDeltaY );

   // Add a safezone, so we don't touch the clipping planes.

   minPt.x -= squareSize; minPt.y -= squareSize;
   maxPt.x += squareSize; maxPt.y += squareSize;

   outMin = minPt;
   outMax = maxPt;
}

/// Generate a triangulated grid spanning the given bounds into the given buffers.

void GroundPlane::generateGrid( U32 width, U32 height, F32 squareSize,
                                const Point2F& min, const Point2F& max,
                                GFXVertexBufferHandle< VertexType >& outVertices,
                                GFXPrimitiveBufferHandle& outPrimitives )
{
   // Generate the vertices.

   VertexType* vertices = outVertices.lock();
   for( F32 y = min.y; y <= max.y; y += squareSize )
      for( F32 x = min.x; x <= max.x; x += squareSize )
      {
         vertices->point.x = x;
         vertices->point.y = y;
         vertices->point.z = 0.0;

         vertices->texCoord.x = ( x / squareSize ) * mScaleU;
         vertices->texCoord.y = ( y / squareSize ) * -mScaleV;

         vertices->normal.x = 0.0f;
         vertices->normal.y = 0.0f;
         vertices->normal.z = 1.0f;

         vertices->tangent.x = 1.0f;
         vertices->tangent.y = 0.0f;
         vertices->tangent.z = 0.0f;

         vertices->binormal.x = 0.0f;
         vertices->binormal.y = 1.0f;
         vertices->binormal.z = 0.0f;

         vertices++;
      }
   outVertices.unlock();

   // Generate the indices.

   U16* indices;
   outPrimitives.lock( &indices );
   
   U16 corner1 = 0;
   U16 corner2 = 1;
   U16 corner3 = width + 1;
   U16 corner4 = width + 2;
   
   for( U32 y = 0; y < height; ++ y )
   {
      for( U32 x = 0; x < width; ++ x )
      {
         indices[ 0 ] = corner3;
         indices[ 1 ] = corner2;
         indices[ 2 ] = corner1;

         indices += 3;

         indices[ 0 ] = corner3;
         indices[ 1 ] = corner4;
         indices[ 2 ] = corner2;

         indices += 3;

         corner1 ++;
         corner2 ++;
         corner3 ++;
         corner4 ++;
      }

      corner1 ++;
      corner2 ++;
      corner3 ++;
      corner4 ++;
   }

   outPrimitives.unlock();
}

ConsoleMethod( GroundPlane, postApply, void, 2, 2, "")
{
	object->inspectPostApply();
}