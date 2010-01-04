//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "collision/clippedPolyList.h"

#include "math/mMath.h"
#include "console/console.h"
#include "platform/profiler.h"

#include "core/tAlgorithm.h"

bool ClippedPolyList::allowClipping = true;


//----------------------------------------------------------------------------

ClippedPolyList::ClippedPolyList()
{
   VECTOR_SET_ASSOCIATION(mPolyList);
   VECTOR_SET_ASSOCIATION(mVertexList);
   VECTOR_SET_ASSOCIATION(mIndexList);
   VECTOR_SET_ASSOCIATION(mPolyPlaneList);
   VECTOR_SET_ASSOCIATION(mPlaneList);
   VECTOR_SET_ASSOCIATION(mNormalList);

   mNormal.set(0,0,0);
   mIndexList.reserve(IndexListReserveSize);
}

ClippedPolyList::~ClippedPolyList()
{
}


//----------------------------------------------------------------------------

void ClippedPolyList::clear()
{
   // Only clears internal data
   mPolyList.clear();
   mVertexList.clear();
   mIndexList.clear();
   mPolyPlaneList.clear();
   mNormalList.clear();
}

bool ClippedPolyList::isEmpty() const
{
   return mPolyList.size() == 0;
}


//----------------------------------------------------------------------------

U32 ClippedPolyList::addPoint(const Point3F& p)
{
   mVertexList.increment();
   Vertex& v = mVertexList.last();
   v.point.x = p.x * mScale.x;
   v.point.y = p.y * mScale.y;
   v.point.z = p.z * mScale.z;
   mMatrix.mulP(v.point);

   // Build the plane mask
   register U32      mask = 1;
   register S32      count = mPlaneList.size();
   register PlaneF * plane = mPlaneList.address();

   v.mask = 0;
   while(--count >= 0) {
      if (plane++->distToPlane(v.point) > 0)
         v.mask |= mask;
      mask <<= 1;
   }

   return mVertexList.size() - 1;
}


U32 ClippedPolyList::addPlane(const PlaneF& plane)
{
   mPolyPlaneList.increment();
   mPlaneTransformer.transform(plane, mPolyPlaneList.last());

   return mPolyPlaneList.size() - 1;
}


//----------------------------------------------------------------------------

void ClippedPolyList::begin(BaseMatInstance* material,U32 surfaceKey)
{
   mPolyList.increment();
   Poly& poly = mPolyList.last();
   poly.object = mCurrObject;
   poly.material = material;
   poly.vertexStart = mIndexList.size();
   poly.surfaceKey = surfaceKey;

   poly.polyFlags = 0;
   if(ClippedPolyList::allowClipping)
      poly.polyFlags = CLIPPEDPOLYLIST_FLAG_ALLOWCLIPPING;
}


//----------------------------------------------------------------------------

void ClippedPolyList::plane(U32 v1,U32 v2,U32 v3)
{
   mPolyList.last().plane.set(mVertexList[v1].point,
      mVertexList[v2].point,mVertexList[v3].point);
}

void ClippedPolyList::plane(const PlaneF& p)
{
   mPlaneTransformer.transform(p, mPolyList.last().plane);
}

void ClippedPolyList::plane(const U32 index)
{
   AssertFatal(index < mPolyPlaneList.size(), "Out of bounds index!");
   mPolyList.last().plane = mPolyPlaneList[index];
}

const PlaneF& ClippedPolyList::getIndexedPlane(const U32 index)
{
   AssertFatal(index < mPolyPlaneList.size(), "Out of bounds index!");
   return mPolyPlaneList[index];
}


//----------------------------------------------------------------------------

void ClippedPolyList::vertex(U32 vi)
{
   mIndexList.push_back(vi);
}


//----------------------------------------------------------------------------

