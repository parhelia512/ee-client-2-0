//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/decal/decalManager.h"

#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "ts/tsShapeInstance.h"
#include "console/console.h"
#include "console/dynamicTypes.h"
#include "gfx/primBuilder.h"
#include "console/consoleTypes.h"
#include "platform/profiler.h"
#include "gfx/gfxTransformSaver.h"
#include "lighting/lightManager.h"
#include "lighting/lightInfo.h"
#include "gfx/gfxDrawUtil.h"
#include "gfx/sim/gfxStateBlockData.h"
#include "materials/shaderData.h"
#include "renderInstance/renderPassManager.h"
#include "core/resourceManager.h"
#include "gfx/gfxDebugEvent.h"
#include "math/util/quadTransforms.h"
#include "core/volume.h"

#include "T3D/decal/decalData.h"

/// A bias applied to the nearPlane for Decal and DecalRoad rendering.
/// Is set by by LevelInfo.
F32 gDecalBias = 0.0015f;

bool      DecalManager::smDecalsOn = true;
F32       DecalManager::smDecalLifeTimeScale = 1.0f;
const U32 DecalManager::smMaxVerts = 6000;
const U32 DecalManager::smMaxIndices = 10000;

DecalManager *gDecalManager = NULL;

IMPLEMENT_CONOBJECT(DecalManager);

namespace {

int QSORT_CALLBACK cmpDecalInstance(const void* p1, const void* p2)
{
   const DecalInstance** pd1 = (const DecalInstance**)p1;
   const DecalInstance** pd2 = (const DecalInstance**)p2;

   return int(((char *)(*pd1)->mDataBlock) - ((char *)(*pd2)->mDataBlock));
}

int QSORT_CALLBACK cmpPointsXY( const void *p1, const void *p2 )
{
   const Point3F *pnt1 = (const Point3F*)p1;
   const Point3F *pnt2 = (const Point3F*)p2;

   if ( pnt1->x < pnt2->x )
      return -1;
   else if ( pnt1->x > pnt2->x )
      return 1;
   else if ( pnt1->y < pnt2->y )
      return -1;
   else if ( pnt1->y > pnt2->y )
      return 1;
   else
      return 0;
}

int QSORT_CALLBACK cmpQuadPointTheta( const void *p1, const void *p2 )
{
   const Point4F *pnt1 = (const Point4F*)p1;
   const Point4F *pnt2 = (const Point4F*)p2;

   if ( mFabs( pnt1->w ) > mFabs( pnt2->w ) )
      return 1;
   else if ( mFabs( pnt1->w ) < mFabs( pnt2->w ) )
      return -1;
   else
      return 0;
}

static Point3F gSortPoint;

int QSORT_CALLBACK cmpDecalDistance( const void *p1, const void *p2 )
{
   const DecalInstance** pd1 = (const DecalInstance**)p1;
   const DecalInstance** pd2 = (const DecalInstance**)p2;

   F32 dist1 = ( (*pd1)->mPosition - gSortPoint ).lenSquared();
   F32 dist2 = ( (*pd2)->mPosition - gSortPoint ).lenSquared();

   return dist1 - dist2;
}

int QSORT_CALLBACK cmpDecalRenderOrder( const void *p1, const void *p2 )
{   
   const DecalInstance** pd1 = (const DecalInstance**)p1;
   const DecalInstance** pd2 = (const DecalInstance**)p2;

   if ( ( (*pd2)->mFlags & SaveDecal ) && !( (*pd1)->mFlags & SaveDecal ) )
      return -1;
   else if ( !( (*pd2)->mFlags & SaveDecal ) && ( (*pd1)->mFlags & SaveDecal ) )
      return 1;
   else
   {
      int priority = (*pd1)->getRenderPriority() - (*pd2)->getRenderPriority();

      if ( priority != 0 )
         return priority;

      if ( (*pd2)->mFlags & SaveDecal )
      {
         int id = ( (*pd1)->mDataBlock->getMaterial()->getId() - (*pd2)->mDataBlock->getMaterial()->getId() );      
         if ( id != 0 )
            return id;

         return (*pd1)->mCreateTime - (*pd2)->mCreateTime;
      }
      else
         return (*pd1)->mCreateTime - (*pd2)->mCreateTime;
   }
}

} // namespace {}

// These numbers should be tweaked to get as many dynamically placed decals
// as possible to allocate buffer arrays with the FreeListChunker.
enum
{
   SIZE_CLASS_0 = 256,
   SIZE_CLASS_1 = 512,
   SIZE_CLASS_2 = 1024,
   
   NUM_SIZE_CLASSES = 3
};

//-------------------------------------------------------------------------
// DecalManager
//-------------------------------------------------------------------------
DecalManager::DecalManager()
{
   mObjBox.minExtents.set(-1e7, -1e7, -1e7);
   mObjBox.maxExtents.set( 1e7,  1e7,  1e7);
   mWorldBox.minExtents.set(-1e7, -1e7, -1e7);
   mWorldBox.maxExtents.set( 1e7,  1e7,  1e7);

   mDataFileName = NULL;

   mTypeMask |= EnvironmentObjectType;

   mDirty = false;

   mChunkers[0] = new FreeListChunkerUntyped( SIZE_CLASS_0 * sizeof( U8 ) );
   mChunkers[1] = new FreeListChunkerUntyped( SIZE_CLASS_1 * sizeof( U8 ) );
   mChunkers[2] = new FreeListChunkerUntyped( SIZE_CLASS_2 * sizeof( U8 ) );
}

DecalManager::~DecalManager()
{
   clearData();

   for( U32 i = 0; i < NUM_SIZE_CLASSES; ++ i )
      delete mChunkers[ i ];
}

void DecalManager::consoleInit()
{
   Con::addVariable( "$pref::decalsOn", TypeBool, &smDecalsOn );   
   Con::addVariable( "$pref::Decal::decalLifeTimeScale", TypeF32, &smDecalLifeTimeScale );   
}

