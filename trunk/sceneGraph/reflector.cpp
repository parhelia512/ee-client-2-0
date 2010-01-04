//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "sceneGraph/reflector.h"

#include "console/consoleTypes.h"
#include "gfx/gfxCubemap.h"
#include "gfx/gfxDebugEvent.h"
#include "gfx/gfxTransformSaver.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "core/stream/bitStream.h"
#include "sceneGraph/reflectionManager.h"
#include "gui/3d/guiTSControl.h"
#include "ts/tsShapeInstance.h"
#include "gfx/gfxOcclusionQuery.h"
#include "lighting/shadowMap/lightShadowMap.h"


extern ColorI gCanvasClearColor;


//-------------------------------------------------------------------------
// ReflectorDesc
//-------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1( ReflectorDesc );

ReflectorDesc::ReflectorDesc() 
{
   texSize = 256;
   nearDist = 0.1f;
   farDist = 1000.0f;
   objectTypeMask = 0xFFFFFFFF;
   detailAdjust = 1.0f;
   priority = 1.0f;
   maxRateMs = 15;
   useOcclusionQuery = true;
}

ReflectorDesc::~ReflectorDesc()
{
}

void ReflectorDesc::initPersistFields()
{
   addField( "texSize", TypeS32, Offset( texSize, ReflectorDesc ) );
   addField( "nearDist", TypeF32, Offset( nearDist, ReflectorDesc ) );
   addField( "farDist", TypeF32, Offset( farDist, ReflectorDesc ) );
   addField( "objectTypeMask", TypeS32, Offset( objectTypeMask, ReflectorDesc ) );
   addField( "detailAdjust", TypeF32, Offset( detailAdjust, ReflectorDesc ) );
   addField( "priority", TypeF32, Offset( priority, ReflectorDesc ) );
   addField( "maxRateMs", TypeS32, Offset( maxRateMs, ReflectorDesc ) );
   addField( "useOcclusionQuery", TypeBool, Offset( useOcclusionQuery, ReflectorDesc ) );

   Parent::initPersistFields();
}

void ReflectorDesc::packData( BitStream *stream )
{
   Parent::packData( stream );

   stream->write( texSize );
   stream->write( nearDist );
   stream->write( farDist );
   stream->write( objectTypeMask );
   stream->write( detailAdjust );
   stream->write( priority );
   stream->write( maxRateMs );
   stream->writeFlag( useOcclusionQuery );
}

void ReflectorDesc::unpackData( BitStream *stream )
{
   Parent::unpackData( stream );

   stream->read( &texSize );
   stream->read( &nearDist );
   stream->read( &farDist );
   stream->read( &objectTypeMask );
   stream->read( &detailAdjust );
   stream->read( &priority );
   stream->read( &maxRateMs );
   useOcclusionQuery = stream->readFlag();
}

bool ReflectorDesc::preload( bool server, String &errorStr )
{
   if ( !Parent::preload( server, errorStr ) )
      return false;

   return true;
}

//-------------------------------------------------------------------------
// ReflectorBase
//-------------------------------------------------------------------------
ReflectorBase::ReflectorBase()
{
   mEnabled = false;
   mOccluded = false;
   mIsRendering = false;
   mDesc = NULL;
   mObject = NULL;
   mOcclusionQuery = GFX->createOcclusionQuery();
}

ReflectorBase::~ReflectorBase()
{
   delete mOcclusionQuery;
}

void ReflectorBase::unregisterReflector()
{
   if ( mEnabled )
   {
      REFLECTMGR->unregisterReflector( this );
      mEnabled = false;
   }
}

