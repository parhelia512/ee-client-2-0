//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "math/util/frustum.h"

#include "math/mMathFn.h"
#include "platform/profiler.h"

const U32 Frustum::smEdgeIndices[12][2] =
{
   {NearTopLeft, NearTopRight},
   {NearBottomLeft, NearBottomRight},
   {NearTopLeft, NearBottomLeft},
   {NearTopRight, NearBottomRight},
   {FarTopLeft, FarTopRight},
   {FarBottomLeft, FarBottomRight},
   {NearTopLeft, FarTopLeft},
   {NearTopRight, FarTopRight},
   {NearBottomLeft, FarBottomLeft},
   {NearBottomRight, FarBottomRight},
   {FarTopLeft, FarBottomLeft},
   {FarTopRight, FarBottomRight}
};

const U32 Frustum::smOBBFaceIndices[6][4] =
{
   {0, 1, 2, 3},
   {4, 5, 6, 7},
   {0, 1, 5, 4},
   {3, 2, 6, 7},
   {0, 4, 7, 3},
   {1, 5, 6, 2}
};


Frustum::Frustum( bool isOrtho,
                  F32 nearLeft, 
                  F32 nearRight, 
                  F32 nearTop, 
                  F32 nearBottom, 
                  F32 nearDist,                  
                  F32 farDist,
                  const MatrixF &transform )
{
   mTransform = transform;

   mNearLeft = nearLeft;
   mNearRight = nearRight;
   mNearTop = nearTop;
   mNearBottom = nearBottom;
   mNearDist = nearDist;
   mFarDist = farDist;
   mIsOrtho = isOrtho;

   _updatePlanes();
}

Frustum::Frustum( const Frustum& frustum )
{
   set( frustum );
}

Frustum& Frustum::operator =( const Frustum& frustum )
{
   set( frustum );
   return *this;
}

void Frustum::set( const Frustum& frustum )
{
   mNearLeft = frustum.mNearLeft;
   mNearRight = frustum.mNearRight;
   mNearTop = frustum.mNearTop;
   mNearBottom = frustum.mNearBottom;
   mNearDist = frustum.mNearDist;
   mFarDist = frustum.mFarDist;
   mIsOrtho = frustum.mIsOrtho;

   mTransform = frustum.mTransform;
   mBounds = frustum.mBounds;

   dMemcpy( mPoints, frustum.mPoints, sizeof( mPoints ) );
   dMemcpy( mPlanes, frustum.mPlanes, sizeof( mPlanes ) );
}

void Frustum::set(   bool isOrtho,
                     F32 fovInRadians, 
                     F32 aspectRatio, 
                     F32 nearDist, 
                     F32 farDist,
                     const MatrixF &transform )
{
   F32 left    = -nearDist * mTan( fovInRadians / 2.0f );
   F32 right   = -left;
   F32 bottom  = left / aspectRatio;
   F32 top     = -bottom;

   set( isOrtho, left, right, top, bottom, nearDist, farDist, transform );
}

void Frustum::set(   bool isOrtho,
                     F32 nearLeft, 
                     F32 nearRight, 
                     F32 nearTop, 
                     F32 nearBottom, 
                     F32 nearDist, 
                     F32 farDist,
                     const MatrixF &transform )
{
   mTransform = transform;

   mNearLeft = nearLeft;
   mNearRight = nearRight;
   mNearTop = nearTop;
   mNearBottom = nearBottom;
   mNearDist = nearDist;
   mFarDist = farDist;
   mIsOrtho = isOrtho;

   _updatePlanes();
}

