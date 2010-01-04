//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "ts/tsShapeInstance.h"

#include "sceneGraph/sceneObject.h"
#include "collision/convex.h"
#include "T3D/tsStatic.h" // TODO: We shouldn't have this dependancy!

#include "platform/profiler.h"

#include "opcode/Opcode.h"
#include "opcode/Ice/IceAABB.h"
#include "opcode/Ice/IcePoint.h"
#include "opcode/OPC_AABBTree.h"
#include "opcode/OPC_AABBCollider.h"

static bool gOpcodeInitialized = false;

//-------------------------------------------------------------------------------------
// Collision methods
//-------------------------------------------------------------------------------------

bool TSShapeInstance::buildPolyList(AbstractPolyList * polyList, S32 dl)
{
   // if dl==-1, nothing to do
   if (dl==-1)
      return false;

   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::buildPolyList");

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   // nothing emitted yet...
   bool emitted = false;
   U32 surfaceKey = 0;

   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;
   if (start<end)
   {
      MatrixF initialMat;
      Point3F initialScale;
      polyList->getTransform(&initialMat,&initialScale);

      // set up for first object's node
      MatrixF mat;
      MatrixF scaleMat(true);
      F32* p = scaleMat;
      p[0]  = initialScale.x;
      p[5]  = initialScale.y;
      p[10] = initialScale.z;
      const MatrixF *previousMat = &mMeshObjects[start].getTransform();
      mat.mul(initialMat,scaleMat);
      mat.mul(*previousMat);
      polyList->setTransform(&mat,Point3F(1, 1, 1));

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         MeshObjectInstance * mesh = &mMeshObjects[i];

         if (od >= mesh->object->numMeshes)
            continue;

         if (&mesh->getTransform() != previousMat)
         {
            // different node from before, set up for this node
            previousMat = &mesh->getTransform();

            if (previousMat != NULL)
            {
               mat.mul(initialMat,scaleMat);
               mat.mul(*previousMat);
               polyList->setTransform(&mat,Point3F(1, 1, 1));
            }
         }
         // collide...
         emitted |= mesh->buildPolyList(od,polyList,surfaceKey,mMaterialList);
      }

      // restore original transform...
      polyList->setTransform(&initialMat,initialScale);
   }

   return emitted;
}

bool TSShapeInstance::getFeatures(const MatrixF& mat, const Point3F& n, ConvexFeature* cf, S32 dl)
{
   // if dl==-1, nothing to do
   if (dl==-1)
      return false;

   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::buildPolyList");

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   // nothing emitted yet...
   bool emitted = false;
   U32 surfaceKey = 0;

   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;
   if (start<end)
   {
      MatrixF final;
      const MatrixF* previousMat = &mMeshObjects[start].getTransform();
      final.mul(mat, *previousMat);

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         MeshObjectInstance * mesh = &mMeshObjects[i];

         if (od >= mesh->object->numMeshes)
            continue;

         if (&mesh->getTransform() != previousMat)
         {
            previousMat = &mesh->getTransform();
            final.mul(mat, *previousMat);
         }
         emitted |= mesh->getFeatures(od, final, n, cf, surfaceKey);
      }
   }
   return emitted;
}

bool TSShapeInstance::castRay(const Point3F & a, const Point3F & b, RayInfo * rayInfo, S32 dl)
{
   // if dl==-1, nothing to do
   if (dl==-1)
      return false;

   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::castRay");

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;
   RayInfo saveRay;
   saveRay.t = 1.0f;
   const MatrixF * saveMat = NULL;
   bool found = false;
   if (start<end)
   {
      Point3F ta, tb;

      // set up for first object's node
      MatrixF mat;
      const MatrixF * previousMat = &mMeshObjects[start].getTransform();
      mat = *previousMat;
      mat.inverse();
      mat.mulP(a,&ta);
      mat.mulP(b,&tb);

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         MeshObjectInstance * mesh = &mMeshObjects[i];

         if (od >= mesh->object->numMeshes)
            continue;

         if (&mesh->getTransform() != previousMat)
         {
            // different node from before, set up for this node
            previousMat = &mesh->getTransform();

            if (previousMat != NULL)
            {
               mat = *previousMat;
               mat.inverse();
               mat.mulP(a,&ta);
               mat.mulP(b,&tb);
            }
         }
         // collide...
         if (mesh->castRay(od,ta,tb,rayInfo, mMaterialList))
         {
            if (!rayInfo)
               return true;

            if (rayInfo->t <= saveRay.t)
            {
               saveRay = *rayInfo;
               saveMat = previousMat;
            }
            found = true;
         }
      }
   }

   // collide with any skins for this detail level...
   // TODO: if ever...

   // finalize the deal...
   if (found)
   {
      *rayInfo = saveRay;
      saveMat->mulV(rayInfo->normal);
      rayInfo->point  = b-a;
      rayInfo->point *= rayInfo->t;
      rayInfo->point += a;
   }
   return found;
}

