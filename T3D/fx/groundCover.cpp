//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/fx/groundCover.h"

#include "core/resourceManager.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneState.h"
#include "terrain/terrData.h"
#include "renderInstance/renderPassManager.h"
#include "gfx/gfxDrawUtil.h"
#include "gfx/primBuilder.h"
#include "T3D/gameConnection.h"
#include "gfx/gfxVertexBuffer.h"
#include "gfx/gfxStructs.h"
#include "ts/tsShapeInstance.h"
#include "lighting/lightManager.h"
#include "lighting/lightInfo.h"
#include "materials/shaderData.h"
#include "gfx/gfxTransformSaver.h"
#include "shaderGen/shaderGenVars.h"
#include "materials/matTextureTarget.h"
#include "gfx/util/screenspace.h"

/// This is used for rendering ground cover billboards.
GFXImplementVertexFormat( GCVertex )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::COLOR, GFXDeclType_Color );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float4, 0 );
};

/// This defines one grid cell.
class GroundCoverCell
{
protected:

   friend class GroundCover;

   struct Placement
   {
      Point3F     point;
      Point3F     size;
      F32         rotation;
      U32         type;
      F32         windAmplitude;
      Box3F       worldBox;
      ColorF      lmColor;
   };

   /// This is the x,y index for this cell.
   Point2I mIndex;

   /// The worldspace bounding box this cell.
   Box3F mBounds;

   /// The worldspace bounding box of the renderable
   /// content within this cell.
   Box3F mRenderBounds;

   /// The instances of billboard cover elements in this cell.
   Vector<Placement> mBillboards;

   /// The instances of shape cover elements in this cell.
   Vector<Placement> mShapes;

   typedef GFXVertexBufferHandle<GCVertex> VBHandle;
   typedef Vector< VBHandle > VBHandleVector;

   /// The vertex buffers that hold all the 
   /// prepared billboards for this cell.
   VBHandleVector mVBs;

   /// Used to mark the cell dirty and in need
   /// of a rebuild.
   bool mDirty;

   /// Repacks the billboards into the vertex buffer.
   void _rebuildVB();

public:

   GroundCoverCell() {}

   ~GroundCoverCell() 
   {
      mVBs.clear();
   }

   const Point2I& shiftIndex( const Point2I& shift ) { return mIndex += shift; }
   
   /// The worldspace bounding box this cell.
   const Box3F& getBounds() const { return mBounds; }

   /// The worldspace bounding box of the renderable
   /// content within this cell.
   const Box3F& getRenderBounds() const { return mRenderBounds; }

   Point3F getCenter() const { return mBounds.getCenter(); }

   VectorF getSize() const { return VectorF( mBounds.len_x() / 2.0f,
                                             mBounds.len_y() / 2.0f,
                                             mBounds.len_z() / 2.0f ); }
   
   /// Renders all the billboard batches returning the 
   /// total billboards rendered.
   U32 renderBillboards( GFXPrimitiveBufferHandle& primBuffer );

   U32 renderShapes(    const TSRenderState &rdata, 
                        Frustum *culler, 
                        TSShapeInstance** shapes );
};

void GroundCoverCell::_rebuildVB()
{
   if ( mBillboards.empty() )
      return;

   PROFILE_SCOPE(GroundCover_RebuildVB);

   // The maximum verts we can put in one vertex buffer batch.
   const U32 MAX_BILLBOARDS = 0xFFFF / 4;

   // How many batches will we need in total?
   const U32 batches = mCeil( (F32)mBillboards.size() / (F32)MAX_BILLBOARDS );

   // So... how many billboards do we need in
   // each batch? We're trying to evenly divide
   // the amount across all the VBs.
   const U32 batchBB = mBillboards.size() / batches;

   // Init the vertex buffer list to the right size.  Any
   // VBs already in there will remain unless we're truncating
   // the list... those are freed.
   mVBs.setSize( batches ); 

   // Get the iter to the first billboard.
   Vector<Placement>::const_iterator iter = mBillboards.begin();

   // Prepare each batch.
   U32 bb, remaining = mBillboards.size();
   for ( U32 b = 0; b < batches; b++ )
   {
      // Grab a reference to the vb.
      VBHandle &vb = mVBs[b];

      // How many billboards in this batch?
      bb = getMin( batchBB, remaining );
      remaining -= bb;

      // Ok... now how many verts is that?
      const U32 verts = bb * 4;

      // Create the VB hasn't been created or if its
      // too small then resize it.
      if ( vb.isNull() || vb->mNumVerts < verts )
      {
         PROFILE_START(GroundCover_CreateVB);
         vb.set( GFX, verts, GFXBufferTypeStatic );
         PROFILE_END();
      }

      // Fill this puppy!
      GCVertex* vertPtr = vb.lock( 0, verts );
      
      GFXVertexColor color;

      Vector<Placement>::const_iterator last = iter + bb;
      for ( ; iter != last; iter++ )
      {
         const Point3F &position = (*iter).point;
         const S32 &type = (*iter).type;
         const Point3F &size = (*iter).size;
         const F32 &windAmplitude = (*iter).windAmplitude;
         GFXVertexColor color = (ColorI)(*iter).lmColor;
         U8 *col = (U8 *)const_cast<U32 *>( (const U32 *)color );

         vertPtr->point = position;
         vertPtr->params.x = size.x;
         vertPtr->params.y = size.y;
         vertPtr->params.z = type;
         vertPtr->params.w = 0;
         col[3] = 0;
         vertPtr->ambient = color;
         ++vertPtr;

         vertPtr->point = position;
         vertPtr->params.x = size.x;
         vertPtr->params.y = size.y;
         vertPtr->params.z = type;
         vertPtr->params.w = 0;
         col[3] = 1;
         vertPtr->ambient = color;
         ++vertPtr;

         vertPtr->point = position;
         vertPtr->params.x = size.x;
         vertPtr->params.y = size.y;
         vertPtr->params.z = type;
         vertPtr->params.w = windAmplitude;
         col[3] = 2;
         vertPtr->ambient = color;
         ++vertPtr;

         vertPtr->point = position;
         vertPtr->params.x = size.x;
         vertPtr->params.y = size.y;
         vertPtr->params.z = type;
         vertPtr->params.w = windAmplitude;
         col[3] = 3;
         vertPtr->ambient = color;
         ++vertPtr;
      }

      vb.unlock();
   }
}