bool DecalManager::clipDecal( DecalInstance *decal, Vector<Point3F> *edgeVerts, const Point2F *clipDepth )
{
   PROFILE_SCOPE( DecalManager_clipDecal );

   // Free old verts and indices.
   _freeBuffers( decal );

   ClippedPolyList clipper;

   clipper.mNormal.set( Point3F( 0, 0, 0 ) );
   clipper.mPlaneList.setSize(6);

   F32 halfSize = decal->mSize * 0.5f;
   
   // Ugly hack for ProjectedShadow!
   F32 halfSizeZ = clipDepth ? clipDepth->x : halfSize;
   F32 negHalfSize = clipDepth ? clipDepth->y : halfSize;
   Point3F decalHalfSize( halfSize, halfSize, halfSize );
   Point3F decalHalfSizeZ( halfSizeZ, halfSizeZ, halfSizeZ );

   MatrixF projMat( true );
   decal->getWorldMatrix( &projMat );

   const VectorF &crossVec = decal->mNormal;
   const Point3F &decalPos = decal->mPosition;

   VectorF newFwd, newRight;
   projMat.getColumn( 0, &newRight );
   projMat.getColumn( 1, &newFwd );   

   VectorF objRight( 1.0f, 0, 0 );
   VectorF objFwd( 0, 1.0f, 0 );
   VectorF objUp( 0, 0, 1.0f );

   // See above re: decalHalfSizeZ hack.
   clipper.mPlaneList[0].set( ( decalPos + ( -newRight * halfSize ) ), -newRight );
   clipper.mPlaneList[1].set( ( decalPos + ( -newFwd * halfSize ) ), -newFwd );
   clipper.mPlaneList[2].set( ( decalPos + ( -crossVec * decalHalfSizeZ ) ), -crossVec );
   clipper.mPlaneList[3].set( ( decalPos + ( newRight * halfSize ) ), newRight );
   clipper.mPlaneList[4].set( ( decalPos + ( newFwd * halfSize ) ), newFwd );
   clipper.mPlaneList[5].set( ( decalPos + ( crossVec * negHalfSize ) ), crossVec );

   clipper.mNormal = -decal->mNormal;

   Box3F box( -decalHalfSizeZ, decalHalfSizeZ );

   projMat.mul( box );

   DecalData *decalData = decal->mDataBlock;

   PROFILE_START( DecalManager_clipDecal_buildPolyList );
   getContainer()->buildPolyList( box, decalData->clippingMasks, &clipper );   
   PROFILE_END();

   clipper.cullUnusedVerts();
   clipper.triangulate();
   clipper.generateNormals();
   
   if ( clipper.mVertexList.empty() )
      return false;

#ifdef DECALMANAGER_DEBUG
   mDebugPlanes.clear();
   mDebugPlanes.merge( clipper.mPlaneList );
#endif

   decal->mVertCount = clipper.mVertexList.size();
   decal->mIndxCount = clipper.mIndexList.size();
   
   Vector<Point3F> tmpPoints;

   tmpPoints.push_back(( objFwd * decalHalfSize ) + ( objRight * decalHalfSize ));
   tmpPoints.push_back(( objFwd * decalHalfSize ) + ( -objRight * decalHalfSize ));
   tmpPoints.push_back(( -objFwd * decalHalfSize ) + ( -objRight * decalHalfSize ));
   
   Point3F lowerLeft(( -objFwd * decalHalfSize ) + ( objRight * decalHalfSize ));

   projMat.inverse();

   _generateWindingOrder( lowerLeft, &tmpPoints );

   BiQuadToSqr quadToSquare( Point2F( lowerLeft.x, lowerLeft.y ),
                             Point2F( tmpPoints[0].x, tmpPoints[0].y ), 
                             Point2F( tmpPoints[1].x, tmpPoints[1].y ), 
                             Point2F( tmpPoints[2].x, tmpPoints[2].y ) );  

   Point2F uv( 0, 0 );
   Point3F vecX(0.0f, 0.0f, 0.0f);

   // Allocate memory for vert and index arrays
   _allocBuffers( decal );  
   
   Point3F vertPoint( 0, 0, 0 );

   for ( U32 i = 0; i < clipper.mVertexList.size(); i++ )
   {
      const ClippedPolyList::Vertex &vert = clipper.mVertexList[i];
      vertPoint = vert.point;

      // Transform this point to
      // object space to look up the
      // UV coordinate for this vertex.
      projMat.mulP( vertPoint );

      // Clamp the point to be within the quad.
      vertPoint.x = mClampF( vertPoint.x, -decalHalfSize.x, decalHalfSize.x );
      vertPoint.y = mClampF( vertPoint.y, -decalHalfSize.y, decalHalfSize.y );

      // Get our UV.
      uv = quadToSquare.transform( Point2F( vertPoint.x, vertPoint.y ) );

      const RectF &rect = decal->mDataBlock->texRect[decal->mTextureRectIdx];

      uv *= rect.extent;
      uv += rect.point;      

      // Set the world space vertex position.
      decal->mVerts[i].point = vert.point;
      
      decal->mVerts[i].texCoord.set( uv.x, uv.y );
      
      decal->mVerts[i].normal = clipper.mNormalList[i];
      
      decal->mVerts[i].normal.normalize();

      if( mFabs( decal->mVerts[i].normal.z ) > 0.8f ) 
         mCross( decal->mVerts[i].normal, Point3F( 1.0f, 0.0f, 0.0f ), &vecX );
      else if ( mFabs( decal->mVerts[i].normal.x ) > 0.8f )
         mCross( decal->mVerts[i].normal, Point3F( 0.0f, 1.0f, 0.0f ), &vecX );
      else if ( mFabs( decal->mVerts[i].normal.y ) > 0.8f )
         mCross( decal->mVerts[i].normal, Point3F( 0.0f, 0.0f, 1.0f ), &vecX );
   
      decal->mVerts[i].tangent = mCross( decal->mVerts[i].normal, vecX );
   }

   U32 curIdx = 0;
   for ( U32 j = 0; j < clipper.mPolyList.size(); j++ )
   {
      // Write indices for each Poly
      ClippedPolyList::Poly *poly = &clipper.mPolyList[j];                  

      AssertFatal( poly->vertexCount == 3, "Got non-triangle poly!" );

      decal->mIndices[curIdx] = clipper.mIndexList[poly->vertexStart];         
      curIdx++;
      decal->mIndices[curIdx] = clipper.mIndexList[poly->vertexStart + 1];            
      curIdx++;
      decal->mIndices[curIdx] = clipper.mIndexList[poly->vertexStart + 2];                
      curIdx++;
   } 

   if ( !edgeVerts )
      return true;

   Point3F tmpHullPt( 0, 0, 0 );
   Vector<Point3F> tmpHullPts;

   for ( U32 i = 0; i < clipper.mVertexList.size(); i++ )
   {
      const ClippedPolyList::Vertex &vert = clipper.mVertexList[i];
      tmpHullPt = vert.point;
      projMat.mulP( tmpHullPt );
      tmpHullPts.push_back( tmpHullPt );
   }

   edgeVerts->clear();
   U32 verts = _generateConvexHull( tmpHullPts, edgeVerts );
   edgeVerts->setSize( verts );

   projMat.inverse();
   for ( U32 i = 0; i < edgeVerts->size(); i++ )
      projMat.mulP( (*edgeVerts)[i] );

   return true;
}

