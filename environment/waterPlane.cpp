//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "environment/waterPlane.h"

#include "core/util/safeDelete.h"
#include "sceneGraph/sceneState.h"
#include "sceneGraph/sceneGraph.h"
#include "lighting/lightInfo.h"
#include "core/stream/bitStream.h"
#include "math/mathIO.h"
#include "math/mathUtils.h"
#include "console/consoleTypes.h"
#include "gui/3d/guiTSControl.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDebugEvent.h"
#include "gfx/gfxOcclusionQuery.h"
#include "renderInstance/renderPassManager.h"
#include "sim/netConnection.h"
#include "sceneGraph/reflectionManager.h"
#include "ts/tsShapeInstance.h"
#include "T3D/gameFunctions.h"
#include "postFx/postEffect.h"
#include "math/util/matrixSet.h"

extern ColorI gCanvasClearColor;

#define BLEND_TEX_SIZE 256
#define V_SHADER_PARAM_OFFSET 50

IMPLEMENT_CO_NETOBJECT_V1(WaterPlane);

//*****************************************************************************
// WaterPlane
//*****************************************************************************
WaterPlane::WaterPlane()
{
   mGridElementSize = 1.0f;
   mGridSize = 101;
   mGridSizeMinusOne = mGridSize - 1;

   mNetFlags.set(Ghostable | ScopeAlways);

   mVertCount = 0;
   mIndxCount = 0;
   mPrimCount = 0;   
}

WaterPlane::~WaterPlane()
{
}

//-----------------------------------------------------------------------------
// onAdd
//-----------------------------------------------------------------------------
bool WaterPlane::onAdd()
{
   if ( !Parent::onAdd() )
      return false;
   
   setGlobalBounds();
   resetWorldBox();
   addToScene();

   mWaterFogData.plane.set( 0, 0, 1, -getPosition().z );

   return true;
}

//-----------------------------------------------------------------------------
// onRemove
//-----------------------------------------------------------------------------
void WaterPlane::onRemove()
{   
   removeFromScene();

   Parent::onRemove();
}

//-----------------------------------------------------------------------------
// packUpdate
//-----------------------------------------------------------------------------
U32 WaterPlane::packUpdate(NetConnection* con, U32 mask, BitStream* stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   stream->write( mGridSize );
   stream->write( mGridElementSize );   

   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      stream->write( getPosition().z );        
   }

   return retMask;
}

//-----------------------------------------------------------------------------
// unpackUpdate
//-----------------------------------------------------------------------------
void WaterPlane::unpackUpdate(NetConnection* con, BitStream* stream)
{
   Parent::unpackUpdate(con, stream);

   U32 inGridSize;
   stream->read( &inGridSize );
   setGridSize( inGridSize );

   F32 inGridElementSize;
   stream->read( &inGridElementSize );
   setGridElementSize( inGridElementSize );

   if( stream->readFlag() ) // UpdateMask
   {
      float posZ;
      stream->read( &posZ );
      Point3F newPos = getPosition();
      newPos.z = posZ;
      setPosition( newPos );
   }  
}