U32 GroundCoverCell::renderShapes(  const TSRenderState &rdata,
                                    Frustum *culler, 
                                    TSShapeInstance** shapes )
{
   MatrixF worldMat;
   TSShapeInstance* shape;
   Point3F camVector;
   F32 dist;
   F32 invScale;

   const SceneState *state = rdata.getSceneState();

   U32 totalRendered = 0;

   Vector<Placement>::const_iterator iter = mShapes.begin();
   for ( ; iter != mShapes.end(); iter++ )
   {
      // Grab a reference here once.
      const Placement& inst = (*iter);

      // If we were pass a culler then us it to test the shape world box.
      if ( culler && !culler->intersects( inst.worldBox ) )
         continue;

      shape = shapes[ inst.type ];

      camVector = inst.point - state->getDiffuseCameraPosition();
      dist = getMax( camVector.len(), 0.01f );

      worldMat.set( EulerF(0, 0, inst.rotation), inst.point );

      // TSShapeInstance::render() uses the 
      // world matrix for the RenderInst.
      worldMat.scale( inst.size );
      GFX->setWorldMatrix( worldMat );

      // Obey the normal screen space lod metrics.  The shapes should
      // be tuned to lod out quickly for ground cover.
      //
      // Note: The profile doesn't indicate that lod selection is
      // very expensive... in fact its less than 1/10th of the cost 
      // of the render() call below.
      PROFILE_START(GroundCover_RenderShapes_SelectDetail);

         invScale = (1.0f/getMax(getMax(inst.size.x,inst.size.y),inst.size.z));
         shape->setDetailFromDistance( state, dist * invScale );

      PROFILE_END(); // GroundCover_RenderShapes_SelectDetail
   
      // Note: This is the most expensive call of this loop.  We 
      // need to rework the render call completely to optimize it.
      PROFILE_START(GroundCover_RenderShapes_Render);

         shape->render( rdata );

      PROFILE_END(); // GroundCover_RenderShapes_Render

      totalRendered++;
   }

   return totalRendered;
}

U32 GroundCoverCell::renderBillboards( GFXPrimitiveBufferHandle& primBuffer )
{
   if ( mDirty )
   {
      _rebuildVB();
      mDirty = false;
   }

   // Do we have anything to render?
   if ( mBillboards.size() == 0 || mVBs.empty() )
      return 0;

   // TODO: Maybe add support for non-facing billboards
   // with random rotations and optional crosses.  We could
   // stick them into the buffer after the normal billboards,
   // then change shader consts.
   //

   // Setup the primitive buffer once.
   GFX->setPrimitiveBuffer( primBuffer );

   // Draw each batch.
   U32 remaining = mBillboards.size();
   const U32 batches = mVBs.size();
   const U32 batchBB = remaining / batches;

   for ( U32 b = 0; b < batches; b++ )
   {
      // Grab a reference to the vb.
      VBHandle &vb = mVBs[b];

      // How many billboards in this batch?
      U32 bb = getMin( batchBB, remaining );
      remaining -= bb;

      // Setup and render it!
      GFX->setVertexBuffer( vb );
      GFX->drawIndexedPrimitive( GFXTriangleList, 
                                 0, 
                                 0, 
                                 bb * 4, 
                                 0, 
                                 bb * 2 );
   }

   return mBillboards.size();
}


U32 GroundCover::smStatRenderedCells = 0;
U32 GroundCover::smStatRenderedBillboards = 0;
U32 GroundCover::smStatRenderedBatches = 0;
U32 GroundCover::smStatRenderedShapes = 0;
U32 GroundCover::smLastState = 0;
F32 GroundCover::smQualityScale = 1.0f;


GroundCover::GroundCover()
{
   mTypeMask |= StaticObjectType;
   mNetFlags.set( Ghostable | ScopeAlways );

   mRadius = 200.0f;
   mZOffset = 0.0f;
   mFadeRadius = 50.0f;
   mShapeCullRadius = 75.0f;
   mReflectRadiusScale = 0.25f;

   mGridSize = 7;

   // By initializing this to a big value we
   // ensure we warp on first render.
   mGridIndex.set( S32_MAX, S32_MAX );

   mMaxPlacement = 1000;
   mLastPlacementCount = 0;

   mDebugRenderCells = false;
   mDebugNoBillboards = false;
   mDebugNoShapes = false;
   mDebugLockFrustum = false;

   mRandomSeed = 1;

   mTextureName = NULL;

   mMaxBillboardTiltAngle = 90.0f;

   // TODO: This really doesn't belong here... we need a
   // real wind system for Torque scenes.  This data
   // would be part of a global scene wind or area wind
   // emitter.
   //
   // Tom Spilman - 10/16/2007

   mWindGustLength = 20.0f;
   mWindGustFrequency = 0.5f;
   mWindGustStrength = 0.5f;
   mWindDirection.set( 1.0f, 0.0f );
   mWindTurbulenceFrequency = 1.2f;
   mWindTurbulenceStrength = 0.125f;

   for ( S32 i=0; i < MAX_COVERTYPES; i++ )
   {
      mProbability[i] = 0.0f;

      mSizeMin[i] = 1.0f;
      mSizeMax[i] = 1.0f;
      mSizeExponent[i] = 1.0f;

      mWindScale[i] = 1.0f;

      mMaxSlope[i] = 0.0f;

      mMinElevation[i] = -99999.0f;
      mMaxElevation[i] = 99999.0f;

      mLayer[i] = -1;
      mInvertLayer[i] = false;

      mMinClumpCount[i] = 1;
      mMaxClumpCount[i] = 1;
      mClumpCountExponent[i] = 1.0f;
      mClumpRadius[i] = 1.0f;

      mBillboardRects[i].point.set( 0.0f, 0.0f );
      mBillboardRects[i].extent.set( 1.0f, 1.0f );

      mShapeFilenames[i] = NULL;
      mShapeInstances[i] = NULL;

      mBillboardAspectScales[i] = 1.0f;

      mNormalizedProbability[i] = 0.0f;
   }
}

IMPLEMENT_CO_NETOBJECT_V1(GroundCover);