DecalInstance* DecalManager::addDecal( const Point3F &pos,
                                       const Point3F &normal,
                                       F32 rotAroundNormal,
                                       DecalData *decalData,
                                       F32 decalScale,
                                       S32 decalTexIndex,
                                       U8 flags )
{      
   MatrixF mat( true );
   MathUtils::getMatrixFromUpVector( normal, &mat );

   AngAxisF rot( normal, rotAroundNormal );
   MatrixF rotmat;
   rot.setMatrix( &rotmat );
   mat.mul( rotmat );
 
   Point3F tangent;
   mat.getColumn( 1, &tangent );

   return addDecal( pos, normal, tangent, decalData, decalScale, decalTexIndex, flags );   
}

DecalInstance* DecalManager::addDecal( const Point3F &pos,
                                      const Point3F &normal,
                                      const Point3F &tangent,
                                      DecalData *decalData,
                                      F32 decalScale,
                                      S32 decalTexIndex,
                                      U8 flags )
{
   if ( !mData && !_createDataFile() )
      return NULL;   

   // only dirty the manager if this decal should be saved
   if ( flags & SaveDecal )
      mDirty = true;

   Point3F vecX, vecY, norm;
   DecalInstance *newDecal = mData->_allocateInstance();

   newDecal->mPosition = pos;
   newDecal->mNormal = normal;
   newDecal->mTangent = tangent;

   newDecal->mSize = decalData->size * decalScale;

   newDecal->mDataBlock = decalData;

	S32 frame = newDecal->mDataBlock->frame;
   // randomize the frame if the flag is set. this number is used directly below us
	// when calculating render coords
	if ( decalData->randomize )
      frame = gRandGen.randI();

   frame %= getMax( decalData->texCoordCount, 0 ) + 1;

	
   newDecal->mTextureRectIdx = frame;

   newDecal->mVisibility = 1.0f;

   newDecal->mCreateTime = Sim::getCurrentTime();

   newDecal->mVerts = NULL;
   newDecal->mIndices = NULL;
   newDecal->mVertCount = 0;
   newDecal->mIndxCount = 0;

   newDecal->mFlags = flags;
   newDecal->mFlags |= ClipDecal;

   mData->addDecal( newDecal );

   return newDecal;
}

void DecalManager::removeDecal( DecalInstance *inst )
{
   // If this is a decal we save then we need 
   // to set the dirty flag.
   if ( inst->mFlags & SaveDecal )
      mDirty = true;

   // Remove the decal from the instance vector.
   
	if( inst->mId != -1 && inst->mId < mDecalInstanceVec.size() )
      mDecalInstanceVec[ inst->mId ] = NULL;
   
   // Release its geometry (if it has any).

   _freeBuffers( inst );
   
   // Remove it from the decal file.

   if ( mData )      
      mData->removeDecal( inst );
}

DecalInstance* DecalManager::getDecal( S32 id )
{
   if( id < 0 || id >= mDecalInstanceVec.size() )
      return NULL;

   return mDecalInstanceVec[id];
}

void DecalManager::notifyDecalModified( DecalInstance *inst )
{
   // If this is a decal we save then we need 
   // to set the dirty flag.
   if ( inst->mFlags & SaveDecal )
      mDirty = true;

   if ( mData )
      mData->notifyDecalModified( inst );
}