bool TSShapeInstance::castRayRendered(const Point3F & a, const Point3F & b, RayInfo * rayInfo, S32 dl)
{
   // if dl==-1, nothing to do
   if (dl==-1)
      return false;

   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::castRayRendered");

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   if ( ss == -1 )
      return false;

   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;
   RayInfo saveRay;
   saveRay.t = 1.0f;
   const MatrixF * saveMat = NULL;
   bool found = false;
   if (start<end)
   {
      Point3F ta, tb;

      // set up for first object's node
      MatrixF mat;
      const MatrixF * previousMat = &mMeshObjects[start].getTransform();
      mat = *previousMat;
      mat.inverse();
      mat.mulP(a,&ta);
      mat.mulP(b,&tb);

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         MeshObjectInstance * mesh = &mMeshObjects[i];

         if (od >= mesh->object->numMeshes)
            continue;

         if (&mesh->getTransform() != previousMat)
         {
            // different node from before, set up for this node
            previousMat = &mesh->getTransform();

            if (previousMat != NULL)
            {
               mat = *previousMat;
               mat.inverse();
               mat.mulP(a,&ta);
               mat.mulP(b,&tb);
            }
         }
         // collide...
         if (mesh->castRayRendered(od,ta,tb,rayInfo, mMaterialList))
         {
            if (!rayInfo)
               return true;

            if (rayInfo->t <= saveRay.t)
            {
               saveRay = *rayInfo;
               saveMat = previousMat;
            }
            found = true;
         }
      }
   }

   // collide with any skins for this detail level...
   // TODO: if ever...

   // finalize the deal...
   if (found)
   {
      *rayInfo = saveRay;
      saveMat->mulV(rayInfo->normal);
      rayInfo->point  = b-a;
      rayInfo->point *= rayInfo->t;
      rayInfo->point += a;
   }
   return found;
}

Point3F TSShapeInstance::support(const Point3F & v, S32 dl)
{
   // if dl==-1, nothing to do
   AssertFatal(dl != -1, "Error, should never try to collide with a non-existant detail level!");
   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::support");

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;

   F32     currMaxDP   = -1e9f;
   Point3F currSupport = Point3F(0, 0, 0);
   const MatrixF * previousMat = NULL;
   MatrixF mat;
   if (start<end)
   {
      Point3F va;

      // set up for first object's node
      previousMat = &mMeshObjects[start].getTransform();
      mat = *previousMat;
      mat.inverse();

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         MeshObjectInstance * mesh = &mMeshObjects[i];

         if (od >= mesh->object->numMeshes)
            continue;

         TSMesh* physMesh = mesh->getMesh(od);
         if (physMesh && mesh->visible > 0.01f)
         {
            // collide...
            if (&mesh->getTransform() != previousMat)
            {
               // different node from before, set up for this node
               previousMat = &mesh->getTransform();
               mat = *previousMat;
               mat.inverse();
            }
            mat.mulV(v, &va);
            physMesh->support(mesh->frame, va, &currMaxDP, &currSupport);
         }
      }
   }

   if (currMaxDP != -1e9f)
   {
      previousMat->mulP(currSupport);
      return currSupport;
   }
   else
   {
      return Point3F(0, 0, 0);
   }
}