void Frustum::set( const MatrixF &projMat, bool normalize )
{ 
   // From "Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix"
   // by Gil Gribb and Klaus Hartmann.
   //
   // http://www2.ravensoft.com/users/ggribb/plane%20extraction.pdf

   // Right clipping plane.
   mPlanes[ PlaneRight ].set( projMat[3] - projMat[0], 
                              projMat[7] - projMat[4], 
                              projMat[11] - projMat[8],
                              projMat[15] - projMat[12] );

   // Left clipping plane.
   mPlanes[ PlaneLeft ].set(  projMat[3] + projMat[0], 
                              projMat[7] + projMat[4], 
                              projMat[11] + projMat[8], 
                              projMat[15] + projMat[12] );

   // Bottom clipping plane.
   mPlanes[ PlaneBottom ].set(   projMat[3] + projMat[1], 
                                 projMat[7] + projMat[5], 
                                 projMat[11] + projMat[9], 
                                 projMat[15] + projMat[13] );

   // Top clipping plane.
   mPlanes[ PlaneTop ].set(   projMat[3] - projMat[1], 
                              projMat[7] - projMat[5], 
                              projMat[11] - projMat[9], 
                              projMat[15] - projMat[13] );

   // Near clipping plane
   mPlanes[ PlaneNear ].set(  projMat[3] + projMat[2], 
                              projMat[7] + projMat[6], 
                              projMat[11] + projMat[10],
                              projMat[15] + projMat[14] );

   // Far clipping plane.
   mPlanes[ PlaneFar ].set(   projMat[3] - projMat[2], 
                              projMat[7] - projMat[6], 
                              projMat[11] - projMat[10], 
                              projMat[15] - projMat[14] );

   if ( normalize )
   {
      for ( S32 i=0; i < PlaneCount; i++ )
         mPlanes[i].normalize();
   }

   /* // Create the corner points via plane intersections.
   mPlanes[ PlaneNear ].intersect( mPlanes[ PlaneTop ], mPlanes[ PlaneLeft ], &mPoints[ NearTopLeft ] );
   mPlanes[ PlaneNear ].intersect( mPlanes[ PlaneTop ], mPlanes[ PlaneRight ], &mPoints[ NearTopRight ] );
   mPlanes[ PlaneNear ].intersect( mPlanes[ PlaneBottom ], mPlanes[ PlaneLeft ], &mPoints[ NearBottomLeft ] );
   mPlanes[ PlaneNear ].intersect( mPlanes[ PlaneBottom ], mPlanes[ PlaneRight ], &mPoints[ NearBottomRight ] );
   mPlanes[ PlaneFar ].intersect( mPlanes[ PlaneTop ], mPlanes[ PlaneLeft ], &mPoints[ FarTopLeft ] );
   mPlanes[ PlaneFar ].intersect( mPlanes[ PlaneTop ], mPlanes[ PlaneRight ], &mPoints[ FarTopRight ] );
   mPlanes[ PlaneFar ].intersect( mPlanes[ PlaneBottom ], mPlanes[ PlaneLeft ], &mPoints[ FarBottomLeft ] );
   mPlanes[ PlaneFar ].intersect( mPlanes[ PlaneBottom ], mPlanes[ PlaneRight ], &mPoints[ FarBottomRight ] );
   */
   // Update the axis aligned bounding box.
   _updateBounds();
}

void Frustum::setNearDist( F32 nearDist )
{
   setNearFarDist( nearDist, mFarDist );
}

void Frustum::setFarDist( F32 farDist )
{
   setNearFarDist( mNearDist, farDist );
}

void Frustum::setNearFarDist( F32 nearDist, F32 farDist )
{
   // Extract the fov and aspect ratio.
   F32 fovInRadians = ( mAtan2( mNearDist, mNearLeft ) * 2.0f ) - M_PI_F;
   F32 aspectRatio = mNearLeft / mNearBottom;

   // Store the inverted state.
   bool wasInverted = isInverted();

   // Recalculate the frustum.
   MatrixF xfm( mTransform ); 
   set( mIsOrtho, fovInRadians, aspectRatio, nearDist, farDist, xfm );

   // If the cull does not match then we need to invert.
   if ( wasInverted != isInverted() )
      invert();
}

void Frustum::cropNearFar(F32 newNearDist, F32 newFarDist)
{
   const F32 newOverOld = newNearDist / mNearDist;

   set( mIsOrtho, mNearLeft * newOverOld, mNearRight * newOverOld, mNearTop * newOverOld, mNearBottom * newOverOld, 
      newNearDist, newFarDist, mTransform);
}

