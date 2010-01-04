//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TSSTATIC_H_
#define _TSSTATIC_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _CONVEX_H_
#include "collision/convex.h"
#endif
#ifndef __RESOURCE_H__
#include "core/resource.h"
#endif
#ifndef _TSSHAPE_H_
#include "ts/tsShape.h"
#endif
#ifndef _ITICKABLE_H_
#include "core/iTickable.h"
#endif

class TSShapeInstance;
class TSThread;
class TSStatic;
class PhysicsStatic;
struct ObjectRenderInst;

//--------------------------------------------------------------------------
class TSStaticPolysoupConvex : public Convex
{
   typedef Convex Parent;
   friend class TSMesh;

public:
   TSStaticPolysoupConvex();
   ~TSStaticPolysoupConvex() {};

public:
   Box3F                box;
   Point3F              verts[4];
   PlaneF               normal;
   S32                  idx;
   TSMesh               *mesh;

   static SceneObject* smCurObject;

public:

   // Returns the bounding box in world coordinates
   Box3F getBoundingBox() const;
   Box3F getBoundingBox(const MatrixF& mat, const Point3F& scale) const;

   void getFeatures(const MatrixF& mat,const VectorF& n, ConvexFeature* cf);

   // This returns a list of convex faces to collide against
   void getPolyList(AbstractPolyList* list);

   // This returns the furthest point from the input vector
   Point3F support(const VectorF& v) const;
};

//--------------------------------------------------------------------------
class TSStatic : public SceneObject, public ITickable
{
   typedef SceneObject Parent;

   static U32 smUniqueIdentifier;

   enum Constants 
   {
      LOSOverrideOffset = 8
   };

   enum MaskBits 
   {
      AdvancedStaticOptionsMask = Parent::NextFreeMask,
      UpdateCollisionMask = Parent::NextFreeMask << 1,
      NextFreeMask = Parent::NextFreeMask << 2
   };

public:

   enum CollisionType
   {
      None = 0,
      Bounds = 1,
      CollisionMesh = 2,
      VisibleMesh = 3
   };
   
  protected:
   bool onAdd();
   void onRemove();

   // Collision
   void prepCollision();
   bool castRay(const Point3F &start, const Point3F &end, RayInfo* info);
   bool castRayRendered(const Point3F &start, const Point3F &end, RayInfo* info);
   bool buildPolyList(AbstractPolyList* polyList, const Box3F &box, const SphereF& sphere);
   bool buildRenderedPolyList(AbstractPolyList* polyList, const Box3F &box, const SphereF &sphere);
   void buildConvex(const Box3F& box, Convex* convex);
   
   bool _createShape();

   void _renderNormals( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );

   void _onResourceChanged( ResourceBase::Signature sig, const Torque::Path &path );

   // iTickable interface
   virtual void interpolateTick( F32 delta );
   virtual void processTick();
   virtual void advanceTime( F32 timeDelta );

  protected:

   Convex* mConvexList;

   StringTableEntry  mShapeName;
   U32               mShapeHash;
   Resource<TSShape> mShape;
   TSShapeInstance*  mShapeInstance;

   bool              mPlayAmbient;
   TSThread*         mAmbientThread;

   CollisionType     mCollisionType;

   bool mAllowPlayerStep;

   PhysicsStatic *mPhysicsRep;

   F32 mRenderNormalScalar;

public:
   Vector<S32>            mCollisionDetails;
   Vector<S32>            mLOSDetails;

   // Rendering
  protected:
   
   bool prepRenderImage  ( SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseZoneState=false);
   void renderObject     ( SceneState *state);

  public:
   TSStatic();
   ~TSStatic();

   DECLARE_CONOBJECT(TSStatic);
   static void initPersistFields();

   // NetObject
   U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   void unpackUpdate( NetConnection *conn, BitStream *stream );

   // SceneObject
   void setTransform( const MatrixF &mat );
   void setScale( const VectorF &scale );

   void inspectPostApply();

   CollisionType getCollisionType() { return mCollisionType; }

   bool allowPlayerStep() const { return mAllowPlayerStep; }

   Resource<TSShape> getShape() const { return mShape; }
	StringTableEntry getShapeFileName() { return mShapeName; }
  
   TSShapeInstance* getShapeInstance() const { return mShapeInstance; }

   const Vector<S32>& getCollisionDetails() const { return mCollisionDetails; }

   const Vector<S32>& getLOSDetails() const { return mLOSDetails; }

};

#endif // _H_TSSTATIC