F32 ReflectorBase::calcScore( const ReflectParams &params )
{
   // First check the occlusion query to see if we're hidden.
   if (  mDesc->useOcclusionQuery && 
         mOcclusionQuery &&
         mOcclusionQuery->getStatus( true ) == GFXOcclusionQuery::Occluded )
      mOccluded = true;
   else
      mOccluded = false;

   // If we're disabled for any reason then there
   // is nothing more left to do.
   if (  !mEnabled || 
         mOccluded || 
         !params.culler.intersects( mObject->getWorldBox() ) )
   {
      score = 0;
      return score;
   }

   // This mess is calculating a score based on LOD.

   /*
   F32 sizeWS = getMax( object->getWorldBox().len_z(), 0.001f );      
   Point3F cameraOffset = params.culler.getPosition() - object->getPosition(); 
   F32 dist = getMax( cameraOffset.len(), 0.01f );      
   F32 worldToScreenScaleY = ( params.culler.getNearDist() * params.viewportExtent.y ) / 
                             ( params.culler.getNearTop() - params.culler.getNearBottom() );
   F32 sizeSS = sizeWS / dist * worldToScreenScaleY;
   */
   F32 lodFactor = 1.0f; //sizeSS;

   F32 maxRate = getMax( (F32)mDesc->maxRateMs, 1.0f );
   U32 delta = params.startOfUpdateMs - lastUpdateMs;      
   F32 timeFactor = getMax( (F32)delta / maxRate - 1.0f, 0.0f );

   score = mDesc->priority * timeFactor * lodFactor;

   return score;
}


//-------------------------------------------------------------------------
// CubeReflector
//-------------------------------------------------------------------------

CubeReflector::CubeReflector()
 : mLastTexSize( 0 )
{
}

void CubeReflector::registerReflector( SceneObject *object, 
                                       ReflectorDesc *desc )
{
   if ( mEnabled )
      return;

   mEnabled = true;
   mObject = object;
   mDesc = desc;
   REFLECTMGR->registerReflector( this );
}

void CubeReflector::unregisterReflector()
{
   if ( !mEnabled )
      return;

   REFLECTMGR->unregisterReflector( this );

   mEnabled = false;
}

void CubeReflector::updateReflection( const ReflectParams &params )
{
   GFXDEBUGEVENT_SCOPE( CubeReflector_UpdateReflection, ColorI::WHITE );

   mIsRendering = true;

   // Setup textures and targets...

   if ( mDesc->texSize <= 0 )
      mDesc->texSize = 12;

   bool texResize = ( mDesc->texSize != mLastTexSize );   

   const GFXFormat reflectFormat = REFLECTMGR->getReflectFormat();

   if (  texResize || 
         cubemap.isNull() ||
         cubemap->getFormat() != reflectFormat )
   {
      cubemap = GFX->createCubemap();
      cubemap->initDynamic( mDesc->texSize, reflectFormat );
   }
   
   GFXTexHandle depthBuff = LightShadowMap::_getDepthTarget( mDesc->texSize, mDesc->texSize );

   if ( renderTarget.isNull() )
      renderTarget = GFX->allocRenderToTextureTarget();   

   GFX->pushActiveRenderTarget();
   renderTarget->attachTexture( GFXTextureTarget::DepthStencil, depthBuff );

  
   F32 oldVisibleDist = gClientSceneGraph->getVisibleDistance();
   gClientSceneGraph->setVisibleDistance( mDesc->farDist );   


   for ( U32 i = 0; i < 6; i++ )
      updateFace( params, i );
   

   GFX->popActiveRenderTarget();

   gClientSceneGraph->setVisibleDistance(oldVisibleDist);

   mIsRendering = false;
   mLastTexSize = mDesc->texSize;
}