void Frustum::_updatePlanes()
{
   PROFILE_SCOPE( Frustum_UpdatePlanes );

   // Build the frustum points in camera space first.

   if ( mIsOrtho )
   {
      mPoints[ CameraPosition ].zero();
      mPoints[ NearTopLeft ].set( mNearLeft, mNearDist, mNearTop );
      mPoints[ NearTopRight ].set( mNearRight, mNearDist, mNearTop );
      mPoints[ NearBottomLeft ].set( mNearLeft, mNearDist, mNearBottom );
      mPoints[ NearBottomRight ].set( mNearRight, mNearDist, mNearBottom );
      mPoints[ FarTopLeft ].set( mNearLeft, mFarDist, mNearTop );
      mPoints[ FarTopRight ].set( mNearRight, mFarDist, mNearTop );
      mPoints[ FarBottomLeft ].set( mNearLeft, mFarDist, mNearBottom );
      mPoints[ FarBottomRight ].set( mNearRight, mFarDist, mNearBottom );
   }
   else
   {
      const F32 farOverNear = mFarDist / mNearDist;

      mPoints[ CameraPosition ].zero();
      mPoints[ NearTopLeft ].set( mNearLeft, mNearDist, mNearTop );
      mPoints[ NearTopRight ].set( mNearRight, mNearDist, mNearTop );
      mPoints[ NearBottomLeft ].set( mNearLeft, mNearDist, mNearBottom );
      mPoints[ NearBottomRight ].set( mNearRight, mNearDist, mNearBottom );
      mPoints[ FarTopLeft ].set( mNearLeft * farOverNear, mFarDist, mNearTop * farOverNear );
      mPoints[ FarTopRight ].set( mNearRight * farOverNear, mFarDist, mNearTop * farOverNear );
      mPoints[ FarBottomLeft ].set( mNearLeft * farOverNear, mFarDist, mNearBottom * farOverNear );
      mPoints[ FarBottomRight ].set( mNearRight * farOverNear, mFarDist, mNearBottom * farOverNear );
   }

   // Transform the points into the desired culling space.
   for ( S32 i=0; i < PlaneLeftCenter; i++ )
      mTransform.mulP( mPoints[i] );

   // Update the axis aligned bounding box from 
   // the newly transformed points.
   _updateBounds();

   // Finally build the planes.
   if ( mIsOrtho )
   {
      mPlanes[ PlaneLeft ].set(  mPoints[ NearBottomLeft ], 
                                 mPoints[ FarTopLeft ], 
                                 mPoints[ FarBottomLeft ] );

      mPlanes[ PlaneRight ].set( mPoints[ NearTopRight ], 
                                 mPoints[ FarBottomRight ], 
                                 mPoints[ FarTopRight ] );

      mPlanes[ PlaneTop ].set(   mPoints[ FarTopRight ], 
                                 mPoints[ NearTopLeft ], 
                                 mPoints[ NearTopRight ] );

      mPlanes[ PlaneBottom ].set(   mPoints[ NearBottomRight ], 
                                    mPoints[ FarBottomLeft ], 
                                    mPoints[ FarBottomRight ] );

      mPlanes[ PlaneNear ].set(  mPoints[ NearTopLeft ], 
                                 mPoints[ NearBottomLeft ], 
                                 mPoints[ NearTopRight ] );

      mPlanes[ PlaneFar ].set(   mPoints[ FarTopLeft ], 
                                 mPoints[ FarTopRight ], 
                                 mPoints[ FarBottomLeft ] );
   }
   else
   {
      mPlanes[ PlaneLeft ].set(  mPoints[ CameraPosition ], 
                                 mPoints[ NearTopLeft ], 
                                 mPoints[ NearBottomLeft ] );

      mPlanes[ PlaneRight ].set( mPoints[ CameraPosition ], 
                                 mPoints[ NearBottomRight ], 
                                 mPoints[ NearTopRight ] );

      mPlanes[ PlaneTop ].set(   mPoints[ CameraPosition ], 
                                 mPoints[ NearTopRight ], 
                                 mPoints[ NearTopLeft ] );

      mPlanes[ PlaneBottom ].set(   mPoints[ CameraPosition ], 
                                    mPoints[ NearBottomLeft ], 
                                    mPoints[ NearBottomRight ] );

      mPlanes[ PlaneNear ].set(  mPoints[ NearTopLeft ], 
                                 mPoints[ NearBottomLeft ], 
                                 mPoints[ NearTopRight ] );

      mPlanes[ PlaneFar ].set(   mPoints[ FarTopLeft ], 
                                 mPoints[ FarTopRight ], 
                                 mPoints[ FarBottomLeft ] );
   }

   // And now the center points... mostly just used in debug rendering.
   mPoints[ PlaneLeftCenter ] = (   mPoints[ NearTopLeft ] + 
                                    mPoints[ NearBottomLeft ] + 
                                    mPoints[ FarTopLeft ] + 
                                    mPoints[ FarBottomLeft ] ) / 4.0f;

   mPoints[ PlaneRightCenter ] = (  mPoints[ NearTopRight ] + 
                                    mPoints[ NearBottomRight ] + 
                                    mPoints[ FarTopRight ] + 
                                    mPoints[ FarBottomRight ] ) / 4.0f;

   mPoints[ PlaneTopCenter ] = ( mPoints[ NearTopLeft ] + 
                                 mPoints[ NearTopRight ] + 
                                 mPoints[ FarTopLeft ] + 
                                 mPoints[ FarTopRight ] ) / 4.0f;

   mPoints[ PlaneBottomCenter ] = ( mPoints[ NearBottomLeft ] + 
                                    mPoints[ NearBottomRight ] + 
                                    mPoints[ FarBottomLeft ] + 
                                    mPoints[ FarBottomRight ] ) / 4.0f;

   mPoints[ PlaneNearCenter ] = (   mPoints[ NearTopLeft ] + 
                                    mPoints[ NearTopRight ] + 
                                    mPoints[ NearBottomLeft ] + 
                                    mPoints[ NearBottomRight ] ) / 4.0f;

   mPoints[ PlaneFarCenter ] = ( mPoints[ FarTopLeft ] + 
                                 mPoints[ FarTopRight ] + 
                                 mPoints[ FarBottomLeft ] + 
                                 mPoints[ FarBottomRight ] ) / 4.0f;
}