//-----------------------------------------------------------------------------
// Setup vertex and index buffers
//-----------------------------------------------------------------------------
void WaterPlane::setupVBIB( const Point3F &camPos )
{
   F32 squareSize = mGridElementSize;

   // Position of the first vertex
   // x and y components of which will
   // be the camera position x and y offset
   // from the center of the grid to the corner
   // dependent on the dimensions of the grid squares.

   ColorI waterColor(31, 56, 64, 127);
   GFXVertexColor vertCol(waterColor);

   F32 offsetAmt = (F32)(mGridSize - 1) / 2.0f;

   Point3F cornerPosition(0, 0, 0);
   cornerPosition.x -= squareSize * offsetAmt;
   cornerPosition.y -= squareSize * offsetAmt;

   mIndxCount = (mGridSizeMinusOne) * (mGridSizeMinusOne);
   mPrimCount = mIndxCount * 2;
   
   mPrimBuff.set( GFX, mIndxCount * 6, 1, GFXBufferTypeStatic ); 

   mVertCount = (mGridSize * mGridSize);
   
   mVertBuff.set( GFX, mVertCount, GFXBufferTypeStatic );
   
   GFXWaterVertex *vertPtr = mVertBuff.lock();

   F32 xVal = 0, yVal = 0;

   U32 innerEdgeIdx = mGridSizeMinusOne - 1;

   F32 frac = (mFrustum.getFarDist() - 130.0f) - (squareSize * offsetAmt);

   F32 cornerOffset = 0.5f;
   F32 edgeOffset = 0.98f;

   // Build the full grid first.
   for ( U32 i = 0; i < mGridSize; i++ )
   {
      for ( U32 j = 0; j < mGridSize; j++ )
      {
         yVal = cornerPosition.y + (F32)(i * squareSize);
         xVal = cornerPosition.x + (F32)(j * squareSize);

         U32 index = (i * mGridSize) + j;

         vertPtr[index].horizonFactor.set( 0, 0, 1.0f, 0 );
         vertPtr[index].point.set( xVal, yVal, 0 );
         vertPtr[index].normal.set( 0, 0, 1.0f );
         vertPtr[index].undulateData.set(xVal,yVal);
         vertPtr[index].color = waterColor;
      }
   }

   bool front, back, left, right;
   bool upperLeft, lowerLeft, upperRight, lowerRight;

   front = back = left = right = false;
   upperLeft = lowerLeft = upperRight = lowerRight = false;

   for ( U32 i = 0; i < mGridSize; i++ )
   {
      for ( U32 j = 0; j < mGridSize; j++ )
      {
         U32 index = (i * mGridSize) + j;

         yVal = cornerPosition.y + (i * squareSize);
         xVal = cornerPosition.x + (j * squareSize);

         if (  i == 0 || 
               j == 0 || 
               i == mGridSizeMinusOne || 
               j == mGridSizeMinusOne  )
         {
            front = index >= (mGridSizeMinusOne * mGridSize) && index <= (mGridSizeMinusOne * mGridSize) + mGridSizeMinusOne;
            back = index >= 0 && index <= mGridSizeMinusOne;
            left = (index % mGridSize) == 0 && index != 0;
            right = ((index+1) % mGridSize) == 0 && index != 0;

            upperLeft = index == mGridSize * mGridSizeMinusOne;
            lowerLeft = index == 0;

            upperRight = index == (mGridSize * mGridSize) - 1;
            lowerRight = index == mGridSizeMinusOne;

            if ( front )
               yVal += frac * edgeOffset;
            else if ( back )
               yVal -= frac * edgeOffset;
            else if ( left )
               xVal -= frac * edgeOffset;
            else if ( right )
               xVal += frac * edgeOffset;

            if ( upperLeft )
            {
               yVal = cornerPosition.y + (i * squareSize);
               xVal = cornerPosition.x + (j * squareSize);
               yVal += frac;
               xVal -= frac;
               
               xVal *= cornerOffset;
               yVal *= cornerOffset;
            }
            else if ( lowerLeft )
            {
               yVal = cornerPosition.y + (i * squareSize);
               xVal = cornerPosition.x + (j * squareSize);
               yVal -= frac;
               xVal -= frac;
                              
               xVal *= cornerOffset;
               yVal *= cornerOffset;
            }
            else if ( upperRight )
            {
               yVal = cornerPosition.y + (i * squareSize);
               xVal = cornerPosition.x + (j * squareSize);
               yVal += frac;
               xVal += frac;
                              
               xVal *= cornerOffset;
               yVal *= cornerOffset;
            }
            else if ( lowerRight )
            {
               yVal = cornerPosition.y + (i * squareSize);
               xVal = cornerPosition.x + (j * squareSize);
               yVal -= frac;
               xVal += frac;
                              
               xVal *= cornerOffset;
               yVal *= cornerOffset;
            }

            // Set edge vert factor to 1.0f here.
            vertPtr[index].horizonFactor.x = 1.0f;
            vertPtr[index].horizonFactor.y = 1.0f;
            vertPtr[index].point.set( xVal, yVal, 0 );
         }
         else if (   (i >= 1 && i <= innerEdgeIdx) && (j == 1 || j == innerEdgeIdx) ||
                     (i == 1 && (j >= 1 && j <= innerEdgeIdx ) ) || 
                     (i == innerEdgeIdx && (j >= 1 && j <= innerEdgeIdx ) ) )
         {
            front = index >= (mGridSizeMinusOne * mGridSizeMinusOne) && index <= ((mGridSizeMinusOne * mGridSize) - 2);
            back = index >= (mGridSize + 1) && index <= (mGridSize + (mGridSizeMinusOne));
            left = ((index-1) % mGridSize) == 0 && index != 0;
            right = ((index + 2) % mGridSize) == 0 && index != 0;

            upperLeft = index == (mGridSizeMinusOne * mGridSizeMinusOne);
            lowerLeft = index == mGridSize + 1;

            upperRight = index == (mGridSize * mGridSizeMinusOne) - 2;
            lowerRight = index == (mGridSizeMinusOne - 1) + mGridSize;

            if ( front )
               yVal += (frac * edgeOffset) + squareSize;
            else if ( back )
               yVal -= (frac * edgeOffset) + squareSize;
            else if ( left )
               xVal -= (frac * edgeOffset) + squareSize;
            else if ( right )
               xVal += (frac * edgeOffset) + squareSize;

            if ( upperLeft )
            {
               yVal = cornerPosition.y + (i * squareSize);
               xVal = cornerPosition.x + (j * squareSize);
               yVal += frac + squareSize;
               xVal -= frac + squareSize;
                              
               xVal *= cornerOffset;
               yVal *= cornerOffset;
            }
            else if ( lowerLeft )
            {
               yVal = cornerPosition.y + (i * squareSize);
               xVal = cornerPosition.x + (j * squareSize);
               yVal -= frac + squareSize;
               xVal -= frac + squareSize;
                              
               xVal *= cornerOffset;
               yVal *= cornerOffset;
            }
            else if ( upperRight )
            {
               yVal = cornerPosition.y + (i * squareSize);
               xVal = cornerPosition.x + (j * squareSize);
               yVal += frac + squareSize;
               xVal += frac + squareSize;
                              
               xVal *= cornerOffset;
               yVal *= cornerOffset;
            }
            else if ( lowerRight )
            {
               yVal = cornerPosition.y + (i * squareSize);
               xVal = cornerPosition.x + (j * squareSize);
               yVal -= frac + squareSize;
               xVal += frac + squareSize;
                              
               xVal *= cornerOffset;
               yVal *= cornerOffset;
            }

            vertPtr[index].horizonFactor.x = 0.0f;
            vertPtr[index].horizonFactor.y = 1.0f;
            vertPtr[index].point.set( xVal, yVal, 0 );
         }
         
         vertPtr[index].normal.set( 0, 0, 1.0f );
         vertPtr[index].undulateData.set(xVal,yVal);
         vertPtr[index].color = waterColor;
      }
   }
   mVertBuff.unlock();

   U16 *idxPtr;
   mPrimBuff.lock(&idxPtr);     
   U32 curIdx = 0;

   // Temporaries to hold indices for the corner points of a quad.
   U32 p00, p01, p11, p10;
   U32 offset = 0;

   for ( U32 i = 1; i < mGridSize; i++ )
   {
      offset = ((i-1) * mGridSize);

	     for ( U32 j = 0; j < mGridSizeMinusOne; j++ )
	     {		
         p00 = offset;
         p01 = offset + 1;
         p11 = p01 + mGridSize;
         p10 = p00 + mGridSize;

         // Upper-Left triangle
         idxPtr[curIdx] = p00;
         curIdx++;
         idxPtr[curIdx] = p01;
         curIdx++;
         idxPtr[curIdx] = p11;
         curIdx++;

         // Lower-Right Triangle
         idxPtr[curIdx] = p00;
         curIdx++;
         idxPtr[curIdx] = p11;
         curIdx++;
         idxPtr[curIdx] = p10;
         curIdx++;      

         offset += 1;
      }
   }

   mPrimBuff.unlock();
}

