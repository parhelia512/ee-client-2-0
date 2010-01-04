//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "environment/sun.h"

#include "gfx/bitmap/gBitmap.h"
#include "math/mathIO.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "math/mathUtils.h"
#include "lighting/lightInfo.h"
#include "lighting/lightManager.h"
#include "sceneGraph/sceneState.h"
#include "renderInstance/renderPassManager.h"
#include "sim/netConnection.h"
#include "environment/timeOfDay.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/primBuilder.h"

IMPLEMENT_CO_NETOBJECT_V1(Sun);

//-----------------------------------------------------------------------------

Sun::Sun()
{
   mNetFlags.set(Ghostable | ScopeAlways);
   mTypeMask = EnvironmentObjectType | LightObjectType;

   mLightColor.set(0.7f, 0.7f, 0.7f);
   mLightAmbient.set(0.3f, 0.3f, 0.3f);
   mBrightness = 1.0f;
   mSunAzimuth = 0.0f;
   mSunElevation = 35.0f;
   mCastShadows = true;

   mAnimateSun = false;
   mTotalTime = 0.0f;
   mCurrTime = 0.0f;
   mStartAzimuth = 0.0f;
   mEndAzimuth = 0.0f;
   mStartElevation = 0.0f;
   mEndElevation = 0.0f;

   mLight = LightManager::createLightInfo();
   mLight->setType( LightInfo::Vector );

   mFlareData = NULL;
   mFlareState.clear();
   mFlareScale = 1.0f;

   mCoronaEnabled = true;
   mCoronaScale = 1.0f;
   mCoronaTint.set( 1.0f, 1.0f, 1.0f, 1.0f );
   mCoronaUseLightColor = true;
}

Sun::~Sun()
{
   SAFE_DELETE( mLight );
}

bool Sun::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   // Register as listener to TimeOfDay update events
   TimeOfDay::getTimeOfDayUpdateSignal().notify( this, &Sun::_updateTimeOfDay );

	// Make this thing have a global bounds so that its 
   // always returned from spatial light queries.
	setGlobalBounds();
	resetWorldBox();
	setRenderTransform( mObjToWorld );
	addToScene();

   _initCorona();

   // Update the light parameters.
   _conformLights();

   return true;
}

void Sun::onRemove()
{   
   TimeOfDay::getTimeOfDayUpdateSignal().remove( this, &Sun::_updateTimeOfDay );

   removeFromScene();
   Parent::onRemove();
}

void Sun::initPersistFields()
{
   addGroup( "Orbit" );

   addField( "azimuth", TypeF32, Offset( mSunAzimuth, Sun ), 
      "The horizontal angle of the sun measured clockwise from the positive Y world axis." );

   addField( "elevation", TypeF32, Offset( mSunElevation, Sun ),
      "The elevation angle of the sun above or below the horizon." );

   endGroup( "Orbit" );	

   // We only add the basic lighting options that all lighting
   // systems would use... the specific lighting system options
   // are injected at runtime by the lighting system itself.

   addGroup( "Lighting" );

   addField( "color", TypeColorF, Offset( mLightColor, Sun ), "Color shading applied to surfaces in "
      "direct contact with light source.");
   addField( "ambient", TypeColorF, Offset( mLightAmbient, Sun ), "Color shading applied to surfaces not "
      "in direct contact with light source, such as in the shadows or interiors.");       
   addField( "brightness", TypeF32, Offset( mBrightness, Sun ), "Adjust the Sun's global contrast/intensity");      
   addField( "castShadows", TypeBool, Offset( mCastShadows, Sun ), "Enables/disables shadows cast by "
      "objects due to Sun light");      

   endGroup( "Lighting" );

   addGroup( "Corona" );

   addField( "coronaEnabled", TypeBool, Offset( mCoronaEnabled, Sun ) );
   addField( "coronaTexture", TypeImageFilename, Offset( mCoronaTextureName, Sun ) );
   addField( "coronaScale", TypeF32, Offset( mCoronaScale, Sun ) );
   addField( "coronaTint", TypeColorF, Offset( mCoronaTint, Sun ) );
   addField( "coronaUseLightColor", TypeBool, Offset( mCoronaUseLightColor, Sun ) );

   endGroup( "Corona" );


   addGroup( "Misc" );

   addField( "flareType", TypeLightFlareDataPtr, Offset( mFlareData, Sun ), "Datablock for the flare and corona produced "
      "by the Sun");
   addField( "flareScale", TypeF32, Offset( mFlareScale, Sun ), "Changes the size and intensity of the flare" );

   endGroup( "Misc" );

   // Now inject any light manager specific fields.
   LightManager::initLightFields();

   Parent::initPersistFields();
}

void Sun::inspectPostApply()
{
   _conformLights();
   setMaskBits(UpdateMask);
}