void Frustum::_updateBounds()
{
   // Note this code depends on the order of the
   // enum in the header... don't change it.

   mBounds.minExtents.set( mPoints[FirstCornerPoint] );
   mBounds.maxExtents.set( mPoints[FirstCornerPoint] );

   for ( S32 i=FirstCornerPoint + 1; i <= LastCornerPoint; i++ )
      mBounds.extend( mPoints[i] );      
}

void Frustum::invert()
{
   for( U32 i = 0; i < PlaneCount; i++ )
      mPlanes[i].invert();
}

bool Frustum::isInverted() const
{
   Point3F position;
   mTransform.getColumn( 3, &position );
   return mPlanes[ PlaneNear ].whichSide( position ) != PlaneF::Back;
}

void Frustum::scaleFromCenter( F32 scale )
{
   // Extract the fov and aspect ratio.
   F32 fovInRadians = ( mAtan2( mNearDist, mNearLeft ) * 2.0f ) - M_PI_F;
   F32 aspectRatio = mNearLeft / mNearBottom;

   // Now move the near and far planes out.
   F32 halfDist = ( mFarDist - mNearDist ) / 2.0f;
   mNearDist   -= halfDist * ( scale - 1.0f );
   mFarDist    += halfDist * ( scale - 1.0f );

   // Setup the new scaled frustum.
   set( mIsOrtho, fovInRadians, aspectRatio, mNearDist, mFarDist, mTransform );
}

