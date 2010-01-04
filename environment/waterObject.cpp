//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "environment/waterObject.h"

#include "console/consoleTypes.h"
#include "materials/materialParameters.h"
#include "materials/baseMatInstance.h"
#include "materials/materialManager.h"
#include "materials/customMaterialDefinition.h"
#include "materials/sceneData.h"
#include "core/stream/bitStream.h"
#include "sceneGraph/reflectionManager.h"
#include "sceneGraph/sceneState.h"
#include "lighting/lightInfo.h"
#include "math/mathIO.h"
#include "postFx/postEffect.h"
#include "T3D/gameConnection.h"
#include "T3D/shapeBase.h"
#include "gfx/gfxOcclusionQuery.h"
#include "gfx/sim/cubemapData.h"
#include "math/util/matrixSet.h"

GFXImplementVertexFormat( GFXWaterVertex )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::COLOR, GFXDeclType_Color );   
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float4, 1 );   
}

void WaterMatParams::clear()
{
   mRippleDirSC = NULL;
   mRippleTexScaleSC = NULL;
   mRippleSpeedSC = NULL;
   mRippleMagnitudeSC = NULL;
   mWaveDirSC = NULL;
   mWaveDataSC = NULL;   
   mReflectTexSizeSC = NULL;
   mBaseColorSC = NULL;
   mMiscParamsSC = NULL;
   mReflectParamsSC = NULL;
   mReflectNormalSC = NULL;
   mHorizonPositionSC = NULL;
   mFogParamsSC = NULL;   
   mMoreFogParamsSC = NULL;
   mFarPlaneDistSC = NULL;
   mWetnessParamsSC = NULL;
   mDistortionParamsSC = NULL;
   mUndulateMaxDistSC = NULL;
   mAmbientColorSC = NULL;
   mFoamParamsSC = NULL;
   mFoamColorModulateSC = NULL;
   mGridElementSizeSC = NULL;
   mElapsedTimeSC = NULL;
   mFoamSamplerSC = NULL;
   mRippleSamplerSC = NULL;
   mCubemapSamplerSC = NULL;
}

void WaterMatParams::init( BaseMatInstance* matInst )
{
   clear();

   mRippleDirSC = matInst->getMaterialParameterHandle( "$rippleDir" );
   mRippleTexScaleSC = matInst->getMaterialParameterHandle( "$rippleTexScale" );
   mRippleSpeedSC = matInst->getMaterialParameterHandle( "$rippleSpeed" );
   mRippleMagnitudeSC = matInst->getMaterialParameterHandle( "$rippleMagnitude" );
   mWaveDirSC = matInst->getMaterialParameterHandle( "$waveDir" );
   mWaveDataSC = matInst->getMaterialParameterHandle( "$waveData" );
   mReflectTexSizeSC = matInst->getMaterialParameterHandle( "$reflectTexSize" );
   mBaseColorSC = matInst->getMaterialParameterHandle( "$baseColor" );
   mMiscParamsSC = matInst->getMaterialParameterHandle( "$miscParams" );
   mReflectParamsSC = matInst->getMaterialParameterHandle( "$reflectParams" );   
   mReflectNormalSC = matInst->getMaterialParameterHandle( "$reflectNormal" );
   mHorizonPositionSC = matInst->getMaterialParameterHandle( "$horizonPos" );
   mFogParamsSC = matInst->getMaterialParameterHandle( "$fogParams" ); 
   mMoreFogParamsSC = matInst->getMaterialParameterHandle( "$moreFogParams" );
   mFarPlaneDistSC = matInst->getMaterialParameterHandle( "$farPlaneDist" );
   mWetnessParamsSC = matInst->getMaterialParameterHandle( "$wetnessParams" );
   mDistortionParamsSC = matInst->getMaterialParameterHandle( "$distortionParams" );
   mUndulateMaxDistSC = matInst->getMaterialParameterHandle( "$undulateMaxDist" );   
   mAmbientColorSC = matInst->getMaterialParameterHandle( "$ambientColor" );
   mFoamParamsSC = matInst->getMaterialParameterHandle( "$foamParams" );
   mFoamColorModulateSC = matInst->getMaterialParameterHandle( "$foamColorMod" );
   mGridElementSizeSC = matInst->getMaterialParameterHandle( "$gridElementSize" );
   mElapsedTimeSC = matInst->getMaterialParameterHandle( "$elapsedTime" );
   mModelMatSC = matInst->getMaterialParameterHandle( "$modelMat" );
   mFoamSamplerSC = matInst->getMaterialParameterHandle( "$foamMap" );
   mRippleSamplerSC = matInst->getMaterialParameterHandle( "$bumpMap" );
   mCubemapSamplerSC = matInst->getMaterialParameterHandle( "$skyMap" );
}