void GroundCover::initPersistFields()
{
   addGroup( "GroundCover General" );
      
      addField( "radius", TypeF32, Offset( mRadius, GroundCover ) );
      addField( "dissolveRadius", TypeF32, Offset( mFadeRadius, GroundCover ) );
      addField( "reflectScale", TypeF32, Offset( mReflectRadiusScale, GroundCover ) );

      addField( "gridSize", TypeS32, Offset( mGridSize, GroundCover ) );
      addField( "zOffset", TypeF32, Offset( mZOffset, GroundCover ) );

      addField( "seed", TypeS32, Offset( mRandomSeed, GroundCover ) );
      addField( "maxElements", TypeS32, Offset( mMaxPlacement, GroundCover ) );

      addField( "billboardTexture", TypeFilename, Offset( mTextureName, GroundCover ) );
      addField( "maxBillboardTiltAngle", TypeF32, Offset( mMaxBillboardTiltAngle, GroundCover ) );
      addField( "shapeCullRadius", TypeF32, Offset( mShapeCullRadius, GroundCover ) );      

      addArray( "Types", MAX_COVERTYPES );

         addField( "billboardUVs", TypeRectF, Offset( mBillboardRects, GroundCover ), MAX_COVERTYPES );

         addField( "shapeFilename", TypeFilename, Offset( mShapeFilenames, GroundCover ), MAX_COVERTYPES );

         addField( "layer", TypeS32, Offset( mLayer, GroundCover ), MAX_COVERTYPES );

         addField( "invertLayer", TypeBool, Offset( mInvertLayer, GroundCover ), MAX_COVERTYPES );

         addField( "probability", TypeF32, Offset( mProbability, GroundCover ), MAX_COVERTYPES );

         addField( "sizeMin", TypeF32, Offset( mSizeMin, GroundCover ), MAX_COVERTYPES );

         addField( "sizeMax", TypeF32, Offset( mSizeMax, GroundCover ), MAX_COVERTYPES );

         addField( "sizeExponent", TypeF32, Offset( mSizeExponent, GroundCover ), MAX_COVERTYPES );

         addField( "windScale", TypeF32, Offset( mWindScale, GroundCover ), MAX_COVERTYPES );

         addField( "maxSlope", TypeF32, Offset( mMaxSlope, GroundCover ), MAX_COVERTYPES );

         addField( "minElevation", TypeF32, Offset( mMinElevation, GroundCover ), MAX_COVERTYPES );

         addField( "maxElevation", TypeF32, Offset( mMaxElevation, GroundCover ), MAX_COVERTYPES );

         addField( "minClumpCount", TypeS32, Offset( mMinClumpCount, GroundCover ), MAX_COVERTYPES );
      
         addField( "maxClumpCount", TypeS32, Offset( mMaxClumpCount, GroundCover ), MAX_COVERTYPES );

         addField( "clumpExponent", TypeF32, Offset( mClumpCountExponent, GroundCover ), MAX_COVERTYPES );

         addField( "clumpRadius", TypeF32, Offset( mClumpRadius, GroundCover ), MAX_COVERTYPES );

      endArray( "Types" );

   endGroup( "GroundCover General" );

   addGroup( "GroundCover Wind" );

      addField( "windDirection", TypePoint2F, Offset( mWindDirection, GroundCover ) );

      addField( "windGustLength", TypeF32, Offset( mWindGustLength, GroundCover ) );
      addField( "windGustFrequency", TypeF32, Offset( mWindGustFrequency, GroundCover ) );
      addField( "windGustStrength", TypeF32, Offset( mWindGustStrength, GroundCover ) );

      addField( "windTurbulenceFrequency", TypeF32, Offset( mWindTurbulenceFrequency, GroundCover ) );
      addField( "windTurbulenceStrength", TypeF32, Offset( mWindTurbulenceStrength, GroundCover ) );

   endGroup( "GroundCover Wind" );

   addGroup( "GroundCover Debug" );

      addField( "lockFrustum", TypeBool, Offset( mDebugLockFrustum, GroundCover ) );
      addField( "renderCells", TypeBool, Offset( mDebugRenderCells, GroundCover ) );
      addField( "noBillboards", TypeBool, Offset( mDebugNoBillboards, GroundCover ) );
      addField( "noShapes", TypeBool, Offset( mDebugNoShapes, GroundCover ) );

   endGroup( "GroundCover Debug" );  

   Parent::initPersistFields();
}

void GroundCover::consoleInit()
{  
   Con::addVariable( "$GroundCover::renderedCells", TypeS32, &smStatRenderedCells );
   Con::addVariable( "$GroundCover::renderedBillboards", TypeS32, &smStatRenderedBillboards );
   Con::addVariable( "$GroundCover::renderedBatches", TypeS32, &smStatRenderedBatches );
   Con::addVariable( "$GroundCover::renderedShapes", TypeS32, &smStatRenderedShapes );
}

bool GroundCover::onAdd()
{
   if (!Parent::onAdd())
      return false;

   // We don't use any bounds.
	mObjBox.minExtents.set(-1e5, -1e5, -1e5);
	mObjBox.maxExtents.set( 1e5,  1e5,  1e5);
   resetWorldBox();

   // Prepare some client side things.
   if ( isClientObject() )
   {
      if ( !_initShader() )
      {
         Con::warnf( "GroundCover - failed to find and load billboard shader GroundCoverShaderData!" );
         return false;
      }

      LightManager::smActivateSignal.notify( this, &GroundCover::_onLMActivate );

      _initShapes();

      // Hook ourselves up to get terrain change notifications.
      TerrainBlock::smUpdateSignal.notify( this, &GroundCover::onTerrainUpdated );
   }

   addToScene();

   return true;
}

void GroundCover::onRemove()
{
   Parent::onRemove();

   _deleteCells();
   _deleteShapes();
   
   if ( isClientObject() )
   {
      TerrainBlock::smUpdateSignal.remove( this, &GroundCover::onTerrainUpdated );
      LightManager::smActivateSignal.remove( this, &GroundCover::_onLMActivate );

      mBBShader = NULL;
      mStateBlock = NULL;
      mConstBuffer = NULL;
      mLightInfoTarget = NULL;
   }

   removeFromScene();
}

void GroundCover::inspectPostApply()
{
   Parent::inspectPostApply();

   // We flag all the parameters as changed because
   // we're feeling lazy and there is not a good way
   // to track what parameters changed.
   //
   // TODO: Add a mask bit option to addField() and/or
   // addGroup() which is passed to inspectPostApply
   // for detection of changed elements.
   //
   setMaskBits(U32(-1) );
}

U32 GroundCover::packUpdate( NetConnection *connection, U32 mask, BitStream *stream )
{
   Parent::packUpdate( connection, mask, stream );

   if (stream->writeFlag(mask & InitialUpdateMask))
   {
      // TODO: We could probably optimize a few of these
      // based on reasonable units at some point.

      stream->write( mRadius );
      stream->write( mZOffset );
      stream->write( mFadeRadius );
      stream->write( mShapeCullRadius );
      stream->write( mReflectRadiusScale );
      stream->write( mGridSize );
      stream->write( mRandomSeed );
      stream->write( mMaxPlacement );
      stream->write( mMaxBillboardTiltAngle );

      stream->writeString( mTextureName );

      stream->write( mWindDirection.x );
      stream->write( mWindDirection.y );
      stream->write( mWindGustLength );
      stream->write( mWindGustFrequency );
      stream->write( mWindGustStrength );
      stream->write( mWindTurbulenceFrequency );
      stream->write( mWindTurbulenceStrength );

      for ( S32 i=0; i < MAX_COVERTYPES; i++ )
      {
         stream->write( mProbability[i] );
         stream->write( mSizeMin[i] );
         stream->write( mSizeMax[i] );
         stream->write( mSizeExponent[i] );
         stream->write( mWindScale[i] );
         
         stream->write( mMaxSlope[i] );
         
         stream->write( mMinElevation[i] );
         stream->write( mMaxElevation[i] );     

         stream->write( mLayer[i] );
         stream->writeFlag( mInvertLayer[i] );      

         stream->write( mMinClumpCount[i] );
         stream->write( mMaxClumpCount[i] );
         stream->write( mClumpCountExponent[i] );
         stream->write( mClumpRadius[i] );

         stream->write( mBillboardRects[i].point.x );
         stream->write( mBillboardRects[i].point.y );
         stream->write( mBillboardRects[i].extent.x );
         stream->write( mBillboardRects[i].extent.y );

         stream->writeString( mShapeFilenames[i] );
      }

      stream->writeFlag( mDebugRenderCells );
      stream->writeFlag( mDebugNoBillboards );
      stream->writeFlag( mDebugNoShapes );
      stream->writeFlag( mDebugLockFrustum );
   }

   return 0;  
}