SceneGraphData WaterPlane::setupSceneGraphInfo( SceneState *state )
{
   SceneGraphData sgData;

   LightManager* lm = gClientSceneGraph->getLightManager();
   sgData.lights[0] = lm->getSpecialLight( LightManager::slSunLightType );

   // fill in water's transform
   sgData.objTrans = getRenderTransform();

   // fog
   sgData.setFogParams( gClientSceneGraph->getFogData() );

   // misc
   sgData.backBuffTex = REFLECTMGR->getRefractTex();
   sgData.reflectTex = mPlaneReflector.reflectTex;
   sgData.wireframe = GFXDevice::getWireframe() || smWireframe;

   return sgData;
}

//-----------------------------------------------------------------------------
// set shader parameters
//-----------------------------------------------------------------------------
void WaterPlane::setShaderParams( SceneState *state, BaseMatInstance* mat, const WaterMatParams& paramHandles)
{
   // Set variables that will be assigned to shader consts within WaterCommon
   // before calling Parent::setShaderParams

   mUndulateMaxDist = mGridElementSize * mGridSizeMinusOne * 0.5f;

   Parent::setShaderParams( state, mat, paramHandles );   

   // Now set the rest of the shader consts that are either unique to this
   // class or that WaterObject leaves to us to handle...    

   MaterialParameters* matParams = mat->getMaterialParameters();

   // set vertex shader constants
   //-----------------------------------   
   matParams->set(paramHandles.mGridElementSizeSC, (F32)mGridElementSize);
   //matParams->set( paramHandles.mReflectTexSizeSC, mReflectTexSize );
   matParams->set(paramHandles.mModelMatSC, getRenderTransform(), GFXSCT_Float3x3);

   // set pixel shader constants
   //-----------------------------------

   ColorF c( mWaterFogData.color );
   matParams->set( paramHandles.mBaseColorSC, c );   
   
   F32 reflect = mPlaneReflector.isEnabled() && !isUnderwater( state->getCameraPosition() ) ? 0.0f : 1.0f;
   //Point4F reflectParams( getRenderPosition().z, mReflectMinDist, mReflectMaxDist, reflect );
   Point4F reflectParams( getRenderPosition().z, 0.0f, 1000.0f, reflect );
   
   // TODO: This is a hack... why is this broken... check after
   // we merge advanced lighting with trunk!
   //
   reflectParams.z = 0.0f;
   matParams->set( paramHandles.mReflectParamsSC, reflectParams );

   VectorF reflectNorm( 0, 0, 1 );
   matParams->set(paramHandles.mReflectNormalSC, reflectNorm ); 
}

