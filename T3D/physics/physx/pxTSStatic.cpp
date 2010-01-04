//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxTSStatic.h"

#include "T3D/physics/physX/px.h"
#include "T3D/physics/physX/pxWorld.h"
#include "T3D/physics/physX/pxMaterial.h"

#include "T3D/physics/physX/pxStream.h"

#include "T3D/physics/physX/pxUserData.h"

#include "collision/concretePolyList.h"
#include "core/frameAllocator.h"
#include "ts/tsShapeInstance.h"
#include "T3D/tsStatic.h"


PxTSStatic::TriangleMeshMap PxTSStatic::smCachedTriangleMeshes;


PxTSStatic::PxTSStatic() :
   mWorld( NULL ),
   mActor( NULL ),
   mTSStatic( NULL ),
   mScale( Point3F::One )
{   
}

PxTSStatic::~PxTSStatic()
{
   _releaseActor();
}

void PxTSStatic::_releaseActor()
{
   if ( !mWorld )
      return;

   if ( mActor )
      mWorld->releaseActor( *mActor );

   mWorld = NULL;
   mActor = NULL;
   mScale = Point3F::One;

   // TODO: We need to check the getReferenceCount() on the 
   // mesh objects and release them... if not unscaled collision
   // shapes never get freed.

   if ( !mTriangleMeshes.empty() )
   {
      // releaseConvexMesh() requires that both the server 
      // and client scenes to be writeable.
      PxWorld::releaseWriteLocks();

      for ( U32 i=0; i < mTriangleMeshes.size(); i++ )
         gPhysicsSDK->releaseTriangleMesh( *mTriangleMeshes[i] );

      mTriangleMeshes.clear();
   }
}

void PxTSStatic::freeMeshCache()
{
   TriangleMeshMap::Iterator trimesh = smCachedTriangleMeshes.begin();
   for ( ; trimesh != smCachedTriangleMeshes.end(); trimesh++ )
   {
      if ( trimesh->value )
         gPhysicsSDK->releaseTriangleMesh( *trimesh->value );
   }
   smCachedTriangleMeshes.clear();
}

PxTSStatic* PxTSStatic::create( TSStatic *tsStatic, PxWorld *world )
{
   AssertFatal( world, "PxTSStatic::create() - No world found!" );

   if ( tsStatic->getCollisionType() == TSStatic::None )
      return NULL;

   PxTSStatic *pxStatic = new PxTSStatic();
   
   bool isOk = false;
   if ( tsStatic->getCollisionType() == TSStatic::CollisionMesh ||
        tsStatic->getCollisionType() == TSStatic::VisibleMesh )
      isOk = pxStatic->_initTriangle( world, tsStatic );         

   if ( !isOk )
   {
      delete pxStatic;
      return NULL;
   }
   
   return pxStatic;
}

bool PxTSStatic::_initTriangle( PxWorld *world, TSStatic *tsStatic )
{
   // Without a shape we have nothing to do!
   if ( !tsStatic->getShapeInstance() ) 
      return false;

   mWorld = world;
   mTSStatic = tsStatic;
   mScale = tsStatic->getScale();
   mUserData.setObject( tsStatic );

   // Mesh cooking requires that both 
   // scenes not be write locked!
   PxWorld::releaseWriteLocks();

   Vector<NxTriangleMeshShapeDesc> triangleShapeDescs;

   // If the static is unscaled then grab the 
   // convex meshes from the cache.
   const bool unscaled = mScale.equal( VectorF( 1.0f, 1.0f, 1.0f ) );
   if ( unscaled )
   {
      TriangleMeshMap::Iterator iter = smCachedTriangleMeshes.find( tsStatic->getShape() );

      while ( iter != smCachedTriangleMeshes.end() &&
              iter->key == tsStatic->getShape() )
      {
         triangleShapeDescs.increment();
         triangleShapeDescs.last().meshData = iter->value;
         iter++;
      }
   }
   
   // If we still don't have shapes either they
   // haven't been cached yet or we're scaled.
   if ( triangleShapeDescs.empty() )
   {
      _loadTriangleMeshes( tsStatic, &mTriangleMeshes );

      for ( U32 i=0; i < mTriangleMeshes.size(); i++ )
      {
         triangleShapeDescs.increment();
         triangleShapeDescs.last().meshData = mTriangleMeshes[i];
   
         // If it was unscaled cache it for later!
         if ( unscaled )
            smCachedTriangleMeshes.insertEqual( tsStatic->getShape(), 
                                                mTriangleMeshes[i] );
      }

      // If we're unscaled we can clear the
      // meshes as the cache will delete them.
      if ( unscaled )
         mTriangleMeshes.clear();
   }

   // Still without shapes?  We have nothing to build.
   if ( triangleShapeDescs.empty() )
      return false;

   // Create the actor.
   NxActorDesc actorDesc;
   actorDesc.body = NULL;
   actorDesc.name = tsStatic->getName();
   actorDesc.userData = &mUserData;
   actorDesc.globalPose.setRowMajor44( tsStatic->getTransform() );
   for ( U32 i=0; i < triangleShapeDescs.size(); i++ )
      actorDesc.shapes.push_back( &triangleShapeDescs[i] );

   mActor = mWorld->getScene()->createActor( actorDesc );

   return true;
}