void GroundCover::unpackUpdate( NetConnection *connection, BitStream *stream )
{
   Parent::unpackUpdate( connection, stream );

   if (stream->readFlag())
   {
      stream->read( &mRadius );
      stream->read( &mZOffset );
      stream->read( &mFadeRadius );
      stream->read( &mShapeCullRadius );
      stream->read( &mReflectRadiusScale );   
      stream->read( &mGridSize );
      stream->read( &mRandomSeed );
      stream->read( &mMaxPlacement );
      stream->read( &mMaxBillboardTiltAngle );

      mTextureName = stream->readSTString();

      stream->read( &mWindDirection.x );
      stream->read( &mWindDirection.y );
      stream->read( &mWindGustLength );
      stream->read( &mWindGustFrequency );
      stream->read( &mWindGustStrength );
      stream->read( &mWindTurbulenceFrequency );
      stream->read( &mWindTurbulenceStrength );

      for ( S32 i=0; i < MAX_COVERTYPES; i++ )
      {
         stream->read( &mProbability[i] );
         stream->read( &mSizeMin[i] );
         stream->read( &mSizeMax[i] );
         stream->read( &mSizeExponent[i] );
         stream->read( &mWindScale[i] );

         stream->read( &mMaxSlope[i] );

         stream->read( &mMinElevation[i] );
         stream->read( &mMaxElevation[i] );     

         stream->read( &mLayer[i] );
         mInvertLayer[i] = stream->readFlag();

         stream->read( &mMinClumpCount[i] );
         stream->read( &mMaxClumpCount[i] );
         stream->read( &mClumpCountExponent[i] );
         stream->read( &mClumpRadius[i] );

         stream->read( &mBillboardRects[i].point.x );
         stream->read( &mBillboardRects[i].point.y );
         stream->read( &mBillboardRects[i].extent.x );
         stream->read( &mBillboardRects[i].extent.y );

         mShapeFilenames[i] = stream->readSTString();
      }

      mDebugRenderCells    = stream->readFlag();
      mDebugNoBillboards   = stream->readFlag();
      mDebugNoShapes       = stream->readFlag();
      mDebugLockFrustum    = stream->readFlag();

      // We have no way to easily know what changed, so by clearing
      // the cells we force a reinit and regeneration of the cells.
      // It's sloppy, but it works for now.
      _freeCells();
   }
}

bool GroundCover::_initShader()
{
   ShaderData *shaderData;
   if ( !Sim::findObject( "GroundCoverShaderData", shaderData ) )
      return false;

   // Get the lightinfo conditioner macros.
   Vector<GFXShaderMacro> macros;
   mLightInfoTarget = MatTextureTarget::findTargetByName( "lightinfo" );
   if ( mLightInfoTarget )
      mLightInfoTarget->getTargetShaderMacros( &macros );

   // Get the shader.
   mBBShader = shaderData->getShader( macros );
   if ( !mBBShader )
      return false;

   GFXStateBlockDesc desc;
   desc.setCullMode( GFXCullNone );
   desc.setZReadWrite( true );
   desc.zWriteEnable = true;
   desc.setAlphaTest( true, GFXCmpGreater, 84 );
   desc.samplersDefined = true;
   desc.samplers[0] = GFXSamplerStateDesc::getClampLinear();
   desc.samplers[1] = GFXSamplerStateDesc::getClampLinear();
   desc.samplers[2] = GFXSamplerStateDesc::getWrapLinear();
   mStateBlock = GFX->createStateBlock( desc );

   mConstBuffer = mBBShader->allocConstBuffer();
   shaderData->mapSamplerNames( mConstBuffer );

   mModelViewProjectConst = mBBShader->getShaderConstHandle( "$modelViewProj" );
   mCamPosConst = mBBShader->getShaderConstHandle( "$camPos" );
   mCamRightConst = mBBShader->getShaderConstHandle( "$camRight" );
   mCamUpConst = mBBShader->getShaderConstHandle( "$camUp" );
   mFadeParamConst = mBBShader->getShaderConstHandle( "$fadeParams" );
   mWindDirConst = mBBShader->getShaderConstHandle( "$windDir" );
   mGustInfoConst = mBBShader->getShaderConstHandle( "$gustInfo" );
   mTurbInfoConst = mBBShader->getShaderConstHandle( "$turbInfo" );
   mTypeRectsConst = mBBShader->getShaderConstHandle( "$typeRects" );
   mLightRTConst = mBBShader->getShaderConstHandle( "$lightRT" );
   mTextureConst = mBBShader->getShaderConstHandle( "$diffuseMap" );

   return true;
}

void GroundCover::_initShapes()
{
   _deleteShapes();

   for ( S32 i=0; i < MAX_COVERTYPES; i++ )
   {
      if ( !mShapeFilenames[i] || !mShapeFilenames[i][0] )
         continue;

      // Load the shape.
      Resource<TSShape> shape = ResourceManager::get().load(mShapeFilenames[i]);
      if ( !(bool)shape )
      {
         Con::warnf( "GroundCover::_initShapes() unable to load shape: %s", mShapeFilenames[i] );
         continue;
      }

      if ( isClientObject() && !shape->preloadMaterialList(shape.getPath()) && NetConnection::filesWereDownloaded() )
      {
         Con::warnf( "GroundCover::_initShapes() material preload failed for shape: %s", mShapeFilenames[i] );
         continue;
      }

      // Create the shape instance.
      mShapeInstances[i] = new TSShapeInstance( shape, isClientObject() );
   }
}

void GroundCover::_deleteShapes()
{
   for ( S32 i=0; i < MAX_COVERTYPES; i++ )
   {
      delete mShapeInstances[i];
      mShapeInstances[i] = NULL;
   }
}

void GroundCover::_deleteCells()
{
   // Delete the allocation list.
   for ( S32 i=0; i < mAllocCellList.size(); i++ )
      delete mAllocCellList[i];
   mAllocCellList.clear();

   // Zero out the rest of the stuff.
   _freeCells();
}

void GroundCover::_freeCells()
{
   // Zero the grid and scratch space.
   mCellGrid.clear();
   mScratchGrid.clear();

   // Compact things... remove excess allocated cells.
   const U32 maxCells = mGridSize * mGridSize;
   if ( mAllocCellList.size() > maxCells )
   {
      for ( S32 i=maxCells; i < mAllocCellList.size(); i++ )
        delete mAllocCellList[i];
      mAllocCellList.setSize( maxCells );
   }

   // Move all the alloced cells into the free list.
   mFreeCellList.clear();
   mFreeCellList.merge( mAllocCellList );

   // Release the primitive buffer.
   mPrimBuffer = NULL;
}

void GroundCover::_recycleCell( GroundCoverCell* cell )
{
   mFreeCellList.push_back( cell );
}