bool WaterObject::smWireframe = false;

//-------------------------------------------------------------------------
// WaterObject Class
//-------------------------------------------------------------------------

WaterObject::WaterObject()
 : mViscosity( 1.0f ),
   mDensity( 1.0f ),
   mReflectNormalUp( true ),   
   mDistortStartDist( 0.1f ),
   mDistortEndDist( 20.0f ),
   mDistortFullDepth( 3.5f ),
   mUndulateMaxDist(50.0f),
   mFoamScale( 1.0f ),
   mFoamMaxDepth( 2.0f ),
   mFoamColorModulate( 0.5f, 0.5f, 0.5f ),
   mUnderwaterPostFx( NULL ),
   mLiquidType( "Water" ),
   mFresnelBias( 0.3f ),
   mFresnelPower( 6.0f ),
   mClarity( 0.5f ),
   mBasicLighting( false ),
   mMiscParamW( 0.0f ),
   mOverallWaveMagnitude( 1.0f ),
   mOverallRippleMagnitude( 1.0f ),
   mCubemap( NULL )
{
   mTypeMask = WaterObjectType;

   for( U32 i=0; i < MAX_WAVES; i++ )
   {
      mRippleDir[i].set( 0.0f, 0.0f );
      mRippleSpeed[i] = 0.0f;
      mRippleTexScale[i].set( 0.0f, 0.0f );

      mWaveDir[i].set( 0.0f, 0.0f );
      mWaveSpeed[i] = 0.0f;      
      mWaveMagnitude[i] = 0.0f;
   }

   mRippleMagnitude[0] = 1.0f;
   mRippleMagnitude[1] = 1.0f;
   mRippleMagnitude[2] = 0.3f;

   mWaterFogData.density = 0.1f;
   mWaterFogData.densityOffset = 1.0f;     
   mWaterFogData.wetDepth = 1.5f;
   mWaterFogData.wetDarkening = 0.2f;
   mWaterFogData.color = ColorI::BLUE;

   mSurfMatName[WaterMat] = "Water";
   mSurfMatName[UnderWaterMat] = "UnderWater";
   mSurfMatName[BasicWaterMat] = "WaterBasic";   
   mSurfMatName[BasicUnderWaterMat] = "UnderWaterBasic";

   dMemset( mMatInstances, 0, sizeof(mMatInstances) );

   mWaterPos.set( 0,0,0 );
   mWaterPlane.set( mWaterPos, Point3F(0,0,1) );

   mGenerateVB = true;

   mMatrixSet = reinterpret_cast<MatrixSet *>(dAligned_malloc(sizeof(MatrixSet), 16));
   constructInPlace(mMatrixSet);
}

WaterObject::~WaterObject()
{
   dAligned_free(mMatrixSet);
}


