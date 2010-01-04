//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MATHUTILS_H_
#define _MATHUTILS_H_

#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif
#ifndef _MATHUTIL_FRUSTUM_H_
#include "math/util/frustum.h"
#endif

class Box3F;
class RectI;

/// Miscellaneous math utility functions.
namespace MathUtils
{
   /// A simple helper struct to define a line.
   struct Line
   {
      Point3F origin;
      VectorF direction;
   };

   /// A ray is also a line.
   typedef Line Ray;

   /// A simple helper struct to define a line segment.
   struct LineSegment
   {
      Point3F p0;
      Point3F p1;
   };   

   /// A simple helper struct to define a clockwise 
   /// winding quad.
   struct Quad
   {
      Point3F p00;
      Point3F p01;
      Point3F p10;
      Point3F p11;
   };

   /// Used by mTriangleDistance() to pass along collision info
   struct IntersectInfo
   {
      LineSegment    segment;    // Starts at given point, ends at collision
      Point3F        bary;       // Barycentric coords for collision
   };

   /// Generates a projection matrix with the near plane
   /// moved forward by the bias amount.  This function is a helper primarily
   /// for working around z-fighting issues.
   ///
   /// @param bias      The amount to move the near plane forward.
   /// @param frustum   The frustum to generate the new projection matrix from.
   /// @param outMat    The resulting z-biased projection matrix.  Note: It must be initialized before the call.
   /// @param rotate    Optional parameter specifying whether to rotate the projection matrix similarly to GFXDevice.
   ///
   void getZBiasProjectionMatrix( F32 bias, const Frustum &frustum, MatrixF *outMat, bool rotate = true );

   /// Creates orientation matrix from a direction vector.  Assumes ( 0 0 1 ) is up.
   MatrixF createOrientFromDir( const Point3F &direction );

   /// Creates an orthonormal basis matrix with the unit length
   /// input vector in column 2 (up vector).
   ///
   /// @param up     The non-zero unit length up vector.
   /// @param outMat The output matrix which must be initialized prior to the call.
   ///
   void getMatrixFromUpVector( const VectorF &up, MatrixF *outMat );   

   /// Creates an orthonormal basis matrix with the unit length
   /// input vector in column 1 (forward vector).
   ///
   /// @param forward   The non-zero unit length forward vector.
   /// @param outMat    The output matrix which must be initialized prior to the call.
   ///
   void getMatrixFromForwardVector( const VectorF &forward, MatrixF *outMat );   

   /// Creates random direction given angle parameters similar to the particle system.
   ///
   /// The angles are relative to the specified axis. Both phi and theta are in degrees.
   Point3F randomDir( const Point3F &axis, F32 thetaAngleMin, F32 thetaAngleMax, F32 phiAngleMin = 0.0, F32 phiAngleMax = 360.0 );

   /// Returns yaw and pitch angles from a given vector.
   ///
   /// Angles are in RADIANS.
   ///
   /// Assumes north is (0.0, 1.0, 0.0), the degrees move upwards clockwise.
   ///
   /// The range of yaw is 0 - 2PI.  The range of pitch is -PI/2 - PI/2.
   ///
   /// <b>ASSUMES Z AXIS IS UP</b>
   void    getAnglesFromVector( const VectorF &vec, F32 &yawAng, F32 &pitchAng );

   /// Returns vector from given yaw and pitch angles.
   ///
   /// Angles are in RADIANS.
   ///
   /// Assumes north is (0.0, 1.0, 0.0), the degrees move upwards clockwise.
   ///
   /// The range of yaw is 0 - 2PI.  The range of pitch is -PI/2 - PI/2.
   ///
   /// <b>ASSUMES Z AXIS IS UP</b>
   void    getVectorFromAngles( VectorF &vec, F32 yawAng, F32 pitchAng );

   /// Simple reflection equation - pass in a vector and a normal to reflect off of
   inline Point3F reflect( Point3F &inVec, Point3F &norm )
   {
      return inVec - norm * ( mDot( inVec, norm ) * 2.0f );
   }

   bool capsuleCapsuleOverlap(const Point3F & a1, const Point3F & b1, F32 radius1, const Point3F & a2, const Point3F & b2, F32 radius2);
   bool capsuleSphereNearestOverlap(const Point3F & A0, const Point3F A1, F32 radA, const Point3F & B, F32 radB, F32 & t);
   F32 segmentSegmentNearest(const Point3F & p1, const Point3F & q1, const Point3F & p2, const Point3F & q2, F32 & s, F32 & t, Point3F & c1, Point3F & c2);

   // transform bounding box making sure to keep original box entirely contained - JK...
   void transformBoundingBox(const Box3F &sbox, const MatrixF &mat, const Point3F scale, Box3F &dbox);

   bool mProjectWorldToScreen(   const Point3F &in, 
                                 Point3F *out,
                                 const RectI &view,
                                 const MatrixF &world, 
                                 const MatrixF &projection );

   void mProjectScreenToWorld(   const Point3F &in, 
                                 Point3F *out, 
                                 const RectI &view, 
                                 const MatrixF &world, 
                                 const MatrixF &projection, 
                                 F32 far, 
                                 F32 near);

   /// Returns true if the test point is within the polygon.
   /// @param verts  The array of points which forms the polygon.
   /// @param vertCount The number of points in the polygon.
   /// @param testPt The point to test.
   bool pointInPolygon( const Point2F *verts, U32 vertCount, const Point2F &testPt );
   

   /// Calculates the shortest line segment between two lines.
   /// 
   /// @param outSegment   The result where .p0 is the point on line0 and .p1 is the point on line1.
   ///
   void mShortestSegmentBetweenLines( const Line &line0, const Line &line1, LineSegment *outSegment );
   
   /// Returns the greatest common divisor of two positive integers.
   U32 greatestCommonDivisor( U32 u, U32 v );
   
   /// Returns the barycentric coordinates and time of intersection between
   /// a line segment and a triangle.
   ///
   /// @param p1 The first point of the line segment.
   /// @param p2 The second point of the line segment.
   /// @param t1 The first point of the triangle.
   /// @param t2 The second point of the triangle.
   /// @param t2 The third point of the triangle.
   /// @param outUVW The optional output barycentric coords.
   /// @param outT The optional output time of intersection.
   ///
   /// @return Returns true if a collision occurs.
   ///
   bool mLineTriangleCollide( const Point3F &p1, const Point3F &p2, 
                              const Point3F &t1, const Point3F &t2, const Point3F &t3,
                              Point3F *outUVW = NULL,
                              F32 *outT = NULL );

   /// Returns the uv coords and time of intersection between 
   /// a ray and a quad.
   ///
   /// @param quad The quad.
   /// @param ray The ray.
   /// @param outUV The optional output UV coords of the intersection.
   /// @param outT The optional output time of intersection.
   ///
   /// @return Returns true if a collision occurs.
   ///
   bool mRayQuadCollide(   const Quad &quad, 
                           const Ray &ray, 
                           Point2F *outUV = NULL,
                           F32 *outT = NULL );

   /// Returns the distance between a point and triangle 'abc'.
   F32 mTriangleDistance( const Point3F &a, const Point3F &b, const Point3F &c, const Point3F &p, IntersectInfo* info=NULL );

   /// Returns the closest point on the segment defined by
   /// points a, b to the point p.   
   Point3F mClosestPointOnSegment(  const Point3F &a, 
                                    const Point3F &b, 
                                    const Point3F &p );
   

} // namespace MathUtils

#endif // _MATHUTILS_H_