void TSShapeInstance::computeBounds(S32 dl, Box3F & bounds)
{
   // if dl==-1, nothing to do
   if (dl==-1)
      return;

   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::computeBounds");

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;

   // run through objects and updating bounds as we go
   bounds.minExtents.set( 10E30f, 10E30f, 10E30f);
   bounds.maxExtents.set(-10E30f,-10E30f,-10E30f);
   Box3F box;
   for (S32 i=start; i<end; i++)
   {
      MeshObjectInstance * mesh = &mMeshObjects[i];

      if (od >= mesh->object->numMeshes)
         continue;

      if (mesh->getMesh(od))
      {
         mesh->getMesh(od)->computeBounds(mesh->getTransform(),box);
         bounds.minExtents.setMin(box.minExtents);
         bounds.maxExtents.setMax(box.maxExtents);
      }
   }
}

//-------------------------------------------------------------------------------------
// Object (MeshObjectInstance & PluginObjectInstance) collision methods
//-------------------------------------------------------------------------------------

bool TSShapeInstance::ObjectInstance::buildPolyList(S32 objectDetail, AbstractPolyList *polyList, U32 &surfaceKey, TSMaterialList *materials )
{
   TORQUE_UNUSED( objectDetail );
   TORQUE_UNUSED( polyList );
   TORQUE_UNUSED( surfaceKey );
   TORQUE_UNUSED( materials );

   AssertFatal(0,"TSShapeInstance::ObjectInstance::buildPolyList:  no default method.");
   return false;
}

bool TSShapeInstance::ObjectInstance::getFeatures(S32 objectDetail, const MatrixF& mat, const Point3F& n, ConvexFeature* cf, U32& surfaceKey)
{
   TORQUE_UNUSED( objectDetail );
   TORQUE_UNUSED( mat );
   TORQUE_UNUSED( n );
   TORQUE_UNUSED( cf );
   TORQUE_UNUSED( surfaceKey );

   AssertFatal(0,"TSShapeInstance::ObjectInstance::buildPolyList:  no default method.");
   return false;
}

void TSShapeInstance::ObjectInstance::support(S32, const Point3F&, F32*, Point3F*)
{
   AssertFatal(0,"TSShapeInstance::ObjectInstance::supprt:  no default method.");
}

bool TSShapeInstance::ObjectInstance::castRay( S32 objectDetail, const Point3F &start, const Point3F &end, RayInfo *rayInfo, TSMaterialList *materials )
{
   TORQUE_UNUSED( objectDetail );
   TORQUE_UNUSED( start );
   TORQUE_UNUSED( end );
   TORQUE_UNUSED( rayInfo );

   AssertFatal(0,"TSShapeInstance::ObjectInstance::castRay:  no default method.");
   return false;
}

bool TSShapeInstance::MeshObjectInstance::buildPolyList( S32 objectDetail, AbstractPolyList *polyList, U32 &surfaceKey, TSMaterialList *materials )
{
   TSMesh * mesh = getMesh(objectDetail);
   if (mesh && visible>0.01f)
      return mesh->buildPolyList(frame,polyList,surfaceKey,materials);
   return false;
}

bool TSShapeInstance::MeshObjectInstance::getFeatures(S32 objectDetail, const MatrixF& mat, const Point3F& n, ConvexFeature* cf, U32& surfaceKey)
{
   TSMesh* mesh = getMesh(objectDetail);
   if (mesh && visible > 0.01f)
      return mesh->getFeatures(frame, mat, n, cf, surfaceKey);
   return false;
}

void TSShapeInstance::MeshObjectInstance::support(S32 objectDetail, const Point3F& v, F32* currMaxDP, Point3F* currSupport)
{
   TSMesh* mesh = getMesh(objectDetail);
   if (mesh && visible > 0.01f)
      mesh->support(frame, v, currMaxDP, currSupport);
}


bool TSShapeInstance::MeshObjectInstance::castRay( S32 objectDetail, const Point3F & start, const Point3F & end, RayInfo * rayInfo, TSMaterialList* materials )
{
   TSMesh* mesh = getMesh( objectDetail );
   if( mesh && visible > 0.01f )
      return mesh->castRay( frame, start, end, rayInfo, materials );
   return false;
}

bool TSShapeInstance::MeshObjectInstance::castRayRendered( S32 objectDetail, const Point3F & start, const Point3F & end, RayInfo * rayInfo, TSMaterialList* materials )
{
   TSMesh* mesh = getMesh( objectDetail );
   if( mesh && visible > 0.01f )
      return mesh->castRayRendered( frame, start, end, rayInfo, materials );
   return false;
}

