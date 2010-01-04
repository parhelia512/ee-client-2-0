//-----------------------------------------------------------------------------
// Torque Game Engine Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "scatterSky.h"

#include "sceneGraph/sceneState.h"
#include "lighting/lightInfo.h"
#include "math/util/sphereMesh.h"

#include "gfx/sim/gfxStateBlockData.h"
#include "materials/shaderData.h"
#include "gfx/gfxTransformSaver.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "gfx/gfxDrawUtil.h"
#include "environment/timeOfDay.h"
#include "sim/netConnection.h"
#include "math/mathUtils.h"
#include "gfx/sim/cubemapData.h"

IMPLEMENT_CO_NETOBJECT_V1(ScatterSky);

const F32 ScatterSky::smEarthRadius = (6378.0f * 1000.0f);
const F32 ScatterSky::smAtmosphereRadius = 200000.0f;
const F32 ScatterSky::smViewerHeight = 1.0f;

GFXImplementVertexFormat( ScatterSkyVertex )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::COLOR, GFXDeclType_Color );
}

ScatterSky::ScatterSky()
{ 
   mPrimCount = 0;
   mVertCount = 0;
   
   // Rayleigh scattering constant.
   mRayleighScattering = 0.0035f;		
   mRayleighScattering4PI = mRayleighScattering * 4.0f * M_PI_F;

   // Mie scattering constant.
   mMieScattering = 0.0045f;		
   mMieScattering4PI = mMieScattering * 4.0f * M_PI_F;

   // Overall scatter scalar.
   mSkyBrightness = 25.0f;		

   // The Mie phase asymmetry factor.
   mMiePhaseAssymetry = -0.75f;		

   mSphereInnerRadius = 1.0f;
   mSphereOuterRadius = 1.0f * 1.025f;
   mScale = 1.0f / (mSphereOuterRadius - mSphereInnerRadius);
   
   // 650 nm for red 
   // 570 nm for green 
   // 475 nm for blue
   mWavelength.set( 0.650f, 0.570f, 0.475f, 0 );		

   mWavelength4[0] = mPow(mWavelength[0], 4.0f);
   mWavelength4[1] = mPow(mWavelength[1], 4.0f);
   mWavelength4[2] = mPow(mWavelength[2], 4.0f);

   mRayleighScaleDepth = 0.25f;
   mMieScaleDepth = 0.1f;

   mAmbientColor.set( 0, 0, 0, 1.0f );
   mAmbientScale.set( 1.0f, 1.0f, 1.0f, 1.0f );
   
   mSunColor.set( 0, 0, 0, 1.0f );
   mSunScale.set( 1.0f, 1.0f, 1.0f, 1.0f );

   mFogColor.set( 0, 0, 0, 1.0f );

   mExposure = 1.0f;
   mNightInterpolant = 0;

   mShader = NULL;
   
   mTimeOfDay = 0;

   mSunAzimuth = 0.0f;
   mSunElevation = 35.0f;

   mBrightness = 1.0f;

   mCastShadows = true;
   mDirty = true;

   mLight = LightManager::createLightInfo();
   mLight->setType( LightInfo::Vector );   

   mFlareData = NULL;
   mFlareState.clear();
   mFlareScale = 1.0f;

   mMoonEnabled = true;
   mMoonScale = 0.3f;
   mMoonTint.set( 0.192157f, 0.192157f, 0.192157f, 1.0f );
   MathUtils::getVectorFromAngles( mMoonLightDir, 0.0f, 45.0f );
   mMoonLightDir.normalize();
   mMoonLightDir = -mMoonLightDir;
   mNightCubemap = NULL;
   mNightColor.set( 0.0196078f, 0.0117647f, 0.109804f, 1.0f );
   mUseNightCubemap = false;

   mNetFlags.set( Ghostable | ScopeAlways );
   mTypeMask |= EnvironmentObjectType | LightObjectType;

   _generateSkyPoints();
}

ScatterSky::~ScatterSky()
{
   SAFE_DELETE( mLight );
}

bool ScatterSky::onAdd()
{
   PROFILE_SCOPE(ScatterSky_onAdd);
   
   // onNewDatablock for the server is called here
   // for the client it is called in unpackUpdate
   if ( !Parent::onAdd() )
      return false;

   if ( isClientObject() )
      TimeOfDay::getTimeOfDayUpdateSignal().notify( this, &ScatterSky::_updateTimeOfDay );

   setGlobalBounds(); 
   resetWorldBox();

   addToScene();    

   if ( isClientObject() )
   {
      _initMoon();
      Sim::findObject( mNightCubemapName, mNightCubemap );
   }

   return true;
}

void ScatterSky::onRemove()
{
   removeFromScene();

   if ( isClientObject() )
      TimeOfDay::getTimeOfDayUpdateSignal().remove( this, &ScatterSky::_updateTimeOfDay );

   Parent::onRemove();
}

