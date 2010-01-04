//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------


#include "platform/platform.h"
#include "T3D/portal.h"

#include "console/consoleTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxTransformSaver.h"
#include "core/stream/bitStream.h"
#include "math/mathIO.h"
#include "renderInstance/renderPassManager.h"
#include "gfx/gfxDrawUtil.h"
#include "math/mathUtils.h"
#include "gfx/gfxTransformSaver.h"
#include "sceneGraph/sceneRoot.h"

IMPLEMENT_CO_NETOBJECT_V1( Portal );

bool Portal::smRenderPortals = false;

Portal::Portal()
{
   mNetFlags.set( Ghostable | ScopeAlways );
   mTypeMask |= StaticObjectType;

   mObjScale.set( 1.0f, 0.25f, 1.0f );
}

Portal::~Portal()
{
}

void Portal::initPersistFields()
{
   Parent::initPersistFields();
}

void Portal::consoleInit()
{
   Con::addVariable( "$Portal::renderPortals", TypeBool, &smRenderPortals );
}

bool Portal::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   generateOBBPoints();

   return true;
}

void Portal::onRemove()
{
   _clearZones();
   Parent::onRemove();
}

void Portal::setTransform(const MatrixF & mat)
{
   Parent::setTransform( mat );
   generateOBBPoints();

   setMaskBits( TransformMask );
}

U32 Portal::packUpdate( NetConnection *con, U32 mask, BitStream *stream )
{
   U32 retMask = Parent::packUpdate( con, mask, stream );

   if ( stream->writeFlag( mask & TransformMask ) ) 
   {
      mathWrite( *stream, mObjToWorld );
      mathWrite( *stream, mObjScale );
   }

   return retMask;
}

void Portal::unpackUpdate( NetConnection *con, BitStream *stream )
{
   Parent::unpackUpdate( con, stream );

   if ( stream->readFlag() )  // TransformMask
   {
      mathRead(*stream, &mObjToWorld);
      mathRead(*stream, &mObjScale);

      if ( isProperlyAdded() )
         setTransform( mObjToWorld );
   }
}

U32 Portal::getPointZone( const Point3F &p )
{
   PlaneF portalPlane( getPosition(), mObjToWorld.getForwardVector() );

   // If the point is contained,
   // do a test to see which side of the
   // Portal's plane it's on, and return the 
   // appropriate Zone's mZoneRangeStart value.
   Point3F objPoint( 0, 0, 0 );
   getWorldTransform().mulP( p, &objPoint );
   objPoint.convolveInverse( getScale() );

   if ( mObjBox.isContained( objPoint ) )
   {
      S32 pointSide = portalPlane.whichSide( p );
      S32 zoneOneSide = mZones[0] ? portalPlane.whichSide( mZones[0]->getPosition() ) : -2;
      S32 zoneTwoSide = mZones[1] ? portalPlane.whichSide( mZones[1]->getPosition() ) : -2;
   
      U32 frontZoneIdx = zoneOneSide == PlaneF::Front ? 0 : 1;

     if ( pointSide == zoneOneSide && zoneOneSide != -2 )
        return mZones[0]->getZoneRangeStart();
     else if ( pointSide == zoneTwoSide && zoneTwoSide != -2 )
        return mZones[1]->getZoneRangeStart();
     else if ( mZones[frontZoneIdx] )
        return mZones[frontZoneIdx]->getZoneRangeStart();
   }

   return 0;
}

bool Portal::getOverlappingZones(  SceneObject *obj,
                                 U32 *zones,
                                 U32 *numZones )
{
   // If this portal is connected
   // to nothing, don't treat it
   // like a Zone.
   if ( !mZones[0] && !mZones[1] )
   {
      *numZones = 0;
      return true;
   }

   return Parent::getOverlappingZones( obj, zones, numZones );
}

bool Portal::scopeObject( const Point3F &rootPosition,
                           const F32 rootDistance, 
                           bool *zoneScopeState )
{
   // The sky zone is always in scope!
   //*zoneScopeState = true;
   return false;
}

bool Portal::prepRenderImage(   SceneState* state, 
                                 const U32 stateKey,
                                 const U32 startZone, 
                                 const bool modifyBaseState )
{
   if ( isLastState(state, stateKey ) )
      return false;
   
   setLastState( state, stateKey );

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if (state->isObjectRendered(this)) 
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &Portal::renderObject );
      ri->type = RenderPassManager::RIT_Object;
      ri->defaultKey = 0;
      ri->defaultKey2 = 0;
      state->getRenderPass()->addInst( ri );
   }
   
   return false;
}