bool TSShapeInstance::ObjectInstance::castRayOpcode( S32 objectDetail, const Point3F & start, const Point3F & end, RayInfo *rayInfo, TSMaterialList* materials )
{
   TORQUE_UNUSED( objectDetail );
   TORQUE_UNUSED( start );
   TORQUE_UNUSED( end );
   TORQUE_UNUSED( rayInfo );
   TORQUE_UNUSED( materials );

   return false;
}

bool TSShapeInstance::ObjectInstance::buildPolyListOpcode( S32 objectDetail, AbstractPolyList *polyList, U32 &surfaceKey, TSMaterialList *materials )
{
   TORQUE_UNUSED( objectDetail );
   TORQUE_UNUSED( polyList );
   TORQUE_UNUSED( surfaceKey );
   TORQUE_UNUSED( materials );

   return false;
}

bool TSShapeInstance::ObjectInstance::buildConvexOpcode( const MatrixF &mat, S32 objectDetail, const Box3F &bounds, Convex *c, Convex *list )
{
   TORQUE_UNUSED( mat );
   TORQUE_UNUSED( objectDetail );
   TORQUE_UNUSED( bounds );
   TORQUE_UNUSED( c );
   TORQUE_UNUSED( list );

   return false;
}

bool TSShapeInstance::MeshObjectInstance::castRayOpcode( S32 objectDetail, const Point3F & start, const Point3F & end, RayInfo *info, TSMaterialList* materials )
{
   TSMesh * mesh = getMesh(objectDetail);
   if (mesh && visible>0.01f)
      return mesh->castRayOpcode(start, end, info, materials);
   return false;
}

bool TSShapeInstance::MeshObjectInstance::buildPolyListOpcode( S32 objectDetail, AbstractPolyList *polyList, const Box3F &box, TSMaterialList *materials )
{
   TSMesh * mesh = getMesh(objectDetail);
   if ( mesh && visible > 0.01f && box.isOverlapped( mesh->getBounds() ) )
      return mesh->buildPolyListOpcode(frame,polyList,box,materials);
   return false;
}

bool TSShapeInstance::MeshObjectInstance::buildConvexOpcode( const MatrixF &mat, S32 objectDetail, const Box3F &bounds, Convex *c, Convex *list)
{
   TSMesh * mesh = getMesh(objectDetail);
   if ( mesh && visible > 0.01f && bounds.isOverlapped( mesh->getBounds() ) )
      return mesh->buildConvexOpcode(mat, bounds, c, list);
   return false;
}

bool TSShapeInstance::buildPolyListOpcode( S32 dl, AbstractPolyList *polyList, const Box3F &box )
{
	PROFILE_SCOPE( TSShapeInstance_buildPolyListOpcode_MeshObjInst );

   if (dl==-1)
      return false;

   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::buildPolyListOpcode");

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   if ( ss < 0 )
      return false;

   S32 od = detail->objectDetailNum;

   // nothing emitted yet...
   bool emitted = false;

   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;
   if (start<end)
   {
      MatrixF initialMat;
      Point3F initialScale;
      polyList->getTransform(&initialMat,&initialScale);

      // set up for first object's node
      MatrixF mat;
      MatrixF scaleMat(true);
      F32* p = scaleMat;
      p[0]  = initialScale.x;
      p[5]  = initialScale.y;
      p[10] = initialScale.z;
      const MatrixF * previousMat = &mMeshObjects[start].getTransform();
      mat.mul(initialMat,scaleMat);
      mat.mul(*previousMat);
      polyList->setTransform(&mat,Point3F(1, 1, 1));

      // Update our bounding box...
      Box3F localBox = box;
      MatrixF otherMat = mat;
      otherMat.inverse();
      otherMat.mul(localBox);

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         MeshObjectInstance * mesh = &mMeshObjects[i];

         if (od >= mesh->object->numMeshes)
            continue;

         if (&mesh->getTransform() != previousMat)
         {
            // different node from before, set up for this node
            previousMat = &mesh->getTransform();

            if (previousMat != NULL)
            {
               mat.mul(initialMat,scaleMat);
               mat.mul(*previousMat);
               polyList->setTransform(&mat,Point3F(1, 1, 1));

               // Update our bounding box...
               otherMat = mat;
               otherMat.inverse();
               localBox = box;
               otherMat.mul(localBox);
            }
         }

         // collide...
         emitted |= mesh->buildPolyListOpcode(od,polyList,localBox,mMaterialList);
      }

      // restore original transform...
      polyList->setTransform(&initialMat,initialScale);
   }

   return emitted;
}

