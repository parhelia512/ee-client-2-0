//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PXTSSTATIC_H_
#define _T3D_PHYSICS_PXTSSTATIC_H_

#ifndef _T3D_PHYSICS_PHYSICSSTATIC_H_
#include "T3D/physics/physicsStatic.h"
#endif
#ifndef _PXUSERDATA_H_
#include "T3D/physics/physx/pxUserData.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif

class NxActor;
class NxConvexMesh;
class NxTriangleMesh;
class PxWorld;
class TSStatic;
class TSShape;


class PxTSStatic : public PhysicsStatic
{
protected:

   TSStatic *mTSStatic;

   PxWorld *mWorld;

   NxActor *mActor;

   /// The userdata object assigned to our actor.
   PxUserData mUserData;

   Point3F mScale;

   Vector<NxTriangleMesh*> mTriangleMeshes;

   typedef HashTable<TSShape*,NxTriangleMesh*> TriangleMeshMap;
   static TriangleMeshMap smCachedTriangleMeshes;

   static void _loadTriangleMeshes( TSStatic *tsStatic, Vector<NxTriangleMesh*> *triangleMeshes );

   void _releaseActor();

   PxTSStatic();

   bool _initTriangle( PxWorld *world, TSStatic *tsStatic );

public:

   virtual ~PxTSStatic();

   // PhysicsStatic
   void setTransform( const MatrixF &xfm );
   void setScale( const Point3F &scale );


   static PxTSStatic* create( TSStatic *tsStatic, PxWorld *world );

   static void freeMeshCache();

};

#endif // _T3D_PHYSICS_PXTSSTATIC_H_