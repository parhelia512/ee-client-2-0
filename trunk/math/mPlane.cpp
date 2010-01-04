//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "math/mPlane.h"


bool mIntersect(  const PlaneF &p1, const PlaneF &p2, 
                  Point3F *outLinePt, VectorF *outLineDir )
{
   // Compute direction of intersection line.
   *outLineDir = mCross( p1, p2 );

   // If d is zero, the planes are parallel (and separated)
   // or coincident, so they're not considered intersecting
   F32 denom = mDot( *outLineDir, *outLineDir );
   if ( denom < 0.00001f ) 
      return false;

   // Compute point on intersection line
   *outLinePt = mCross( p1.d * p2 - p2.d * p1,
                        *outLineDir ) / denom;

   return true;
}