void ScatterSky::_conformLights()
{
   _initCurves();
   F32 val = mCurves[0].getVal( mTimeOfDay );
   mNightInterpolant = 1.0f - val;

   VectorF lightDirection;   
   F32 brightness;

   if ( mNightInterpolant == 1.0f )
   {
      lightDirection = -mMoonLightDir;      
      brightness = mCurves[1].getVal( mTimeOfDay );
   }
   else
   {
      // Build the light direction from the azimuth and elevation.
      F32 yaw = mDegToRad(mClampF(mSunAzimuth,0,359));
      F32 pitch = mDegToRad(mClampF(mSunElevation,-360,+360));      
      MathUtils::getVectorFromAngles(lightDirection, yaw, pitch);
      lightDirection.normalize();

      brightness = val;
   }
   
   mLight->setDirection( -lightDirection );
   mLight->setBrightness( brightness * mBrightness );
   mLightDir = lightDirection;   
   
   // Have to do interpolation
   // after the light direction is set
   // otherwise the sun color will be invalid.
   _interpolateColors();

   if ( mNightInterpolant == 1.0f )
      mAmbientColor += mAmbientColor * 2.0f * ( 1.0f - brightness );

   mLight->setAmbient( mAmbientColor );
   mLight->setColor( mSunColor );
   
   
   bool castShadows =   mCastShadows &&
                        mSunColor != mAmbientColor && 
                        brightness > 0.1f;  
                        
   //bool castShadows = false;
   mLight->setCastShadows( castShadows );

   FogData fog = gClientSceneGraph->getFogData();
   fog.color = mFogColor;
   gClientSceneGraph->setFogData( fog );   
}

void ScatterSky::submitLights( LightManager *lm, bool staticLighting )
{
   if ( mDirty )
   {
      _conformLights();
      mDirty = false;
   }

   // The sun is a special light and needs special registration.
   lm->setSpecialLight( LightManager::slSunLightType, mLight );
}

void ScatterSky::setAzimuth( F32 azimuth )
{
   mSunAzimuth = azimuth;
   mDirty = true;
   setMaskBits( TimeMask );
}

void ScatterSky::setElevation( F32 elevation )
{
   mSunElevation = elevation;
   
   while( elevation < 0 )
      elevation += 360.0f;

   while( elevation >= 360.0f )
      elevation -= 360.0f;

   mTimeOfDay = elevation / 180.0f;
   mDirty = true;
   setMaskBits( TimeMask );
}

void ScatterSky::inspectPostApply()
{
   mDirty = true;
	setMaskBits( 0xFFFFFFFF );
}

void ScatterSky::initPersistFields()
{
   addGroup( "ScatterSky", 
      "Only azimuth and elevation are networked fields. To trigger a full update of all other fields use the applyChanges ConsoleMethod." );

      addField( "skyBrightness",        TypeF32,    Offset( mSkyBrightness, ScatterSky ) );
      addField( "mieScattering",       TypeF32,    Offset( mMieScattering, ScatterSky ) );
      addField( "rayleighScattering",  TypeF32,    Offset( mRayleighScattering, ScatterSky ) );      
      addField( "sunScale",            TypeColorF, Offset( mSunScale, ScatterSky ) );  
      addField( "ambientScale",        TypeColorF, Offset( mAmbientScale, ScatterSky ) );    
      addField( "exposure",            TypeF32,    Offset( mExposure, ScatterSky ) );

   endGroup( "ScatterSky" );

   addGroup( "Orbit" );
   
      addProtectedField( "azimuth", TypeF32, Offset( mSunAzimuth, ScatterSky ), &ScatterSky::ptSetAzimuth, &defaultProtectedGetFn,
            "The horizontal angle of the sun measured clockwise from the positive Y world axis. This field is networked." );

      addProtectedField( "elevation", TypeF32, Offset( mSunElevation, ScatterSky ), &ScatterSky::ptSetElevation, &defaultProtectedGetFn,
            "The elevation angle of the sun above or below the horizon. This field is networked." );

   endGroup( "Orbit" );	

   // We only add the basic lighting options that all lighting
   // systems would use... the specific lighting system options
   // are injected at runtime by the lighting system itself.

   addGroup( "Lighting" );
      
      addField( "castShadows", TypeBool, Offset( mCastShadows, ScatterSky ) );
      addField( "brightness", TypeF32, Offset( mBrightness, ScatterSky ), 
         "The brightness of the ScatterSky's light object." );    

   endGroup( "Lighting" );

   addGroup( "Misc" );

      addField( "flareType", TypeLightFlareDataPtr, Offset( mFlareData, ScatterSky ) );
      addField( "flareScale", TypeF32, Offset( mFlareScale, ScatterSky ) );

   endGroup( "Misc" );

   addGroup( "Night" );

      addField( "nightColor", TypeColorF, Offset( mNightColor, ScatterSky ) );
      addField( "moonEnabled", TypeBool, Offset( mMoonEnabled, ScatterSky ) );
      addField( "moonTexture", TypeImageFilename, Offset( mMoonTextureName, ScatterSky ) );
      addField( "moonScale", TypeF32, Offset( mMoonScale, ScatterSky ) );
      addField( "moonTint", TypeColorF, Offset( mMoonTint, ScatterSky ) );
      addField( "useNightCubemap", TypeBool, Offset( mUseNightCubemap, ScatterSky ) );
      addField( "nightCubemap", TypeCubemapName, Offset( mNightCubemapName, ScatterSky ) );            

   endGroup( "Night" );

   // Now inject any light manager specific fields.
   LightManager::initLightFields();
   
   Parent::initPersistFields();
}

