//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MATHUTIL_FRUSTUM_H_
#define _MATHUTIL_FRUSTUM_H_

#ifndef _MBOX_H_
#include "math/mBox.h"
#endif
#ifndef _MPLANE_H_
#include "math/mPlane.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif
#ifndef _MQUAT_H_
#include "math/mQuat.h"
#endif


/// This class implements a view frustum for use in culling
/// scene objects and rendering the scene graph.
class Frustum
{
   public:
      
      static const U32 smOBBFaceIndices[6][4];
      static const U32 smEdgeIndices[12][2];

      /// Used to index into point array.
      enum 
      {
         /// The corner points of the frustum.
         NearTopLeft,
         NearTopRight,
         NearBottomLeft,
         NearBottomRight,
         FarTopLeft,
         FarTopRight,
         FarBottomLeft,
         FarBottomRight,

         /// The apex of the frustum.
         CameraPosition,

         /// The center points of the frustum planes.
         PlaneLeftCenter,
         PlaneRightCenter,
         PlaneTopCenter,
         PlaneBottomCenter,
         PlaneNearCenter,
         PlaneFarCenter,

         /// The total number of frustum points.
         PointCount,

         FirstCornerPoint = NearTopLeft,
         LastCornerPoint = FarBottomRight,
         CornerPointCount = 8
      };

      /// Used to index into the plane array.  
      ///
      /// Note that these are ordered for optimal early 
      /// rejection.  By culling with the left and right
      /// planes first you cull most of the objects in 
      /// the typical horizontal scene.
      enum
      {
         PlaneLeft,
         PlaneRight,
         PlaneNear,
         PlaneFar,
         PlaneTop,
         PlaneBottom,

         /// The total number of frustum planes.
         PlaneCount
      };

      /// Used to mask out planes for testing.
      enum 
      {
         PlaneMaskLeft     = ( 1 << PlaneLeft ),
         PlaneMaskRight    = ( 1 << PlaneRight ),
         PlaneMaskTop      = ( 1 << PlaneTop ),
         PlaneMaskBottom   = ( 1 << PlaneBottom ),
         PlaneMaskNear     = ( 1 << PlaneNear ),
         PlaneMaskFar      = ( 1 << PlaneFar ),

         PlaneMaskAll      = 0xFFFFFFFF,
      };

   protected:
      /// The clipping planes used during culling.
#ifdef TORQUE_COMPILER_VISUALC
      __declspec(align(16)) 
#endif
      PlaneF mPlanes[ PlaneCount ]
#if defined(TORQUE_COMPILER_GCC)
      __attribute__ ((aligned (16)))
#endif
      ;

      /// The points of the frustum that make up
      /// the the clipping planes.
      Point3F mPoints[ PointCount ];

      /// Determines whether this Frustum
      /// is orthographic or perspective.
      bool mIsOrtho;

      /// The axis aligned bounding box which contains
      /// the extents of the frustum.
      Box3F mBounds;

      /// Used to transform the frustum points from camera
      /// space into the desired clipping space.
      MatrixF mTransform;

      /// The size of the near plane used to generate
      /// the frustum points and planes.
      F32 mNearLeft;
      F32 mNearRight;
      F32 mNearTop;
      F32 mNearBottom;
      F32 mNearDist;
      F32 mFarDist;

      /// Called to initialize the planes after frustum
      /// settings are changed.
      void _updatePlanes();

      /// Called to recalculate the bounds from the frustum
      /// points when the planes are updated or transformed.
      void _updateBounds();

   public:
      
      /// @name Constructors
      ///
      /// @{

      /// 
      Frustum( bool orthographic = false,
               F32 nearLeft = -1.0f, 
               F32 nearRight = 1.0f, 
               F32 nearTop = -1.0f, 
               F32 nearBottom = 1.0f, 
               F32 nearDist = 0.1f, 
               F32 farDist = 1.0f,
               const MatrixF &transform = MatrixF( true ) );

      /// Copy constructor.
      Frustum( const Frustum &frustum );

      /// @}


      /// @name Operators
      ///
      /// @{

      /// Convenience operator for copying frustums.
      Frustum& operator =( const Frustum& frustum );

      // Comparison operators
      bool operator==( const Frustum& frustum ) const;
      bool operator!=( const Frustum& frustum ) const;

      /// @}


      /// @name Initialization
      ///
      /// Functions used to initialize the frustum.
      ///
      /// @{

      /// Set the frustum via a copy.
      void set( const Frustum &frustum );
      
      /// Sets the frustum from the field of view, screen aspect
      /// ratio, and the near and far distances.  You can pass an
      /// matrix to transform the frustum.
      void set(   bool isOrtho,
                  F32 fovInRadians, 
                  F32 aspectRatio, 
                  F32 nearDist, 
                  F32 farDist,
                  const MatrixF &mat = MatrixF( true ) );

      /// Sets the frustum from the near plane dimensions and
      /// near and far distances.
      void set(   bool isOrtho,
                  F32 nearLeft, 
                  F32 nearRight, 
                  F32 nearTop, 
                  F32 nearBottom, 
                  F32 nearDist, 
                  F32 farDist,
                  const MatrixF &transform = MatrixF( true ) );