void CubeReflector::updateFace( const ReflectParams &params, U32 faceidx )
{
   GFXDEBUGEVENT_SCOPE( CubeReflector_UpdateFace, ColorI::WHITE );

   // store current matrices
   GFXTransformSaver saver;   

   // set projection to 90 degrees vertical and horizontal
   GFX->setFrustum(90.0f, 1.0f, mDesc->nearDist, mDesc->farDist );

   // We don't use a special clipping projection, but still need to initialize 
   // this for objects like SkyBox which will use it during a reflect pass.
   gClientSceneGraph->setNonClipProjection( GFX->getProjectionMatrix() );

   // Standard view that will be overridden below.
   VectorF vLookatPt(0.0f, 0.0f, 0.0f), vUpVec(0.0f, 0.0f, 0.0f), vRight(0.0f, 0.0f, 0.0f);

   switch( faceidx )
   {
   case 0 : // D3DCUBEMAP_FACE_POSITIVE_X:
      vLookatPt = VectorF( 1.0f, 0.0f, 0.0f );
      vUpVec    = VectorF( 0.0f, 1.0f, 0.0f );
      break;
   case 1 : // D3DCUBEMAP_FACE_NEGATIVE_X:
      vLookatPt = VectorF( -1.0f, 0.0f, 0.0f );
      vUpVec    = VectorF( 0.0f, 1.0f, 0.0f );
      break;
   case 2 : // D3DCUBEMAP_FACE_POSITIVE_Y:
      vLookatPt = VectorF( 0.0f, 1.0f, 0.0f );
      vUpVec    = VectorF( 0.0f, 0.0f,-1.0f );
      break;
   case 3 : // D3DCUBEMAP_FACE_NEGATIVE_Y:
      vLookatPt = VectorF( 0.0f, -1.0f, 0.0f );
      vUpVec    = VectorF( 0.0f, 0.0f, 1.0f );
      break;
   case 4 : // D3DCUBEMAP_FACE_POSITIVE_Z:
      vLookatPt = VectorF( 0.0f, 0.0f, 1.0f );
      vUpVec    = VectorF( 0.0f, 1.0f, 0.0f );
      break;
   case 5: // D3DCUBEMAP_FACE_NEGATIVE_Z:
      vLookatPt = VectorF( 0.0f, 0.0f, -1.0f );
      vUpVec    = VectorF( 0.0f, 1.0f, 0.0f );
      break;
   }

   // create camera matrix
   VectorF cross = mCross( vUpVec, vLookatPt );
   cross.normalizeSafe();

   MatrixF matView(true);
   matView.setColumn( 0, cross );
   matView.setColumn( 1, vLookatPt );
   matView.setColumn( 2, vUpVec );
   matView.setPosition( mObject->getPosition() );
   matView.inverse();

   GFX->setWorldMatrix(matView);

   renderTarget->attachTexture( GFXTextureTarget::Color0, cubemap, faceidx );
   GFX->setActiveRenderTarget( renderTarget );
   GFX->clear( GFXClearStencil | GFXClearTarget | GFXClearZBuffer, gCanvasClearColor, 1.0f, 0 );

   SceneState *baseState = gClientSceneGraph->createBaseState( SPT_Reflect );   
   baseState->setDiffuseCameraTransform( params.query->cameraMatrix );

   // render scene
   gClientSceneGraph->getLightManager()->registerGlobalLights( &baseState->getFrustum(), false );
   gClientSceneGraph->renderScene( baseState, mDesc->objectTypeMask );
   gClientSceneGraph->getLightManager()->unregisterAllLights();

   // Clean up.
   delete baseState;
   renderTarget->resolve();   
}

F32 CubeReflector::calcFaceScore( const ReflectParams &params, U32 faceidx )
{
   if ( Parent::calcScore( params ) <= 0.0f )
      return score;
   
   VectorF vLookatPt(0.0f, 0.0f, 0.0f);

   switch( faceidx )
   {
   case 0 : // D3DCUBEMAP_FACE_POSITIVE_X:
      vLookatPt = VectorF( 1.0f, 0.0f, 0.0f );      
      break;
   case 1 : // D3DCUBEMAP_FACE_NEGATIVE_X:
      vLookatPt = VectorF( -1.0f, 0.0f, 0.0f );      
      break;
   case 2 : // D3DCUBEMAP_FACE_POSITIVE_Y:
      vLookatPt = VectorF( 0.0f, 1.0f, 0.0f );      
      break;
   case 3 : // D3DCUBEMAP_FACE_NEGATIVE_Y:
      vLookatPt = VectorF( 0.0f, -1.0f, 0.0f );      
      break;
   case 4 : // D3DCUBEMAP_FACE_POSITIVE_Z:
      vLookatPt = VectorF( 0.0f, 0.0f, 1.0f );      
      break;
   case 5: // D3DCUBEMAP_FACE_NEGATIVE_Z:
      vLookatPt = VectorF( 0.0f, 0.0f, -1.0f );      
      break;
   }

   VectorF cameraDir;
   params.query->cameraMatrix.getColumn( 1, &cameraDir );

   F32 dot = mDot( cameraDir, -vLookatPt );

   dot = getMax( ( dot + 1.0f ) / 2.0f, 0.1f );

   score *= dot;

   return score;
}

F32 CubeReflector::CubeFaceReflector::calcScore( const ReflectParams &params )
{
   score = cube->calcFaceScore( params, faceIdx );
   mOccluded = cube->isOccluded();
   return score;
}


//-------------------------------------------------------------------------
// PlaneReflector
//-------------------------------------------------------------------------