void GroundCover::_initialize( U32 cellCount, U32 cellPlacementCount )
{
   // Cleanup everything... we're starting over.
   _freeCells();
   _deleteShapes();
   mTexture.free();

   // Nothing to do without a count!
   if ( cellPlacementCount == 0 )
      return;

   // Reset the grid sizes.
   mCellGrid.setSize( cellCount );
   dMemset( mCellGrid.address(), 0, mCellGrid.memSize() );
   mScratchGrid.setSize( cellCount );

   // Reload the texture.
   if ( mTextureName && mTextureName[0] )
      mTexture.set( mTextureName, &GFXDefaultStaticDiffuseProfile, avar("%s() - mTexture (line %d)", __FUNCTION__, __LINE__) );//&GFXMaterialStaticDXT5Profile );

   // Grab the texture aspect ratio.
   F32 texAspect = 1.0f;
   if ( !mTexture.isNull() )
      texAspect = mTexture.getWidth() / (F32)mTexture.getHeight();

   // Rebuild the texture aspect scales for each type.
   for ( S32 i=0; i < MAX_COVERTYPES; i++ )
   {
      if ( mBillboardRects[i].len_y() > 0.0f )
         mBillboardAspectScales[i] = texAspect * ( mBillboardRects[i].len_x() / mBillboardRects[i].len_y() );
      else
         mBillboardAspectScales[i] = 0.0f;
   }

   // Load the shapes again.
   _initShapes();

   // Set the primitive buffer up for the maximum placement in a cell.
   mPrimBuffer.set( GFX, cellPlacementCount * 6, 0, GFXBufferTypeStatic );
   U16 *idxBuff;
   mPrimBuffer.lock(&idxBuff);
   for ( U32 i=0; i < cellPlacementCount; i++ )
   {
      //
      // The vertex pattern in the VB for each 
      // billboard is as follows...
      //
      //     0----1
      //     |\   |
      //     | \  |
      //     |  \ |
      //     |   \|
      //     3----2
      //
      // We setup the index order below to ensure
      // sequential, cache friendly, access.
      //
      U32 offset = i * 4;
      idxBuff[i*6+0] = 0 + offset;
      idxBuff[i*6+1] = 1 + offset;
      idxBuff[i*6+2] = 2 + offset;
      idxBuff[i*6+3] = 2 + offset;
      idxBuff[i*6+4] = 3 + offset;
      idxBuff[i*6+5] = 0 + offset;
   }   
   mPrimBuffer.unlock();

   // Generate the normalised probability.
   F32 total = 0.0f;
   for ( S32 i=0; i < MAX_COVERTYPES; i++ )
   {
      // If the element isn't gonna render... then
      // set the probability to zero.
      if ( mShapeInstances[i] == NULL && mBillboardAspectScales[i] <= 0.0001f )
      {
         mNormalizedProbability[i] = 0.0f;
      }
      else
      {
         mNormalizedProbability[i] = mProbability[i];

         total += mProbability[i];
      }
   }
   if ( total > 0.0f )
   {
      for ( S32 i=0; i < MAX_COVERTYPES; i++ )
         mNormalizedProbability[i] /= total;
   }
}

void GroundCover::_findTerrainCallback( SceneObject *obj, void *param )
{
   Vector<TerrainBlock*> *terrains = reinterpret_cast<Vector<TerrainBlock*>*>( param );

   TerrainBlock *terrain = dynamic_cast<TerrainBlock*>( obj );
   if ( terrain )
      terrains->push_back( terrain );
}

