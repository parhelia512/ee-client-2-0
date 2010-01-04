//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "math/mMath.h"
#include "core/color.h"
#include "console/console.h"
#include "collision/optimizedPolyList.h"
#include "materials/baseMatInstance.h"
#include "materials/materialDefinition.h"

//----------------------------------------------------------------------------

OptimizedPolyList::OptimizedPolyList()
{
   VECTOR_SET_ASSOCIATION(mPoints);
   VECTOR_SET_ASSOCIATION(mNormals);
   VECTOR_SET_ASSOCIATION(mUV0s);
   VECTOR_SET_ASSOCIATION(mUV1s);
   VECTOR_SET_ASSOCIATION(mVertexList);
   VECTOR_SET_ASSOCIATION(mIndexList);
   VECTOR_SET_ASSOCIATION(mPlaneList);
   VECTOR_SET_ASSOCIATION(mPolyList);

   mIndexList.reserve(100);

   mCurrObject       = NULL;
   mBaseMatrix       = MatrixF::Identity;
   mMatrix           = MatrixF::Identity;
   mTransformMatrix  = MatrixF::Identity;
   mScale.set(1.0f, 1.0f, 1.0f);

   mPlaneTransformer.setIdentity();

   mInterestNormalRegistered = false;
}

OptimizedPolyList::~OptimizedPolyList()
{
   mPoints.clear();
   mNormals.clear();
   mUV0s.clear();
   mUV1s.clear();
   mVertexList.clear();
   mIndexList.clear();
   mPlaneList.clear();
   mPolyList.clear();
}


//----------------------------------------------------------------------------
void OptimizedPolyList::clear()
{
   mPoints.clear();
   mNormals.clear();
   mUV0s.clear();
   mUV1s.clear();
   mVertexList.clear();
   mIndexList.clear();
   mPlaneList.clear();
   mPolyList.clear();
}

//----------------------------------------------------------------------------
U32 OptimizedPolyList::insertPoint(const Point3F& point)
{
   S32 retIdx = -1;

   for (U32 i = 0; i < mPoints.size(); i++)
   {
      if (mPoints[i].equal(point))
      {
         retIdx = i;
         break;
      }
   }

   if (retIdx == -1)
   {
      retIdx = mPoints.size();
      mPoints.push_back(point);

      // Apply the transform
      mPoints.last() *= mScale;
      mMatrix.mulP(mPoints.last());
   }

   return (U32)retIdx;
}

U32 OptimizedPolyList::insertNormal(const Point3F& normal)
{
   S32 retIdx = -1;

   for (U32 i = 0; i < mNormals.size(); i++)
   {
      if (mNormals[i].equal(normal))
      {
         retIdx = i;
         break;
      }
   }

   if (retIdx == -1)
   {
      retIdx = mNormals.size();
      mNormals.push_back(normal);
   }

   return (U32)retIdx;
}

U32 OptimizedPolyList::insertUV0(const Point2F& uv)
{
   S32 retIdx = -1;

   for (U32 i = 0; i < mUV0s.size(); i++)
   {
      if (mUV0s[i].equal(uv))
      {
         retIdx = i;
         break;
      }
   }

   if (retIdx == -1)
   {
      retIdx = mUV0s.size();
      mUV0s.push_back(uv);
   }

   return (U32)retIdx;
}

U32 OptimizedPolyList::insertUV1(const Point2F& uv)
{
   S32 retIdx = -1;

   for (U32 i = 0; i < mUV1s.size(); i++)
   {
      if (mUV1s[i].equal(uv))
      {
         retIdx = i;
         break;
      }
   }

   if (retIdx == -1)
   {
      retIdx = mUV1s.size();
      mUV1s.push_back(uv);
   }

   return (U32)retIdx;
}

U32 OptimizedPolyList::insertPlane(const PlaneF& plane)
{
   S32 retIdx = -1;

   for (U32 i = 0; i < mPlaneList.size(); i++)
   {
      if (mPlaneList[i].equal(plane) &&
          mFabs( mPlaneList[i].d - plane.d ) < POINT_EPSILON)
      {
         retIdx = i;
         break;
      }
   }

   if (retIdx == -1)
   {
      retIdx = mPlaneList.size();

      // Apply the transform
      PlaneF transPlane;
      mPlaneTransformer.transform(plane, transPlane);

      mPlaneList.push_back(transPlane);
   }

   return (U32)retIdx;
}