DecalInstance* DecalManager::getClosestDecal( const Point3F &pos )
{
   if ( !mData )
      return NULL;

   const Vector<DecalSphere*> &grid = mData->getGrid();

   DecalInstance *inst = NULL;
   SphereF worldPickSphere( pos, 0.5f );
   SphereF worldInstSphere( Point3F( 0, 0, 0 ), 1.0f );

   Vector<DecalInstance*> collectedInsts;

   for ( U32 i = 0; i < grid.size(); i++ )
   {
      DecalSphere *decalSphere = grid[i];
      const SphereF &worldSphere = decalSphere->mWorldSphere;
      if (  !worldSphere.isIntersecting( worldPickSphere ) && 
            !worldSphere.isContained( pos ) )
         continue;

      const Vector<DecalInstance*> &items = decalSphere->mItems;
      for ( U32 n = 0; n < items.size(); n++ )
      {
         inst = items[n];
         if ( !inst )
            continue;

         worldInstSphere.center = inst->mPosition;
         worldInstSphere.radius = inst->mSize;

         if ( !worldInstSphere.isContained( inst->mPosition ) )
            continue;

         collectedInsts.push_back( inst );
      }
   }

   F32 closestDistance = F32_MAX;
   F32 currentDist = 0;
   U32 closestIndex = 0;
   for ( U32 i = 0; i < collectedInsts.size(); i++ )
   {
      inst = collectedInsts[i];
      currentDist = (inst->mPosition - pos).len();
      if ( currentDist < closestDistance )
      {
         closestIndex = i;
         closestDistance = currentDist;
         worldInstSphere.center = inst->mPosition;
         worldInstSphere.radius = inst->mSize;
      }
   }

   if (  !collectedInsts.empty() && 
         collectedInsts[closestIndex] && 
         closestDistance < 1.0f || 
         worldInstSphere.isContained( pos ) )
      return collectedInsts[closestIndex];
   else
      return NULL;
}

DecalInstance* DecalManager::raycast( const Point3F &start, const Point3F &end, bool savedDecalsOnly )
{
   if ( !mData )
      return NULL;

   const Vector<DecalSphere*> &grid = mData->getGrid();

   DecalInstance *inst = NULL;
   SphereF worldSphere( Point3F( 0, 0, 0 ), 1.0f );

   Vector<DecalInstance*> hitDecals;

   for ( U32 i = 0; i < grid.size(); i++ )
   {
      DecalSphere *decalSphere = grid[i];
      if ( !decalSphere->mWorldSphere.intersectsRay( start, end ) )
         continue;

      const Vector<DecalInstance*> &items = decalSphere->mItems;
      for ( U32 n = 0; n < items.size(); n++ )
      {
         inst = items[n];
         if ( !inst )
            continue;

         if ( savedDecalsOnly && !(inst->mFlags & SaveDecal) )
            continue;

         worldSphere.center = inst->mPosition;
         worldSphere.radius = inst->mSize;

         if ( !worldSphere.intersectsRay( start, end ) )
            continue;
			
			RayInfo ri;
			bool containsPoint = false;
			if ( gServerContainer.castRayRendered( start, end, STATIC_COLLISION_MASK, &ri ) )
			{        
				Point2F poly[4];
				poly[0].set( inst->mPosition.x - (inst->mSize / 2), inst->mPosition.y + (inst->mSize / 2));
				poly[1].set( inst->mPosition.x - (inst->mSize / 2), inst->mPosition.y - (inst->mSize / 2));
				poly[2].set( inst->mPosition.x + (inst->mSize / 2), inst->mPosition.y - (inst->mSize / 2));
				poly[3].set( inst->mPosition.x + (inst->mSize / 2), inst->mPosition.y + (inst->mSize / 2));
				
				if ( MathUtils::pointInPolygon( poly, 4, Point2F(ri.point.x, ri.point.y) ) )
					containsPoint = true;
			}

			if( !containsPoint )
				continue;

         hitDecals.push_back( inst );
      }
   }

   if ( hitDecals.empty() )
      return NULL;

   gSortPoint = start;
   dQsort( hitDecals.address(), hitDecals.size(), sizeof(DecalInstance*), cmpDecalDistance );
   return hitDecals[0];
}

U32 DecalManager::_generateConvexHull( const Vector<Point3F> &points, Vector<Point3F> *outPoints )
{
   // chainHull_2D(): Andrew's monotone chain 2D convex hull algorithm
   //     Input:  P[] = an array of 2D points
   //                   presorted by increasing x- and y-coordinates
   //             n = the number of points in P[]
   //     Output: H[] = an array of the convex hull vertices (max is n)
   //     Return: the number of points in H[]
   //int

   if ( points.size() < 3 )
   {
      outPoints->merge( points );
      return outPoints->size();
   }

   // Sort our input points.
   dQsort( points.address(), points.size(), sizeof( Point3F ), cmpPointsXY ); 

   U32 n = points.size();

   Vector<Point3F> tmpPoints;
   tmpPoints.setSize( n );

   // the output array H[] will be used as the stack
   S32    bot=0, top=(-1);  // indices for bottom and top of the stack
   S32    i;                // array scan index
   S32 toptmp = 0;

   // Get the indices of points with min x-coord and min|max y-coord
   S32 minmin = 0, minmax;
   F32 xmin = points[0].x;
   for ( i = 1; i < n; i++ )
     if (points[i].x != xmin) 
        break;

   minmax = i - 1;
   if ( minmax == n - 1 ) 
   {       
      // degenerate case: all x-coords == xmin
      toptmp = top + 1;
      if ( toptmp < n )
         tmpPoints[++top] = points[minmin];

      if ( points[minmax].y != points[minmin].y ) // a nontrivial segment
      {
         toptmp = top + 1;
         if ( toptmp < n )
            tmpPoints[++top] = points[minmax];
      }

      toptmp = top + 1;
      if ( toptmp < n )
         tmpPoints[++top] = points[minmin];           // add polygon endpoint

      return top+1;
   }

   // Get the indices of points with max x-coord and min|max y-coord
   S32 maxmin, maxmax = n-1;
   F32 xmax = points[n-1].x;

   for ( i = n - 2; i >= 0; i-- )
     if ( points[i].x != xmax ) 
        break;
   
   maxmin = i + 1;

   // Compute the lower hull on the stack H
   toptmp = top + 1;
   if ( toptmp < n )
      tmpPoints[++top] = points[minmin];      // push minmin point onto stack

   i = minmax;
   while ( ++i <= maxmin )
   {
      // the lower line joins P[minmin] with P[maxmin]
      if ( isLeft( points[minmin], points[maxmin], points[i]) >= 0 && i < maxmin )
         continue;          // ignore P[i] above or on the lower line

      while (top > 0)        // there are at least 2 points on the stack
      {
         // test if P[i] is left of the line at the stack top
         if ( isLeft( tmpPoints[top-1], tmpPoints[top], points[i]) > 0)
             break;         // P[i] is a new hull vertex
         else
            top--;         // pop top point off stack
      }

      toptmp = top + 1;
      if ( toptmp < n )
         tmpPoints[++top] = points[i];       // push P[i] onto stack
   }

   // Next, compute the upper hull on the stack H above the bottom hull
   if (maxmax != maxmin)      // if distinct xmax points
   {
      toptmp = top + 1;
      if ( toptmp < n )
         tmpPoints[++top] = points[maxmax];  // push maxmax point onto stack
   }

   bot = top;                 // the bottom point of the upper hull stack
   i = maxmin;
   while (--i >= minmax)
   {
      // the upper line joins P[maxmax] with P[minmax]
      if ( isLeft( points[maxmax], points[minmax], points[i] ) >= 0 && i > minmax )
         continue;          // ignore P[i] below or on the upper line

      while ( top > bot )    // at least 2 points on the upper stack
      { 
         // test if P[i] is left of the line at the stack top
         if ( isLeft( tmpPoints[top-1], tmpPoints[top], points[i] ) > 0 )
             break;         // P[i] is a new hull vertex
         else
            top--;         // pop top point off stack
      }

      toptmp = top + 1;
      if ( toptmp < n )
         tmpPoints[++top] = points[i];       // push P[i] onto stack
   }

   if (minmax != minmin)
   {
      toptmp = top + 1;
      if ( toptmp < n )
         tmpPoints[++top] = points[minmin];  // push joining endpoint onto stack
   }

   outPoints->merge( tmpPoints );

   return top + 1;
}