GroundCoverCell* GroundCover::_generateCell( const Point2I& index, 
                                             const Box3F& bounds, 
                                             U32 placementCount,
                                             S32 randSeed )
{
   PROFILE_SCOPE(GroundCover_GenerateCell);

   Vector<TerrainBlock*> terrainBlocks;
   getContainer()->findObjects( bounds, TerrainObjectType, _findTerrainCallback, &terrainBlocks );
   if ( terrainBlocks.empty() )
      return NULL;

   // Grab a free cell or allocate a new one.
   GroundCoverCell* cell;
   if ( mFreeCellList.empty() )
   {
      cell = new GroundCoverCell();
      mAllocCellList.push_back( cell );
   }
   else
   {
      cell = mFreeCellList.last();
      mFreeCellList.pop_back();
   }

   cell->mDirty = true;
   cell->mIndex = index;
   cell->mBounds = bounds;

   Point3F pos( 0, 0, 0 );

   Box3F renderBounds = bounds;
   Point3F point;
   Point3F normal;
   F32 h;
   Point2F cp, uv;
   bool hit;
   GroundCoverCell::Placement p;
   F32 rotation;
   F32 size;
   F32 sizeExponent;
   Point2I lpos;
   //F32 value;
   VectorF right;
   U8 matIndex;
   bool firstElem = true;

   TerrainBlock *terrainBlock = NULL;

   cell->mBillboards.clear();
   cell->mBillboards.reserve( placementCount );
   cell->mShapes.clear();
   cell->mShapes.reserve( placementCount );

   F32   terrainSquareSize, 
         oneOverTerrainLength, 
         oneOverTerrainSquareSize;
   const GBitmap* terrainLM = NULL;

   // The RNG that we'll use in generation.
   MRandom rand( 0 );

   // We process one type at a time.
   for ( U32 type=0; type < MAX_COVERTYPES; type++ )
   {
      // How many cover elements do we need to generate for this type?
      const S32 typeCount = mNormalizedProbability[type] * (F32)placementCount;
      if ( typeCount <= 0 )
         continue;

      // Grab the terrain layer for this type.
      /*
      const TerrainDataLayer* dataLayer = NULL;
      const bool typeInvertLayer = mInvertLayer[type];
      if ( mLayer[type] > -1 )
      {
         dataLayer = mTerrainBlock->getDataLayer( mLayer[type] );
         if ( dataLayer )
         {
            // Do an initial check to see if we can place any place anything
            // at all...  if the layer area for this element is empty then 
            // there is nothing more to do.

            RectI area( (S32)mFloor( ( bounds.minExtents.x - pos.x ) * oneOverTerrainSquareSize ),
                        (S32)mFloor( ( bounds.minExtents.y - pos.y ) * oneOverTerrainSquareSize ),
                        (S32)mCeil( ( bounds.maxExtents.x - pos.x ) * oneOverTerrainSquareSize ),
                        (S32)mCeil( ( bounds.maxExtents.y - pos.y ) * oneOverTerrainSquareSize ) );
            area.extent -= area.point;

            if ( dataLayer->testFill( area, typeInvertLayer ? 255 : 0 ) )
               continue;
         }
      }

      // If the layer is not inverted and we have no data 
      // then we have nothing to draw.
      if ( !typeInvertLayer && !dataLayer )
         continue;
      */

      // We set the seed we were passed which is based on this grids position
      // in the world and add the type value.  This keeps changes to one type
      // from effecting the outcome of the others.
      rand.setSeed( randSeed + type );

      // Setup for doing clumps.
      S32 clumps = 0;
      Point2F clumpCenter(0.0f, 0.0f);
      const S32 clumpMin = getMax( 1, (S32)mMinClumpCount[type] );
      F32 clumpExponent;   

      // We mult this by -1 each billboard we make then use
      // it to scale the billboard x axis to flip them.  This
      // essentially gives us twice the variation for free.
      F32 flipBB = -1.0f;

      // Precompute a few other type specific values.
      const F32 typeSizeRange = mSizeMax[type] - mSizeMin[type];
      const F32 typeMaxSlope = mMaxSlope[type];
      const F32 typeMaxElevation = mMaxElevation[type];
      const F32 typeMinElevation = mMinElevation[type];
      const bool typeIsShape = mShapeInstances[ type ] != NULL;
      const Box3F typeShapeBounds = typeIsShape ? mShapeInstances[ type ]->getShape()->bounds : Box3F();
      const F32 typeWindScale = mWindScale[type];
      const S32 typeLayer = mLayer[type];

      // We can set this once here... all the placements for this are the same.
      p.type = type;
      p.windAmplitude = typeWindScale;
      p.lmColor.set(1.0f,1.0f,1.0f);

      // Generate all the cover elements for this type.
      for ( S32 i=0; i < typeCount; i++ )
      {
         // Do all the other random things here first as to not 
         // disturb the random sequence if the terrain geometry
         // or cover layers change.

         // Get the random position.      
         cp.set( rand.randF(), rand.randF() );

         // Prepare the clump info.
         clumpExponent = mClampF( mPow( rand.randF(), mClumpCountExponent[type] ), 0.0f, 1.0f );
         if ( clumps <= 0 )
         {
            // We're starting a new clump.
            clumps = ( clumpMin + mFloor( ( mMaxClumpCount[type] - clumpMin ) * clumpExponent ) ) - 1;
            cp.set(  bounds.minExtents.x + cp.x * bounds.len_x(),
                     bounds.minExtents.y + cp.y * bounds.len_y() );
            clumpCenter = cp;
         }
         else
         {
            clumps--;
            cp.set( clumpCenter.x - ( ( cp.x - 0.5f ) * mClumpRadius[type] ),
                    clumpCenter.y - ( ( cp.y - 0.5f ) * mClumpRadius[type] ) );
         }

         // Which terrain do I place on?
         if ( terrainBlocks.size() == 1 )
            terrainBlock = terrainBlocks.first();
         else
         {
            for ( U32 i = 0; i < terrainBlocks.size(); i++ )
            {
               TerrainBlock *terrain = terrainBlocks[i];
               const Box3F &terrBounds = terrain->getWorldBox();

               if (  cp.x < terrBounds.minExtents.x || cp.x > terrBounds.maxExtents.x ||
                     cp.y < terrBounds.minExtents.y || cp.y > terrBounds.maxExtents.y )
                  continue;

               terrainBlock = terrain;
               break;
            }
         }

         // This should only happen if the generation went off
         // the edge of the terrain blocks.
         if ( !terrainBlock )
            continue;

         terrainLM = terrainBlock->getLightMap();
         pos = terrainBlock->getPosition();

         terrainSquareSize = (F32)terrainBlock->getSquareSize();
         oneOverTerrainLength = 1.0f / terrainBlock->getWorldBlockSize();
         oneOverTerrainSquareSize = 1.0f / terrainSquareSize;

         // The size is calculated using an exponent to control 
         // the frequency between min and max sizes.
         sizeExponent = mClampF( mPow( rand.randF(), mSizeExponent[type] ), 0.0f, 1.0f );
         size = mSizeMin[type] + ( typeSizeRange * sizeExponent );

         // Generate a random z rotation.
         rotation = rand.randF() * M_2PI_F;

         // Flip the billboard now for the next generation.
         flipBB *= -1.0f;

         PROFILE_START( GroundCover_TerrainRayCast );
         hit = terrainBlock->getNormalHeightMaterial( Point2F( cp.x - pos.x, cp.y - pos.y ), 
                                                      &normal, &h, &matIndex );
         
         // TODO: When did we loose the world space elevation when
         // getting the terrain height?
         h += pos.z;

         PROFILE_END(); // GroundCover_TerrainRayCast
         if (  !hit || h > typeMaxElevation || h < typeMinElevation || 
               ( typeLayer != -1 && matIndex != typeLayer ) )
            continue;

         // Do we need to check slope?
         if ( !mIsZero( typeMaxSlope ) )
         {
            if (mAcos(normal.z) > mDegToRad(typeMaxSlope))
               continue;
         }

         point.set( cp.x, cp.y, h );
         p.point = point;
         p.rotation = rotation;

         // Grab the terrain lightmap color at this position.
         //
         // TODO: Can't we remove this test?  The terrain 
         // lightmap should never be null... NEVER!
         //
         if ( terrainLM )
         {
            // TODO: We could probably call terrainLM->getBits()
            // once outside the loop then pre-calculate the scalar
            // for converting a world position into a lexel...
            // avoiding the extra protections inside of sampleTexel().

            uv.x = (point.x + pos.x) * oneOverTerrainLength;
            uv.y = (point.y + pos.y) * oneOverTerrainLength;
            uv.x -= mFloor(uv.x);
            uv.y -= mFloor(uv.y);
            p.lmColor = terrainLM->sampleTexel(uv.x,uv.y);
         }

         // Put it into the right list by type.
         //
         // TODO: Could we break up the generation into
         // two separate loops for shapes and billboards
         // and gain performance?
         //
         if ( typeIsShape )
         {
            // TODO: Convert the size into a real size... not scale!

            // TODO: We could probably cache the shape bounds
            // into a primitive array and avoid the double pointer
            // dereference per placement.

            p.size.set( size, size, size );
            p.worldBox = typeShapeBounds;
            p.worldBox.minExtents *= size;
            p.worldBox.maxExtents *= size;
            p.worldBox.minExtents += point;
            p.worldBox.maxExtents += point;

            cell->mShapes.push_back( p );
         }
         else
         {
            p.size.y = size;
            p.size.x = size * flipBB * mBillboardAspectScales[type]; 
            p.worldBox.maxExtents = p.worldBox.minExtents = point;

            cell->mBillboards.push_back( p );
         }

         // Update the render bounds.
         if ( firstElem )
         {
            renderBounds = p.worldBox;
            firstElem = false;
         }
         else
         {
            renderBounds.extend( p.worldBox.minExtents );
            renderBounds.extend( p.worldBox.maxExtents );
         }

      } // for ( S32 i=0; i < typeCount; i++ )

   } // for ( U32 type=0; type < NumCoverTypes; type++ )
      

   cell->mRenderBounds = renderBounds;
   cell->mBounds.minExtents.z = renderBounds.minExtents.z;
   cell->mBounds.maxExtents.z = renderBounds.maxExtents.z;

   return cell;
}

void GroundCover::onTerrainUpdated( U32 flags, TerrainBlock *tblock, const Point2I& min, const Point2I& max )
{
   if ( isServerObject() ) 
      return;

   // Free all the cells if we've gotten a lightmap update.
   if ( flags & TerrainBlock::LightmapUpdate )
   {
      _freeCells();
      return;
   }

   // TODO: EmptyUpdate doesn't work yet... fix editor/terrain.

   // If this is a height or opacity update only clear
   // the cells that have changed.
   if (  flags & TerrainBlock::HeightmapUpdate || 
         flags & TerrainBlock::LayersUpdate ||
         flags & TerrainBlock::EmptyUpdate )
   {
      // Convert the min and max into world space.
      const F32 size = tblock->getSquareSize();
      const Point3F pos = tblock->getPosition();

      // TODO: I don't think this works right with tiling!
      Box3F dirty(   F32( min.x * size ) + pos.x, F32( min.y * size ) + pos.y, 0.0f,
                     F32( max.x * size ) + pos.x, F32( max.y * size ) + pos.y, 0.0f );
      
      // Now free any cells that overlap it!
      for ( S32 i = 0; i < mCellGrid.size(); i++ )
      {
         GroundCoverCell* cell = mCellGrid[ i ];
         if ( !cell )
            continue;

         const Box3F& bounds = cell->getBounds();
         dirty.minExtents.z = bounds.minExtents.z;
         dirty.maxExtents.z = bounds.maxExtents.z;
         if ( bounds.isOverlapped( dirty ) )
         {
            mCellGrid[ i ] = NULL;
            _recycleCell( cell );
         }
      }
   }
}