bool WaterPlane::prepRenderImage( SceneState* state, 
                                  const U32   stateKey,
                                  const U32, 
                                  const bool )
{
   PROFILE_SCOPE(WaterPlane_prepRenderImage);

   if ( !state->isDiffusePass() || mPlaneReflector.isRendering() )
      return false;

   if ( isLastState( state, stateKey ) )
      return false;

   setLastState(state, stateKey);

   if ( !state->isObjectRendered( this ) )
      return false;

   mBasicLighting = dStricmp( gClientSceneGraph->getLightManager()->getId(), "BLM" ) == 0;
   mUnderwater = isUnderwater( state->getCameraPosition() );

   mMatrixSet->setSceneView(GFX->getWorldMatrix());
   
   const Frustum &frustum = state->getFrustum();

   if ( mPrimBuff.isNull() || 
        mGenerateVB ||         
        frustum != mFrustum )
   {      
      mFrustum = frustum;
      setupVBIB( state->getCameraPosition() );
      mGenerateVB = false;

      MatrixF proj( true );
      MathUtils::getZBiasProjectionMatrix( 0.0001f, mFrustum, &proj );
      mMatrixSet->setSceneProjection(proj);
   }

   _getWaterPlane( state->getCameraPosition(), mWaterPlane, mWaterPos );
   mWaterFogData.plane = mWaterPlane;
   mPlaneReflector.refplane = mWaterPlane;
   updateUnderwaterEffect( state );

   ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
   ri->renderDelegate.bind( this, &WaterObject::renderObject );
   ri->type = RenderPassManager::RIT_Water;
   state->getRenderPass()->addInst( ri );

   //mRenderUpdateCount++;

   return false;
}

