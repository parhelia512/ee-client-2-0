//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PHYSX_CASTS_H_
#define _PHYSX_CASTS_H_

#ifndef _PHYSX_H_
#include "physX/physX.h"
#endif
#ifndef NX_PHYSICS_NXBIG
#include "NxExtended.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif
#ifndef _MBOX_H_
#include "math/mBox.h"
#endif


template <class T, class F> inline T pxCast( const F &from );

//-------------------------------------------------------------------------

template<>
inline Point3F pxCast( const NxVec3 &vec )
{
   return Point3F( vec.x, vec.y, vec.z );
}

template<>
inline NxVec3 pxCast( const Point3F &point )
{
   return NxVec3( point.x, point.y, point.z );
}

//-------------------------------------------------------------------------

template<>
inline NxBounds3 pxCast( const Box3F &box )
{
   NxBounds3 bounds;
   bounds.set( box.minExtents.x, 
               box.minExtents.y,
               box.minExtents.z,
               box.maxExtents.x,
               box.maxExtents.y,
               box.maxExtents.z );
   return bounds;
}

template<>
inline Box3F pxCast( const NxBounds3 &bounds )
{
   return Box3F(  bounds.min.x, 
                  bounds.min.y,
                  bounds.min.z,
                  bounds.max.x,
                  bounds.max.y,
                  bounds.max.z );
}

//-------------------------------------------------------------------------

template<>
inline NxVec3 pxCast( const NxExtendedVec3 &xvec )
{
   return NxVec3( xvec.x, xvec.y, xvec.z );
}

template<>
inline NxExtendedVec3 pxCast( const NxVec3 &vec )
{
   return NxExtendedVec3( vec.x, vec.y, vec.z );
}

//-------------------------------------------------------------------------

template<>
inline NxExtendedVec3 pxCast( const Point3F &point )
{
   return NxExtendedVec3( point.x, point.y, point.z );
}

template<>
inline Point3F pxCast( const NxExtendedVec3 &xvec )
{
   return Point3F( xvec.x, xvec.y, xvec.z );
}

//-------------------------------------------------------------------------

template<>
inline NxBox pxCast( const NxExtendedBounds3 &exBounds )
{
   NxExtendedVec3 center;
   exBounds.getCenter( center );
   NxVec3 extents;
   exBounds.getExtents( extents );

   NxBox box;
   box.center.set( center.x, center.y, center.z );
   box.extents = extents;
   box.rot.id();

   return box;
}

template<>
inline NxExtendedBounds3 pxCast( const NxBox &box )
{
   AssertFatal( false, "Casting a NxBox to NxExtendedBounds3 is impossible without losing rotation data!" );
   return NxExtendedBounds3();
}

#endif // _PHYSX_CASTS_H_