void DecalManager::_generateWindingOrder( const Point3F &cornerPoint, Vector<Point3F> *sortPoints )
{
   // This block of code is used to find 
   // the winding order for the points in our quad.

   // First, choose an arbitrary corner point.
   // We'll use the "lowerRight" point.
   Point3F relPoint( 0, 0, 0 );

   // See comment below about radius.
   //F32 radius = 0;

   F32 theta = 0;
   
   Vector<Point4F> tmpPoints;

   for ( U32 i = 0; i < (*sortPoints).size(); i++ )
   {
      const Point3F &pnt = (*sortPoints)[i];
      relPoint = cornerPoint - pnt;

      // Get the radius (r^2 = x^2 + y^2).
      
      // This is commented because for a quad
      // you typically can't have the same values
      // for theta, which is the caveat which would
      // require sorting by the radius.
      //radius = mSqrt( (relPoint.x * relPoint.x) + (relPoint.y * relPoint.y) );

      // Get the theta value for the
      // interval -PI, PI.

      // This algorithm for determining the
      // theta value is defined by
      //          | arctan( y / x )  if x > 0
      //          | arctan( y / x )  if x < 0 and y >= 0
      // theta =  | arctan( y / x )  if x < 0 and y < 0
      //          | PI / 2           if x = 0 and y > 0
      //          | -( PI / 2 )      if x = 0 and y < 0
      if ( relPoint.x > 0.0f )
         theta = mAtan2( relPoint.y, relPoint.x );
      else if ( relPoint.x < 0.0f )
      {
         if ( relPoint.y >= 0.0f )
            theta = mAtan2( relPoint.y, relPoint.x ) + M_PI_F;
         else if ( relPoint.y < 0.0f )
            theta = mAtan2( relPoint.y, relPoint.x ) - M_PI_F;
      }
      else if ( relPoint.x == 0.0f )
      {
         if ( relPoint.y > 0.0f )
            theta = M_PI_F / 2.0f;
         else if ( relPoint.y < 0.0f )
            theta = -(M_PI_F / 2.0f);
      }

      tmpPoints.push_back( Point4F( pnt.x, pnt.y, pnt.z, theta ) );
   }

   dQsort( tmpPoints.address(), tmpPoints.size(), sizeof( Point4F ), cmpQuadPointTheta ); 

   for ( U32 i = 0; i < tmpPoints.size(); i++ )
   {
      const Point4F &tmpPoint = tmpPoints[i];
      (*sortPoints)[i].set( tmpPoint.x, tmpPoint.y, tmpPoint.z );
   }
}

void DecalManager::_allocBuffers( DecalInstance *inst )
{
   const S32 sizeClass = _getSizeClass( inst );
   
   void* data;
   if ( sizeClass == -1 )
      data = dMalloc( sizeof( DecalVertex ) * inst->mVertCount + sizeof( U16 ) * inst->mIndxCount );
   else
      data = mChunkers[sizeClass]->alloc();

   inst->mVerts = reinterpret_cast< DecalVertex* >( data );
   data = (U8*)data + sizeof( DecalVertex ) * inst->mVertCount;
   inst->mIndices = reinterpret_cast< U16* >( data );
}

void DecalManager::_freeBuffers( DecalInstance *inst )
{
   if ( inst->mVerts != NULL )
   {
      const S32 sizeClass = _getSizeClass( inst );
      
      if ( sizeClass == -1 )
         dFree( inst->mVerts );
      else
      {
         // Use FreeListChunker
         mChunkers[sizeClass]->free( inst->mVerts );      
      }

      inst->mVerts = NULL;
      inst->mVertCount = 0;
      inst->mIndices = NULL;
      inst->mIndxCount = 0;
   }   
}

S32 DecalManager::_getSizeClass( DecalInstance *inst ) const
{
   U32 bytes = inst->mVertCount * sizeof( DecalVertex ) + inst->mIndxCount * sizeof ( U16 );

   if ( bytes <= SIZE_CLASS_0 )
      return 0;
   if ( bytes <= SIZE_CLASS_1 )
      return 1;
   if ( bytes <= SIZE_CLASS_2 )
      return 2;

   // Size is outside of the largest chunker.
   return -1;
}