U32 ScatterSky::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   if ( stream->writeFlag( mask & TimeMask ) )
   {
      stream->write( mSunAzimuth );
      stream->write( mSunElevation );
   }

   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      stream->write( mRayleighScattering );
      mRayleighScattering4PI = mRayleighScattering * 4.0f * M_PI_F;

      stream->write( mRayleighScattering4PI );

      stream->write( mMieScattering );
      mMieScattering4PI = mMieScattering * 4.0f * M_PI_F;

      stream->write( mMieScattering4PI );
      
      stream->write( mSkyBrightness );
      
      stream->write( mMiePhaseAssymetry );
      
      stream->write( mSphereInnerRadius );
      stream->write( mSphereOuterRadius );
      
      stream->write( mScale );
      
      stream->write( mWavelength );
      
      stream->write( mWavelength4[0] );
      stream->write( mWavelength4[1] );
      stream->write( mWavelength4[2] );
      
      stream->write( mRayleighScaleDepth );
      stream->write( mMieScaleDepth );

      stream->write( mNightColor );
      stream->write( mAmbientScale );
      stream->write( mSunScale );

      stream->write( mExposure );

      stream->write( mBrightness );

      stream->writeFlag( mCastShadows );

      stream->write( mFlareScale );

      if ( stream->writeFlag( mFlareData ) )
      {
         stream->writeRangedU32( mFlareData->getId(),
                                 DataBlockObjectIdFirst, 
                                 DataBlockObjectIdLast );
      }

      stream->writeFlag( mMoonEnabled );
      stream->write( mMoonTextureName );
      stream->write( mMoonScale );
      stream->write( mMoonTint );
      stream->writeFlag( mUseNightCubemap );
      stream->write( mNightCubemapName );


      mLight->packExtended( stream );    
   }

   return retMask;
}

void ScatterSky::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);
   
   if ( stream->readFlag() ) // TimeMask
   {
      F32 temp = 0;
      stream->read( &temp );
      setAzimuth( temp );

      stream->read( &temp );
      setElevation( temp );
   }

   if ( stream->readFlag() ) // UpdateMask
   {
      stream->read( &mRayleighScattering );
      stream->read( &mRayleighScattering4PI );

      stream->read( &mMieScattering );
      stream->read( &mMieScattering4PI );
      
      stream->read( &mSkyBrightness );
      
      stream->read( &mMiePhaseAssymetry );
      
      stream->read( &mSphereInnerRadius );
      stream->read( &mSphereOuterRadius );
      
      stream->read( &mScale );
      
      ColorF tmpColor( 0, 0, 0 );

      stream->read( &tmpColor );
      
      stream->read( &mWavelength4[0] );
      stream->read( &mWavelength4[1] );
      stream->read( &mWavelength4[2] );
      
      stream->read( &mRayleighScaleDepth );
      stream->read( &mMieScaleDepth );

      stream->read( &mNightColor );
      stream->read( &mAmbientScale );
      stream->read( &mSunScale );

      if ( tmpColor != mWavelength )
      {
         mWavelength = tmpColor;
         mWavelength4[0] = mPow(mWavelength[0], 4.0f);
         mWavelength4[1] = mPow(mWavelength[1], 4.0f);
         mWavelength4[2] = mPow(mWavelength[2], 4.0f);
      }

      stream->read( &mExposure );

      stream->read( &mBrightness );

      mCastShadows = stream->readFlag();

      stream->read( &mFlareScale );

      if ( stream->readFlag() )
      {
         SimObjectId id = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );  
         LightFlareData *datablock = NULL;

         if ( Sim::findObject( id, datablock ) )
            mFlareData = datablock;
         else
         {
            con->setLastError( "ScatterSky::unpackUpdate() - invalid LightFlareData!" );
            mFlareData = NULL;
         }
      }
      else
         mFlareData = NULL;

      mMoonEnabled = stream->readFlag();
      stream->read( &mMoonTextureName );
      stream->read( &mMoonScale );
      stream->read( &mMoonTint );
      mUseNightCubemap = stream->readFlag();
      stream->read( &mNightCubemapName );   

      mLight->unpackExtended( stream ); 

      if ( isProperlyAdded() )
      {
         mDirty = true;
         _initMoon();
         Sim::findObject( mNightCubemapName, mNightCubemap );
      }
   }
}

bool ScatterSky::prepRenderImage( SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseState )
{
   if ( !(state->isDiffusePass() || state->isReflectPass()) )
      return false;

   if (isLastState(state, stateKey))
      return false;

   setLastState(state, stateKey);

   // Test portal visibility.
   if ( !state->isObjectRendered(this) )
      return false;

   // Regular sky render instance.
   ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
   ri->renderDelegate.bind( this, &ScatterSky::_render );
   ri->type = RenderPassManager::RIT_Sky;
   ri->defaultKey = 10;
   ri->defaultKey2 = 0;
   state->getRenderPass()->addInst( ri );

   // Debug render instance.
   if ( Con::getBoolVariable( "$ScatterSky::debug", false ) )
   {
      ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &ScatterSky::_debugRender );
      ri->type = RenderPassManager::RIT_Object;
      state->getRenderPass()->addInst( ri );
   }

   // Light flare effect render instance.
   if ( mFlareData && mNightInterpolant != 1.0f )
   {
      mFlareState.fullBrightness = mBrightness;
      mFlareState.scale = mFlareScale;
      mFlareState.lightInfo = mLight;

      Point3F lightPos = state->getCameraPosition() - state->getFarPlane() * mLight->getDirection() * 0.9f;
      mFlareState.lightMat.identity();
      mFlareState.lightMat.setPosition( lightPos );

      mFlareData->prepRender( state, &mFlareState );
   }

   // Render instances for Night effects.
   if ( mNightInterpolant <= 0.0f )
      return false;

   // Render instance for Moon sprite.   
   if ( mMoonEnabled && mMoonTexture.isValid() )
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &ScatterSky::_renderMoon );
      ri->type = RenderPassManager::RIT_Sky;      
      // Render after sky objects and before CloudLayer!
      ri->defaultKey = 5;
      ri->defaultKey2 = 0;
      state->getRenderPass()->addInst( ri );
   }

   return false;
}

