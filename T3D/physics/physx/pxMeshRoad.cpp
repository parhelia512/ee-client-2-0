//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxMeshRoad.h"

#include "T3D/physics/physX/px.h"
#include "T3D/physics/physX/pxWorld.h"
#include "T3D/physics/physX/pxMaterial.h"
#include "T3D/physics/physX/pxStream.h"
#include "T3D/physics/physX/pxUserData.h"
#include "collision/concretePolyList.h"
#include "core/frameAllocator.h"
#include "environment/meshRoad.h"



PxMeshRoad::PxMeshRoad() 
 : mWorld( NULL ),
   mActor( NULL ),
   mRoad( NULL )
{
}

PxMeshRoad::~PxMeshRoad()
{
   _releaseActor();
}

void PxMeshRoad::_releaseActor()
{
   if ( !mWorld )
      return;

   if ( mActor )
      mWorld->releaseActor( *mActor );

   mWorld = NULL;
   mActor = NULL;

   // TODO: We need to check the getReferenceCount() on the 
   // mesh objects and release them... if not unscaled collision
   // shapes never get freed.

   if ( !mTriangleMeshes.empty() )
   {
      // releaseConvexMesh() requires that both the server 
      // and client scenes to be writable.
      PxWorld::releaseWriteLocks();

      for ( U32 i=0; i < mTriangleMeshes.size(); i++ )
         gPhysicsSDK->releaseTriangleMesh( *mTriangleMeshes[i] );

      mTriangleMeshes.clear();      
   }
}

PxMeshRoad* PxMeshRoad::create( MeshRoad *road, PxWorld *world )
{
   AssertFatal( world, "PxMeshRoad::create() - No world found!" );

   PxMeshRoad *pxMeshRoad = new PxMeshRoad();

   bool result = pxMeshRoad->_initTriangle( world, road );         

   if ( !result )
   {
      delete pxMeshRoad;
      return NULL;
   }

   return pxMeshRoad;
}

bool PxMeshRoad::_initTriangle( PxWorld *world, MeshRoad *road )
{
   mWorld = world;
   mRoad = road;

   // Mesh cooking requires that both 
   // scenes not be write locked!
   PxWorld::releaseWriteLocks();

   // Loop through MeshRoadSegment(s) building TriangleMeshShapeDesc(s).
   // If the road is long enough, we split it into multiples.
   
   Vector<NxTriangleMeshShapeDesc> triangleShapeDescs;
   F32 length = 0.0f;
   //const F32 tolLen = 25.0f;
   S32 start = -1;

   NxInitCooking();

   for ( U32 i = 0; i < road->getSegmentCount(); i++ )
   {
      const MeshRoadSegment &seg = road->getSegment( i );

      if ( start == -1 )
         start = i;

      length += seg.length();

      //if ( true || length > tolLen || i == road->getSegmentCount() - 1 )
      {         
         ConcretePolyList polyList;
         bool capFront = ( start == 0 );
         bool capEnd = ( i == road->getSegmentCount() - 1 );

         road->buildSegmentPolyList( &polyList, start, i, capFront, capEnd );
         
         // Build the triangle mesh.
         NxTriangleMeshDesc meshDesc;
         meshDesc.numVertices                = polyList.mVertexList.size();
         meshDesc.numTriangles               = polyList.mIndexList.size() / 3;
         meshDesc.pointStrideBytes           = sizeof(NxVec3);
         meshDesc.triangleStrideBytes        = 3*sizeof(NxU32);
         meshDesc.points                     = polyList.mVertexList.address();
         meshDesc.triangles                  = polyList.mIndexList.address();                           
         meshDesc.flags                      = NX_MF_FLIPNORMALS;

         // Ok... cook the mesh!
         NxCookingParams params;
         params.targetPlatform = PLATFORM_PC;
         params.skinWidth = 0.01f;
         params.hintCollisionSpeed = false;
         NxSetCookingParams( params );
         PxMemStream cooked;	
         if ( NxCookTriangleMesh( meshDesc, cooked ) )
         {
            cooked.resetPosition();
            NxTriangleMesh *pxMesh = gPhysicsSDK->createTriangleMesh( cooked );
            mTriangleMeshes.push_back( pxMesh );
            triangleShapeDescs.increment();
            triangleShapeDescs.last().meshData = pxMesh;
         }

         start = -1;
         length = 0.0f;
      }         
   }
      
   NxCloseCooking();

   // Create the actor.
   NxActorDesc actorDesc;
   actorDesc.body = NULL;
   actorDesc.name = road->getName();
   //actorDesc.globalPose.setRowMajor44( road->getTransform() );
   //actorDesc.shapes.push_back( meshDesc );
   for ( U32 i = 0; i < triangleShapeDescs.size(); i++ )
      actorDesc.shapes.push_back( &triangleShapeDescs[i] );

   mUserData.setObject( road );
   actorDesc.userData = &mUserData;

   mActor = mWorld->getScene()->createActor( actorDesc );

   return true;
}

void PxMeshRoad::setTransform( const MatrixF &xfm )
{
   if ( !mActor )
      return;

   mWorld->releaseWriteLock();

   NxMat34 pose;
   pose.setRowMajor44( xfm );
   mActor->setGlobalPose( pose );
}

void PxMeshRoad::setScale( const Point3F &scale )
{          
}