bool DecalManager::prepRenderImage(SceneState* state, const U32 stateKey,
                                   const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
   PROFILE_SCOPE( DecalManager_RenderDecals );

   if ( !smDecalsOn || !mData ) 
      return false;

   if (isLastState(state, stateKey))
      return false;
   setLastState(state, stateKey);

   if ( !state->isDiffusePass() && !state->isReflectPass() )
      return false;

   PROFILE_START( DecalManager_RenderDecals_SphereTreeCull );

   // Grab this before anything here changes it.
   mCuller = state->getFrustum();

   // Populate vector of decal instances to be rendered with all
   // decals from visible decal spheres.

   mDecalQueue.clear();

   const Vector<DecalSphere*> &grid = mData->getGrid();
   
   for ( U32 i = 0; i < grid.size(); i++ )
   {
      const DecalSphere *decalSphere = grid[i];
      const SphereF &worldSphere = decalSphere->mWorldSphere;
      if ( !mCuller.sphereInFrustum( worldSphere.center, worldSphere.radius ) )
         continue;

      // TODO: If each sphere stored its largest decal instance we
      // could do an LOD step on it here and skip adding any of the
      // decals in the sphere.

      mDecalQueue.merge( decalSphere->mItems );
   }

   PROFILE_END();

   PROFILE_START( DecalManager_RenderDecals_Update );

   const U32 &curSimTime = Sim::getCurrentTime();
   const Point2I &viewportExtent = state->getViewportExtent();
   Point3F cameraOffset; 
   F32   decalSize, 
         pixelRadius;
   U32 delta, diff;
   DecalInstance *dinst;

   // Loop through DecalQueue once for preRendering work.
   // 1. Update DecalInstance fade (over time)
   // 2. Clip geometry if flagged to do so.
   // 3. Calculate lod - if decal is far enough away it will not render.
   for ( U32 i = 0; i < mDecalQueue.size(); i++ )
   {
      dinst = mDecalQueue[i];

      // LOD calculation.
      // See TSShapeInstance::setDetailFromDistance for more
      // information on these calculations.
      decalSize = getMax( dinst->mSize, 0.001f );
      pixelRadius = dinst->calcPixelRadius( state );

      // Need to clamp here.
      if ( pixelRadius < dinst->calcEndPixRadius( viewportExtent ) )
      {
         mDecalQueue.erase_fast( i );
         i--;
         continue;
      }

      // We're gonna try to render this decal... so do any 
      // final adjustments to it before rendering.

      // Update fade and delete expired.
      if ( !( dinst->mFlags & PermanentDecal || dinst->mFlags & CustomDecal ) )
      {         
         delta = ( curSimTime - dinst->mCreateTime );
         if ( delta > dinst->mDataBlock->lifeSpan )         
         {            
            diff = delta - dinst->mDataBlock->lifeSpan;
            dinst->mVisibility = 1.0f - (F32)diff / (F32)dinst->mDataBlock->fadeTime;

            if ( dinst->mVisibility <= 0.0f )
            {
               mDecalQueue.erase_fast( i );
               removeDecal( dinst );               
               i--;
               continue;
            }
         }
      }

      // Build clipped geometry for this decal if needed.
      if ( dinst->mFlags & ClipDecal/* && !( dinst->mFlags & CustomDecal ) */)
      {  
         // Turn off the flag so we don't continually try to clip
         // if it fails.
		  if(!clipDecal( dinst ))
		  {
				dinst->mFlags = dinst->mFlags & ~ClipDecal;
				if ( !(dinst->mFlags & CustomDecal) )
				{
					// Clipping failed to get any geometry...

					// Remove it from the render queue.
					mDecalQueue.erase_fast( i );
					i--;

					// If the decal is one placed at run-time (not the editor)
					// then we should also permanently delete the decal instance.
					if ( !(dinst->mFlags & SaveDecal) )
					{
						removeDecal( dinst );
					}
				}       
				// If this is a decal placed by the editor it will be
				// flagged to attempt clipping again the next time it is
				// modified. For now we just skip rendering it.      
				continue;
		  }  
      }

      // If we get here and the decal still does not have any geometry
      // skip rendering it. It must be an editor placed decal that failed
      // to clip any geometry but has not yet been flagged to try again.
      if ( !dinst->mVerts || dinst->mVertCount == 0 || dinst->mIndxCount == 0 )
      {
         mDecalQueue.erase_fast( i );
         i--;
         continue;
      }

      //
      F32 alpha = pixelRadius / (dinst->mDataBlock->startPixRadius * decalSize) - 1.0f;
      if ( dinst->mFlags & CustomDecal )
      {
         alpha = mClampF( alpha, 0.0f, 1.0f );
         alpha *= dinst->mVisibility;
      }
      else
         alpha = mClampF( alpha * dinst->mVisibility, 0.0f, 1.0f );

      //
      for ( U32 v = 0; v < dinst->mVertCount; v++ )
         dinst->mVerts[v].color.set( 255, 255, 255, alpha * 255.0f );
   }

   PROFILE_END();   

   if ( mDecalQueue.empty() )
      return false;

   // Sort queued decals...
   // 1. Editor decals - in render priority order first, creation time second, and material third.
   // 2. Dynamic decals - in render priority order first and creation time second.
   //
   // With the constraint that decals with different render priority cannot
   // be rendered together in the same draw call.

   PROFILE_START( DecalManager_RenderDecals_Sort );
   dQsort( mDecalQueue.address(), mDecalQueue.size(), sizeof(DecalInstance*), cmpDecalRenderOrder );
   PROFILE_END();

   PROFILE_SCOPE( DecalManager_RenderDecals_RenderBatch );

   mPrimBuffs.clear();
   mVBs.clear();

   RenderPassManager *renderPass = state->getRenderPass();

   // Base render instance for convenience we use for convenience.
   // Data shared by all instances we allocate below can be copied
   // from the base instance at the same time.
   MeshRenderInst baseRenderInst;
   baseRenderInst.clear();   

   MatrixF *tempMat = renderPass->allocUniqueXform( MatrixF( true ) );
   MathUtils::getZBiasProjectionMatrix( gDecalBias, mCuller, tempMat );
   baseRenderInst.projection = tempMat;

   baseRenderInst.objectToWorld = &MatrixF::Identity;
   baseRenderInst.worldToCamera = renderPass->allocSharedXform(RenderPassManager::View);

   baseRenderInst.type = RenderPassManager::RIT_Decal;      

   // Make it the sort distance the max distance so that 
   // it renders after all the other opaque geometry in 
   // the prepass bin.
   baseRenderInst.sortDistSq = F32_MAX;

   // Get the best lights for the current camera position.
   LightManager *lm = state->getLightManager();
   if ( lm )
   {
      lm->setupLights(  NULL, 
                        mCuller.getPosition(),
                        mCuller.getTransform().getForwardVector(),
                        mCuller.getFarDist() );
      lm->getBestLights( baseRenderInst.lights, 4 );
      lm->resetLights();
   }

   Vector<DecalBatch> batches;
   DecalBatch *currentBatch = NULL;

   // Loop through DecalQueue collecting them into render batches.
   for ( U32 i = 0; i < mDecalQueue.size(); i++ )
   {
      DecalInstance *decal = mDecalQueue[i];      
      DecalData *data = decal->mDataBlock;
      Material *mat = data->getMaterial();

      if ( currentBatch == NULL )
      {
         // Start a new batch, beginning with this decal.

         batches.increment();
         currentBatch = &batches.last();
         currentBatch->startDecal = i;
         currentBatch->decalCount = 1;
         currentBatch->iCount = decal->mIndxCount;
         currentBatch->vCount = decal->mVertCount;
         currentBatch->mat = mat;
         currentBatch->matInst = decal->mDataBlock->getMaterialInstance();
         currentBatch->priority = decal->getRenderPriority();         
         currentBatch->dynamic = !(decal->mFlags & SaveDecal);

         continue;
      }

      if ( currentBatch->iCount + decal->mIndxCount >= smMaxIndices || 
           currentBatch->vCount + decal->mVertCount >= smMaxVerts ||
           currentBatch->mat != mat ||
           currentBatch->priority != decal->getRenderPriority() ||
           decal->mCustomTex )
      {
         // End batch.

         currentBatch = NULL;
         i--;
         continue;
      }

      // Add on to current batch.
      currentBatch->decalCount++;
      currentBatch->iCount += decal->mIndxCount;
      currentBatch->vCount += decal->mVertCount;
   }

   // Loop through batches allocating buffers and submitting render instances.
   for ( U32 i = 0; i < batches.size(); i++ )
   {
      DecalBatch &currentBatch = batches[i];

      // Allocate buffers...

      GFXVertexBufferHandle<DecalVertex> vb;
      vb.set( GFX, currentBatch.vCount, GFXBufferTypeDynamic );
      DecalVertex *vpPtr = vb.lock();

      GFXPrimitiveBufferHandle pb;
      pb.set( GFX, currentBatch.iCount, 0, GFXBufferTypeDynamic );
      U16 *pbPtr;
      pb.lock( &pbPtr );

      // Copy data into the buffers from all decals in this batch...

      U32 lastDecal = currentBatch.startDecal + currentBatch.decalCount;

      U32 voffset = 0;
      U32 ioffset = 0;

      // This is an ugly hack for ProjectedShadow!
      GFXTextureObject *customTex = NULL;

      for ( U32 j = currentBatch.startDecal; j < lastDecal; j++ )
      {
         DecalInstance *dinst = mDecalQueue[j];

         for ( U32 k = 0; k < dinst->mIndxCount; k++ )
         {
            *( pbPtr + ioffset + k ) = dinst->mIndices[k] + voffset;            
         }

         ioffset += dinst->mIndxCount;

         dMemcpy( vpPtr + voffset, dinst->mVerts, sizeof( DecalVertex ) * dinst->mVertCount );
         voffset += dinst->mVertCount;

         // Ugly hack for ProjectedShadow!
         if ( (dinst->mFlags & CustomDecal) && dinst->mCustomTex != NULL )
            customTex = *dinst->mCustomTex;
      }

      AssertFatal( ioffset == currentBatch.iCount, "bad" );
      AssertFatal( voffset == currentBatch.vCount, "bad" );
        
      pb.unlock();
      vb.unlock();

      // DecalManager must hold handles to these buffers so they remain valid,
      // we don't actually use them elsewhere.
      mPrimBuffs.push_back( pb );
      mVBs.push_back( vb );

      // Submit render inst...

      MeshRenderInst *ri = renderPass->allocInst<MeshRenderInst>();

      *ri = baseRenderInst;

      ri->primBuff = &mPrimBuffs.last();
      ri->vertBuff = &mVBs.last();

      ri->matInst = currentBatch.matInst;

      ri->prim = renderPass->allocPrim();
      ri->prim->type = GFXTriangleList;
      ri->prim->minIndex = 0;
      ri->prim->startIndex = 0;
      ri->prim->numPrimitives = currentBatch.iCount / 3;
      ri->prim->startVertex = 0;
      ri->prim->numVertices = currentBatch.vCount;

      // Ugly hack for ProjectedShadow!
      if ( customTex )
         ri->miscTex = customTex;

      // The decal bin will contain render instances for both decals and decalRoad's.
      // Dynamic decals render last, then editor decals and roads in priority order.
      // DefaultKey is sorted in descending order.
      ri->defaultKey = currentBatch.dynamic ? 0xFFFFFFFF : (U32)currentBatch.priority;
      ri->defaultKey2 = 1;//(U32)lastDecal->mDataBlock;

      renderPass->addInst( ri );
   }

   return false;
}