void WaterObject::initPersistFields()
{
   addGroup( "WaterObject" );

      addField( "density", TypeF32, Offset( mDensity, WaterObject ), "Affects buoyancy of an object, thus affecting the Z velocity"
		  " of a player (jumping, falling, etc.");
      addField( "viscosity", TypeF32, Offset( mViscosity, WaterObject ), "Affects drag force applied to an object submerged in this container." );
      addField( "liquidType", TypeRealString, Offset( mLiquidType, WaterObject ), "Liquid type of WaterBlock, such as water, ocean, lava"
		  " Currently only Water is defined and used.");
      addField( "baseColor", TypeColorI,  Offset( mWaterFogData.color, WaterObject ), "Changes color of water fog." );
      addField( "fresnelBias",  TypeF32,  Offset( mFresnelBias, WaterObject ), "Extent of fresnel affecting reflection fogging." );
      addField( "fresnelPower",  TypeF32,  Offset( mFresnelPower, WaterObject ), "Measures intensity of affect on reflection based on fogging." );

      addArray( "Waves (vertex undulation)", MAX_WAVES );

         addField( "waveDir",       TypePoint2F,  Offset( mWaveDir, WaterObject ), MAX_WAVES, 0, "Direction waves flow toward shores." );
         addField( "waveSpeed",     TypeF32,  Offset( mWaveSpeed, WaterObject ), MAX_WAVES, 0, "Speed of water undulation." );
         addField( "waveMagnitude", TypeF32, Offset( mWaveMagnitude, WaterObject ), MAX_WAVES, 0, "Height of water undulation." );

      endArray( "Waves (vertex undulation)" );

      addField( "overallWaveMagnitude", TypeF32, Offset( mOverallWaveMagnitude, WaterObject ), "Master variable affecting entire body" 
		  " of water's undulation" );  
      
      addField( "rippleTex", TypeImageFilename, Offset( mRippleTexName, WaterObject ), "Normal map used to simulate small surface ripples" );

      addArray( "Ripples (texture animation)", MAX_WAVES );

         addField( "rippleDir",       TypePoint2F, Offset( mRippleDir, WaterObject ), MAX_WAVES, 0, "Modifies the direction of ripples on the surface." );
         addField( "rippleSpeed",     TypeF32, Offset( mRippleSpeed, WaterObject ), MAX_WAVES, 0, "Modifies speed of surface ripples.");
         addField( "rippleTexScale",  TypePoint2F, Offset( mRippleTexScale, WaterObject ), MAX_WAVES, 0, "Intensifies the affect of the normal map "
			 "applied to the surface.");
         addField( "rippleMagnitude", TypeF32, Offset( mRippleMagnitude, WaterObject ), MAX_WAVES, 0, "Intensifies the vertext modification of the surface." );

      endArray( "Ripples (texture animation)" );

      addField( "overallRippleMagnitude", TypeF32, Offset( mOverallRippleMagnitude, WaterObject ), "Master variable affecting entire surface");

   endGroup( "WaterObject" );

   addGroup( "Reflect" );

      addField( "cubemap", TypeCubemapName, Offset( mCubemapName, WaterObject ), "Cubemap used instead of reflection texture if fullReflect is off." );
      
      addProtectedField( "fullReflect", TypeBool, Offset( mFullReflect, WaterObject ), 
         &WaterObject::_setFullReflect, 
         &defaultProtectedGetFn, 
         "Enables dynamic reflection rendering." );

      addField( "reflectPriority", TypeF32, Offset( mReflectorDesc.priority, WaterObject ), "Affects the sort order of reflected objects." );
      addField( "reflectMaxRateMs", TypeS32, Offset( mReflectorDesc.maxRateMs, WaterObject ), "Affects the sort time of reflected objects." );
      //addField( "reflectMaxDist", TypeF32, Offset( mReflectMaxDist, WaterObject ), "vert distance at which only cubemap color is used" );
      //addField( "reflectMinDist", TypeF32, Offset( mReflectMinDist, WaterObject ), "vert distance at which only reflection color is used" );
      addField( "reflectDetailAdjust", TypeF32, Offset( mReflectorDesc.detailAdjust, WaterObject ), "scale up or down the detail level for objects rendered in a reflection" );
      addField( "reflectNormalUp", TypeBool, Offset( mReflectNormalUp, WaterObject ), "always use z up as the reflection normal" );
      addField( "useOcclusionQuery", TypeBool, Offset( mReflectorDesc.useOcclusionQuery, WaterObject ), "turn off reflection rendering when occluded (delayed)." );
      addField( "reflectTexSize", TypeS32, Offset( mReflectorDesc.texSize, WaterObject ), "The texture size used for reflections (square)" );

   endGroup( "Reflect" );   

   addGroup( "Underwater Fogging" );

      addField( "waterFogDensity", TypeF32, Offset( mWaterFogData.density, WaterObject ), "Intensity of underwater fogging." );
      addField( "waterFogDensityOffset", TypeF32, Offset( mWaterFogData.densityOffset, WaterObject ), "Delta, or limit, applied to waterFogDensity." );
      addField( "wetDepth", TypeF32, Offset( mWaterFogData.wetDepth, WaterObject ), "The depth in world units at which full darkening will be received,"
		  " giving a wet look to objects underwater." );
      addField( "wetDarkening", TypeF32, Offset( mWaterFogData.wetDarkening, WaterObject ), "The refract color intensity scaled at wetDepth." );

   endGroup( "Underwater Fogging" );

   addGroup( "Misc" );
      
      addField( "foamTex", TypeImageFilename, Offset( mFoamTexName, WaterObject ), "Diffuse texture for foam in shallow water (advanced lighting only)" );
      addField( "foamScale", TypeF32, Offset( mFoamScale, WaterObject ), "Size of the foam generated by WaterBlock hitting shore." );
      addField( "foamMaxDepth", TypeF32, Offset( mFoamMaxDepth, WaterObject ), "Controls how deep foam will be visible from underwater." );
      addField( "foamColorModulate", TypePoint3F, Offset( mFoamColorModulate, WaterObject ), "An RGB value taht linearly interpolates "
		  "between the base foam color and ambient color so there are not bright white"
		  " white colors during inappropriate situations, such as night.");

   endGroup( "Misc" );

   addGroup( "Distortion" );

      addField( "distortStartDist", TypeF32, Offset( mDistortStartDist, WaterObject ), "Determines start of distortion effect where water"
		  " surface intersects the camera near plane.");
      addField( "distortEndDist", TypeF32, Offset( mDistortEndDist, WaterObject ), "Max distance that distortion algorithm is performed. "
		  "The lower, the more distorted the effect.");
      addField( "distortFullDepth", TypeF32, Offset( mDistortFullDepth, WaterObject ), "Determines the scaling down of distortion "
		  "in shallow water.");

   endGroup( "Distortion" ); 

   addGroup( "Basic Lighting" );

      addField( "clarity", TypeF32, Offset( mClarity, WaterObject ), "Relative opacity or transparency of the water surface." );
      addField( "underwaterColor", TypeColorI, Offset( mUnderwaterColor, WaterObject ), "Changes the color shading of objects beneath"
		  " the water surface.");

   endGroup( "Basic Lighting" );

   Parent::initPersistFields();

   Con::addVariable( "$WaterObject::wireframe", TypeBool, &smWireframe );
}