void PlaneReflector::registerReflector(   SceneObject *object,
                                          ReflectorDesc *desc )
{
   mEnabled = true;
   mObject = object;
   mDesc = desc;
   mLastDir = Point3F::One;
   mLastPos = Point3F::Max;

   REFLECTMGR->registerReflector( this );
}

F32 PlaneReflector::calcScore( const ReflectParams &params )
{
   if ( Parent::calcScore( params ) <= 0.0f )
      return score;

   // The planar reflection is view dependent to score it
   // higher if the view direction and/or position has changed.

   // Get the current camera info.
   VectorF camDir = params.query->cameraMatrix.getForwardVector();
   Point3F camPos = params.query->cameraMatrix.getPosition();

   // Scale up the score based on the view direction change.
   F32 dot = mDot( camDir, mLastDir );
   dot = ( 1.0f - dot ) * 1000.0f;
   score += dot * mDesc->priority;

   // Also account for the camera movement.
   score += ( camPos - mLastPos ).lenSquared() * mDesc->priority;   

   return score;
}

void PlaneReflector::updateReflection( const ReflectParams &params )
{
   PROFILE_SCOPE(PlaneReflector_updateReflection);   
   GFXDEBUGEVENT_SCOPE( PlaneReflector_updateReflection, ColorI::WHITE );

   mIsRendering = true;

   if ( mDesc->texSize <= 0 )
      mDesc->texSize = 12;

   bool texResize = ( mDesc->texSize != mLastTexSize );  
   
   mLastTexSize = mDesc->texSize;

   const Point2I texSize( mDesc->texSize, mDesc->texSize );

   if (  texResize || 
         reflectTex.isNull() ||
         reflectTex->getFormat() != REFLECTMGR->getReflectFormat() )
      reflectTex = REFLECTMGR->allocRenderTarget( texSize );

   GFXTexHandle depthBuff = LightShadowMap::_getDepthTarget( texSize.x, texSize.y );

   // store current matrices
   GFXTransformSaver saver;
   MatrixF proj = GFX->getProjectionMatrix();
   
   GFX->setFrustum( mRadToDeg(params.query->fov), 
                    F32(params.viewportExtent.x) / F32(params.viewportExtent.y ),
                    params.query->nearPlane, 
                    params.query->farPlane );
   
   gClientSceneGraph->mNormCamPos = params.query->cameraMatrix.getPosition();
   
   // Store the last view info for scoring.
   mLastDir = params.query->cameraMatrix.getForwardVector();
   mLastPos = params.query->cameraMatrix.getPosition();

   if ( objectSpace )
   {
      // set up camera transform relative to object
      MatrixF invObjTrans = mObject->getRenderTransform();
      invObjTrans.inverse();
      MatrixF relCamTrans = invObjTrans * params.query->cameraMatrix;

      MatrixF camReflectTrans = getCameraReflection( relCamTrans );
      MatrixF camTrans = mObject->getRenderTransform() * camReflectTrans;
      camTrans.inverse();

      GFX->setWorldMatrix( camTrans );

      // use relative reflect transform for modelview since clip plane is in object space
      camTrans = camReflectTrans;
      camTrans.inverse();

      // set new projection matrix
      gClientSceneGraph->setNonClipProjection( (MatrixF&) GFX->getProjectionMatrix() );
      MatrixF clipProj = getFrustumClipProj( camTrans );
      GFX->setProjectionMatrix( clipProj );
   }    
   else
   {
      MatrixF camTrans = params.query->cameraMatrix;

      // set world mat from new camera view
      MatrixF camReflectTrans = getCameraReflection( camTrans );
      camReflectTrans.inverse();
      GFX->setWorldMatrix( camReflectTrans );

      // set new projection matrix
      gClientSceneGraph->setNonClipProjection( (MatrixF&) GFX->getProjectionMatrix() );
      MatrixF clipProj = getFrustumClipProj( camReflectTrans );
      GFX->setProjectionMatrix( clipProj );
   }   

   // Adjust the detail amount
   F32 detailAdjustBackup = TSShapeInstance::smDetailAdjust;
   TSShapeInstance::smDetailAdjust *= mDesc->detailAdjust;


   if(reflectTarget.isNull())
      reflectTarget = GFX->allocRenderToTextureTarget();
   reflectTarget->attachTexture( GFXTextureTarget::Color0, reflectTex );
   reflectTarget->attachTexture( GFXTextureTarget::DepthStencil, depthBuff );
   GFX->pushActiveRenderTarget();
   GFX->setActiveRenderTarget( reflectTarget );

   GFX->clear( GFXClearZBuffer | GFXClearStencil | GFXClearTarget, gCanvasClearColor, 1.0f, 0 );

   SceneState *baseState = gClientSceneGraph->createBaseState( SPT_Reflect );   
   baseState->setDiffuseCameraTransform( params.query->cameraMatrix );

   U32 objTypeFlag = -1;
   gClientSceneGraph->getLightManager()->registerGlobalLights( &baseState->getFrustum(), false );
   gClientSceneGraph->renderScene( baseState, objTypeFlag );
   gClientSceneGraph->getLightManager()->unregisterAllLights();

   // Clean up.
   delete baseState;
   reflectTarget->resolve();
   GFX->popActiveRenderTarget();

   // Restore detail adjust amount.
   TSShapeInstance::smDetailAdjust = detailAdjustBackup;

   mIsRendering = false;
}