U32 Frustum::testPlanes( const Box3F &bounds, U32 planeMask, F32 expand ) const
{
   PROFILE_SCOPE( Frustum_TestPlanes );

   // This is based on the paper "A Faster Overlap Test for a Plane and a Bounding Box" 
   // by Kenny Hoff.  See http://www.cs.unc.edu/~hoff/research/vfculler/boxplane.html

   U32 retMask = 0;

   Point3F minPoint, maxPoint;
   F32 maxDot, minDot;
   U32 mask;

   // Note the planes are ordered left, right, near, 
   // far, top, bottom for getting early rejections
   // from the typical horizontal scene.
   for ( S32 i = 0; i < PlaneCount; i++ )
   {
      mask = ( 1 << i );

      if ( !( planeMask & mask ) )
         continue;

      const PlaneF& plane = mPlanes[i];

      if ( plane.x > 0 )
      {
         maxPoint.x = bounds.maxExtents.x;
         minPoint.x = bounds.minExtents.x;
      }
      else
      {
         maxPoint.x = bounds.minExtents.x;
         minPoint.x = bounds.maxExtents.x;
      }

      if ( plane.y > 0 )
      {
         maxPoint.y = bounds.maxExtents.y;
         minPoint.y = bounds.minExtents.y;
      }
      else
      {
         maxPoint.y = bounds.minExtents.y;
         minPoint.y = bounds.maxExtents.y;
      }

      if ( plane.z > 0 )
      {
         maxPoint.z = bounds.maxExtents.z;
         minPoint.z = bounds.minExtents.z;
      }
      else
      {
         maxPoint.z = bounds.minExtents.z;
         minPoint.z = bounds.maxExtents.z;
      }

      maxDot = mDot( maxPoint, plane );

      if ( maxDot <= -( plane.d + expand ) )
         return -1;

      minDot = mDot( minPoint, plane );

      if ( ( minDot + plane.d ) < 0.0f )
         retMask |= mask;
   }

   return retMask;
}

bool Frustum::edgeFaceIntersect( const Point3F &edgeA, const Point3F &edgeB, 
                                 const Point3F &faceA, const Point3F &faceB, const Point3F &faceC, const Point3F &faceD, Point3F *intersection ) const
{
   VectorF edgeAB = edgeB - edgeA;
   VectorF edgeAFaceA = faceA - edgeA;
   VectorF edgeAFaceB = faceB - edgeA;
   VectorF edgeAFaceC = faceC - edgeA;

   VectorF m = mCross( edgeAFaceC, edgeAB );
   F32 v = mDot( edgeAFaceA, m );
   if ( v >= 0.0f )
   {
      F32 u = -mDot( edgeAFaceB, m );
      if ( u < 0.0f )
         return false;

      VectorF tmp = mCross( edgeAFaceB, edgeAB );
      F32 w = mDot( edgeAFaceA, tmp );
      if ( w < 0.0f )
         return false;

      F32 denom = 1.0f / (u + v + w );
      u *= denom;
      v *= denom;
      w *= denom;

      (*intersection) = u * faceA + v * faceB + w * faceC;
   }
   else
   {
      VectorF edgeAFaceD = faceD - edgeA;
      F32 u = mDot( edgeAFaceD, m );
      if ( u < 0.0f )
         return false;

      VectorF tmp = mCross( edgeAFaceA, edgeAB );
      F32 w = mDot( edgeAFaceD, tmp );
      if ( w < 0.0f )
         return false;

      v = -v;

      F32 denom = 1.0f / ( u + v + w );
      u *= denom;
      v *= denom;
      w *= denom;

      (*intersection) = u * faceA + v * faceD + w * faceC;
   }

   return true;
}

bool Frustum::intersectOBB( const Point3F *points ) const
{
   U32 bitMask[8] = { 0 };

   F32 maxDot;
   
   PROFILE_SCOPE( Frustum_OBB_Intersects );

   // For each of eight points
   // loop through each plane of
   // the frustum (near, left, right, bottom, top, far).
   for ( U32 j = 0; j < 8; j++ )
   {
      for ( S32 i = 0; i < PlaneCount; i++ )
      {
         const PlaneF &plane = mPlanes[ i ];

         // For each plane, test the plane equation
         // for the current point.  Set the appropriate
         // bit for the current point and plane.
         maxDot = mDot( points[j], plane ) + plane.d;
         if ( maxDot < 0.0f )
            bitMask[j] = bitMask[j] | BIT( i );
      }
   }

   // If the logical AND of all 
   // eight 6-bit sequences is 
   // not zero, the box is rejected.
   if (  (bitMask[0] &
         bitMask[1] &
         bitMask[2] &
         bitMask[3] &
         bitMask[4] &
         bitMask[5] &
         bitMask[6] &
         bitMask[7]) != 0 )
      return false;
         
   // If the box has not been rejected, 
   // loop through the sequences.  
   // If any are 0, accept.
   for ( U32 i = 0; i < 8; i++ )
   {
      if ( !bitMask[i] )
         return true;
   }

   // Special cases below.
   
   // First, check each of the 12
   // frustum edges against the 6
   // OBB faces.  On the first occurrance
   // of an edge-face intersection, accept.
   Point3F intersection( 0, 0, 0 );

   for ( U32 i = 0; i < 12; i++ )
   {
      for ( U32 j = 0; j < 6; j++ )
      {
         if ( edgeFaceIntersect( mPoints[smEdgeIndices[i][0]], mPoints[smEdgeIndices[i][1]], 
                                 points[smOBBFaceIndices[j][0]],   
                                 points[smOBBFaceIndices[j][1]],
                                 points[smOBBFaceIndices[j][2]],
                                 points[smOBBFaceIndices[j][3]], &intersection ) )
            return true;
      }
   }
   
   // If all edge-face intersections fail,
   // check to see if the frustum origin
   // is contained in the OBB.

   // If both special cases fail, reject.
   return false;
}