U32 Sun::packUpdate(NetConnection *conn, U32 mask, BitStream *stream )
{
   Parent::packUpdate( conn, mask, stream );

   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      stream->write( mSunAzimuth );
      stream->write( mSunElevation );
      stream->write( mLightColor );
      stream->write( mLightAmbient );
      stream->write( mBrightness );      
      stream->writeFlag( mCastShadows );
      stream->write( mFlareScale );

      if ( stream->writeFlag( mFlareData ) )
      {
         stream->writeRangedU32( mFlareData->getId(),
            DataBlockObjectIdFirst, 
            DataBlockObjectIdLast );
      }

      stream->writeFlag( mCoronaEnabled );
      stream->write( mCoronaTextureName );
      stream->write( mCoronaScale );
      stream->write( mCoronaTint );
      stream->writeFlag( mCoronaUseLightColor );

      mLight->packExtended( stream ); 
   }

   return 0;
}

void Sun::unpackUpdate( NetConnection *conn, BitStream *stream )
{
   Parent::unpackUpdate( conn, stream );

   if ( stream->readFlag() ) // UpdateMask
   {
      stream->read( &mSunAzimuth );
      stream->read( &mSunElevation );
      stream->read( &mLightColor );
      stream->read( &mLightAmbient );
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
            conn->setLastError( "Sun::unpackUpdate() - invalid LightFlareData!" );
            mFlareData = NULL;
         }
      }
      else
         mFlareData = NULL;

      mCoronaEnabled = stream->readFlag();
      stream->read( &mCoronaTextureName );
      stream->read( &mCoronaScale );
      stream->read( &mCoronaTint );
      mCoronaUseLightColor = stream->readFlag();

      mLight->unpackExtended( stream ); 
   }

   if ( isProperlyAdded() )
   {
      _initCorona();
      _conformLights();
   }
}

void Sun::submitLights( LightManager *lm, bool staticLighting )
{
   // The sun is a special light and needs special registration.
   lm->setSpecialLight( LightManager::slSunLightType, mLight );
}


void Sun::advanceTime( F32 timeDelta )
{
   if (mAnimateSun)
   {
      if (mCurrTime >= mTotalTime)
      {
         mAnimateSun = false;
         mCurrTime = 0.0f;
      }
      else
      {
         mCurrTime += timeDelta;

         F32 fract   = mCurrTime / mTotalTime;
         F32 inverse = 1.0f - fract;

         F32 newAzimuth   = mStartAzimuth * inverse + mEndAzimuth * fract;
         F32 newElevation = mStartElevation * inverse + mEndElevation * fract;

         if (newAzimuth > 360.0f)
            newAzimuth -= 360.0f;
         if (newElevation > 360.0f)
            newElevation -= 360.0f;

         setAzimuth(newAzimuth);
         setElevation(newElevation);
      }
   }
}


bool Sun::prepRenderImage( SceneState *state, const U32 stateKey, const U32, const bool )
{
   if ( isLastState( state, stateKey ) )
      return false;

   setLastState( state, stateKey );

   if ( !state->isObjectRendered( this ) || 
      !(state->isDiffusePass() || state->isReflectPass()) )
      return false;

   // Render instance for Corona effect.   
   if ( mCoronaEnabled && mCoronaTexture.isValid() )
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &Sun::_renderCorona );
      ri->type = RenderPassManager::RIT_Sky;      
      // Render after sky objects and before CloudLayer!
      ri->defaultKey = 5;
      ri->defaultKey2 = 0;
      state->getRenderPass()->addInst( ri );
   }

   // LightFlareData handles rendering flare effects.
   if ( mFlareData )
   {
      mFlareState.fullBrightness = mBrightness;
      mFlareState.scale = mFlareScale;
      mFlareState.lightInfo = mLight;

      Point3F lightPos = state->getCameraPosition() - state->getFarPlane() * mLight->getDirection() * 0.9f;
      mFlareState.lightMat.identity();
      mFlareState.lightMat.setPosition( lightPos );
      
      F32 dist = ( lightPos - state->getCameraPosition() ).len();
      F32 radius = ( dist / ( GFX->getViewport().extent.x / 640.0 ) ) / 2;  
      radius *= mCoronaScale;
      radius = ( radius / dist ) * state->getWorldToScreenScale().y;

      mFlareState.worldRadius = radius;

      mFlareData->prepRender( state, &mFlareState );
   }

   return false;
}

void Sun::setAzimuth( F32 azimuth )
{
   mSunAzimuth = azimuth;
   _conformLights();
   setMaskBits( UpdateMask ); // TODO: Break out the masks to save bandwidth!
}