bool ScatterSky::_initShader()
{
   ShaderData *shaderData;
   if ( !Sim::findObject( "ScatterSkyShaderData", shaderData ) )
   {
      Con::warnf( "ScatterSky::_initShader - failed to locate shader ScatterSkyShaderData!" );
      return false;
   }

   mShader = shaderData->getShader();
   if ( !mShader )
      return false;

   if ( mStateBlock.isNull() )
   {
      GFXStateBlockData *data = NULL;
      if ( !Sim::findObject( "ScatterSkySBData", data ) )
         Con::warnf( "ScatterSky::_initShader - failed to locate ScatterSkySBData!" );
      else
         mStateBlock = GFX->createStateBlock( data->getState() );
	  }

   if ( !mStateBlock ) 
      return false;

   mShaderConsts = mShader->allocConstBuffer();
   mModelViewProjSC = mShader->getShaderConstHandle( "$modelView" );

   // Camera height, cam height squared, scale and scale over depth.
   mMiscSC = mShader->getShaderConstHandle( "$misc" );          

   // Inner and out radius, and inner and outer radius squared.
   mSphereRadiiSC = mShader->getShaderConstHandle( "$sphereRadii" );         

   // Rayleigh sun brightness, mie sun brightness and 4 * PI * coefficients.
   mScatteringCoefficientsSC = mShader->getShaderConstHandle( "$scatteringCoeffs" );   
   mCamPosSC = mShader->getShaderConstHandle( "$camPos" );
   mLightDirSC = mShader->getShaderConstHandle( "$lightDir" );
   mPixLightDirSC = mShader->getShaderConstHandle( "$pLightDir" );
   mNightColorSC = mShader->getShaderConstHandle( "$nightColor" );
   mInverseWavelengthSC = mShader->getShaderConstHandle( "$invWaveLength" );
   mNightInterpolantAndExposureSC = mShader->getShaderConstHandle( "$nightInterpAndExposure" );
   mUseCubemapSC = mShader->getShaderConstHandle( "$useCubemap" );

   return true;
}

void ScatterSky::_initVBIB()
{
   U32 rings=60, segments=20;//rings=160, segments=20; 

   // Set vertex count and index count.
   U32 vertCount = ( rings + 1 ) * ( segments + 1 ) ;
   U32 idxCount = 2 * rings * ( segments + 1 ) ;

   mVertCount = vertCount;
   mPrimCount = idxCount;
   
   // If the VB or PB haven't been created then create them.
   if ( mPrimBuffer.isNull() )
      mPrimBuffer.set( GFX, idxCount, 0, GFXBufferTypeStatic );

   if ( mVB.isNull() )
      mVB.set( GFX, vertCount, GFXBufferTypeStatic );

   Point3F tmpPoint( 0, 0, 0 );
   Point3F horizPoint( 0, 0, 0 );

   ScatterSkyVertex *verts = mVB.lock();
   U16 *idxBuff;
   mPrimBuffer.lock( &idxBuff );

   // Establish constants used in sphere generation.
   F32 deltaRingAngle = ( M_PI_F / (F32)(rings * 2) );
   F32 deltaSegAngle = ( 2.0f * M_PI_F / (F32)segments );

   U32 vertIdx = 0; 
   // Generate the group of rings for the sphere.
   for( int ring = 0; ring < rings + 1 ; ring++ )
   {
      F32 r0 = mSin( ring * deltaRingAngle );
      F32 y0 = mCos( ring * deltaRingAngle ); 

      // Generate the group of segments for the current ring.
      for( int seg = 0; seg < segments + 1 ; seg++ )
      {
         F32 x0 = r0 * sinf( seg * deltaSegAngle );
         F32 z0 = r0 * cosf( seg * deltaSegAngle );

         tmpPoint.set( x0, z0, y0 );
         tmpPoint.normalizeSafe();

         tmpPoint.x *= (6378.0f * 1000.0f) + 200000.0f;
         tmpPoint.y *= (6378.0f * 1000.0f) + 200000.0f;
         tmpPoint.z *= (6378.0f * 1000.0f) + 200000.0f;
         tmpPoint.z -= (6378.0f * 1000.0f);

         // Add one vertices to the strip which makes up the sphere.
         verts->point = tmpPoint;
         verts++;

         // Add two indices except for last ring.
         if ( ring != rings ) 
         {
            *idxBuff = vertIdx; 
            idxBuff++;
            *idxBuff = vertIdx + (U32)( segments + 1 ) ; 
            idxBuff++;
            vertIdx++; 
         }; 
      }; // End for seg.
   } // End for ring.

   mPrimCount = vertIdx / 2;

   mVB.unlock();
   mPrimBuffer.unlock();
}