void WaterObject::inspectPostApply()
{
   Parent::inspectPostApply();

   setMaskBits( UpdateMask | WaveMask | TextureMask );
}

bool WaterObject::_setFullReflect( void *obj, const char *data )
{
   WaterObject *water = static_cast<WaterObject*>( obj );
   water->mFullReflect = dAtob( data );
   
   if ( water->isProperlyAdded() && water->isClientObject() )
   {
      bool isEnabled = water->mPlaneReflector.isEnabled();
      if ( water->mFullReflect && !isEnabled )
         water->mPlaneReflector.registerReflector( water, &water->mReflectorDesc );
      else if ( !water->mFullReflect && isEnabled )
         water->mPlaneReflector.unregisterReflector();
   }

   return false;
}

U32 WaterObject::packUpdate( NetConnection * conn, U32 mask, BitStream *stream )
{
   U32 retMask = Parent::packUpdate( conn, mask, stream );

   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      stream->write( mDensity );
      stream->write( mViscosity );
      stream->write( mLiquidType );

      if ( stream->writeFlag( mFullReflect ) )
      {
         stream->write( mReflectorDesc.priority );
         stream->writeInt( mReflectorDesc.maxRateMs, 32 );
         //stream->write( mReflectMaxDist );
         //stream->write( mReflectMinDist );
         stream->write( mReflectorDesc.detailAdjust );
         stream->writeFlag( mReflectNormalUp );
         stream->writeFlag( mReflectorDesc.useOcclusionQuery );
         stream->writeInt( mReflectorDesc.texSize, 32 );
      }

      stream->write( mWaterFogData.density );
      stream->write( mWaterFogData.densityOffset );      
      stream->write( mWaterFogData.wetDepth );
      stream->write( mWaterFogData.wetDarkening );

      stream->write( mDistortStartDist );
      stream->write( mDistortEndDist );
      stream->write( mDistortFullDepth );

      stream->write( mFoamScale );
      stream->write( mFoamMaxDepth );
      mathWrite( *stream, mFoamColorModulate );      

      stream->write( mWaterFogData.color );

      stream->write( mFresnelBias );
      stream->write( mFresnelPower );

      stream->write( mClarity );
      stream->write( mUnderwaterColor );

      stream->write( mOverallRippleMagnitude );
      stream->write( mOverallWaveMagnitude );
   }

   if ( stream->writeFlag( mask & WaveMask ) )
   {
      for( U32 i=0; i<MAX_WAVES; i++ )
      {
         stream->write( mRippleSpeed[i] );
         mathWrite( *stream, mRippleDir[i] );
         mathWrite( *stream, mRippleTexScale[i] );
         stream->write( mRippleMagnitude[i] );

         stream->write( mWaveSpeed[i] );
         mathWrite( *stream, mWaveDir[i] );
         stream->write( mWaveMagnitude[i] );  
      }
   }

   if ( stream->writeFlag( mask & MaterialMask ) )
   {
      for ( U32 i = 0; i < NumMatTypes; i++ )      
         stream->write( mSurfMatName[i] );
   }

   if ( stream->writeFlag( mask & TextureMask ) )
   {
      stream->write( mRippleTexName );
      stream->write( mFoamTexName );
      stream->write( mCubemapName );
   }

   return retMask;
}