bool TSShapeInstance::castRayOpcode( S32 dl, const Point3F & startPos, const Point3F & endPos, RayInfo *info)
{
   // if dl==-1, nothing to do
   if (dl==-1)
      return false;

   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::castRayOpcode");

   info->t = 100.f;

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   if ( ss < 0 )
      return false;

   S32 od = detail->objectDetailNum;

   // nothing emitted yet...
   bool emitted = false;

   const MatrixF* saveMat = NULL;
   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;
   if (start<end)
   {
      MatrixF mat;
      const MatrixF * previousMat = &mMeshObjects[start].getTransform();
      mat = *previousMat;
      mat.inverse();
      Point3F localStart, localEnd;
      mat.mulP(startPos, &localStart);
      mat.mulP(endPos, &localEnd);

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         MeshObjectInstance * mesh = &mMeshObjects[i];

         if (od >= mesh->object->numMeshes)
            continue;

         if (&mesh->getTransform() != previousMat)
         {
            // different node from before, set up for this node
            previousMat = &mesh->getTransform();

            if (previousMat != NULL)
            {
               mat = *previousMat;
               mat.inverse();
               mat.mulP(startPos, &localStart);
               mat.mulP(endPos, &localEnd);
            }
         }

         // collide...
         if ( mesh->castRayOpcode(od,localStart, localEnd, info, mMaterialList) )
         {
            saveMat = previousMat;
            emitted = true;
         }
      }
   }

   if ( emitted )
   {
      saveMat->mulV(info->normal);
      info->point  = endPos - startPos;
      info->point *= info->t;
      info->point += startPos;
   }

   return emitted;
}

bool TSShapeInstance::buildConvexOpcode( const MatrixF &objMat, const Point3F &objScale, S32 dl, const Box3F &bounds, Convex *c, Convex *list )
{
   AssertFatal(dl>=0 && dl<mShape->details.size(),"TSShapeInstance::buildConvexOpcode");

   // get subshape and object detail
   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   // nothing emitted yet...
   bool emitted = false;

   S32 start = mShape->subShapeFirstObject[ss];
   S32 end   = mShape->subShapeNumObjects[ss] + start;
   if (start<end)
   {
      MatrixF initialMat = objMat;
      Point3F initialScale = objScale;

      // set up for first object's node
      MatrixF mat;
      MatrixF scaleMat(true);
      F32* p = scaleMat;
      p[0]  = initialScale.x;
      p[5]  = initialScale.y;
      p[10] = initialScale.z;
      const MatrixF * previousMat = &mMeshObjects[start].getTransform();
      mat.mul(initialMat,scaleMat);
      mat.mul(*previousMat);

      // Update our bounding box...
      Box3F localBox = bounds;
      MatrixF otherMat = mat;
      otherMat.inverse();
      otherMat.mul(localBox);

      // run through objects and collide
      for (S32 i=start; i<end; i++)
      {
         MeshObjectInstance * mesh = &mMeshObjects[i];

         if (od >= mesh->object->numMeshes)
            continue;

         if (&mesh->getTransform() != previousMat)
         {
            // different node from before, set up for this node
            previousMat = &mesh->getTransform();

            if (previousMat != NULL)
            {
               mat.mul(initialMat,scaleMat);
               mat.mul(*previousMat);

               // Update our bounding box...
               otherMat = mat;
               otherMat.inverse();
               localBox = bounds;
               otherMat.mul(localBox);
            }
         }

         // collide... note we pass the original mech transform
         // here so that the convex data returned is in mesh space.
         emitted |= mesh->buildConvexOpcode(*previousMat,od,localBox,c, list);
      }
   }

   return emitted;
}