void ClippedPolyList::end()
{
   Poly& poly = mPolyList.last();

   // Anything facing away from the mNormal is rejected
   if (mDot(poly.plane,mNormal) > 0) {
      mIndexList.setSize(poly.vertexStart);
      mPolyList.decrement();
      return;
   }

   // Build initial inside/outside plane masks
   U32 indexStart = poly.vertexStart;
   U32 vertexCount = mIndexList.size() - indexStart;

   U32 frontMask = 0,backMask = 0;
   U32 i;
   for (i = indexStart; i < mIndexList.size(); i++) {
      U32 mask = mVertexList[mIndexList[i]].mask;
      frontMask |= mask;
      backMask |= ~mask;
   }

   // Trivial accept if all the vertices are on the backsides of
   // all the planes.
   if (!frontMask) {
      poly.vertexCount = vertexCount;
      return;
   }

   // Trivial reject if any plane not crossed has all it's points
   // on the front.
   U32 crossMask = frontMask & backMask;
   if (~crossMask & frontMask) {
      mIndexList.setSize(poly.vertexStart);
      mPolyList.decrement();
      return;
   }

   // Potentially, this will add up to mPlaneList.size() * (indexStart - indexEnd) 
   // elements to mIndexList, so ensure that it has enough space to store that
   // so we can use push_back_noresize. If you find this code block getting hit
   // frequently, changing the value of 'IndexListReserveSize' or doing some selective
   // allocation is suggested
   //
   // TODO: Re-visit this, since it obviously does not work correctly, and than
   // re-enable the push_back_noresize
   //while(mIndexList.size() + mPlaneList.size() * (mIndexList.size() - indexStart) > mIndexList.capacity() )
   //   mIndexList.reserve(mIndexList.capacity() * 2);

   // Need to do some clipping
   for (U32 p = 0; p < mPlaneList.size(); p++) {
      U32 pmask = 1 << p;
      // Only test against this plane if we have something
      // on both sides
      if (crossMask & pmask) {
         U32 indexEnd = mIndexList.size();
         U32 i1 = indexEnd - 1;
         U32 mask1 = mVertexList[mIndexList[i1]].mask;

         for (U32 i2 = indexStart; i2 < indexEnd; i2++) {
            U32 mask2 = mVertexList[mIndexList[i2]].mask;
            if ((mask1 ^ mask2) & pmask) {
               //
               mVertexList.increment();
               VectorF& v1 = mVertexList[mIndexList[i1]].point;
               VectorF& v2 = mVertexList[mIndexList[i2]].point;
               VectorF vv = v2 - v1;
               F32 t = -mPlaneList[p].distToPlane(v1) / mDot(mPlaneList[p],vv);

               mIndexList.push_back/*_noresize*/(mVertexList.size() - 1);
               Vertex& iv = mVertexList.last();
               iv.point.x = v1.x + vv.x * t;
               iv.point.y = v1.y + vv.y * t;
               iv.point.z = v1.z + vv.z * t;
               iv.mask = 0;

               // Test against the remaining planes
               for (U32 i = p + 1; i < mPlaneList.size(); i++)
                  if (mPlaneList[i].distToPlane(iv.point) > 0) {
                     iv.mask = 1 << i;
                     break;
                  }
            }
            if (!(mask2 & pmask)) {
               U32 index = mIndexList[i2];
               mIndexList.push_back/*_noresize*/(index);
            }
            mask1 = mask2;
            i1 = i2;
         }

         // Check for degenerate
         indexStart = indexEnd;
         if (mIndexList.size() - indexStart < 3) {
            mIndexList.setSize(poly.vertexStart);
            mPolyList.decrement();
            return;
         }
      }
   }

   // Emit what's left and compress the index list.
   poly.vertexCount = mIndexList.size() - indexStart;
   memcpy(&mIndexList[poly.vertexStart],
      &mIndexList[indexStart],poly.vertexCount);
   mIndexList.setSize(poly.vertexStart + poly.vertexCount);
}


//----------------------------------------------------------------------------

void ClippedPolyList::memcpy(U32* dst, U32* src,U32 size)
{
   U32* end = src + size;
   while (src != end)
      *dst++ = *src++;
}