void PxTSStatic::_loadTriangleMeshes( TSStatic *tsStatic, Vector<NxTriangleMesh*> *triangleMeshes )
{
   TSShapeInstance *shapeInst = tsStatic->getShapeInstance();
   TSShape *shape = shapeInst->getShape();

   MatrixF scaledMat;

   NxInitCooking();

   Vector<U32> triangles;
   Vector<Point3F> verts;
   Point3F tempVert;

   for ( S32 i = 0; i < tsStatic->mCollisionDetails.size(); i++ )
   {
      const TSShape::Detail& detail = shape->details[tsStatic->mCollisionDetails[i]];

      const S32 ss = detail.subShapeNum;
      if ( ss < 0 )
         return;

      const S32 start = shape->subShapeFirstObject[ss];
      const S32 end   = start + shape->subShapeNumObjects[ss];

      for ( S32 j = start; j < end; j++ )
      {
         TSShapeInstance::MeshObjectInstance* meshInst = &shapeInst->mMeshObjects[j];
         if ( !meshInst )
            continue;

         TSMesh* mesh = meshInst->getMesh( 0 );

         if( !mesh )
            continue;
      
         // Clear the temp vectors for the next mesh.
         triangles.clear();
         verts.clear();

         // Figure out how many triangles we have...
         U32 triCount = 0;
         const U32 base = 0;
         for ( U32 j = 0; j < mesh->primitives.size(); j++ )
         {
            TSDrawPrimitive & draw = mesh->primitives[j];
            
            const U32 start = draw.start;

            AssertFatal(draw.matIndex & TSDrawPrimitive::Indexed,"TSMesh::buildPolyList (1)");

            // gonna depend on what kind of primitive it is...
            if ( (draw.matIndex & TSDrawPrimitive::TypeMask) == TSDrawPrimitive::Triangles)
               triCount += draw.numElements / 3;
            else
            {
               // Have to walk the tristrip to get a count... may have degenerates
               U32 idx0 = base + mesh->indices[start + 0];
               U32 idx1;
               U32 idx2 = base + mesh->indices[start + 1];
               U32 * nextIdx = &idx1;
               for ( S32 k = 2; k < draw.numElements; k++ )
               {
                  *nextIdx = idx2;
                  //            nextIdx = (j%2)==0 ? &idx0 : &idx1;
                  nextIdx = (U32*) ( (dsize_t)nextIdx ^ (dsize_t)&idx0 ^ (dsize_t)&idx1);
                  idx2 = base + mesh->indices[start + k];
                  if (idx0 == idx1 || idx0 == idx2 || idx1 == idx2)
                     continue;

                  triCount++;
               }
            }
         }
         
         // add the polys...
         for ( U32 j = 0; j < mesh->primitives.size(); j++ )
         {
            TSDrawPrimitive & draw = mesh->primitives[j];
            const U32 start = draw.start;

            AssertFatal(draw.matIndex & TSDrawPrimitive::Indexed,"TSMesh::buildPolyList (1)");

            // gonna depend on what kind of primitive it is...
            if ( (draw.matIndex & TSDrawPrimitive::TypeMask) == TSDrawPrimitive::Triangles)
            {
               for ( S32 k = 0; k < draw.numElements; )
               {

                  triangles.push_back( base + mesh->indices[start + k + 2] );
                  triangles.push_back( base + mesh->indices[start + k + 1] );
                  triangles.push_back( base + mesh->indices[start + k + 0] );

                  k += 3;
               }
            }
            else
            {
               AssertFatal((draw.matIndex & TSDrawPrimitive::TypeMask) == TSDrawPrimitive::Strip,"TSMesh::buildPolyList (2)");

               U32 idx0 = base + mesh->indices[start + 0];
               U32 idx1;
               U32 idx2 = base + mesh->indices[start + 1];
               U32 * nextIdx = &idx1;
               for ( S32 k = 2; k < draw.numElements; k++ )
               {
                  *nextIdx = idx2;
                  //            nextIdx = (j%2)==0 ? &idx0 : &idx1;
                  nextIdx = (U32*) ( (dsize_t)nextIdx ^ (dsize_t)&idx0 ^ (dsize_t)&idx1);
                  idx2 = base + mesh->indices[start + k];
                  if (idx0 == idx1 || idx0 == idx2 || idx1 == idx2)
                     continue;
                  
                  triangles.push_back( idx2 );
                  triangles.push_back( idx1 );
                  triangles.push_back( idx0 );
               }
            }
         }

         scaledMat = meshInst->getTransform();
         const Point3F &scale = tsStatic->getScale();
         
         // MatrixF .scale scales the
         // orthagonal rotation vectors.
         // So we need to scale the position after.
         scaledMat.scale( scale );
         scaledMat[3] *= scale.x;
         scaledMat[7] *= scale.y;
         scaledMat[11] *= scale.z;

         if ( mesh->mVertexData.isReady() )
         {
            for ( S32 j = 0; j < mesh->mNumVerts; j++ )
            {
               tempVert = mesh->mVertexData[j].vert();
               scaledMat.mulP( tempVert );
               verts.push_back( tempVert );
            }            
         }
         else
         {
            for ( S32 j = 0; j < mesh->verts.size(); j++ )
            {
               tempVert = mesh->verts[j];
               scaledMat.mulP( tempVert );
               verts.push_back( tempVert );
            }
         }         
       
         // Build thr triangle mesh.
         NxTriangleMeshDesc meshDesc;
         meshDesc.numVertices                = verts.size();
         meshDesc.numTriangles               = triCount;
         meshDesc.pointStrideBytes           = sizeof(NxVec3);
         meshDesc.triangleStrideBytes        = 3*sizeof(NxU32);
         meshDesc.points                     = verts.address();
         meshDesc.triangles                  = triangles.address();                           
         meshDesc.flags                      = 0;
        

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
            triangleMeshes->push_back( pxMesh );
         }
      }
   }

   NxCloseCooking();
}

void PxTSStatic::setTransform( const MatrixF &xfm )
{
   if ( !mActor )
      return;

   mWorld->releaseWriteLock();

   NxMat34 pose;
   pose.setRowMajor44( xfm );
   mActor->setGlobalPose( pose );
}

void PxTSStatic::setScale( const Point3F &scale )
{
   if ( !mWorld )
      return;

   // This sucks... be the only way to scale collision data
   // in PhysX is to recreate the actor and collision data.

   // To avoid unnessasary expensive work of recreating collision
   // data be sure that the scale has actually changed.
   if ( mActor && mScale.equal( scale ) )
      return;

   PxWorld *world = mWorld;

   _releaseActor();

   if ( mTSStatic->getCollisionType() == TSStatic::None )
      return;

   if ( mTSStatic->getCollisionType() == TSStatic::CollisionMesh ||
        mTSStatic->getCollisionType() == TSStatic::VisibleMesh )
      _initTriangle( world, mTSStatic );
}
