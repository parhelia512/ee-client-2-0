//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MSPHERE_H_
#define _MSPHERE_H_

#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif

#ifndef _MATHUTILS_H_
#include "math/mathUtils.h"
#endif


class SphereF
{
public:
   Point3F center;
   F32     radius;

public:
   SphereF() { }
   SphereF( const Point3F& in_rPosition, const F32 in_rRadius )
    : center(in_rPosition),
      radius(in_rRadius)
   {
      if ( radius < 0.0f )
         radius = 0.0f;
   }

   bool isContained( const Point3F& in_rContain ) const;
   bool isContained( const SphereF& in_rContain ) const;
   bool isIntersecting( const SphereF& in_rIntersect ) const;
   bool intersectsRay( const Point3F &start, const Point3F &end ) const;

   F32 distanceTo( const Point3F &pt ) const;
   F32 squareDistanceTo( const Point3F &pt ) const;
};

//-------------------------------------- INLINES
//
inline bool SphereF::isContained( const Point3F& in_rContain ) const
{
   F32 distSq = (center - in_rContain).lenSquared();

   return (distSq <= (radius * radius));
}

inline bool SphereF::isContained( const SphereF& in_rContain ) const
{
   if (radius < in_rContain.radius)
      return false;

   // Since our radius is guaranteed to be >= other's, we
   //  can dodge the sqrt() here.
   //
   F32 dist = (in_rContain.center - center).lenSquared();

   return (dist <= ((radius - in_rContain.radius) *
                    (radius - in_rContain.radius)));
}

inline bool SphereF::isIntersecting( const SphereF& in_rIntersect ) const
{
   F32 distSq = (in_rIntersect.center - center).lenSquared();

   return (distSq <= ((in_rIntersect.radius + radius) *
                      (in_rIntersect.radius + radius)));
}

inline bool SphereF::intersectsRay( const Point3F &start, const Point3F &end ) const
{
   MatrixF worldToObj( true );
   worldToObj.setPosition( center );
   worldToObj.inverse();

   VectorF dir = end - start;
   dir.normalize();

   Point3F tmpStart = start;
   worldToObj.mulP( tmpStart ); 

   //Compute A, B and C coefficients
   F32 a = mDot(dir, dir);
   F32 b = 2 * mDot(dir, tmpStart);
   F32 c = mDot(tmpStart, tmpStart) - (radius * radius);

   //Find discriminant
   F32 disc = b * b - 4 * a * c;

   // if discriminant is negative there are no real roots, so return 
   // false as ray misses sphere
   if ( disc < 0 )
      return false;

   // compute q as described above
   F32 distSqrt = mSqrt( disc );
   F32 q;
   if ( b < 0 )
      q = (-b - distSqrt)/2.0;
   else
      q = (-b + distSqrt)/2.0;

   // compute t0 and t1
   F32 t0 = q / a;
   F32 t1 = c / q;

   // make sure t0 is smaller than t1
   if ( t0 > t1 )
   {
      // if t0 is bigger than t1 swap them around
      F32 temp = t0;
      t0 = t1;
      t1 = temp;
   }

   // This function doesn't use it
   // but t would be the interpolant
   // value for getting the exact
   // intersection point, by interpolating
   // start to end by t.
   F32 t = 0;
   TORQUE_UNUSED(t);

   // if t1 is less than zero, the object is in the ray's negative direction
   // and consequently the ray misses the sphere
   if ( t1 < 0 )
      return false;

   // if t0 is less than zero, the intersection point is at t1
   if ( t0 < 0 ) // t = t1;     
      return true;
   else // else the intersection point is at t0
      return true; // t = t0;
}

inline F32 SphereF::distanceTo( const Point3F &toPt ) const
{
   return (center - toPt).len() - radius;
}

#endif //_SPHERE_H_
