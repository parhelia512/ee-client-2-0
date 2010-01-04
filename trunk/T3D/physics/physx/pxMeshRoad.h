//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PXMESHROAD_H_
#define _T3D_PHYSICS_PXMESHROAD_H_

#ifndef _T3D_PHYSICS_PHYSICSSTATIC_H_
#include "T3D/physics/physicsStatic.h"
#endif
#ifndef _PXUSERDATA_H_
#include "T3D/physics/physx/pxUserData.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif

class NxActor;
class NxTriangleMesh;
class PxWorld;
class MeshRoad;


class PxMeshRoad : public PhysicsStatic
{
protected:

   MeshRoad *mRoad;

   PxWorld *mWorld;

   NxActor *mActor;

   /// The userdata object assigned to the actor.
   PxUserData mUserData;

   Vector<NxTriangleMesh*> mTriangleMeshes;
   //NxTriangleMesh *mTriangleMesh;
   
   //static void _loadTriangleMeshes( MeshRoad *meshRoad, Vector<NxTriangleMesh*> *triangleMeshes );

   void _releaseActor();

   PxMeshRoad();

   bool _initTriangle( PxWorld *world, MeshRoad *meshRoad );

public:

   virtual ~PxMeshRoad();

   // PhysicsStatic
   void setTransform( const MatrixF &xfm );
   void setScale( const Point3F &scale );


   static PxMeshRoad* create( MeshRoad *road, PxWorld *world );
};

#endif // _T3D_PHYSICS_PXMESHROAD_H_