void WaterObject::unpackUpdate( NetConnection * conn, BitStream *stream )
{
   Parent::unpackUpdate( conn, stream );

   // UpdateMask
   if ( stream->readFlag() )
   {
      stream->read( &mDensity );
      stream->read( &mViscosity );
      stream->read( &mLiquidType );
      
      if ( stream->readFlag() )
      {
         mFullReflect = true;
         stream->read( &mReflectorDesc.priority );
         mReflectorDesc.maxRateMs = stream->readInt( 32 );
         //stream->read( &mReflectMaxDist );    
         //stream->read( &mReflectMinDist );
         stream->read( &mReflectorDesc.detailAdjust );
         mReflectNormalUp = stream->readFlag();
         mReflectorDesc.useOcclusionQuery = stream->readFlag();
         mReflectorDesc.texSize = stream->readInt( 32 );

         if ( isProperlyAdded() && !mPlaneReflector.isEnabled() )
            mPlaneReflector.registerReflector( this, &mReflectorDesc );
      }
      else
      {
         mFullReflect = false;
         if ( isProperlyAdded() && mPlaneReflector.isEnabled() )
            mPlaneReflector.unregisterReflector();
      }

      stream->read( &mWaterFogData.density );
      stream->read( &mWaterFogData.densityOffset );      
      stream->read( &mWaterFogData.wetDepth );
      stream->read( &mWaterFogData.wetDarkening );

      stream->read( &mDistortStartDist );
      stream->read( &mDistortEndDist );
      stream->read( &mDistortFullDepth );

      stream->read( &mFoamScale );
      stream->read( &mFoamMaxDepth );
      mathRead( *stream, &mFoamColorModulate );    

      stream->read( &mWaterFogData.color );

      stream->read( &mFresnelBias );
      stream->read( &mFresnelPower );

      stream->read( &mClarity );
      stream->read( &mUnderwaterColor );

      stream->read( &mOverallRippleMagnitude );
      stream->read( &mOverallWaveMagnitude );
   }

   // WaveMask
   if ( stream->readFlag() )
   {
      for( U32 i=0; i<MAX_WAVES; i++ )
      {
         stream->read( &mRippleSpeed[i] );
         mathRead( *stream, &mRippleDir[i] );
         mathRead( *stream, &mRippleTexScale[i] );         
         stream->read( &mRippleMagnitude[i] );

         stream->read( &mWaveSpeed[i] );
         mathRead( *stream, &mWaveDir[i] );         
         stream->read( &mWaveMagnitude[i] );
      }
   }

   // MaterialMask
   if ( stream->readFlag() ) 
   {
      for ( U32 i = 0; i < NumMatTypes; i++ )      
         stream->read( &mSurfMatName[i] );

      if ( isProperlyAdded() )    
      {
         // So they will be reloaded on next use.
         cleanupMaterials();         
      }
   }  

   // TextureMask
   if ( stream->readFlag() )
   {
      stream->read( &mRippleTexName );
      stream->read( &mFoamTexName );
      stream->read( &mCubemapName );

      if ( isProperlyAdded() )
         initTextures();
   }
}

bool WaterObject::prepRenderImage( SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseZoneState )
{
   PROFILE_SCOPE(WaterObject_prepRenderImage);

   // Are we in Basic Lighting?
   mBasicLighting = dStricmp( gClientSceneGraph->getLightManager()->getId(), "BLM" ) == 0;
   mUnderwater = isUnderwater( state->getCameraPosition() );

   // We only render during the normal diffuse render pass.
   if ( state->isShadowPass() || state->isReflectPass() ) //mPlaneReflector.isRendering() )
      return false;

   if ( isLastState( state, stateKey ) )
      return false;

   setLastState(state, stateKey);

   if ( state->isObjectRendered( this ) )
   {
      // Setup scene transforms
      mMatrixSet->setSceneView(GFX->getWorldMatrix());
      mMatrixSet->setSceneProjection(GFX->getProjectionMatrix());

      _getWaterPlane( state->getCameraPosition(), mWaterPlane, mWaterPos );
      mWaterFogData.plane = mWaterPlane;
      mPlaneReflector.refplane = mWaterPlane;

      updateUnderwaterEffect( state );

      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &WaterObject::renderObject );
      ri->type = RenderPassManager::RIT_Water;
      ri->defaultKey = 1;
      state->getRenderPass()->addInst( ri );

      //mRenderUpdateCount++;
   }

   return false;
}

void WaterObject::renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{
   if ( overrideMat )
      return;   

   // TODO: Revive projection z-bias at some point.
   // The current issue with this method of fixing z-fighting
   // in the WaterBlock is that a constant bias does not alleviate
   // the issue at the extreme end of the view range.
   //GFXTransformSaver saver;

   //MatrixF projMat( true );
   //const Frustum &frustum = ri->state->getFrustum();
   //
   //F32 bias = Con::getFloatVariable( "$waterBlockBias", 0.0002418f );

   //MathUtils::getZBiasProjectionMatrix( bias, frustum, &projMat );
   //GFX->setProjectionMatrix( projMat );
 
   
   GFXOcclusionQuery *query = mPlaneReflector.getOcclusionQuery();
   if ( query && mReflectorDesc.useOcclusionQuery )
      query->begin();


   // Real render call, done by derived class.
   innerRender( state );

   if ( query && mReflectorDesc.useOcclusionQuery )
      query->end();   

   if ( mUnderwater && mBasicLighting )
      drawUnderwaterFilter( state );
}