bool Frustum::pointInFrustum( const Point3F &point ) const
{
   PROFILE_SCOPE( Frustum_PointInFrustum );

   F32 maxDot;

   // Note the planes are ordered left, right, near, 
   // far, top, bottom for getting early rejections
   // from the typical horizontal scene.
   for ( S32 i = 0; i < PlaneCount; i++ )
   {
      const PlaneF &plane = mPlanes[ i ];

      // This is pretty much as optimal as you can
      // get for a plane vs point test...
      // 
      // 1 comparision
      // 2 multiplies
      // 1 adds
      //
      // It will early out as soon as it detects the
      // point is outside one of the planes.

      maxDot = mDot( point, plane ) + plane.d;
      if ( maxDot < 0.0f )
         return false;
   }

   return true;
}

bool Frustum::sphereInFrustum( const Point3F &center, F32 radius ) const
{
   PROFILE_SCOPE( Frustum_SphereInFrustum );

   F32 maxDot;

   // Note the planes are ordered left, right, near, 
   // far, top, bottom for getting early rejections
   // from the typical horizontal scene.
   for ( S32 i = 0; i < PlaneCount; i++ )
   {
      const PlaneF &plane = mPlanes[ i ];

      // This is pretty much as optimal as you can
      // get for a plane vs point test...
      // 
      // 1 comparision
      // 2 multiplies
      // 1 adds
      // 1 negation
      //
      // It will early out as soon as it detects the
      // point is outside one of the planes.

      maxDot = mDot( center, plane ) + plane.d;
      if ( maxDot < -radius )
         return false;
   }

   return true;
}

void Frustum::getCenterPoint( Point3F *center ) const
{
   center->set( mPoints[ FirstCornerPoint ] );

   for ( U32 i = FirstCornerPoint+1; i <= LastCornerPoint; i++ )
      *center += mPoints[ i ];

   *center /= (F32)CornerPointCount;
}

void Frustum::mul( const MatrixF& mat )
{
   mTransform.mul( mat );
   _updatePlanes();
}

void Frustum::mulL( const MatrixF& mat )
{
   MatrixF last( mTransform );
   mTransform.mul( mat, last );

   _updatePlanes();
}

void Frustum::getProjectionMatrix( MatrixF *proj ) const
{
   Point4F row;
   row.x = 2.0*mNearDist / (mNearRight-mNearLeft);
   row.y = 0.0;
   row.z = 0.0;
   row.w = 0.0;
   proj->setRow( 0, row );

   row.x = 0.0;
   row.y = 2.0 * mNearDist / (mNearTop-mNearBottom);
   row.z = 0.0;
   row.w = 0.0;
   proj->setRow( 1, row );

   row.x = (mNearLeft+mNearRight) / (mNearRight-mNearLeft);
   row.y = (mNearTop+mNearBottom) / (mNearTop-mNearBottom);
   row.z = mFarDist / (mNearDist-mFarDist);
   row.w = -1.0;
   proj->setRow( 2, row );

   row.x = 0.0;
   row.y = 0.0;
   row.z = mNearDist * mFarDist / (mNearDist-mFarDist);
   row.w = 0.0;
   proj->setRow( 3, row );

   proj->transpose();

   MatrixF rotMat(EulerF( (M_PI_F / 2.0f), 0.0f, 0.0f));
   proj->mul( rotMat );
}