void GroundCover::_updateCoverGrid( const Frustum &culler )
{
   PROFILE_SCOPE( GroundCover_UpdateCoverGrid );
   
   mGridSize = getMax( mGridSize, (U32)2 );

   // How many cells in the grid?
   const U32 cells = mGridSize * mGridSize;

   // Whats the max placement count for each cell considering 
   // the grid size and quality scale LOD value.
   const S32 placementCount = ( (F32)mMaxPlacement * smQualityScale ) / F32( mGridSize * mGridSize );

   // If the cell grid isn't sized or the placement count
   // changed (most likely because of quality lod) then we
   // need to initialize the system again.
   if ( mCellGrid.empty() || placementCount != mLastPlacementCount )
   {
      _initialize( cells, placementCount );
      mLastPlacementCount = placementCount;
   }

   // Without a count... we don't function at all.
   if ( placementCount == 0 )
      return;

   // Clear the scratch grid.
   dMemset( mScratchGrid.address(), 0, mScratchGrid.memSize() );

   // Calculate the normal cell size here.
   const F32 cellSize = ( mRadius * 2.0f ) / (F32)(mGridSize - 1);

   // Figure out the root index of the new grid based on the camera position.
   Point2I index( (S32)mFloor( ( culler.getPosition().x - mRadius ) / cellSize  ),
                  (S32)mFloor( ( culler.getPosition().y - mRadius ) / cellSize ) );

   // Figure out the cell shift between the old and new grid positions.
   Point2I shift = mGridIndex - index;

   // If we've shifted more than one in either axis then we've warped.
   bool didWarp = shift.x > 1 || shift.x < -1 || 
                  shift.y > 1 || shift.y < -1 ? true : false;

   // Go thru the grid shifting each cell we find and
   // placing them in the scratch grid.
   for ( S32 i = 0; i < mCellGrid.size(); i++ )
   {
      GroundCoverCell* cell = mCellGrid[ i ];
      if ( !cell )
         continue;

      // Whats our new index?
      Point2I newIndex = cell->shiftIndex( shift );

      // Is this cell outside of the new grid?
      if (  newIndex.x < 0 || newIndex.x >= mGridSize ||
            newIndex.y < 0 || newIndex.y >= mGridSize )
      {
         _recycleCell( cell );
         continue;
      }

      // Place the cell in the scratch grid.
      mScratchGrid[ ( newIndex.y * mGridSize ) + newIndex.x ] = cell;
   }

   // Get the terrain elevation range for setting the default cell bounds.
   F32   terrainMinHeight = -5000.0f, 
         terrainMaxHeight = 5000.0f;

   // Go thru the scratch grid copying each cell back to the
   // cell grid and creating new cells as needed.
   //
   // By limiting ourselves to only one new cell generation per
   // update we're lowering the performance hiccup during movement
   // without getting into the complexity of threading.  The delay
   // in generation is rarely noticeable in normal play.
   //
   // The only caveat is that we need to generate the entire visible
   // grid when we warp.
   U32 cellsGenerated = 0;
   for ( S32 i = 0; i < mScratchGrid.size(); i++ )
   {
      GroundCoverCell* cell = mScratchGrid[ i ];
      if ( !cell && ( cellsGenerated == 0 || didWarp ) )
      {
         // Get the index point of this new cell.
         S32 y = i / mGridSize;
         S32 x = i - ( y * mGridSize );
         Point2I newIndex = index + Point2I( x, y );

         // What will be the world placement bounds for this cell.
         Box3F bounds;
         bounds.minExtents.set( newIndex.x * cellSize, newIndex.y * cellSize, terrainMinHeight );
         bounds.maxExtents.set( bounds.minExtents.x + cellSize, bounds.minExtents.y + cellSize, terrainMaxHeight );

         if ( !mCuller.intersects( bounds ) )
         {
            mCellGrid[ i ] = NULL;
            continue;
         }

         // We need to allocate a new cell.
         //
         // TODO: This is the expensive call and where we should optimize. In
         // particular the next best optimization would be to take advantage of
         // multiple cores so that we can generate all the cells in one update.
         //
         // Instead of generating the cell here we would allocate a cell and stick
         // it into a thread safe queue (maybe lockless) as well as the mCellGrid.
         // Once all were allocated we would do something like this...
         //
         // TorqueParallelProcess( cellsToGenerateQueue, _generateCell );
         //
         // Internally this function would pass the queue to some global pre-allocated
         // worker threads which are locked to a particular core.  While the main 
         // thread waits for the worker threads to finish it will process cells itself.
         // 

         cell = _generateCell(   newIndex - index, 
                                 bounds, 
                                 placementCount, 
                                 mRandomSeed + mAbs( newIndex.x ) + mAbs( newIndex.y ) );

         // Increment our generation count.
         if ( cell )
            ++cellsGenerated;
      }

      mCellGrid[ i ] = cell;
   }

   // Store the new grid index.
   mGridIndex = index;
}

bool GroundCover::prepRenderImage( 
   SceneState *state, 
   const U32 stateKey, 
   const U32 startZone, 
   const bool modifyBaseZoneState )
{
   // TODO: Educate myself... WTF does this accomplish?
   // Is it just a protection against double rendering?
   if ( isLastState( state, stateKey ) )
      return false;

   RenderPassManager *defaultPass = NULL;
   Sim::findObject( "DiffuseRenderPassManager", defaultPass );
   const bool isShadowPass = gClientSceneGraph->getRenderPass() != defaultPass;
   if ( isShadowPass )
      return false;

   // Reset the rendering stats on a new scene state!
   //
   // TODO: This doesn't work with multiple ground cover
   // elements in a scene... fix me!
   //
   if ( stateKey != smLastState )
   {
      smStatRenderedCells = 0;
      smStatRenderedBillboards = 0;
      smStatRenderedBatches = 0;
      smStatRenderedShapes = 0;

      smLastState = stateKey;
   }

   setLastState( state, stateKey );

   // Check portal visibility.
   //
   // TODO: Make sure that the ground cover stops rendering
   // if you're inside a zoned interior.
   //
   if ( !state->isObjectRendered( this ) )
      return false;

   GFXTransformSaver saver;

   // Setup the frustum culler.
   if ( mCuller.getPosition().isZero() || !mDebugLockFrustum )
      mCuller = state->getFrustum();

   // Update the cells, but only during the diffuse pass... we don't
   // want cell generation to thrash when the reflection camera 
   // position doesn't match the diffuse camera!
   if ( state->isDiffusePass() )
      _updateCoverGrid( mCuller );

   // Prepare for billboard rendering later.
   if ( !state->isShadowPass() && mBBShader )
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &GroundCover::_renderBillboards );
      ri->type = RenderPassManager::RIT_Object;
      state->getRenderPass()->addInst( ri );
   }

   // Prepare to render the grid shapes.
   PROFILE_SCOPE(GroundCover_RenderShapes);

   // Set up our TS render state.
   TSRenderState rdata;
   rdata.setSceneState( state );

   // TODO: Add a special fade out for DTS?
   mCuller.setFarDist( mShapeCullRadius );

   for ( S32 i = 0; i < mCellGrid.size(); i++ )
   {
      GroundCoverCell* cell = mCellGrid[ i ];
      if ( !cell || mDebugNoShapes )
         continue;

      U32 clipMask = mCuller.testPlanes( cell->getRenderBounds(), Frustum::PlaneMaskAll );
      if ( clipMask == -1 )
         continue;

      smStatRenderedCells++;

      // Render the shapes in this cell... only pass the culler if the
      // cell wasn't fully within the frustum.
      smStatRenderedShapes += cell->renderShapes(  
                                 rdata, 
                                 clipMask != 0 ? &mCuller : NULL, 
                                 mShapeInstances );
   }

   return true;
}