void ScatterSky::_initMoon()
{
   if ( isServerObject() )
      return;

   // Load texture...

   if ( mMoonTextureName.isNotEmpty() )      
      mMoonTexture.set( mMoonTextureName, &GFXDefaultStaticDiffuseProfile, "MoonTexture" );            

   // Make StateBlock...

   if ( mMoonSB.isNull() )
   {
      GFXStateBlockDesc desc;
      desc.setCullMode( GFXCullNone );
      desc.setAlphaTest( true, GFXCmpGreaterEqual, 1 );
      desc.setZReadWrite( false, false );
      desc.setBlend( true );
      desc.samplersDefined = true;
      desc.samplers[0].textureColorOp = GFXTOPModulate;
      desc.samplers[0].colorArg1 = GFXTATexture;
      desc.samplers[0].colorArg2 = GFXTADiffuse;
      desc.samplers[0].alphaOp = GFXTOPModulate;
      desc.samplers[0].alphaArg1 = GFXTATexture;
      desc.samplers[0].alphaArg2 = GFXTADiffuse;

      mMoonSB = GFX->createStateBlock(desc);

      desc.setFillModeWireframe();
      mMoonWireframeSB = GFX->createStateBlock(desc);
   }   
}

void ScatterSky::_initCurves()
{
   if ( mCurves->getSampleCount() > 0 )
      return;

   // Takes time of day (0-2) and returns
   // the night interpolant (0-1) day/night factor.
   mCurves[0].clear();
   mCurves[0].addPoint( 0.0f, 0.5f );
   mCurves[0].addPoint( 0.1f, 1.0f );
   mCurves[0].addPoint( 0.9f, 1.0f );
   mCurves[0].addPoint( 1.0f, 0.5f );
   mCurves[0].addPoint( 1.1f, 0.0f );
   mCurves[0].addPoint( 1.9f, 0.0f );
   mCurves[0].addPoint( 2.0f, 0.5f );
   
   // Takes time of day (0-2) and returns
   // the moon light brightness.
   mCurves[1].clear();
   mCurves[1].addPoint( 0.0f, 0.0f );
   mCurves[1].addPoint( 1.0f, 0.0f );
   mCurves[1].addPoint( 1.1f, 0.0f );
   mCurves[1].addPoint( 1.2f, 0.5f );   
   mCurves[1].addPoint( 1.3f, 1.0f );
   mCurves[1].addPoint( 1.8f, 0.5f );
   mCurves[1].addPoint( 1.9f, 0.0f );
   mCurves[1].addPoint( 2.0f, 0.0f );
}

void ScatterSky::_updateTimeOfDay( TimeOfDay *timeOfDay, F32 time )
{
   setElevation( timeOfDay->getElevationDegrees() );
   setAzimuth( timeOfDay->getAzimuthDegrees() );
}