      /// Sets the frustum by extracting the planes from a projection,
      /// view-projection, or world-view-projection matrix.
      void set( const MatrixF& projMatrix, bool normalize );

      /// Changes the near distance of the frustum.
      void setNearDist( F32 nearDist );

      /// Changes the far distance of the frustum.
      void setFarDist( F32 farDist );

      /// Changes the near and far distance of the frustum.
      void setNearFarDist( F32 nearDist, F32 farDist );

      ///
      void cropNearFar(F32 newNearDist, F32 newFarDist);

      /// Returns the far clip distance used to create 
      /// the frustum planes.
      F32 getFarDist() const { return mFarDist; }

      /// Returns the far clip distance used to create 
      /// the frustum planes.
      F32 getNearDist() const { return mNearDist; }

      ///
      F32 getNearLeft() const { return mNearLeft; }

      ///
      F32 getNearRight() const { return mNearRight; }

      ///
      F32 getNearTop() const { return mNearTop; }

      ///
      F32 getNearBottom() const { return mNearBottom; }

      /// @}


      /// @name Transformation
      ///
      /// These functions for transforming the frustum from
      /// one space to another.
      ///
      /// @{

      /// Sets a new transform for the frustum.
      void setTransform( const MatrixF &transform );

      /// Returns the current transform matrix for the frustum.
      const MatrixF& getTransform() const;

      /// Scales up the frustum from its center point.
      void scaleFromCenter( F32 scale );

      /// Transforms the frustum by F = F * mat.
      void mul( const MatrixF &mat );

      /// Transforms the frustum by F = mat * F.
      void mulL( const MatrixF &mat );

      /// Flip the plane normals which has the result
      /// of reversing the culling results.
      void invert();

      /// Returns true if the frustum planes point outwards.
      bool isInverted() const;


      /// Returns the origin point of the frustum.
      const Point3F& getPosition() const;

      /// Returns the axis aligned bounding box of the frustum
      /// points typically used for early rejection.
      const Box3F& getBounds() const;

      /// Generates a projection matrix from the frustum.
      void getProjectionMatrix( MatrixF *proj ) const;

      /// @}


      /// @name Culling
      ///
      /// Various functions used to cull shapes against the view 
      /// frustum.  For best results always perform an overlap test
      /// against the frustum bounds before using these routines.
      ///
      /// @{

      /// Returns true if the box is completely within or intersecting
      /// one or more of the frustum planes.
      bool intersects( const Box3F &bounds ) const { return m_planeF_intersect_box3F(reinterpret_cast<const F32 *>(&mPlanes), reinterpret_cast<const F32 *>(&bounds)); }
      bool intersectOBB( const Point3F *points ) const;
      bool edgeFaceIntersect( const Point3F &edgeA, const Point3F &edgeB, 
                              const Point3F &faceA, const Point3F &faceB, const Point3F &faceC, const Point3F &faceD, Point3F *intersection ) const;

      /// Returns true if the point is completely within the frustum planes.
      bool pointInFrustum( const Point3F &point ) const;

      /// Returns true if the center point of the sphere
      /// is not less than radius distance from one of the frustum planes.
      bool sphereInFrustum( const Point3F &center, F32 radius ) const;

      /// Returns the bitmask of what planes were hit.
      U32 testPlanes( const Box3F &bounds, U32 planeMask, F32 expand = 0.0f ) const;

      /// @}


      /// @name Points and Planes
      ///
      /// @{    

      /// Returns a reference to a frustum point. 
      const Point3F& getPoint( U32 index ) const;

      /// Returns a pointer to the array of frustum 
      /// points  of PointCount size.
      const Point3F* getPoints() const;

      /// Returns a pointer to the array of frustum 
      /// planes of PlaneCount size.
      const PlaneF* getPlanes() const;

      /// Returns the center point of the frustum by
      /// averaging all the corner points.
      void getCenterPoint( Point3F *center ) const;

      /// @}

      /// @name Projection Type
      ///
      /// @{
      bool isOrtho() const { return mIsOrtho; }
      /// @}

};


inline const MatrixF& Frustum::getTransform() const
{
   return mTransform; 
}

inline const Box3F& Frustum::getBounds() const
{
   return mBounds; 
}

inline const Point3F& Frustum::getPosition() const 
{ 
   return mPoints[ CameraPosition ]; 
}

inline const Point3F& Frustum::getPoint( U32 index ) const
{
   AssertFatal( index >= 0 && index < PointCount, "Bad index!" );
   return mPoints[ index ]; 
}

inline const Point3F* Frustum::getPoints() const 
{
   return mPoints; 
}

inline const PlaneF* Frustum::getPlanes() const 
{ 
   return mPlanes; 
}

inline bool Frustum::operator==(const Frustum& _test) const
{
   return ( ( mNearLeft == _test.mNearLeft ) &&
            ( mNearTop == _test.mNearTop ) &&
            ( mNearBottom == _test.mNearBottom ) &&
            ( mNearDist == _test.mNearDist ) &&
            ( mFarDist == _test.mFarDist ) );
}

inline bool Frustum::operator!=(const Frustum& _test) const
{
   return (operator==(_test) == false);
}

#endif // _MATHUTIL_FRUSTUM_H_