void GroundCover::_renderBillboards( ObjectRenderInst *ri, SceneState *state, BaseMatInstance* overrideMat )
{
   // Skip special materials.
   if ( overrideMat )
      return;

   PROFILE_SCOPE(GroundCover_RenderBillboards);

   // Prepare to render.
   GFXTransformSaver saver;
   MatrixF world = GFX->getWorldMatrix();

   if(mZOffset != 0)
   {
      const F32 &zOffset = mZOffset;
      
      const F32 *b = world;
      world[8] = b[8] + zOffset * b[12];
      world[9] = b[9] + zOffset * b[13];
      world[10]= b[10]+ zOffset * b[14];
      world[11]= b[11]+ zOffset * b[15];

      GFX->setWorldMatrix( world );
   }

   // Set the projection and world transform info.
   MatrixF proj = GFX->getProjectionMatrix();
   proj.mul( world );
   mConstBuffer->set( mModelViewProjectConst, proj );

   F32 cullScale = 1.0f;
   if ( state->isReflectPass() )
      cullScale = mReflectRadiusScale;

   // Prepare to render some batches.
   GFX->setShader( mBBShader );
   GFX->setStateBlock( mStateBlock );
   GFX->setShaderConstBuffer( mConstBuffer );

   // Get the data we need from the camera matrix.
   const MatrixF &camMat = state->getCameraTransform();
   Point3F camRight, camUp, camDir, camPos;
   camMat.getColumn( 0, &camRight );
   camMat.getColumn( 1, &camDir );
   camMat.getColumn( 2, &camUp );
   camMat.getColumn( 3, &camPos );

   // Limit the camera up vector to keep the billboards 
   // from leaning too far down into the terrain.
   VectorF lookDir( camDir.x, camDir.y, 0.0f );
   F32 angle;
   if ( !lookDir.isZero() )
   {
      lookDir.normalize();
      angle = mAcos( mDot( camUp, lookDir ) );
   }
   else
   {
      angle = camDir.z < 0.0f ? 0.0f : ( M_PI_F / 2.0f );
   }

   const F32 maxBillboardTiltRads = mDegToRad( mMaxBillboardTiltAngle );
   if ( angle < (M_PI_F / 2.0f) - maxBillboardTiltRads )
   {
      QuatF quat( AngAxisF( camRight, maxBillboardTiltRads ) );
      quat.mulP( VectorF( 0.0f, 0.0f, 1.0f ), &camUp );
   }

   // Setup the shader consts for the camera.
   mConstBuffer->set( mCamPosConst, camPos );
   mConstBuffer->set( mCamRightConst, camRight );
   mConstBuffer->set( mCamUpConst, camUp );

   // Setup the fade parameters.
   Point2F fadeParams( mFadeRadius * cullScale, mRadius * cullScale );
   mConstBuffer->set( mFadeParamConst, fadeParams );

   // Setup the texture.
   //S32 idx = mTextureConst->getSamplerRegister();
   //if ( idx != -1 )
      GFX->setTexture( 0, mTexture );

   // Pass the cover rects.
   AlignedArray<Point4F> rectData( MAX_COVERTYPES, sizeof( Point4F ), (U8*)mBillboardRects, false ); 
   mConstBuffer->set( mTypeRectsConst, rectData );

   const F32 simTime = Sim::getCurrentTime() * 0.001f;

   // Pass the wind parameters.
   mConstBuffer->set( mWindDirConst, mWindDirection );
   mConstBuffer->set( mGustInfoConst, Point3F(  mWindGustLength, 
                                                mWindGustFrequency * simTime, 
                                                mWindGustStrength ) );
   mConstBuffer->set( mTurbInfoConst, Point2F(  mWindTurbulenceFrequency * simTime, 
                                                mWindTurbulenceStrength ) );

   /// Are we in advanced lighting mode?
   if ( mLightRTConst->isValid() && mLightInfoTarget )
   {
      GFXTextureObject *texObject = mLightInfoTarget->getTargetTexture( 0 );
      GFX->setTexture( 1, texObject );

      const Point3I &targetSz = texObject->getSize();
      const RectI &targetVp = mLightInfoTarget->getTargetViewport();
      Point4F rtParams;

      ScreenSpace::RenderTargetParameters(targetSz, targetVp, rtParams);

      mConstBuffer->set( mLightRTConst, rtParams );
   }

   // Set the far distance for billboards.
   mCuller.setFarDist( mRadius );
   
   // We need to view direction for light and sphere selection!
   for ( S32 i = 0; i < mCellGrid.size(); i++ )
   {
      GroundCoverCell* cell = mCellGrid[ i ];
      if ( !cell || mDebugNoBillboards )
         continue;

      if ( !mCuller.intersects( cell->getRenderBounds() ) )
         continue;

      // Render!
      smStatRenderedBillboards += cell->renderBillboards( mPrimBuffer );

      // TODO: Eventually we may allow more than one billboard batch
      // per cell and we need to account for that!
      smStatRenderedBatches++;
   }

   // We must make a separate pass rendering the debug stuff
   // as the draw util will clobber the render state!
   if ( mDebugRenderCells )
   {
      // Used for debug drawing.
      GFXDrawUtil* drawer = GFX->getDrawUtil();
      drawer->clearBitmapModulation();

      GFXStateBlockDesc desc;
      desc.setZReadWrite( true, false );
      desc.setBlend( true );
      desc.fillMode = GFXFillWireframe;

      for ( S32 i = 0; i < mCellGrid.size(); i++ )
      {
         GroundCoverCell* cell = mCellGrid[ i ];
         if ( !cell || ( cell->mBillboards.size() + cell->mShapes.size() ) == 0 )
            continue;

         if ( !mCuller.intersects( cell->getRenderBounds() ) )
            continue;
         
         drawer->drawCube( desc, cell->getSize(), cell->getCenter(), ColorI( 0, 255, 0 ) );
      }
   }
}



ConsoleStaticMethod( GroundCover, setQualityScale, F32, 2, 2, 
   "GroundCover.setQualityScale( F32 scale )\n"
   "Sets the global ground cover LOD scalar which controls "
   "the percentage of the maximum designed cover to put down. "
   "It scales both rendering cost and placement CPU performance. "
   "Returns the actual value set." )
{
   return GroundCover::setQualityScale( dAtof( argv[1] ) ); 
}

ConsoleStaticMethod( GroundCover, getQualityScale, F32, 1, 1, 
   "GroundCover.getQualityScale()\n"
   "Returns the global quality scale.  See GroundCover::setQualityScale()..." )
{
   return GroundCover::getQualityScale(); 
}