void Sun::setElevation( F32 elevation )
{
   mSunElevation = elevation;
   _conformLights();
   setMaskBits( UpdateMask ); // TODO: Break out the masks to save some space!
}

void Sun::setColor( const ColorF &color )
{
   mLightColor = color;
   _conformLights();
   setMaskBits( UpdateMask ); // TODO: Break out the masks to save some space!
}

void Sun::animate( F32 duration, F32 startAzimuth, F32 endAzimuth, F32 startElevation, F32 endElevation )
{
   mAnimateSun = true;
   mCurrTime = 0.0f;

   mTotalTime = duration;

   mStartAzimuth = startAzimuth;
   mEndAzimuth = endAzimuth;
   mStartElevation = startElevation;
   mEndElevation = endElevation;
}

void Sun::_conformLights()
{
   // Build the light direction from the azimuth and elevation.
   F32 yaw = mDegToRad(mClampF(mSunAzimuth,0,359));
   F32 pitch = mDegToRad(mClampF(mSunElevation,-360,+360));
   VectorF lightDirection;
   MathUtils::getVectorFromAngles(lightDirection, yaw, pitch);
   lightDirection.normalize();
   mLight->setDirection( -lightDirection );
   mLight->setBrightness( mBrightness );

   // Now make sure the colors are within range.
   mLightColor.clamp();
   mLight->setColor( mLightColor );
   mLightAmbient.clamp();
   mLight->setAmbient( mLightAmbient );

   // Optimization... disable shadows if the ambient and 
   // directional color are the same.
   bool castShadows = mLightColor != mLightAmbient && mCastShadows; 
   mLight->setCastShadows( castShadows );
}

void Sun::_initCorona()
{
   if ( isServerObject() )
      return;
      
   // Load texture...

   if ( mCoronaTextureName.isNotEmpty() )      
      mCoronaTexture.set( mCoronaTextureName, &GFXDefaultStaticDiffuseProfile, "CoronaTexture" );            

   // Make stateblock...

   if ( mCoronaSB.isNull() )
   {
      GFXStateBlockDesc desc;
      desc.setCullMode( GFXCullNone );
      desc.setAlphaTest( true, GFXCmpGreaterEqual, 1 );
      desc.setZReadWrite( false, false );
      desc.setBlend( true, GFXBlendSrcColor, GFXBlendOne );
      desc.samplersDefined = true;
      desc.samplers[0].textureColorOp = GFXTOPModulate;
      desc.samplers[0].colorArg1 = GFXTATexture;
      desc.samplers[0].colorArg2 = GFXTADiffuse;
      desc.samplers[0].alphaOp = GFXTOPModulate;
      desc.samplers[0].alphaArg1 = GFXTATexture;
      desc.samplers[0].alphaArg2 = GFXTADiffuse;

      mCoronaSB = GFX->createStateBlock(desc);

      desc.setFillModeWireframe();
      mCoronaWireframeSB = GFX->createStateBlock(desc);
   }
}

void Sun::_renderCorona( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{   
   Point3F sunlightPosition = state->getCameraPosition() - mLight->getDirection() * state->getFarPlane() * 0.9f;

   // Calculate Billboard Radius (in world units) to be constant, independent of distance.
   // Takes into account distance, viewport size, and specified size in editor
   F32 BBRadius = (((sunlightPosition - state->getCameraPosition()).len()) / (GFX->getViewport().extent.x / 640.0)) / 2;  
   BBRadius *= mCoronaScale;

   GFXTransformSaver saver;

   if ( state->isReflectPass() )
      GFX->setProjectionMatrix( gClientSceneGraph->getNonClipProjection() );

   GFX->setStateBlock(mCoronaSB);

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
      points[i] += sunlightPosition;
   }

   // Draw it
   
   if ( mCoronaUseLightColor )
      PrimBuild::color(mLightColor);
   else
      PrimBuild::color(mCoronaTint);
   
   GFX->setTexture(0, mCoronaTexture);

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

void Sun::_updateTimeOfDay( TimeOfDay *timeOfDay, F32 time )
{
   setElevation( timeOfDay->getElevationDegrees() );
   setAzimuth( timeOfDay->getAzimuthDegrees() );
}

ConsoleMethod(Sun, apply, void, 2, 2, "")
{
   object->inspectPostApply();
}

ConsoleMethod(Sun, animate, void, 7, 7, "animate( F32 duration, F32 startAzimuth, F32 endAzimuth, F32 startElevation, F32 endElevation )")
{
   F32 duration = dAtof(argv[2]);
   F32 startAzimuth = dAtof(argv[3]);
   F32 endAzimuth   = dAtof(argv[4]);
   F32 startElevation = dAtof(argv[5]);
   F32 endElevation   = dAtof(argv[6]);

   object->animate(duration, startAzimuth, endAzimuth, startElevation, endElevation);
}