void ScatterSky::_render( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{
   if ( overrideMat || (!mShader && !_initShader()) )
      return;

   GFXTransformSaver saver;

   //mLightDir = -mLight->getDirection();
   //mLightDir.normalize();

   if ( mVB.isNull() || mPrimBuffer.isNull() )
      _initVBIB();

   GFX->setShader( mShader );
   GFX->setShaderConstBuffer( mShaderConsts );

   Point4F sphereRadii( mSphereOuterRadius, mSphereOuterRadius * mSphereOuterRadius,
                        mSphereInnerRadius, mSphereInnerRadius * mSphereInnerRadius );

   Point4F scatteringCoeffs( mRayleighScattering * mSkyBrightness, mRayleighScattering4PI,
                             mMieScattering * mSkyBrightness, mMieScattering4PI );

   Point4F invWavelength(  1.0f / mWavelength4[0], 
                           1.0f / mWavelength4[1], 
                           1.0f / mWavelength4[2], 1.0f );

   Point3F camPos( 0, 0, smViewerHeight );
   Point4F miscParams( camPos.z, camPos.z * camPos.z, mScale, mScale / mRayleighScaleDepth );  

   Frustum frust = state->getFrustum();
   frust.setFarDist( smEarthRadius + smAtmosphereRadius );
   MatrixF proj( true );
   frust.getProjectionMatrix( &proj );

   MatrixF camMat = state->getCameraTransform();
   camMat.inverse();
   MatrixF tmp( true );
   tmp = camMat;
   tmp.setPosition( Point3F( 0, 0, 0 ) );

   proj.mul( tmp );

   mShaderConsts->set( mModelViewProjSC, proj );
   mShaderConsts->set( mMiscSC, miscParams );
   mShaderConsts->set( mSphereRadiiSC, sphereRadii );
   mShaderConsts->set( mScatteringCoefficientsSC, scatteringCoeffs );
   mShaderConsts->set( mCamPosSC, camPos );
   mShaderConsts->set( mLightDirSC, mLightDir );
   mShaderConsts->set( mPixLightDirSC, mLightDir );
   mShaderConsts->set( mNightColorSC, mNightColor );
   mShaderConsts->set( mInverseWavelengthSC, invWavelength );
   mShaderConsts->set( mNightInterpolantAndExposureSC, Point2F( mExposure, mNightInterpolant ) );

   if ( GFXDevice::getWireframe() )
   {
      GFXStateBlockDesc desc( mStateBlock->getDesc() );
      desc.setFillModeWireframe();
      GFX->setStateBlockByDesc( desc );
   }
   else
      GFX->setStateBlock( mStateBlock );
   
   if ( mUseNightCubemap && mNightCubemap )
   {
      mShaderConsts->set( mUseCubemapSC, 1.0f );

      if ( !mNightCubemap->mCubemap )
         mNightCubemap->createMap();

      GFX->setCubeTexture( 0, mNightCubemap->mCubemap );      
   }      
   else
   {
      GFX->setCubeTexture( 0, NULL ); 
      mShaderConsts->set( mUseCubemapSC, 0.0f );
   }

	GFX->setPrimitiveBuffer( mPrimBuffer );
   GFX->setVertexBuffer( mVB );

   GFX->drawIndexedPrimitive( GFXTriangleStrip, 0, 0, mVertCount, 0, mPrimCount );
}

void ScatterSky::_debugRender( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{
   GFXStateBlockDesc desc;
   desc.fillMode = GFXFillSolid;
   desc.setBlend( false, GFXBlendOne, GFXBlendZero );
   desc.setZReadWrite( false, false );
   GFXStateBlockRef sb = GFX->GFX->createStateBlock( desc );

   GFX->setStateBlock( sb );

   PrimBuild::begin( GFXLineStrip, mSkyPoints.size() );
   PrimBuild::color3i( 255, 0, 255 );

   for ( U32 i = 0; i < mSkyPoints.size(); i++ )
   {
      Point3F pnt = mSkyPoints[i];
      pnt.normalize();
      pnt *= 500;
      pnt += state->getCameraPosition();
      PrimBuild::vertex3fv( pnt );
   }

   PrimBuild::end();
}

void ScatterSky::_renderMoon( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{   
   Point3F moonlightPosition = state->getCameraPosition() - /*mLight->getDirection()*/ mMoonLightDir * state->getFarPlane() * 0.5f;

   // Calculate Billboard Radius (in world units) to be constant, independent of distance.
   // Takes into account distance, viewport size, and specified size in editor
   F32 BBRadius = (((moonlightPosition - state->getCameraPosition()).len()) / (GFX->getViewport().extent.x / 640.0)) / 2;  
   BBRadius *= mMoonScale;

   GFXTransformSaver saver;

   if ( state->isReflectPass() )
      GFX->setProjectionMatrix( gClientSceneGraph->getNonClipProjection() );

   GFX->setStateBlock(mMoonSB);

   // Initialize points with basic info
   Point3F points[4];
   points[0] = Point3F(-BBRadius, 0.0, -BBRadius);
   points[1] = Point3F( BBRadius, 0.0, -BBRadius);
   points[2] = Point3F( BBRadius, 0.0,  BBRadius);
   points[3] = Point3F(-BBRadius, 0.0,  BBRadius);

   // Get info we need to adjust points
   MatrixF camView = GFX->getWorldMatrix();
   camView.inverse();

   // Finalize points
   for(int i = 0; i < 4; i++)
   {
      // align with camera
      camView.mulV(points[i]);
      // offset
      points[i] += moonlightPosition;
   }

   // Draw it

   ColorF moonVertColor;
   moonVertColor.set( 1.0f, 1.0f, 1.0f, mNightInterpolant );
   PrimBuild::color( moonVertColor );

   GFX->setTexture(0, mMoonTexture);

   PrimBuild::begin( GFXTriangleFan, 4 );
   PrimBuild::texCoord2f(0, 0);
   PrimBuild::vertex3fv(points[0]);
   PrimBuild::texCoord2f(1, 0);
   PrimBuild::vertex3fv(points[1]);
   PrimBuild::texCoord2f(1, 1);
   PrimBuild::vertex3fv(points[2]);
   PrimBuild::texCoord2f(0, 1);
   PrimBuild::vertex3fv(points[3]);
   PrimBuild::end();
}

void ScatterSky::_generateSkyPoints()
{
   U32 rings=60, segments=20;//rings=160, segments=20; 

   Point3F tmpPoint( 0, 0, 0 );

   // Establish constants used in sphere generation.
   F32 deltaRingAngle = ( M_PI_F / (F32)(rings * 2) );
   F32 deltaSegAngle = ( 2.0f * M_PI_F / (F32)segments );

   // Generate the group of rings for the sphere.
   for( int ring = 0; ring < 2; ring++ )
   {
      F32 r0 = mSin( ring * deltaRingAngle );
      F32 y0 = mCos( ring * deltaRingAngle ); 

      // Generate the group of segments for the current ring.
      for( int seg = 0; seg < segments + 1 ; seg++ )
      {
         F32 x0 = r0 * sinf( seg * deltaSegAngle );
         F32 z0 = r0 * cosf( seg * deltaSegAngle );

         tmpPoint.set( x0, z0, y0 );
         tmpPoint.normalizeSafe();

         tmpPoint.x *= smEarthRadius + smAtmosphereRadius;
         tmpPoint.y *= smEarthRadius + smAtmosphereRadius;
         tmpPoint.z *= smEarthRadius + smAtmosphereRadius;
         tmpPoint.z -= smEarthRadius;

         if ( ring == 1 )
            mSkyPoints.push_back( tmpPoint );
      }
   }
}

void ScatterSky::_interpolateColors()
{
   mFogColor.set( 0, 0, 0, 0 );
   mAmbientColor.set( 0, 0, 0, 0 );
   mSunColor.set( 0, 0, 0, 0 );

   _getFogColor( &mFogColor );   
   _getAmbientColor( &mAmbientColor );
   _getSunColor( &mSunColor );

   mAmbientColor *= mAmbientScale;
   mSunColor *= mSunScale;

   mFogColor.interpolate( mFogColor, mNightColor, mNightInterpolant );   
   mFogColor.alpha = 1.0f;

   mAmbientColor.interpolate( mAmbientColor, mNightColor, mNightInterpolant );
   mSunColor.interpolate( mSunColor, mMoonTint, mNightInterpolant );
}

void ScatterSky::_getSunColor( ColorF *outColor )
{
   PROFILE_SCOPE( ScatterSky_GetSunColor );

   U32 count = 0;
   ColorF tmpColor( 0, 0, 0 );
   VectorF tmpVec( 0, 0, 0 );
   
   tmpVec = mLightDir;
   tmpVec.x *= smEarthRadius + smAtmosphereRadius;
   tmpVec.y *= smEarthRadius + smAtmosphereRadius;
   tmpVec.z *= smEarthRadius + smAtmosphereRadius;
   tmpVec.z -= smAtmosphereRadius;

   for ( U32 i = 0; i < 10; i++ )
   {
      _getColor( tmpVec, &tmpColor );
      (*outColor) += tmpColor;
      tmpVec.x += (smEarthRadius * 0.5f) + (smAtmosphereRadius * 0.5f);
      count++;
   }
   
   if ( count > 0 )
      (*outColor) /= count;
}

void ScatterSky::_getAmbientColor( ColorF *outColor )
{
   PROFILE_SCOPE( ScatterSky_GetAmbientColor );

   ColorF tmpColor( 0, 0, 0, 0 );
   U32 count = 0;

   // Disable mieScattering for purposes of calculating the ambient color.
   F32 oldMieScattering = mMieScattering;
   mMieScattering = 0.0f;

   for ( U32 i = 0; i < mSkyPoints.size(); i++ )
   {
      Point3F pnt( mSkyPoints[i] );

      _getColor( pnt, &tmpColor );
      (*outColor) += tmpColor;
      count++;
   }

   if ( count > 0 )
      (*outColor) /= count;
   //Point3F pColor( outColor->red, outColor->green, outColor->blue );
   //F32 len = pColor.len();
   //if ( len > 0 )
   //   (*outColor) /= len;

   mMieScattering = oldMieScattering;
}

void ScatterSky::_getFogColor( ColorF *outColor )
{
   PROFILE_SCOPE( ScatterSky_GetFogColor );

   VectorF scatterPos( 0, 0, 0 );

   F32 sunBrightness = mSkyBrightness;
   mSkyBrightness *= 0.25f;

   F32 yaw = 0, pitch = 0, originalYaw = 0;
   VectorF fwd( 0, 1.0f, 0 );
   MathUtils::getAnglesFromVector( fwd, yaw, pitch );
   originalYaw = yaw;
   pitch = mDegToRad( 10.0f );

   ColorF tmpColor( 0, 0, 0 );

   U32 i = 0;
   for ( i = 0; i < 10; i++ )
   {
      MathUtils::getVectorFromAngles( scatterPos, yaw, pitch );

      scatterPos.x *= smEarthRadius + smAtmosphereRadius;
      scatterPos.y *= smEarthRadius + smAtmosphereRadius;
      scatterPos.z *= smEarthRadius + smAtmosphereRadius;
      scatterPos.y -= smEarthRadius;

      _getColor( scatterPos, &tmpColor );
      (*outColor) += tmpColor;

      if ( i <= 5 )
         yaw += mDegToRad( 5.0f );
      else
      {
         originalYaw += mDegToRad( -5.0f );
         yaw = originalYaw;
      }

      yaw = mFmod( yaw, M_2PI_F );
   }

   if ( i > 0 )
      (*outColor) /= i;

   mSkyBrightness = sunBrightness;
}

F32 ScatterSky::_vernierScale( F32 fCos )
{
   /*
   F32 x5 = x * 5.25;
   F32 x5p6 = (-6.80 + x5);
   F32 xnew = (3.83 + x * x5p6);
   F32 xfinal = (0.459 + x * xnew);
   F32 xfinal2 = -0.00287 + x * xfinal;
   F32 outx = mExp( xfinal2 ); 
   return 0.25 * outx;*/
   F32 x = 1.0 - fCos;
   return 0.25f * exp( -0.00287f + x * (0.459f + x * (3.83f + x * ((-6.80f + (x * 5.25f))))) ); 
}

F32 ScatterSky::_getMiePhase( F32 fCos, F32 fCos2, F32 g, F32 g2)
{
   return 1.5f * ((1.0f - g2) / (2.0f + g2)) * (1.0f + fCos2) / mPow(mFabs(1.0f + g2 - 2.0f*g*fCos), 1.5f);
}

F32 ScatterSky::_getRayleighPhase( F32 fCos2 )
{
   return 0.75 + 0.75 * fCos2;
}

void ScatterSky::_getColor( const Point3F &pos, ColorF *outColor )
{
   PROFILE_SCOPE( ScatterSky_GetColor );

   F32 scaleOverScaleDepth = mScale / mRayleighScaleDepth;
   F32 rayleighBrightness = mRayleighScattering * mSkyBrightness;
   F32 mieBrightness = mMieScattering * mSkyBrightness;

   Point3F invWaveLength(  1.0f / mWavelength4[0], 
                           1.0f / mWavelength4[1], 
                           1.0f / mWavelength4[2] );

   Point3F v3Pos = pos / 6378000.0f;
   v3Pos.z += mSphereInnerRadius;
   
   Point3F newCamPos( 0, 0, smViewerHeight );

   VectorF v3Ray = v3Pos - newCamPos;
   F32 fFar = v3Ray.len();
   v3Ray / fFar;
   v3Ray.normalizeSafe();

   Point3F v3Start = newCamPos;
   F32 fDepth = mExp( scaleOverScaleDepth * (mSphereInnerRadius - smViewerHeight ) );
   F32 fStartAngle = mDot( v3Ray, v3Start );

   F32 fStartOffset = fDepth * _vernierScale( fStartAngle );

   F32 fSampleLength = fFar / 2.0f;
   F32 fScaledLength = fSampleLength * mScale;
   VectorF v3SampleRay = v3Ray * fSampleLength;
   Point3F v3SamplePoint = v3Start + v3SampleRay * 0.5f;

   Point3F v3FrontColor( 0, 0, 0 );
   for ( U32 i = 0; i < 2; i++ )
   {
      F32 fHeight = v3SamplePoint.len();
      F32 fDepth = mExp( scaleOverScaleDepth * (mSphereInnerRadius - smViewerHeight) );
      F32 fLightAngle = mDot( mLightDir, v3SamplePoint ) / fHeight;
      F32 fCameraAngle = mDot( v3Ray, v3SamplePoint ) / fHeight;

      F32 fScatter = (fStartOffset + fDepth * ( _vernierScale( fLightAngle ) - _vernierScale( fCameraAngle ) ));
      Point3F v3Attenuate( 0, 0, 0 );
      
      F32 tmp = mExp( -fScatter * (invWaveLength[0] * mRayleighScattering4PI + mMieScattering4PI) );
      v3Attenuate.x = tmp;
      
      tmp = mExp( -fScatter * (invWaveLength[1] * mRayleighScattering4PI + mMieScattering4PI) );
      v3Attenuate.y = tmp;

      tmp = mExp( -fScatter * (invWaveLength[2] * mRayleighScattering4PI + mMieScattering4PI) );
      v3Attenuate.z = tmp;
      
      v3FrontColor += v3Attenuate * (fDepth * fScaledLength);
      v3SamplePoint += v3SampleRay;
   }

   Point3F mieColor = v3FrontColor * mieBrightness;
   Point3F rayleighColor = v3FrontColor * (invWaveLength * rayleighBrightness);
   Point3F v3Direction = newCamPos - v3Pos;
   v3Direction.normalize();

   F32 fCos = mDot( mLightDir, v3Direction ) / v3Direction.len();
   F32 fCos2 = fCos * fCos;

   F32 g = -0.991f;
   F32 g2 = g * g;
   F32 miePhase = _getMiePhase( fCos, fCos2, g, g2 );
   //F32 rayleighPhase = _getRayleighPhase( fCos2 );

   Point3F color = rayleighColor + (miePhase * mieColor);
   ColorF tmp( color.x, color.y, color.z, color.y );

   //if ( !tmp.isValidColor() )
   //{
   //   F32 len = color.len();
   //   if ( len > 0 )
   //      color /= len;
   //}

   Point3F expColor( 0, 0, 0 );
   expColor.x = 1.0f - exp(-mExposure * color.x);
   expColor.y = 1.0f - exp(-mExposure * color.y);
   expColor.z = 1.0f - exp(-mExposure * color.z);

   tmp.set( expColor.x, expColor.y, expColor.z, 1.0f );

   if ( !tmp.isValidColor() )
   {
      F32 len = expColor.len();
      if ( len > 0 )
         expColor /= len;
   }

   outColor->set( expColor.x, expColor.y, expColor.z, 1.0f );
}

// Static protected field set methods

bool ScatterSky::ptSetElevation( void *obj, const char *data )
{
   ScatterSky *sky = static_cast<ScatterSky*>( obj );
   F32 val = dAtof( data );

   sky->setElevation( val );

   // we already set the field
   return false;
}

bool ScatterSky::ptSetAzimuth( void *obj, const char *data )
{
   ScatterSky *sky = static_cast<ScatterSky*>( obj );
   F32 val = dAtof( data );

   sky->setAzimuth( val );

   // we already set the field
   return false;
}

// ConsoleMethods

ConsoleMethod( ScatterSky, applyChanges, void, 2, 2, "Apply a full network update of all fields to all clients." )
{
   object->inspectPostApply();
}