void WaterPlane::innerRender( SceneState *state )
{
   GFXDEBUGEVENT_SCOPE( WaterPlane_innerRender, ColorI( 255, 0, 0 ) );

   // Setup SceneGraphData
   SceneGraphData sgData = setupSceneGraphInfo( state );
   const Point3F &camPosition = state->getCameraPosition();

   // set the material

   S32 matIdx = getMaterialIndex( camPosition );
   
   if ( !initMaterial( matIdx ) )
      return;

   BaseMatInstance *mat = mMatInstances[matIdx];
   WaterMatParams matParams = mMatParamHandles[matIdx];

   // render the geometry
   if ( mat )
   {
      // setup proj/world transform
      mMatrixSet->restoreSceneViewProjection();
      mMatrixSet->setWorld(getRenderTransform());

      setShaderParams( state, mat, matParams );     

      while( mat->setupPass( state, sgData ) )
      {    
         mat->setSceneInfo(state, sgData);
         mat->setTransforms(*mMatrixSet, state);
         setCustomTextures( matIdx, mat->getCurPass(), matParams );

         // set vert/prim buffer
         GFX->setVertexBuffer( mVertBuff );
         GFX->setPrimitiveBuffer( mPrimBuff );
         GFX->drawIndexedPrimitive( GFXTriangleList, 0, 0, mVertCount, 0, mPrimCount );
      }
   }
}

//-----------------------------------------------------------------------------
// initPersistFields
//-----------------------------------------------------------------------------
void WaterPlane::initPersistFields()
{
   addGroup( "WaterPlane" );     
      addProtectedField( "gridSize", TypeS32, Offset( mGridSize, WaterPlane ), &protectedSetGridSize, &defaultProtectedGetFn, 1, 0, 
		  "Spacing between vertices in the WaterBlock mesh" );
      addProtectedField( "gridElementSize", TypeF32, Offset( mGridElementSize, WaterPlane ), &protectedSetGridElementSize, &defaultProtectedGetFn, 1, 0, 
		  "Duplicate of gridElementSize for backwards compatility");
   endGroup( "WaterPlane" );

   Parent::initPersistFields();

   removeField( "rotation" );
   removeField( "scale" );
}

bool WaterPlane::isUnderwater( const Point3F &pnt )
{
   F32 height = getPosition().z;

   F32 diff = pnt.z - height;

   return ( diff < 0.1 );
}

void WaterPlane::inspectPostApply()
{
   Parent::inspectPostApply();

   setMaskBits( UpdateMask );
}

void WaterPlane::setTransform( const MatrixF &mat )
{
   // We only accept the z value from the new transform.

   MatrixF newMat( true );
   
   Point3F newPos = getPosition();
   newPos.z = mat.getPosition().z;  
   newMat.setPosition( newPos );

   Parent::setTransform( newMat );

   // Parent::setTransforms ends up setting our worldBox to something other than
   // global, so we have to set it back... but we can't actually call setGlobalBounds
   // again because it does extra work adding and removing us from the container.

   mGlobalBounds = true;
   mObjBox.minExtents.set(-1e10, -1e10, -1e10);
   mObjBox.maxExtents.set( 1e10,  1e10,  1e10);

   // Keep mWaterPlane up to date.
   mWaterFogData.plane.set( 0, 0, 1, -getPosition().z );   
}

void WaterPlane::onStaticModified( const char* slotName, const char*newValue )
{
   Parent::onStaticModified( slotName, newValue );

   if ( dStricmp( slotName, "surfMaterial" ) == 0 )
      setMaskBits( MaterialMask );
}