U32 OptimizedPolyList::insertMaterial(BaseMatInstance* baseMat)
{
   S32 retIdx = -1;

   Material* mat = dynamic_cast<Material*>(baseMat->getMaterial());

   for (U32 i = 0; i < mMaterialList.size(); i++)
   {
      Material* testMat = dynamic_cast<Material*>(mMaterialList[i]->getMaterial());

      if (mat && testMat)
      {
         if (testMat == mat)
         {
            retIdx = i;
            break;
         }
      }
      else if (mMaterialList[i] == baseMat)
      {
         retIdx = i;
         break;
      }
   }

   if (retIdx == -1)
   {
      retIdx = mMaterialList.size();
      mMaterialList.push_back(baseMat);
   }

   return (U32)retIdx;
}

U32 OptimizedPolyList::insertVertex(const Point3F& point, const Point3F& normal,
                                    const Point2F& uv0, const Point2F& uv1)
{
   VertIndex vert;

   vert.vertIdx   = insertPoint(point);
   vert.normalIdx = insertNormal(normal);
   vert.uv0Idx    = insertUV0(uv0);
   vert.uv1Idx    = insertUV1(uv1);

   return mVertexList.push_back_unique(vert);
}

U32 OptimizedPolyList::addPoint(const Point3F& p)
{
   return insertVertex(p);
}

U32 OptimizedPolyList::addPlane(const PlaneF& plane)
{
   return insertPlane(plane);
}


//----------------------------------------------------------------------------

void OptimizedPolyList::begin(BaseMatInstance* material, U32 surfaceKey)
{
   mPolyList.increment();
   Poly& poly = mPolyList.last();
   poly.material = insertMaterial(material);
   poly.vertexStart = mIndexList.size();
   poly.surfaceKey = surfaceKey;
   poly.type = TriangleFan;
   poly.object = mCurrObject;
}

void OptimizedPolyList::begin(BaseMatInstance* material, U32 surfaceKey, PolyType type)
{
   begin(material, surfaceKey);

   // Set the type
   mPolyList.last().type = type;
}


//----------------------------------------------------------------------------

void OptimizedPolyList::plane(U32 v1, U32 v2, U32 v3)
{
   AssertFatal(v1 < mPoints.size() && v2 < mPoints.size() && v3 < mPoints.size(),
      "OptimizedPolyList::plane(): Vertex indices are larger than vertex list size");

   mPolyList.last().plane = addPlane(PlaneF(mPoints[v1], mPoints[v2], mPoints[v3]));
}

void OptimizedPolyList::plane(const PlaneF& p)
{
   mPolyList.last().plane = addPlane(p);
}

void OptimizedPolyList::plane(const U32 index)
{
   AssertFatal(index < mPlaneList.size(), "Out of bounds index!");
   mPolyList.last().plane = index;
}

const PlaneF& OptimizedPolyList::getIndexedPlane(const U32 index)
{
   AssertFatal(index < mPlaneList.size(), "Out of bounds index!");
   return mPlaneList[index];
}


//----------------------------------------------------------------------------

void OptimizedPolyList::vertex(U32 vi)
{
   mIndexList.push_back(vi);
}

void OptimizedPolyList::vertex(const Point3F& p)
{
   mIndexList.push_back(addPoint(p));
}

void OptimizedPolyList::vertex(const Point3F& p,
                               const Point3F& normal,
                               const Point2F& uv0,
                               const Point2F& uv1)
{
   mIndexList.push_back(insertVertex(p, normal, uv0, uv1));
}


//----------------------------------------------------------------------------

bool OptimizedPolyList::isEmpty() const
{
   return !mPolyList.size();
}

void OptimizedPolyList::end()
{
   Poly& poly = mPolyList.last();
   poly.vertexCount = mIndexList.size() - poly.vertexStart;
}