bool TSMesh::buildPolyListOpcode( const S32 od, AbstractPolyList *polyList, const Box3F &nodeBox, TSMaterialList *materials )
{
   PROFILE_SCOPE( TSMesh_buildPolyListOpcode );

   Opcode::AABBCollider opCollider;
   Opcode::AABBCache opCache;

   IceMaths::AABB opBox;
   opBox.SetMinMax(  Point( nodeBox.minExtents.x, nodeBox.minExtents.y, nodeBox.minExtents.z ),
                     Point( nodeBox.maxExtents.x, nodeBox.maxExtents.y, nodeBox.maxExtents.z ) );

   Opcode::CollisionAABB opCBox(opBox);

   //   opCollider.SetTemporalCoherence(true);
   opCollider.SetPrimitiveTests( true );

   if ( !opCollider.Collide( opCache, opCBox, *mOptTree ) )
      return false;

   U32 count = opCollider.GetNbTouchedPrimitives();
   const udword *idx = opCollider.GetTouchedPrimitives();

   Opcode::VertexPointers vp;
   U32 plIdx[3];
   S32 j;
   Point3F tmp;
   const IceMaths::Point **verts;
   const	Opcode::MeshInterface *mi = mOptTree->GetMeshInterface();
   
   for ( S32 i=0; i < count; i++ )
   {
      // Get the triangle...
      mi->GetTriangle( vp, idx[i] );
      verts = vp.Vertex;

      // And register it in the polylist...
      polyList->begin( 0, i );
      
      for( j = 2; j > -1; j-- )
      {
         tmp.set( verts[j]->x, verts[j]->y, verts[j]->z );
         plIdx[j] = polyList->addPoint( tmp );
         polyList->vertex( plIdx[j] );
      }

      polyList->plane( plIdx[0], plIdx[2], plIdx[1] );

      polyList->end();
   }

   // TODO: Add a polyList->getCount() so we can see if we
   // got clipped polys and didn't really emit anything.
   return count > 0;
}

bool TSMesh::buildConvexOpcode( const MatrixF &meshToObjectMat, const Box3F &nodeBox, Convex *convex, Convex *list )
{
   Opcode::AABBCollider opCollider;
   Opcode::AABBCache opCache;

   IceMaths::AABB opBox;
   opBox.SetMinMax(  Point( nodeBox.minExtents.x, nodeBox.minExtents.y, nodeBox.minExtents.z ),
      Point( nodeBox.maxExtents.x, nodeBox.maxExtents.y, nodeBox.maxExtents.z ) );

   Opcode::CollisionAABB opCBox(opBox);

   //   opCollider.SetTemporalCoherence(true);
   opCollider.SetPrimitiveTests( true );

   if( opCollider.Collide( opCache, opCBox, *mOptTree ) )
   {
      U32 cnt = opCollider.GetNbTouchedPrimitives();
      const udword *idx = opCollider.GetTouchedPrimitives();

      Opcode::VertexPointers vp;
      for ( S32 i = 0; i < cnt; i++ )
      {
         // First, check our active convexes for a potential match (and clean things
         // up, too.)
         const U32 curIdx = idx[i];

         // See if the square already exists as part of the working set.
         bool gotMatch = false;
         CollisionWorkingList& wl = convex->getWorkingList();
         for ( CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext )
         {
            if( itr->mConvex->getType() != TSPolysoupConvexType )
               continue;

            const TSStaticPolysoupConvex *chunkc = static_cast<TSStaticPolysoupConvex*>( itr->mConvex );

            if( chunkc->mesh != this )
               continue;

            if( chunkc->idx != curIdx )
               continue;

            // A match! Don't need to add it.
            gotMatch = true;
            break;
         }

         if( gotMatch )
            continue;

         // Get the triangle...
         mOptTree->GetMeshInterface()->GetTriangle( vp, idx[i] );

         Point3F a( vp.Vertex[0]->x, vp.Vertex[0]->y, vp.Vertex[0]->z );
         Point3F b( vp.Vertex[1]->x, vp.Vertex[1]->y, vp.Vertex[1]->z );
         Point3F c( vp.Vertex[2]->x, vp.Vertex[2]->y, vp.Vertex[2]->z );

         // Transform the result into object space!
         meshToObjectMat.mulP( a );
         meshToObjectMat.mulP( b );
         meshToObjectMat.mulP( c );

         PlaneF p( c, b, a );
         Point3F peak = ((a + b + c) / 3.0f) - (p * 0.15f);

         // Set up the convex...
         TSStaticPolysoupConvex *cp = new TSStaticPolysoupConvex();

         list->registerObject( cp );
         convex->addToWorkingList( cp );

         cp->mesh    = this;
         cp->idx     = curIdx;
         cp->mObject = TSStaticPolysoupConvex::smCurObject;

         cp->normal = p;
         cp->verts[0] = a;
         cp->verts[1] = b;
         cp->verts[2] = c;
         cp->verts[3] = peak;

         // Update the bounding box.
         Box3F &bounds = cp->box;
         bounds.minExtents.set( F32_MAX,  F32_MAX,  F32_MAX );
         bounds.maxExtents.set( -F32_MAX, -F32_MAX, -F32_MAX );

         bounds.minExtents.setMin( a );
         bounds.minExtents.setMin( b );
         bounds.minExtents.setMin( c );
         bounds.minExtents.setMin( peak );

         bounds.maxExtents.setMax( a );
         bounds.maxExtents.setMax( b );
         bounds.maxExtents.setMax( c );
         bounds.maxExtents.setMax( peak );
      }

      return true;
   }

   return false;
}