void DecalManager::renderDecalSpheres()
{
   if ( mData && Con::getBoolVariable( "$renderSpheres" ) )
   {
      PROFILE_SCOPE( DecalManager_renderDecalSpheres );

      const Vector<DecalSphere*> &grid = mData->getGrid();

      GFXDrawUtil *drawUtil = GFX->getDrawUtil();
      ColorI sphereLineColor( 0, 255, 0, 25 );
      ColorI sphereColor( 0, 0, 255, 30 );

      GFXStateBlockDesc desc;
      desc.setBlend( true );
      desc.setZReadWrite( true, false );

      for ( U32 i = 0; i < grid.size(); i++ )
      {
         DecalSphere *decalSphere = grid[i];
         const SphereF &worldSphere = decalSphere->mWorldSphere;
         drawUtil->drawSphere( desc, worldSphere.radius, worldSphere.center, sphereColor );
      }
   }
}

void DecalManager::advanceTime( F32 timeDelta )
{      
}

void DecalManager::interpolateTick( F32 delta )
{
}

void DecalManager::processTick()
{
}

bool DecalManager::_createDataFile()
{
   AssertFatal( !mData, "DecalManager::tried to create duplicate data file?" );

   // We need to construct a default file name
   char fileName[1024];
   fileName[0] = 0;

   // See if we know our current mission name
   char missionName[1024];
   dStrcpy( missionName, Con::getVariable( "$Client::MissionFile" ) );
   char *dot = dStrstr((const char*)missionName, ".mis");
   if(dot)
      *dot = '\0';
   
   dSprintf( fileName, sizeof(fileName), "%s.mis.decals", missionName );

   mDataFileName = StringTable->insert( fileName );

   if( !Torque::FS::IsFile( fileName ) )
   {
      DecalDataFile *file = new DecalDataFile();
      file->write( mDataFileName );
      delete file;
   }

   mData = ResourceManager::get().load( mDataFileName );
   return (bool)mData;
}