MatrixF PlaneReflector::getCameraReflection( MatrixF &camTrans )
{
   Point3F normal = refplane;

   // Figure out new cam position
   Point3F camPos = camTrans.getPosition();
   F32 dist = refplane.distToPlane( camPos );
   Point3F newCamPos = camPos - normal * dist * 2.0;

   // Figure out new look direction
   Point3F i, j, k;
   camTrans.getColumn( 0, &i );
   camTrans.getColumn( 1, &j );
   camTrans.getColumn( 2, &k );

   i = MathUtils::reflect( i, normal );
   j = MathUtils::reflect( j, normal );
   k = MathUtils::reflect( k, normal );
   //mCross( i, j, &k );


   MatrixF newTrans(true);
   newTrans.setColumn( 0, i );
   newTrans.setColumn( 1, j );
   newTrans.setColumn( 2, k );

   newTrans.setPosition( newCamPos );

   return newTrans;
}

inline float sgn(float a)
{
   if (a > 0.0F) return (1.0F);
   if (a < 0.0F) return (-1.0F);
   return (0.0F);
}

MatrixF PlaneReflector::getFrustumClipProj( MatrixF &modelview )
{
   static MatrixF rotMat(EulerF( static_cast<F32>(M_PI / 2.f), 0.0, 0.0));
   static MatrixF invRotMat(EulerF( -static_cast<F32>(M_PI / 2.f), 0.0, 0.0));


   MatrixF revModelview = modelview;
   revModelview = rotMat * revModelview;  // add rotation to modelview because it needs to be removed from projection

   // rotate clip plane into modelview space
   Point4F clipPlane;
   Point3F pnt = refplane * -(refplane.d + 0.0 );
   Point3F norm = refplane;

   revModelview.mulP( pnt );
   revModelview.mulV( norm );
   norm.normalize();

   clipPlane.set( norm.x, norm.y, norm.z, -mDot( pnt, norm ) );


   // Manipulate projection matrix
   //------------------------------------------------------------------------
   MatrixF proj = GFX->getProjectionMatrix();
   proj.mul( invRotMat );  // reverse rotation imposed by Torque
   proj.transpose();       // switch to row-major order

   // Calculate the clip-space corner point opposite the clipping plane
   // as (sgn(clipPlane.x), sgn(clipPlane.y), 1, 1) and
   // transform it into camera space by multiplying it
   // by the inverse of the projection matrix
   Vector4F	q;
   q.x = sgn(clipPlane.x) / proj(0,0);
   q.y = sgn(clipPlane.y) / proj(1,1);
   q.z = -1.0F;
   q.w = ( 1.0F - proj(2,2) ) / proj(3,2);

   F32 a = 1.0 / (clipPlane.x * q.x + clipPlane.y * q.y + clipPlane.z * q.z + clipPlane.w * q.w);

   Vector4F c = clipPlane * a;

   // CodeReview [ags 1/23/08] Come up with a better way to deal with this.
   if(GFX->getAdapterType() == OpenGL)
      c.z += 1.0f;

   // Replace the third column of the projection matrix
   proj.setColumn( 2, c );
   proj.transpose(); // convert back to column major order
   proj.mul( rotMat );  // restore Torque rotation

   return proj;
}