void Portal::renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance* overrideMat )
{
   if ( overrideMat )
      return;

   // Only render if the portal render
   // flag is enabled, or this object
   // is currently selected in the editor.
   if ( !smRenderPortals && !isSelected() )
      return;

   GFXStateBlockDesc desc;
   desc.setBlend( true );
   desc.setZReadWrite( true, false );
   desc.setCullMode( GFXCullNone );

   {
      GFXTransformSaver saver;

      MatrixF mat = getRenderTransform();
      
      // Modify the scale to have 
      // a very thin y-extent.
      Point3F scale = getScale();
      scale.y = 0.0f;
      mat.scale( scale );

      GFX->multWorld( mat );

      GFXDrawUtil *drawer = GFX->getDrawUtil();
      if ( !mZones[0] || !mZones[1] )
         drawer->drawCube( desc, mObjBox, ColorI( 0, 0, 255, 45 ) );
      else
         drawer->drawCube( desc, mObjBox, ColorI( 0, 255, 0, 45 ) );
   }

   {
      GFXTransformSaver saver;

      MatrixF mat = getRenderTransform();
      mat.scale( getScale() );

      GFX->multWorld( mat );

      GFXDrawUtil *drawer = GFX->getDrawUtil();
      if ( !mZones[0] || !mZones[1] )
         drawer->drawCube( desc, mObjBox, ColorI( 255, 255, 255, 45 ) );
      else
         drawer->drawCube( desc, mObjBox, ColorI( 255, 255, 255, 45 ) );
   }
}

void Portal::_clearZones()
{
   if ( mZones[0] )
      mZones[0]->_removePortal( this );
   if ( mZones[1] )
      mZones[1]->_removePortal( this );

   SceneRoot *root = isServerObject() ? gServerSceneRoot : gClientSceneRoot;
   root->_removePortal( this );

   mZones[0] = mZones[1] = 0;
}

void Portal::onRezone()
{
   _clearZones();

   SceneObjectRef* walk = mZoneRefHead;
   U32 zoneNum = 0;
   for ( ; walk != NULL; walk = walk->nextInObj )
   {
      // Skip over the outside zone.
      if ( walk->zone == 0 )
         continue;

      Zone *zone = dynamic_cast<Zone*>( mSceneManager->getZoneOwner( walk->zone ) );
      if ( !zone )
         continue;

      mZones[zoneNum++] = zone;
      if ( zoneNum == 2 )
         break;
   }

   if ( mZones[0] )
      mZones[0]->_addPortal( this );
   if ( mZones[1] )
      mZones[1]->_addPortal( this );

   // If we have *one* NULL zone, 
   // we are linked to the outside zone.
   // Need to add ourselves to the SceneRoot.
   if ( (!mZones[0] || !mZones[1]) && !( !mZones[0] && !mZones[1] ) )
   {
      SceneRoot *root = isServerObject() ? gServerSceneRoot : gClientSceneRoot;
      root->_addPortal( this );
   }
}

void Portal::generateOBBPoints()
{
   Point3F boxHalfExtents = mObjScale * 0.5f;

   Point3F center;
   mObjToWorld.getColumn( 3, &center );
   
   VectorF right, fwd, up;
   mObjToWorld.getColumn( 0, &right );
   mObjToWorld.getColumn( 1, &fwd );
   mObjToWorld.getColumn( 2, &up );

   // Near bottom right.
   mOBBPoints[0] = center - (fwd * boxHalfExtents.y) - ( right * boxHalfExtents.x ) - ( up * boxHalfExtents.z );
   // Near top right.
   mOBBPoints[1] = center - (fwd * boxHalfExtents.y) + ( right * boxHalfExtents.x ) + ( up * boxHalfExtents.z );
   // Near top left.
   mOBBPoints[2] = center - (fwd * boxHalfExtents.y) - ( right * boxHalfExtents.x ) + ( up * boxHalfExtents.z );
   // Near bottom left.
   mOBBPoints[3] = center - (fwd * boxHalfExtents.y) - ( right * boxHalfExtents.x ) - ( up * boxHalfExtents.z );
      
   // Far bottom right.
   mOBBPoints[4] = center + (fwd * boxHalfExtents.y) + ( right * boxHalfExtents.x ) - ( up * boxHalfExtents.z );
   // Far top right.
   mOBBPoints[5] = center + (fwd * boxHalfExtents.y) + ( right * boxHalfExtents.x ) + ( up * boxHalfExtents.z );
   // Far top left.
   mOBBPoints[6] = center + (fwd * boxHalfExtents.y) - ( right * boxHalfExtents.x ) + ( up * boxHalfExtents.z );
   // Far bottom left.
   mOBBPoints[7] = center + (fwd * boxHalfExtents.y) - ( right * boxHalfExtents.x ) - ( up * boxHalfExtents.z );

   // Bottom right.
   mOrientedPortalPoints[0] = center + ( right * boxHalfExtents.x ) - ( up * boxHalfExtents.z );
   // Bottom left.
   mOrientedPortalPoints[1] = center - ( right * boxHalfExtents.x ) - ( up * boxHalfExtents.z );
   // Top right.
   mOrientedPortalPoints[2] = center + ( right * boxHalfExtents.x ) + ( up * boxHalfExtents.z );
   // Top left.
   mOrientedPortalPoints[3] = center - ( right * boxHalfExtents.x ) + ( up * boxHalfExtents.z );
}