void TSMesh::prepOpcodeCollision()
{
   // Make sure opcode is loaded!
   if( !gOpcodeInitialized )
   {
      Opcode::InitOpcode();
      gOpcodeInitialized = true;
   }

   // Don't re init if we already have something...
   if ( mOptTree )
      return;

   // Ok, first set up a MeshInterface
   Opcode::MeshInterface *mi = new Opcode::MeshInterface();
   mOpMeshInterface = mi;

   // Figure out how many triangles we have...
   U32 triCount = 0;
   const U32 base = 0;
   for ( U32 i = 0; i < primitives.size(); i++ )
   {
      TSDrawPrimitive & draw = primitives[i];
      const U32 start = draw.start;

      AssertFatal( draw.matIndex & TSDrawPrimitive::Indexed,"TSMesh::buildPolyList (1)" );

      // gonna depend on what kind of primitive it is...
      if ( (draw.matIndex & TSDrawPrimitive::TypeMask) == TSDrawPrimitive::Triangles )
         triCount += draw.numElements / 3;
      else
      {
         // Have to walk the tristrip to get a count... may have degenerates
         U32 idx0 = base + indices[start + 0];
         U32 idx1;
         U32 idx2 = base + indices[start + 1];
         U32 * nextIdx = &idx1;
         for ( S32 j = 2; j < draw.numElements; j++ )
         {
            *nextIdx = idx2;
            //            nextIdx = (j%2)==0 ? &idx0 : &idx1;
            nextIdx = (U32*) ( (dsize_t)nextIdx ^ (dsize_t)&idx0 ^ (dsize_t)&idx1);
            idx2 = base + indices[start + j];
            if ( idx0 == idx1 || idx0 == idx2 || idx1 == idx2 )
               continue;

            triCount++;
         }
      }
   }

   // Just do the first trilist for now.
   mi->SetNbVertices( mVertexData.isReady() ? mNumVerts : verts.size() );
   mi->SetNbTriangles( triCount );

   // Stuff everything into appropriate arrays.
   IceMaths::IndexedTriangle *its = new IceMaths::IndexedTriangle[ mi->GetNbTriangles() ], *curIts = its;
   IceMaths::Point           *pts = new IceMaths::Point[ mi->GetNbVertices() ];
   
   mOpTris = its;
   mOpPoints = pts;

   // add the polys...
   for ( U32 i = 0; i < primitives.size(); i++ )
   {
      TSDrawPrimitive & draw = primitives[i];
      const U32 start = draw.start;

      AssertFatal( draw.matIndex & TSDrawPrimitive::Indexed,"TSMesh::buildPolyList (1)" );

      const U32 matIndex = draw.matIndex & TSDrawPrimitive::MaterialMask;

      // gonna depend on what kind of primitive it is...
      if ( (draw.matIndex & TSDrawPrimitive::TypeMask) == TSDrawPrimitive::Triangles )
      {
         for ( S32 j = 0; j < draw.numElements; )
         {
            curIts->mVRef[2] = base + indices[start + j + 0];
            curIts->mVRef[1] = base + indices[start + j + 1];
            curIts->mVRef[0] = base + indices[start + j + 2];
            curIts->mMatIdx = matIndex;
            curIts++;

            j += 3;
         }
      }
      else
      {
         AssertFatal( (draw.matIndex & TSDrawPrimitive::TypeMask) == TSDrawPrimitive::Strip,"TSMesh::buildPolyList (2)" );

         U32 idx0 = base + indices[start + 0];
         U32 idx1;
         U32 idx2 = base + indices[start + 1];
         U32 * nextIdx = &idx1;
         for ( S32 j = 2; j < draw.numElements; j++ )
         {
            *nextIdx = idx2;
            //            nextIdx = (j%2)==0 ? &idx0 : &idx1;
            nextIdx = (U32*) ( (dsize_t)nextIdx ^ (dsize_t)&idx0 ^ (dsize_t)&idx1);
            idx2 = base + indices[start + j];
            if ( idx0 == idx1 || idx0 == idx2 || idx1 == idx2 )
               continue;

            curIts->mVRef[2] = idx0;
            curIts->mVRef[1] = idx1;
            curIts->mVRef[0] = idx2;
            curIts->mMatIdx = matIndex;
            curIts++;
         }
      }
   }

   AssertFatal( (curIts - its) == mi->GetNbTriangles(), "Triangle count mismatch!" );

   for( S32 i = 0; i < mi->GetNbVertices(); i++ )
      if( mVertexData.isReady() )
         pts[i].Set( mVertexData[i].vert().x, mVertexData[i].vert().y, mVertexData[i].vert().z );
      else
         pts[i].Set( verts[i].x, verts[i].y, verts[i].z );

   mi->SetPointers( its, pts );

   // Ok, we've got a mesh interface populated, now let's build a thingy to collide against.
   mOptTree = new Opcode::Model();

   Opcode::OPCODECREATE opcc;

   opcc.mCanRemap = true;
   opcc.mIMesh = mi;
   opcc.mKeepOriginal = false;
   opcc.mNoLeaf = false;
   opcc.mQuantized = false;
   opcc.mSettings.mLimit = 1;

   mOptTree->Build( opcc );
}