void WaterObject::setCustomTextures( S32 matIdx, U32 pass, const WaterMatParams &paramHandles )
{
   // TODO: Retrieve sampler numbers from parameter handles, see r22631.
   // Always use the ripple texture.
   GFX->setTexture( 0, mRippleTex );

   // Only above-water in advanced-lighting uses the foam texture.
   if ( matIdx == WaterMat )
      GFX->setTexture( 5, mFoamTex );

   // Only use the cubemap if fullReflect is off.
   if ( !mPlaneReflector.isEnabled() )
   {      
      if ( mCubemap )      
         GFX->setCubeTexture( 4, mCubemap->mCubemap );
      else
         GFX->setCubeTexture( 4, NULL );   
   }
   else
      GFX->setCubeTexture( 4, NULL );
}

void WaterObject::drawUnderwaterFilter( SceneState *state )
{
   // set up camera transforms
   MatrixF proj = GFX->getProjectionMatrix();
   MatrixF newMat(true);
   GFX->setProjectionMatrix( newMat );
   GFX->pushWorldMatrix();
   GFX->setWorldMatrix( newMat );   

   // set up render states
   GFX->disableShaders();
   GFX->setStateBlock( mUnderwaterSB );

   /*
   const Frustum &frustum = state->getFrustum();
   const MatrixF &camXfm = state->getCameraTransform();
   F32 nearDist = frustum.getNearDist();
   F32 nearLeft = frustum.getNearLeft();
   F32 nearRight = frustum.getNearRight();
   F32 nearTop = frustum.getNearTop();
   F32 nearBottom = frustum.getNearBottom();
   Point3F centerPnt;
   frustum.getCenterPoint( &centerPnt );

   MatrixF.mul
   Point3F linePnt, lineDir;
   if ( mIntersect( nearPlane, mWaterPlane, &linePnt, &lineDir ) )
   {
      Point3F leftPnt( centerPnt );
      leftPnt.x = near
   }   
   */

   Point2I resolution = GFX->getActiveRenderTarget()->getSize();
   F32 copyOffsetX = 1.0 / resolution.x;
   F32 copyOffsetY = 1.0 / resolution.y;

   /*
   ClippedPolyList polylist;
   polylist.addPoint( Point3F( -1.0f - copyOffsetX, -1.0f + copyOffsetY, 0.0f ) );
   polylist.addPoint( Point3F( -1.0f - copyOffsetX, 1.0f + copyOffsetY, 0.0f ) );
   polylist.addPoint( Point3F( 1.0f - copyOffsetX, 1.0f + copyOffsetY, 0.0f ) );
   polylist.addPoint( Point3F( 1.0f - copyOffsetX, -1.0f + copyOffsetY, 0.0f ) );
   polylist.addPlane( clipPlane );

   polylist.begin( NULL, 0 );
   polylist.vertex( 0 );
   polylist.vertex( 1 );
   polylist.vertex( 2 );
   polylist.vertex( 0 );
   polylist.vertex( 2 );
   polylist.vertex( 3 );
   */

   // draw quad
   

   GFXVertexBufferHandle<GFXVertexPC> verts( GFX, 4, GFXBufferTypeVolatile );
   verts.lock();

   verts[0].point.set( -1.0 - copyOffsetX, -1.0 + copyOffsetY, 0.0 );
   verts[0].color = mUnderwaterColor;

   verts[1].point.set( -1.0 - copyOffsetX, 1.0 + copyOffsetY, 0.0 );
   verts[1].color = mUnderwaterColor;

   verts[2].point.set( 1.0 - copyOffsetX, 1.0 + copyOffsetY, 0.0 );
   verts[2].color = mUnderwaterColor;

   verts[3].point.set( 1.0 - copyOffsetX, -1.0 + copyOffsetY, 0.0 );
   verts[3].color = mUnderwaterColor;

   verts.unlock();

   GFX->setVertexBuffer( verts );
   GFX->drawPrimitive( GFXTriangleFan, 0, 2 );

   // reset states / transforms
   GFX->setProjectionMatrix( proj );
   GFX->popWorldMatrix();
}