void Portal::generatePortalFrustum( SceneState *state, Frustum *outFrustum )
{
   // None of this data changes.
   const Frustum &frust = state->getFrustum();
   const RectI &viewport = GFX->getViewport();
   const MatrixF &worldMat = GFX->getWorldMatrix();
   const MatrixF &projMat = GFX->getProjectionMatrix();

   // Temp variables.
   Point2F vpExtent( (F32)viewport.extent.x, (F32)viewport.extent.y );
   Point3F boxPointsSS[4];

   Point3F *portalPoints = &mOrientedPortalPoints[0];

   U32 count = 0;
   // Project the points to screen space.
   for ( U32 i = 0; i < 4; i++ )
   {
      bool projected = MathUtils::mProjectWorldToScreen( portalPoints[i], &boxPointsSS[i], viewport, worldMat, projMat );
      if ( projected )
         continue;
      
      count++;

      // Point3F intersectionPt( 0, 0, 0 );
      // const Point3F *points = frust.getPoints();
      // for ( U32 j = 0; j < 4; j++ )
      // {
      //    bool intersects = frust.edgeFaceIntersect(   points[Frustum::smEdgeIndices[j][0]], points[Frustum::smEdgeIndices[j][1]],
      //                                                 mOrientedPortalPoints[0], 
      //                                                 mOrientedPortalPoints[2], 
      //                                                 mOrientedPortalPoints[3], 
      //                                                 mOrientedPortalPoints[1], &intersectionPt );

      //    const VectorF &fwd = frust.getTransform().getForwardVector();

      //    if ( intersects )
      //       MathUtils::mProjectWorldToScreen( intersectionPt + fwd, &boxPointsSS[i], viewport, worldMat, projMat );
      // }
   }

   // If more than 3 points failed to project
   // and the camera is fairly close to the portal's
   // plane, go ahead and use the full frustum.
   if ( count > 3 && (state->getCameraPosition() - getPosition()).lenSquared() < 4.0f )
   {
      outFrustum->set( frust );
      return;
   }

   // Clamp results to the viewport.
   for ( U32 i = 0; i < 4; i++ )
   {
      if ( boxPointsSS[i].z > 1.0f )
      {
         boxPointsSS[i].x = -boxPointsSS[i].x;
         boxPointsSS[i].y = -boxPointsSS[i].y;
      }
      boxPointsSS[i].x = mClampF( boxPointsSS[i].x, (F32)viewport.point.x, (F32)viewport.point.x + viewport.extent.x );
      boxPointsSS[i].y = mClampF( boxPointsSS[i].y, (F32)viewport.point.y, (F32)viewport.point.y + viewport.extent.y );
   }

   // Get the min x of all points.
   F32 minX = getMin( getMin( boxPointsSS[0].x, boxPointsSS[1].x ), getMin( boxPointsSS[2].x, boxPointsSS[3].x ) );

   // Get the max x of all points.
   F32 maxX = getMax( getMax( boxPointsSS[0].x, boxPointsSS[1].x ), getMax( boxPointsSS[2].x, boxPointsSS[3].x ) );

   // Get the min y of all points.
   F32 minY = getMin( getMin( boxPointsSS[0].y, boxPointsSS[1].y ), getMin( boxPointsSS[2].y, boxPointsSS[3].y ) );
   
   // Get the max y of all points.
   F32 maxY = getMax( getMax( boxPointsSS[0].y, boxPointsSS[1].y ), getMax( boxPointsSS[2].y, boxPointsSS[3].y ) );

   boxPointsSS[0].set( minX, minY, boxPointsSS[0].z );
   boxPointsSS[1].set( maxX, maxY, boxPointsSS[1].z );

   // Get the extent of the current frustum.
   F32 frustXExtent = mFabs( frust.getNearLeft() - frust.getNearRight() );
   F32 frustYExtent = mFabs( frust.getNearTop() - frust.getNearBottom() );

   // Normalize pixel cordinates to 0 to 1,
   // then convert into the range of negative half frustum extents.
   boxPointsSS[0].x = ( ( boxPointsSS[0].x / vpExtent.x ) * frustXExtent ) - (frustXExtent / 2.0f);
   boxPointsSS[0].y = ( ( boxPointsSS[0].y / vpExtent.y ) * frustYExtent ) - (frustYExtent / 2.0f);
   boxPointsSS[1].x = ( ( boxPointsSS[1].x / vpExtent.x ) * frustXExtent ) - (frustXExtent / 2.0f);
   boxPointsSS[1].y = ( ( boxPointsSS[1].y / vpExtent.y ) * frustYExtent ) - (frustYExtent / 2.0f);

   // Find the real top, left, right, and bottom.
   F32 realRight = getMax( boxPointsSS[0].x, boxPointsSS[1].x );
   F32 realLeft = getMin( boxPointsSS[0].x, boxPointsSS[1].x );
   F32 realTop = getMax( boxPointsSS[0].y, boxPointsSS[1].y );
   F32 realBottom = getMin( boxPointsSS[0].y, boxPointsSS[1].y );

   outFrustum->set( false, realLeft, realRight, -realTop, -realBottom, frust.getNearDist(), frust.getFarDist(), frust.getTransform() );
};