void DecalManager::saveDecals( const UTF8 *fileName )
{
   mDirty = false; 

   if ( mData )
      mData->write( fileName );   
}

bool DecalManager::loadDecals( const UTF8 *fileName )
{
   if( mData )
      clearData();
      
   mData = ResourceManager::get().load( fileName );

   mDirty = false;

   return mData != NULL;
}

void DecalManager::clearData()
{
   mClearDataSignal.trigger();
   
   // Free all geometry buffers.
   
   if( mData )
   {
      const Vector< DecalSphere* > grid = mData->getGrid();
      for( U32 i = 0; i < grid.size(); ++ i )
      {
         DecalSphere* sphere = grid[ i ];
         for( U32 n = 0; n < sphere->mItems.size(); ++ n )
            _freeBuffers( sphere->mItems[ n ] );
      }
   }
   
   mData = NULL;
	mDecalInstanceVec.clear();
}

ConsoleFunction( decalManagerSave, void, 1, 2, "decalManagerSave( mission decal file )" )
{
   if ( argc > 1 )
      gDecalManager->saveDecals( argv[1] );
   else
   {
      char missionName[1024];
      dStrcpy( missionName, Con::getVariable( "$Client::MissionFile" ) );
      char *dot = dStrstr((const char*)missionName, ".mis");

      if(dot)
	        *dot = '\0';
      
      char testName[1024];
      dSprintf( testName, sizeof(testName), "%s.mis.decals", missionName );

      char fullName[1024];
      Platform::makeFullPathName(testName, fullName, sizeof(fullName));
      gDecalManager->saveDecals( fullName );
   }
}

ConsoleFunction( decalManagerLoad, bool, 2, 2, "decalManagerLoad( mission decal file )" )
{
   return gDecalManager->loadDecals( argv[1] );
}

ConsoleFunction( decalManagerDirty, bool, 1, 1, "" )
{
   return gDecalManager->isDirty();
}

ConsoleFunction( decalManagerClear, void, 1, 1, "" )
{
   gDecalManager->clearData();
}

ConsoleFunction( decalManagerAddDecal, S32, 6, 7, "decalManagerAddDecal( %position, %normal, %rotation, %scale, %decalData, [%immortal]) - Place a Decal. Immortal decals don't age and must be removed explicitly. Returns Decal ID" )
{
   Point3F pos;
   dSscanf(argv[1], "%g %g %g", &pos.x, &pos.y, &pos.z);
   Point3F normal;
   dSscanf(argv[2], "%g %g %g", &normal.x, &normal.y, &normal.z);
   F32 rot = dAtof(argv[3]);
   F32 scale = dAtof(argv[4]);

   DecalData *decalData = NULL;
   Sim::findObject<DecalData>(argv[5], decalData);

   if(!decalData)
   {
      Con::warnf("Invalid Decal dataBlock: %s", argv[5]);
      return -1;
   }

   U8 flags = 0;
   if( argc >= 7 && dAtob(argv[6]) )
      flags |= PermanentDecal;

   DecalInstance *inst = gDecalManager->addDecal( pos, normal, rot, decalData, scale, -1, flags );

   if(!inst)
   {
      Con::warnf("Unable to create decal instance.");
      return -1;
   }
   
   // Add the decal to the instance vector.
   
   inst->mId = gDecalManager->mDecalInstanceVec.size();
   gDecalManager->mDecalInstanceVec.push_back( inst );
   
   return inst->mId;
}

ConsoleFunction( decalManagerRemoveDecal, bool, 2, 2, "decalManagerRemoveDecal( %decalId ) - Remove specified decal from the scene. Returns true if successful, false if decal not found.")
{
   S32 id = dAtoi(argv[1]);
   DecalInstance *inst = gDecalManager->getDecal(id);
   if(inst)
   {
      gDecalManager->removeDecal(inst);
      return true;
   }
   return false;
}