bool WaterObject::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   if ( isClientObject() )
   {
      GFXStateBlockDesc desc;
      desc.blendDefined = true;
      desc.blendEnable = true;
      desc.blendSrc = GFXBlendSrcAlpha;
      desc.blendDest = GFXBlendInvSrcAlpha;
      desc.zDefined = true;
      desc.zEnable = false;
      desc.cullDefined = true;
      desc.cullMode = GFXCullNone;
      mUnderwaterSB = GFX->createStateBlock( desc );

      initTextures();
      
      if ( mFullReflect )
         mPlaneReflector.registerReflector( this, &mReflectorDesc );
   }

   return true;
}

void WaterObject::onRemove()
{
   if ( isClientObject() )
   {
      mPlaneReflector.unregisterReflector();
      cleanupMaterials();
   }

   Parent::onRemove();
}

void WaterObject::setShaderParams( SceneState *state, BaseMatInstance *mat, const WaterMatParams &paramHandles )
{
   MaterialParameters* matParams = mat->getMaterialParameters();

   matParams->set( paramHandles.mElapsedTimeSC, (F32)Sim::getCurrentTime() / 1000.0f );
   
   // set vertex shader constants
   //-----------------------------------   

   //Point2F reflectTexSize( mReflectTexSize.x, mReflectTexSize.y );
   Point2F reflectTexSize( mPlaneReflector.reflectTex.getWidth(), mPlaneReflector.reflectTex.getHeight() );
   matParams->set( paramHandles.mReflectTexSizeSC, reflectTexSize );

   if ( mConstArray.getElementSize() == 0 )
      mConstArray.setCapacity( MAX_WAVES, sizeof( Point4F ) );

   // Ripples...

   for ( U32 i = 0; i < MAX_WAVES; i++ )
      mConstArray[i].set( mRippleDir[i].x, mRippleDir[i].y );
   matParams->set( paramHandles.mRippleDirSC, mConstArray );

   Point3F rippleSpeed( mRippleSpeed[0], mRippleSpeed[1], mRippleSpeed[2] );        
   matParams->set( paramHandles.mRippleSpeedSC, rippleSpeed );

   Point3F rippleMagnitude( mRippleMagnitude[0] * mOverallRippleMagnitude, 
                            mRippleMagnitude[1] * mOverallRippleMagnitude, 
                            mRippleMagnitude[2] * mOverallRippleMagnitude );
   matParams->set( paramHandles.mRippleMagnitudeSC, rippleMagnitude );

   for ( U32 i = 0; i < MAX_WAVES; i++ )
   {
      Point2F texScale = mRippleTexScale[i];
      if ( texScale.x > 0.0 )
         texScale.x = 1.0 / texScale.x;
      if ( texScale.y > 0.0 )
         texScale.y = 1.0 / texScale.y;

      mConstArray[i].set( texScale.x, texScale.y );
   }
   matParams->set(paramHandles.mRippleTexScaleSC, mConstArray);

   // Waves...

   for ( U32 i = 0; i < MAX_WAVES; i++ )
      mConstArray[i].set( mWaveDir[i].x, mWaveDir[i].y );
   matParams->set( paramHandles.mWaveDirSC, mConstArray );

   for ( U32 i = 0; i < MAX_WAVES; i++ )
      mConstArray[i].set( mWaveSpeed[i], mWaveMagnitude[i] * mOverallWaveMagnitude );   
   matParams->set( paramHandles.mWaveDataSC, mConstArray );   

   // Other vert params...

   matParams->set( paramHandles.mUndulateMaxDistSC, mUndulateMaxDist );

   // set pixel shader constants
   //-----------------------------------

   Point2F fogParams( mWaterFogData.density, mWaterFogData.densityOffset );
   matParams->set(paramHandles.mFogParamsSC, fogParams );

   matParams->set(paramHandles.mFarPlaneDistSC, (F32)state->getFarPlane() );

   Point2F wetnessParams( mWaterFogData.wetDepth, mWaterFogData.wetDarkening );
   matParams->set(paramHandles.mWetnessParamsSC, wetnessParams );

   Point3F distortionParams( mDistortStartDist, mDistortEndDist, mDistortFullDepth );
   matParams->set(paramHandles.mDistortionParamsSC, distortionParams );

   const ColorF &sunlight = gClientSceneGraph->getLightManager()->getSpecialLight(LightManager::slSunLightType)->getAmbient();
   Point3F ambientColor( sunlight.red, sunlight.green, sunlight.blue );
   matParams->set(paramHandles.mAmbientColorSC, ambientColor );

   Point2F foamParams( mFoamScale, mFoamMaxDepth );
   matParams->set(paramHandles.mFoamParamsSC, foamParams );
   matParams->set(paramHandles.mFoamColorModulateSC, mFoamColorModulate );

   Point4F miscParams( mFresnelBias, mFresnelPower, mClarity, mMiscParamW );
   matParams->set( paramHandles.mMiscParamsSC, miscParams );
}

