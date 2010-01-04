//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TORQUE_T3D_GROUNDPLANE_H_
#define _TORQUE_T3D_GROUNDPLANE_H_

#include "sceneGraph/sceneObject.h"
#include "gfx/gfxVertexBuffer.h"
#include "gfx/gfxPrimitiveBuffer.h"

/// A virtually infinite XY ground plane primitive.
///
/// For rendering, a subset of the plane spanning the view frustum is generated
/// and rendered.  Tesselation is determined by the given squareSize property.
///
/// For collision detection, a finite bounding box is used to deal with finite
/// precision of floating-point operations (we can't use floating-point infinity
/// as infinity*0 is undefined.)
///
/// The ground plane can be textured like regular geometry by assigning a material
/// name to its 'material' property.  UVs mirror grid coordinates so that when
/// using UV wrapping, textures will tile nicely.

class PhysicsStatic;

class GroundPlane : public SceneObject
{
   typedef SceneObject Parent;

private:
   enum MaskBits {
      UpdateMask   = Parent::NextFreeMask,
      NextFreeMask = Parent::NextFreeMask << 1
   };

public:

   DECLARE_CONOBJECT( GroundPlane );

   GroundPlane();
   virtual ~GroundPlane();

   virtual bool      onAdd();
   virtual void      onRemove();
   virtual U32       packUpdate( NetConnection* connection, U32 mask, BitStream* stream );
   virtual void      unpackUpdate( NetConnection* connection, BitStream* stream );
   virtual bool      prepRenderImage( SceneState* state, const U32 stateKey, const U32, const bool );
   virtual bool      castRay( const Point3F& start, const Point3F& end, RayInfo* info );
   virtual void      buildConvex( const Box3F& box, Convex* convex );
   virtual bool      buildPolyList( AbstractPolyList* polyList, const Box3F& box, const SphereF& sphere );
   virtual void      inspectPostApply();
   virtual void      setTransform( const MatrixF &mat );

   static void       initPersistFields();

protected:

   typedef GFXVertexPNTBT VertexType;

   void _updateMaterial();

   void              createGeometry( const Frustum& frustum );
   void              projectFrustum( const Frustum& frustum, F32 squareSize,
                                     Point2F& outMin, Point2F& outMax );
   void              generateGrid( U32 width, U32 height, F32 squareSize,
                                   const Point2F& min, const Point2F& max,
                                   GFXVertexBufferHandle< VertexType >& outVertices,
                                   GFXPrimitiveBufferHandle& outPrimitives );

   Box3F             getPlaneBox();

private:

   typedef GFXVertexBufferHandle< VertexType > VertexBuffer;
   typedef GFXPrimitiveBufferHandle PrimitiveBuffer;

   F32               mSquareSize;   ///< World units per grid cell edge.
   F32               mScaleU;       ///< Scale factor for U texture coordinates.
   F32               mScaleV;       ///< Scale factor for V texture coordinates.
   String            mMaterialName; ///< Object name of material to use.
   BaseMatInstance*  mMaterial;     ///< Instantiated material based on given material name.

   PhysicsStatic *mPhysicsRep;

   /// @name Rendering State
   /// @{

   Point2F           mMin;
   Point2F           mMax;
   Box3F             mPlaneBox;
   VertexBuffer      mVertexBuffer;
   PrimitiveBuffer   mPrimitiveBuffer;
   GFXPrimitive      mPrimitive;

   /// @}

   Convex*           mConvexList;   ///< List of collision convexes we have created; for cleanup.
};

static const F32 GROUND_PLANE_BOX_HEIGHT_HALF = 1.0f;
static const F32 GROUND_PLANE_BOX_EXTENT_HALF = 16000.0f;

inline Box3F GroundPlane::getPlaneBox()
{
   Box3F planeBox;

   EulerF rot = getTransform().toEuler();
   if( rot.x == 0 && rot.y == 0 )
   {
      // rotation in Z only: use regular ground plane collision
      planeBox.minExtents = Point3F( - GROUND_PLANE_BOX_EXTENT_HALF,
                                     - GROUND_PLANE_BOX_EXTENT_HALF,
                                     - GROUND_PLANE_BOX_HEIGHT_HALF );
      planeBox.maxExtents = Point3F( GROUND_PLANE_BOX_EXTENT_HALF,
                                     GROUND_PLANE_BOX_EXTENT_HALF,
                                     GROUND_PLANE_BOX_HEIGHT_HALF );
   } else
   {
      // ground plane is tilted: use larger collision cube
      planeBox.minExtents = Point3F( - GROUND_PLANE_BOX_EXTENT_HALF,
                                     - GROUND_PLANE_BOX_EXTENT_HALF,
                                     - GROUND_PLANE_BOX_EXTENT_HALF );
      planeBox.maxExtents = Point3F( GROUND_PLANE_BOX_EXTENT_HALF,
                                     GROUND_PLANE_BOX_EXTENT_HALF,
                                     GROUND_PLANE_BOX_EXTENT_HALF );
   }

   Point3F center = getPosition();
   center.z -= GROUND_PLANE_BOX_HEIGHT_HALF;

   planeBox.setCenter( center );

   return planeBox;
}

#endif // _TORQUE_T3D_GROUNDPLANE_H_
