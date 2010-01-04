//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _OPTIMIZEDPOLYLIST_H_
#define _OPTIMIZEDPOLYLIST_H_

#ifndef _ABSTRACTPOLYLIST_H_
#include "collision/abstractPolyList.h"
#endif

#define DEV 0.01

/// A concrete, renderable PolyList
///
/// This class is used to store geometry from a PolyList query.
///
/// It allows you to render this data, as well.
/// 
/// @see AbstractPolyList
class OptimizedPolyList : public AbstractPolyList
{
  public:

   enum PolyType
   {
      TriangleFan,
      TriangleStrip,
      TriangleList
   };

   struct VertIndex
   {
      S32 vertIdx;
      S32 normalIdx;
      S32 uv0Idx;
      S32 uv1Idx;

      VertIndex()
         : vertIdx( -1 ),
           normalIdx ( -1 ),
           uv0Idx( -1 ),
           uv1Idx( -1 )
      {
      }

      bool operator==(const VertIndex& _test) const
      {
         return ( vertIdx   == _test.vertIdx &&
                  normalIdx == _test.normalIdx &&
                  uv0Idx    == _test.uv0Idx &&
                  uv1Idx    == _test.uv1Idx );
      }
   };

   struct Poly
   {
      S32 plane;
      S32 material;
      U32 vertexStart;
      U32 vertexCount;
      U32 surfaceKey;

      SceneObject* object;

      PolyType type;

      Poly()
         : plane( -1 ),
           object( NULL ),
           vertexCount( 0 ),
           material( NULL ),
           type( TriangleFan )
      {
      }
   };

   // Vertex data
   Vector<Point3F>   mPoints;
   Vector<Point3F>   mNormals;
   Vector<Point2F>   mUV0s;
   Vector<Point2F>   mUV1s;

   // List of the VertIndex structure that puts
   // all of the vertex data together
   Vector<VertIndex> mVertexList;

   // Polygon data
   Vector<U32>       mIndexList;
   Vector<PlaneF>    mPlaneList;

   Vector<BaseMatInstance*> mMaterialList;

   // The Polygon structure puts the vertex data
   // and the polygon together
   Vector<Poly>      mPolyList;

  public:
   OptimizedPolyList();
   ~OptimizedPolyList();
   void clear();

   // Virtual methods
   U32  addPoint(const Point3F& p);
   U32  addPlane(const PlaneF& plane);

   void begin(BaseMatInstance* material, U32 surfaceKey);
   void begin(BaseMatInstance* material, U32 surfaceKey, PolyType type);
   void plane(U32 v1, U32 v2, U32 v3);
   void plane(const PlaneF& p);
   void plane(const U32 index);
   void vertex(U32 vi);
   void vertex(const Point3F& p);
   void vertex(const Point3F& p,
               const Point3F& normal,
               const Point2F& uv0 = Point2F(0.0f, 0.0f),
               const Point2F& uv1 = Point2F(0.0f, 0.0f));
   void end();

   U32 insertPoint(const Point3F& point);
   U32 insertNormal(const Point3F& normal);
   U32 insertUV0(const Point2F& uv);
   U32 insertUV1(const Point2F& uv);
   U32 insertPlane(const PlaneF& plane);
   U32 insertMaterial(BaseMatInstance* baseMat);

   U32 insertVertex(const Point3F& point,
                    const Point3F& normal = Point3F(0.0f, 0.0f, 1.0f),
                    const Point2F& uv0    = Point2F(0.0f, 0.0f),
                    const Point2F& uv1    = Point2F(0.0f, 0.0f));

   bool isEmpty() const;

  protected:
   const PlaneF& getIndexedPlane(const U32 index);
};

#endif  // _OPTIMIZEDPOLYLIST_H_