//-----------------------------------------------------------------------------
// Set up projection matrix for multipass technique with different geometry.
// It basically just pushes the near plane out.  This should work across 
// fixed-function and shader geometry.
//-----------------------------------------------------------------------------
/*
void WaterBlock::setMultiPassProjection()
{
   F32 nearPlane, farPlane;
   F32 left, right, bottom, top;
   bool ortho = false;
   GFX->getFrustum( &left, &right, &bottom, &top, &nearPlane, &farPlane, &ortho );

   F32 FOV = GameGetCameraFov();
   Point2I size = GFX->getVideoMode().resolution;

//   GFX->setFrustum( FOV, size.x/F32(size.y), nearPlane + 0.010, farPlane + 10.0 );

// note - will have to re-calc left, right, top, bottom if the above technique
// doesn't work through portals
//   GFX->setFrustum( left, right, bottom, top, nearPlane + 0.001, farPlane );


}
*/

bool WaterPlane::castRay(const Point3F& start, const Point3F& end, RayInfo* info )
{
   // Simply look for the hit on the water plane
   // and ignore any future issues with waves, etc.
   const Point3F norm(0,0,1);
   PlaneF plane( Point3F::Zero, norm );

   F32 hit = plane.intersect( start, end );
   if ( hit < 0.0f || hit > 1.0f )
      return false;
   
   info->t = hit;
   info->object = this;
   info->point = start + ( ( end - start ) * hit );
   info->normal = norm;
   info->material = mMatInstances[ WaterMat ];

   return true;
}

F32 WaterPlane::getWaterCoverage( const Box3F &testBox ) const
{
   F32 posZ = getPosition().z;
   
   F32 coverage = 0.0f;

   if ( posZ > testBox.minExtents.z ) 
   {
      if ( posZ < testBox.maxExtents.z )
         coverage = (posZ - testBox.minExtents.z) / (testBox.maxExtents.z - testBox.minExtents.z);
      else
         coverage = 1.0f;
   }

   return coverage;
}

F32 WaterPlane::getSurfaceHeight( const Point2F &pos ) const
{
   return getPosition().z;   
}

void WaterPlane::onReflectionInfoChanged()
{
   /*
   if ( isClientObject() && GFX->getPixelShaderVersion() >= 1.4 )
   {
      if ( mFullReflect )
         REFLECTMGR->registerObject( this, ReflectDelegate( this, &WaterPlane::updateReflection ), mReflectPriority, mReflectMaxRateMs, mReflectMaxDist );
      else
      {
         REFLECTMGR->unregisterObject( this );
         mReflectTex = NULL;
      }
   }
   */
}

void WaterPlane::setGridSize( U32 inSize )
{
   if ( inSize == mGridSize )
      return;

   // GridSize must be an odd number.
   if ( inSize % 2 == 0 )
      inSize++;

   // GridSize must be at least 7
   inSize = getMax( inSize, (U32)7 );

   mGridSize = inSize;
   mGridSizeMinusOne = mGridSize - 1;
   mGenerateVB = true;
   setMaskBits( UpdateMask );
}

void WaterPlane::setGridElementSize( F32 inSize )
{
   if ( inSize == mGridElementSize )
      return;

   // GridElementSize must be greater than 0
   inSize = getMax( inSize, 0.0001f );

   mGridElementSize = inSize;
   mGenerateVB = true;
   setMaskBits( UpdateMask );
}

bool WaterPlane::protectedSetGridSize( void *obj, const char *data )
{
   WaterPlane *object = static_cast<WaterPlane*>(obj);
   S32 size = dAtoi( data );

   object->setGridSize( size );

   // We already set the field.
   return false;
}

bool WaterPlane::protectedSetGridElementSize( void *obj, const char *data )
{
   WaterPlane *object = static_cast<WaterPlane*>(obj);
   F32 size = dAtof( data );

   object->setGridElementSize( size );

   // We already set the field.
   return false;
}

void WaterPlane::_getWaterPlane( const Point3F &camPos, PlaneF &outPlane, Point3F &outPos )
{
   outPos = getPosition();   
   outPlane.set( outPos, Point3F(0,0,1) );   
}