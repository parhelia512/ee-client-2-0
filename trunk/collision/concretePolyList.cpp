//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "math/mMath.h"
#include "console/console.h"
#include "collision/concretePolyList.h"
#include "gfx/gfxDevice.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxStateBlock.h"


//----------------------------------------------------------------------------

ConcretePolyList::ConcretePolyList()
{
   VECTOR_SET_ASSOCIATION(mPolyList);
   VECTOR_SET_ASSOCIATION(mVertexList);
   VECTOR_SET_ASSOCIATION(mIndexList);
   VECTOR_SET_ASSOCIATION(mPolyPlaneList);

   mIndexList.reserve(100);
}

ConcretePolyList::~ConcretePolyList()
{

}


//----------------------------------------------------------------------------
void ConcretePolyList::clear()
{
   // Only clears internal data
   mPolyList.clear();
   mVertexList.clear();
   mIndexList.clear();
   mPolyPlaneList.clear();
}

//----------------------------------------------------------------------------

U32 ConcretePolyList::addPoint(const Point3F& p)
{
   mVertexList.increment();
   Point3F& v = mVertexList.last();
   v.x = p.x * mScale.x;
   v.y = p.y * mScale.y;
   v.z = p.z * mScale.z;
   mMatrix.mulP(v);

   return mVertexList.size() - 1;
}


U32 ConcretePolyList::addPlane(const PlaneF& plane)
{
   mPolyPlaneList.increment();
   mPlaneTransformer.transform(plane, mPolyPlaneList.last());

   return mPolyPlaneList.size() - 1;
}


//----------------------------------------------------------------------------

void ConcretePolyList::begin(BaseMatInstance* material,U32 surfaceKey)
{
   mPolyList.increment();
   Poly& poly = mPolyList.last();
   poly.object = mCurrObject;
   poly.material = material;
   poly.vertexStart = mIndexList.size();
   poly.surfaceKey = surfaceKey;
}


//----------------------------------------------------------------------------

void ConcretePolyList::plane(U32 v1,U32 v2,U32 v3)
{
   mPolyList.last().plane.set(mVertexList[v1],
      mVertexList[v2],mVertexList[v3]);
}

void ConcretePolyList::plane(const PlaneF& p)
{
   mPlaneTransformer.transform(p, mPolyList.last().plane); 
}

void ConcretePolyList::plane(const U32 index)
{
   AssertFatal(index < mPolyPlaneList.size(), "Out of bounds index!");
   mPolyList.last().plane = mPolyPlaneList[index];
}

const PlaneF& ConcretePolyList::getIndexedPlane(const U32 index)
{
   AssertFatal(index < mPolyPlaneList.size(), "Out of bounds index!");
   return mPolyPlaneList[index];
}


//----------------------------------------------------------------------------

void ConcretePolyList::vertex(U32 vi)
{
   mIndexList.push_back(vi);
}


//----------------------------------------------------------------------------

bool ConcretePolyList::isEmpty() const
{
   return mPolyList.empty();
}

void ConcretePolyList::end()
{
   Poly& poly = mPolyList.last();
   poly.vertexCount = mIndexList.size() - poly.vertexStart;
}

void ConcretePolyList::render()
{
   GFXStateBlockDesc solidZDisable;
   solidZDisable.setCullMode( GFXCullNone );
   solidZDisable.setZReadWrite( false, false );
   GFXStateBlockRef sb = GFX->createStateBlock( solidZDisable );
   GFX->setStateBlock( sb );

   PrimBuild::color3i( 255, 0, 255 );

   Poly *p;
   Point3F *pnt;

   for ( p = mPolyList.begin(); p < mPolyList.end(); p++ )
   {
      PrimBuild::begin( GFXLineStrip, p->vertexCount + 1 );      

      for ( U32 i = 0; i < p->vertexCount; i++ )
      {
         pnt = &mVertexList[mIndexList[p->vertexStart + i]];
         PrimBuild::vertex3fv( pnt );
      }

      pnt = &mVertexList[mIndexList[p->vertexStart]];
      PrimBuild::vertex3fv( pnt );

      PrimBuild::end();
   }   
}