bool TSMesh::castRayOpcode( const Point3F &s, const Point3F &e, RayInfo *info, TSMaterialList *materials )
{
   Opcode::RayCollider ray;
   Opcode::CollisionFaces cfs;

   IceMaths::Point dir( e.x - s.x, e.y - s.y, e.z - s.z );
   const F32 rayLen = dir.Magnitude();
   IceMaths::Ray vec( Point(s.x, s.y, s.z), dir.Normalize() );

   ray.SetDestination( &cfs);
   ray.SetFirstContact( false );
   ray.SetClosestHit( true );
   ray.SetPrimitiveTests( true );
   ray.SetCulling( true );
   ray.SetMaxDist( rayLen );

   AssertFatal( ray.ValidateSettings() == NULL, "invalid ray settings" );

   // Do collision.
   bool safety = ray.Collide( vec, *mOptTree );
   AssertFatal( safety, "TSMesh::castRayOpcode - no good ray collide!" );

   // If no hit, just skip out.
   if( cfs.GetNbFaces() == 0 )
      return false;

   // Got a hit!
   AssertFatal( cfs.GetNbFaces() == 1, "bad" );
   const Opcode::CollisionFace &face = cfs.GetFaces()[0];

   // If the cast was successful let's check if the t value is less than what we had
   // and toggle the collision boolean
   // Stupid t... i prefer coffee
   const F32 t = face.mDistance / rayLen;

   if( t < 0.0f || t > 1.0f )
      return false;

   if( t <= info->t )
   {
      info->t = t;

      // Calculate the normal.
      Opcode::VertexPointers vp;
      mOptTree->GetMeshInterface()->GetTriangle( vp, face.mFaceID );

      if ( materials && vp.MatIdx >= 0 && vp.MatIdx < materials->getMaterialCount() )
         info->material = materials->getMaterialInst( vp.MatIdx );

      // Get the two edges.
      const IceMaths::Point baseVert = *vp.Vertex[0];
      const IceMaths::Point a = *vp.Vertex[1] - baseVert;
      const IceMaths::Point b = *vp.Vertex[2] - baseVert;

      IceMaths::Point n;
      n.Cross( a, b );
      n.Normalize();

      info->normal.set( n.x, n.y, n.z );
      return true;
   }

   return false;
}