PostEffect* WaterObject::getUnderwaterEffect()
{
   if ( mUnderwaterPostFx.isValid() )
      return mUnderwaterPostFx;
   
   PostEffect *effect;
   if ( Sim::findObject( "UnderwaterFogPostFx", effect ) )   
      mUnderwaterPostFx = effect;

   return mUnderwaterPostFx;   
}

void WaterObject::updateUnderwaterEffect( SceneState *state )
{
   AssertFatal( isClientObject(), "uWaterObject::updateUnderwaterEffect() called on the server" );

   PostEffect *effect = getUnderwaterEffect();
   if ( !effect )
      return;

   // Never use underwater postFx with Basic Lighting, we don't have depth.
   if ( mBasicLighting )
   {
      effect->disable();
      return;
   }

   GameConnection *conn = GameConnection::getConnectionToServer();
   if ( !conn )
      return;

   GameBase *control = conn->getControlObject();
   if ( !control )
      return;

   WaterObject *water = control->getCurrentWaterObject();
   if ( water == NULL )
      effect->disable();

   else if ( water == this )
   {
      MatrixF mat;      
      conn->getControlCameraTransform( 0, &mat );
      
      if ( mUnderwater )
      {
         effect->enable();
         effect->setOnThisFrame( true );

         gClientSceneGraph->setWaterFogData( mWaterFogData );
      }
      else
         effect->disable();
   }
}

bool WaterObject::initMaterial( S32 idx )
{
   // We must return false for any case which it is NOT safe for the caller
   // to use the indexed material.
   
   if ( idx < 0 || idx > NumMatTypes )
      return false;

   BaseMatInstance *mat = mMatInstances[idx];
   WaterMatParams &matParams = mMatParamHandles[idx];
   
   // Is it already initialized?

   if ( mat && mat->isValid() )
      return true;

   // Do we need to allocate anything?

   if ( mSurfMatName[idx].isNotEmpty() )
   {      
      if ( mat )
         SAFE_DELETE( mat );

      CustomMaterial *custMat;
      if ( Sim::findObject( mSurfMatName[idx], custMat ) && custMat->mShaderData )
         mat = custMat->createMatInstance();
      else
         mat = MATMGR->createMatInstance( mSurfMatName[idx] );

      const GFXVertexFormat *flags = getGFXVertexFormat<GFXVertexPC>();

      if ( mat && mat->init( MATMGR->getDefaultFeatures(), flags ) )
      {      
         mMatInstances[idx] = mat;
         matParams.init( mat );         
         return true;
      }
            
      SAFE_DELETE( mat );      
   }

   return false;
}

void WaterObject::initTextures()
{
   if ( mRippleTexName.isNotEmpty() )
      mRippleTex.set( mRippleTexName, &GFXDefaultStaticDiffuseProfile, "WaterObject::mRippleTex" );
   if ( mRippleTex.isNull() )
      mRippleTex.set( "core/art/warnmat", &GFXDefaultStaticDiffuseProfile, "WaterObject::mRippleTex" );

   if ( mFoamTexName.isNotEmpty() )
      mFoamTex.set( mFoamTexName, &GFXDefaultStaticDiffuseProfile, "WaterObject::mFoamTex" );
   if ( mFoamTex.isNull() )
      mFoamTex.set( "core/art/warnmat", &GFXDefaultStaticDiffuseProfile, "WaterObject::mFoamTex" );

   if ( mCubemapName.isNotEmpty() )
      Sim::findObject( mCubemapName, mCubemap );   
   if ( mCubemap )
      mCubemap->createMap();
}

void WaterObject::cleanupMaterials()
{
   for (U32 i = 0; i < NumMatTypes; i++)
      SAFE_DELETE(mMatInstances[i]);
}

S32 WaterObject::getMaterialIndex( const Point3F &camPos )
{
   bool underwater = isUnderwater( camPos );
   bool basicLighting = dStricmp( gClientSceneGraph->getLightManager()->getId(), "BLM" ) == 0;

   // set the material
   S32 matIdx = -1;
   if ( underwater )
   {
      if ( basicLighting )
         matIdx = BasicUnderWaterMat;
      else
         matIdx = UnderWaterMat;
   }
   else
   {
      if ( basicLighting )
         matIdx = BasicWaterMat;
      else
         matIdx = WaterMat;
   }

   return matIdx;
}