void ClippedPolyList::cullUnusedVerts()
{
   PROFILE_SCOPE( ClippedPolyList_CullUnusedVerts );

   U32 i = 0;
   U32 k, n, numDeleted;
   bool result;

   IndexListIterator iNextIter;
   VertexListIterator nextVIter;
   VertexListIterator vIter;

   for ( vIter = mVertexList.begin(); vIter != mVertexList.end(); vIter++, i++ )
   {
      // Is this vertex used?
      iNextIter = find( mIndexList.begin(), mIndexList.end(), i );
      if ( iNextIter != mIndexList.end() )
         continue;

      // If not, find the next used vertex.

      // i is an unused vertex
      // k is a used vertex
      // delete the vertices from i to j - 1
      k = 0;
      n = i + 1;
      result = false;
      numDeleted = 0;

      for ( nextVIter = vIter + 1; nextVIter != mVertexList.end(); nextVIter++, n++ )
      {
         iNextIter = find( mIndexList.begin(), mIndexList.end(), n );
         
         // If we found a used vertex
         // grab its index for later use
         // and set our result bool.
         if ( (*iNextIter) == n )
         {
            k = n;
            result = true;
            break;
         }
      }

      // All the remaining verts are unused.
      if ( !result )
      {
         mVertexList.setSize( i );
         break;
      }

      // Erase unused verts.
      numDeleted = (k-1) - i + 1;       
      mVertexList.erase( i, numDeleted );

      // Find any references to vertices after those deleted
      // in the mIndexList and correct with an offset
      for ( iNextIter = mIndexList.begin(); iNextIter != mIndexList.end(); iNextIter++ )
      {
         if ( (*iNextIter) > i )
             (*iNextIter) -= numDeleted;
      }

      // After the erase the current iter should
      // point at the used vertex we found... the
      // loop will continue with the next vert.
   }
}

void ClippedPolyList::triangulate()
{
   PROFILE_SCOPE( ClippedPolyList_Triangulate );

   // Build into a new polylist and index list.
   //
   // TODO: There are potential performance issues
   // here as we're not reserving enough space for
   // new generated triangles.
   //
   // We need to either over estimate and shrink or 
   // better yet fix vector to internally grow in 
   // large chunks.
   //
   PolyList polyList;
   polyList.reserve( mPolyList.size() );
   IndexList indexList;
   indexList.reserve( mIndexList.size() );
   
   U32 j, numTriangles;

   //
   PolyListIterator polyIter = mPolyList.begin();
   for ( ; polyIter != mPolyList.end(); polyIter++ )
   {
      const Poly &poly = *polyIter;

      // How many triangles in this poly?
      numTriangles = poly.vertexCount - 2;        

      // Build out the triangles.
      for ( j = 0; j < numTriangles; j++ )
      {
         polyList.increment();
         
         Poly &triangle = polyList.last();
         triangle = poly;
         triangle.vertexCount = 3;
         triangle.vertexStart = indexList.size();

         indexList.push_back( mIndexList[ poly.vertexStart ] );
         indexList.push_back( mIndexList[ poly.vertexStart + 1 + j ] );
         indexList.push_back( mIndexList[ poly.vertexStart + 2 + j ] );
      }
   } 

   mPolyList = polyList;
   mIndexList = indexList;
}

void ClippedPolyList::generateNormals()
{
   PROFILE_SCOPE( ClippedPolyList_GenerateNormals );

   mNormalList.setSize( mVertexList.size() );

   U32 i, polyCount;
   VectorF normal;
   PolyListIterator polyIter;
   IndexListIterator indexIter;

   Vector<VectorF>::iterator normalIter = mNormalList.begin();
   U32 n = 0;
   for ( ; normalIter != mNormalList.end(); normalIter++, n++ )
   {
      // Average all the face normals which 
      // share this vertex index.
      indexIter = mIndexList.begin();
      normal.zero();
      polyCount = 0;
      i = 0;

      for ( ; indexIter != mIndexList.end(); indexIter++, i++ )
      {
         if ( n != *indexIter )
            continue;

         polyIter = mPolyList.begin();
         for ( ; polyIter != mPolyList.end(); polyIter++ )
         {
            const Poly& poly = *polyIter;
            if ( i < poly.vertexStart || i > poly.vertexStart + poly.vertexCount )
               continue;

            ++polyCount;
            normal += poly.plane;
         }        
      }

      // Average it.
      if ( polyCount > 0 )
         normal /= (F32)polyCount;

      // Note: we use a temporary for the normal averaging 
      // then copy the result to limit the number of arrays
      // we're touching during the innermost loop.
      *normalIter = normal;
   }
}
