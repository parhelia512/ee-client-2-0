//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "postFx/postEffect.h"

#include "postFx/postEffectManager.h"
#include "console/consoleTypes.h"
#include "math/util/frustum.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/sim/gfxStateBlockData.h"
#include "sceneGraph/sceneState.h"
#include "shaderGen/shaderGenVars.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "lighting/lightInfo.h"
#include "core/strings/stringUnit.h"
#include "materials/materialManager.h"
#include "materials/shaderData.h"
#include "math/mathUtils.h"
#include "postFx/postEffectVis.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/gfxDebugEvent.h"
#include "gfx/util/screenspace.h"
#include "core/stream/fileStream.h"

// For named buffers
#include "renderInstance/renderPrePassMgr.h"
#include "lighting/advanced/advancedLightBinManager.h"

static const EnumTable::Enums sPFXRenderTimeEnums[] =
{
   { PFXBeforeBin, "PFXBeforeBin" },
   { PFXAfterBin, "PFXAfterBin" },
   { PFXAfterDiffuse, "PFXAfterDiffuse" },
   { PFXEndOfFrame, "PFXEndOfFrame" },
   { PFXTexGenOnDemand, "PFXTexGenOnDemand" }
};

static const EnumTable sPFXRenderTimeTable(
   sizeof( sPFXRenderTimeEnums ) / sizeof( EnumTable::Enums ),
   sPFXRenderTimeEnums );

static const EnumTable::Enums sPFXTargetClearEnums[] =
{
   { PFXTargetClear_None, "PFXTargetClear_None" },
   { PFXTargetClear_OnCreate, "PFXTargetClear_OnCreate" },
   { PFXTargetClear_OnDraw, "PFXTargetClear_OnDraw" },
};

static const EnumTable sPFXTargetClearTable(
   sizeof( sPFXTargetClearEnums ) / sizeof( EnumTable::Enums ),
   sPFXTargetClearEnums );

static EnumTable::Enums gRequirementEnums[] =
{
   { 0, "None" },
   { PostEffect::RequiresDepth, "PrePassDepth" },
   { PostEffect::RequiresNormals, "PrePassNormal" },
   { PostEffect::RequiresNormals |
     PostEffect::RequiresDepth, "PrePassDepthAndNormal" },
   { PostEffect::RequiresLightInfo, "LightInfo" },
};
EnumTable gRequirementEnumTable(5, gRequirementEnums);

GFXImplementVertexFormat( PFXVertex )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float3, 1 );
};

GFX_ImplementTextureProfile(  PostFxTargetProfile,
                              GFXTextureProfile::DiffuseMap,
                              GFXTextureProfile::PreserveSize |
                              GFXTextureProfile::RenderTarget |
                              GFXTextureProfile::Pooled,
                              GFXTextureProfile::None );

IMPLEMENT_CONOBJECT(PostEffect);


GFX_ImplementTextureProfile( PostFxTextureProfile,
                            GFXTextureProfile::DiffuseMap,
                            GFXTextureProfile::Static | GFXTextureProfile::PreserveSize | GFXTextureProfile::NoMipmap,
                            GFXTextureProfile::None );


void PostEffect::EffectConst::set( const String &newVal )
{
   if ( mStringVal == newVal )
      return;

   mStringVal = newVal;
   mDirty = true;
}

void PostEffect::EffectConst::setToBuffer( GFXShaderConstBufferRef buff )
{
   // Nothing to do if the value hasn't changed.
   if ( !mDirty )
      return;
   mDirty = false;

   // If we don't have a handle... get it now.
   if ( !mHandle )
      mHandle = buff->getShader()->getShaderConstHandle( mName );

   // If the handle isn't valid then we're done.
   if ( !mHandle->isValid() )
      return;

   const GFXShaderConstType type = mHandle->getType();

   // For now, we're only going
   // to support float4 arrays.
   // Expand to other types as necessary.
   U32 arraySize = mHandle->getArraySize();

   const char *strVal = mStringVal.c_str();

   if ( type == GFXSCT_Float )
   {
      F32 val;
      Con::setData( TypeF32, &val, 0, 1, &strVal );
      buff->set( mHandle, val );
   }
   else if ( type == GFXSCT_Float2 )
   {
      Point2F val;
      Con::setData( TypePoint2F, &val, 0, 1, &strVal );
      buff->set( mHandle, val );
   }
   else if ( type == GFXSCT_Float3 )
   {
      Point3F val;
      Con::setData( TypePoint3F, &val, 0, 1, &strVal );
      buff->set( mHandle, val );
   }
   else
   {
      Point4F val;

      if ( arraySize > 1 )
      {
         // Do array setup!
         //U32 unitCount = StringUnit::getUnitCount( strVal, "\t" );
         //AssertFatal( unitCount == arraySize, "" );

         String tmpString;
         Vector<Point4F> valArray;

         for ( U32 i = 0; i < arraySize; i++ )
         {
            tmpString = StringUnit::getUnit( strVal, i, "\t" );
            valArray.increment();
            const char *tmpCStr = tmpString.c_str();

            Con::setData( TypePoint4F, &valArray.last(), 0, 1, &tmpCStr );
         }

         AlignedArray<Point4F> rectData( valArray.size(), sizeof( Point4F ), (U8*)valArray.address(), false );
         buff->set( mHandle, rectData );
      }
      else
      {
         // Do regular setup.
         Con::setData( TypePoint4F, &val, 0, 1, &strVal );
         buff->set( mHandle, val );
      }
   }
}


//-------------------------------------------------------------------------
// PostEffect
//-------------------------------------------------------------------------

PostEffect::PostEffect()
   :  mRenderTime( PFXAfterDiffuse ),
      mRenderPriority( 1.0 ),
      mEnabled( false ),
      mSkip( false ),
      mIsNamedTarget( false ),
      mUpdateShader( true ),
      mStateBlockData( NULL ),
      mAllowReflectPass( false ),
      mTargetClear( PFXTargetClear_None ),
      mTargetScale( Point2F::One ),
      mTargetSize( Point2I::Zero ),
      mTargetFormat( GFXFormatR8G8B8A8 ),
      mTargetClearColor( ColorF::BLACK ),
      mOneFrameOnly( false ),
      mOnThisFrame( true ),
      mShaderReloadKey( 0 ),
      mPostEffectRequirements( U32_MAX ),
      mRequirementsMet( true ),
      mRTSizeSC( NULL ),
      mOneOverRTSizeSC( NULL ),
      mViewportOffsetSC( NULL ),
      mFogDataSC( NULL ),
      mFogColorSC( NULL ),
      mEyePosSC( NULL ),
      mMatWorldToScreenSC( NULL ),
      mMatScreenToWorldSC( NULL ),
      mMatPrevScreenToWorldSC( NULL ),
      mNearFarSC( NULL ),
      mInvNearFarSC( NULL ),
      mWorldToScreenScaleSC( NULL ),
      mWaterColorSC( NULL ),
      mWaterFogDataSC( NULL ),
      mAmbientColorSC( NULL ),
      mWaterFogPlaneSC( NULL ),
      mScreenSunPosSC( NULL ),
      mLightDirectionSC( NULL ),
      mCameraForwardSC( NULL ),
      mAccumTimeSC( NULL ),
      mDeltaTimeSC( NULL ),
      mInvCameraMatSC( NULL )
{
   dMemset( mActiveTextures, 0, sizeof( GFXTextureObject* ) * NumTextures );
   dMemset( mActiveNamedTarget, 0, sizeof( MatTextureTarget* ) * NumTextures );
   dMemset( mTexSizeSC, 0, sizeof( GFXShaderConstHandle* ) * NumTextures );
   dMemset( mTexSizeSC, 0, sizeof( GFXShaderConstHandle* ) * NumTextures );
}

PostEffect::~PostEffect()
{
   EffectConstTable::Iterator iter = mEffectConsts.begin();
   for ( ; iter != mEffectConsts.end(); iter++ )
      delete iter->value;
}

void PostEffect::initPersistFields()
{
   addField( "shader", TypeRealString, Offset( mShaderName, PostEffect ) );

   addField( "stateBlock", TypeSimObjectPtr, Offset( mStateBlockData,  PostEffect ) );

   addField( "target", TypeRealString, Offset( mTargetName, PostEffect ) );
   
   addField( "targetScale", TypePoint2F, Offset( mTargetScale, PostEffect ),
       "If targetSize is zero this is used to set a relative size from the current target." );
       
   addField( "targetSize", TypePoint2I, Offset( mTargetSize, PostEffect ), 
      "If non-zero this is used as the absolute target size." );   
      
   addField( "targetFormat", TypeEnum, Offset( mTargetFormat, PostEffect ), 1, &gTextureFormatEnumTable );
   addField( "targetClearColor", TypeColorF, Offset( mTargetClearColor, PostEffect ) );
   addField( "targetClear", TypeEnum, Offset( mTargetClear, PostEffect ), 1, &sPFXTargetClearTable );

   addField( "texture", TypeImageFilename, Offset( mTexFilename, PostEffect ), NumTextures );

   addField( "renderTime", TypeEnum, Offset( mRenderTime, PostEffect ), 1, &sPFXRenderTimeTable );
   addField( "renderBin", TypeRealString, Offset( mRenderBin, PostEffect ) );
   addField( "renderPriority", TypeF32, Offset( mRenderPriority, PostEffect ), 
      "PostEffects are processed in DESCENDING order of renderPriority if more than one has the same renderBin/Time." );

   addField( "allowReflectPass", TypeBool, Offset( mAllowReflectPass, PostEffect ) );

   addProtectedField( "isEnabled", TypeBool, Offset( mEnabled, PostEffect),
      &PostEffect::_setIsEnabled, &defaultProtectedGetFn,
      "Toggles the effect on and off." );

   addField( "onThisFrame", TypeBool, Offset( mOnThisFrame, PostEffect ), "Allows you to turn on a posteffect for only a single frame." );
   addField( "oneFrameOnly", TypeBool, Offset( mOneFrameOnly, PostEffect ), "Allows you to turn on a posteffect for only a single frame." );

   addField( "skip", TypeBool, Offset( mSkip, PostEffect ), "Skip processing of this PostEffect and its children even if its parent is enabled. Parent and sibling PostEffects in the chain are still processed." );

   addField( "requirements", TypeEnum, Offset( mPostEffectRequirements, PostEffect ), 1, &gRequirementEnumTable );

   Parent::initPersistFields();
}

bool PostEffect::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   LightManager::smActivateSignal.notify( this, &PostEffect::_onLMActivate );
   mUpdateShader = true;

   // Grab the script path.
   Torque::Path scriptPath( Con::getVariable( "$Con::File" ) );
   scriptPath.setFileName( String::EmptyString );
   scriptPath.setExtension( String::EmptyString );

   // Find additional textures
   for( int i = 0; i < NumTextures; i++ )
   {
      String texFilename = mTexFilename[i];

      // Skip empty stages or ones with variable or target names.
      if (  texFilename.isEmpty() ||
            texFilename[0] == '$' ||
            texFilename[0] == '#' )
         continue;

      // If '/', then path is specified, open normally
      if ( texFilename[0] != '/' )
         texFilename = scriptPath.getFullPath() + '/' + texFilename;

      // Try to load the texture.
      mTextures[i].set( texFilename, &PostFxTextureProfile, avar( "%s() - (line %d)", __FUNCTION__, __LINE__ ) );
   }

   // Is the target a named target?
   if ( mTargetName.isNotEmpty() && mTargetName[0] == '#' )
   {
      mIsNamedTarget = true;
      MatTextureTarget::registerTarget( mTargetName.substr( 1 ), this );
      GFXTextureManager::addEventDelegate( this, &PostEffect::_onTextureEvent );
   }
   else
      mIsNamedTarget = false;

   // Call onAdd in script
   Con::executef(this, "onAdd", Con::getIntArg(getId()));

   // Should we start enabled?
   if ( mEnabled )
   {
      mEnabled = false;
      enable();
   }

   return true;
}

void PostEffect::onRemove()
{
   Parent::onRemove();

   PFXMGR->_removeEffect( this );

   LightManager::smActivateSignal.remove( this, &PostEffect::_onLMActivate );

   mShader = NULL;
   _cleanTargets();

   if ( mIsNamedTarget )
   {
      GFXTextureManager::removeEventDelegate( this, &PostEffect::_onTextureEvent );
      MatTextureTarget::unregisterTarget( mTargetName.substr( 1 ), this );
   }
}

void PostEffect::_updateScreenGeometry(   const Frustum &frustum,
                                          GFXVertexBufferHandle<PFXVertex> *outVB )
{
   outVB->set( GFX, 4, GFXBufferTypeVolatile );

   const Point3F *frustumPoints = frustum.getPoints();

   PFXVertex *vert = outVB->lock();

   vert->point.set( -1.0, -1.0, 0.0 );
   vert->texCoord.set( 0.0f, 1.0f );
   vert->wsEyeRay = frustumPoints[Frustum::FarBottomLeft] - frustumPoints[ Frustum::CameraPosition ];
   vert++;

   vert->point.set( -1.0, 1.0, 0.0 );
   vert->texCoord.set( 0.0f, 0.0f );
   vert->wsEyeRay = frustumPoints[Frustum::FarTopLeft] - frustumPoints[ Frustum::CameraPosition ];
   vert++;

   vert->point.set( 1.0, 1.0, 0.0 );
   vert->texCoord.set( 1.0f, 0.0f );
   vert->wsEyeRay = frustumPoints[Frustum::FarTopRight] - frustumPoints[ Frustum::CameraPosition ];
   vert++;

   vert->point.set( 1.0, -1.0, 0.0 );
   vert->texCoord.set( 1.0f, 1.0f );
   vert->wsEyeRay = frustumPoints[Frustum::FarBottomRight] - frustumPoints[ Frustum::CameraPosition ];
   vert++;

   outVB->unlock();
}

void PostEffect::_setupStateBlock( const SceneState *state )
{
   if ( mStateBlock.isNull() )
   {
      GFXStateBlockDesc desc;
      if ( mStateBlockData )
         desc = mStateBlockData->getState();

      mStateBlock = GFX->createStateBlock( desc );
   }

   GFX->setStateBlock( mStateBlock );
}

void PostEffect::_setupConstants( const SceneState *state )
{
   // Alloc the const buffer.
   if ( mShaderConsts.isNull() )
   {
      mShaderConsts = mShader->allocConstBuffer();

      mRTSizeSC = mShader->getShaderConstHandle( "$targetSize" );
      mOneOverRTSizeSC = mShader->getShaderConstHandle( "$oneOverTargetSize" );

      mTexSizeSC[0] = mShader->getShaderConstHandle( "$texSize0" );
      mTexSizeSC[1] = mShader->getShaderConstHandle( "$texSize1" );
      mTexSizeSC[2] = mShader->getShaderConstHandle( "$texSize2" );
      mTexSizeSC[3] = mShader->getShaderConstHandle( "$texSize3" );
      mRenderTargetParamsSC[0] = mShader->getShaderConstHandle( "$rtParams0" );
      mRenderTargetParamsSC[1] = mShader->getShaderConstHandle( "$rtParams1" );
      mRenderTargetParamsSC[2] = mShader->getShaderConstHandle( "$rtParams2" );
      mRenderTargetParamsSC[3] = mShader->getShaderConstHandle( "$rtParams3" );

      //mViewportSC = shader->getShaderConstHandle( "$viewport" );

      mFogDataSC = mShader->getShaderConstHandle( ShaderGenVars::fogData );
      mFogColorSC = mShader->getShaderConstHandle( ShaderGenVars::fogColor );

      mEyePosSC = mShader->getShaderConstHandle( ShaderGenVars::eyePosWorld );

      mNearFarSC = mShader->getShaderConstHandle( "$nearFar" );
      mInvNearFarSC = mShader->getShaderConstHandle( "$invNearFar" );
      mWorldToScreenScaleSC = mShader->getShaderConstHandle( "$worldToScreenScale" );

      mMatWorldToScreenSC = mShader->getShaderConstHandle( "$matWorldToScreen" );
      mMatScreenToWorldSC = mShader->getShaderConstHandle( "$matScreenToWorld" );
      mMatPrevScreenToWorldSC = mShader->getShaderConstHandle( "$matPrevScreenToWorld" );

      mWaterColorSC = mShader->getShaderConstHandle( "$waterColor" );
      mAmbientColorSC = mShader->getShaderConstHandle( "$ambientColor" );
      mWaterFogDataSC = mShader->getShaderConstHandle( "$waterFogData" );
      mWaterFogPlaneSC = mShader->getShaderConstHandle( "$waterFogPlane" );
      mScreenSunPosSC = mShader->getShaderConstHandle( "$screenSunPos" );
      mLightDirectionSC = mShader->getShaderConstHandle( "$lightDirection" );
      mCameraForwardSC = mShader->getShaderConstHandle( "$camForward" );

      mAccumTimeSC = mShader->getShaderConstHandle( "$accumTime" );
      mDeltaTimeSC = mShader->getShaderConstHandle( "$deltaTime" );

      mInvCameraMatSC = mShader->getShaderConstHandle( "$invCameraMat" );
   }

   // Set up shader constants for source image size
   if ( mRTSizeSC->isValid() )
   {
      const Point2I &resolution = GFX->getActiveRenderTarget()->getSize();
      Point2F pixelShaderConstantData;

      pixelShaderConstantData.x = resolution.x;
      pixelShaderConstantData.y = resolution.y;

      mShaderConsts->set( mRTSizeSC, pixelShaderConstantData );
   }

   if ( mOneOverRTSizeSC->isValid() )
   {
      const Point2I &resolution = GFX->getActiveRenderTarget()->getSize();
      Point2F oneOverTargetSize( 1.0f / (F32)resolution.x, 1.0f / (F32)resolution.y );

      mShaderConsts->set( mOneOverRTSizeSC, oneOverTargetSize );
   }

   // Set up additional textures
   Point2F texSizeConst;
   for( U32 i = 0; i < NumTextures; i++ )
   {
      if( !mActiveTextures[i] )
         continue;

      if ( mTexSizeSC[i]->isValid() )
      {
         texSizeConst.x = (F32)mActiveTextures[i]->getWidth();
         texSizeConst.y = (F32)mActiveTextures[i]->getHeight();
         mShaderConsts->set( mTexSizeSC[i], texSizeConst );
      }
   }

   for ( U32 i = 0; i < NumTextures; i++ )
   {
      if( !mActiveTextures[i] )
         continue;

      if ( mRenderTargetParamsSC[i]->isValid() )
      {
         const Point3I &targetSz = mActiveTextures[i]->getSize();
         RectI targetVp = mActiveTextureViewport[i];

         /*
         if ( mActiveNamedTarget[i] )
            targetVp = mActiveNamedTarget[i]->getTargetViewport();
         else
         {
            targetVp = GFX->getViewport();
            //targetVp.set( 0, 0, targetSz.x, targetSz.y );
         }
         */

         Point4F rtParams;

         ScreenSpace::RenderTargetParameters(targetSz, targetVp, rtParams);

         mShaderConsts->set( mRenderTargetParamsSC[i], rtParams );
      }
   }

   // Pull damage flash information from the game connection
   /*
   GameConnection *conToServer = GameConnection::getConnectionToServer();
   // Set up the damage/whiteout/blackout in a pixel shader constant if they are
   // available
   Point4F gameConnEffects( 0.0f, 0.0f, 0.0f, 0.0f );
   if( conToServer != NULL )
   {
      gameConnEffects.x = conToServer->getDamageFlash();
      gameConnEffects.y = conToServer->getWhiteOut();
      gameConnEffects.z = conToServer->getBlackOut();
   }

   static const String sGameConEfx( "$gameConEfx" );
   mShaderConsts->set( sGameConEfx, gameConnEffects );
   */

   // Set the fog data.
   if ( mFogDataSC->isValid() )
   {
      const FogData &data = gClientSceneGraph->getFogData();

      Point3F params;
      params.x = data.density;
      params.y = data.densityOffset;

      if ( !mIsZero( data.atmosphereHeight ) )
         params.z = 1.0f / data.atmosphereHeight;
      else
         params.z = 0.0f;

      mShaderConsts->set( mFogDataSC, params );
   }

   if ( mFogColorSC->isValid() )
      mShaderConsts->set( mFogColorSC, gClientSceneGraph->getFogData().color );

   if ( mEyePosSC->isValid() && state )
      mShaderConsts->set( mEyePosSC, /*gClientSceneGraph->mNormCamPos*/ state->getDiffuseCameraPosition() );

   if ( mNearFarSC->isValid() && state )
      mShaderConsts->set( mNearFarSC, Point2F( state->getNearPlane(), state->getFarPlane() ) );

   if ( mInvNearFarSC->isValid() && state )
      mShaderConsts->set( mInvNearFarSC, Point2F( 1.0f / state->getNearPlane(), 1.0f / state->getFarPlane() ) );

   if ( mWorldToScreenScaleSC->isValid() && state )
      mShaderConsts->set( mWorldToScreenScaleSC, state->getWorldToScreenScale() );

   if ( mMatWorldToScreenSC->isValid() || mMatScreenToWorldSC->isValid() )
   {
      const PFXFrameState &thisFrame = PFXMGR->getFrameState();

      // Screen space->world space
      MatrixF tempMat = thisFrame.cameraToScreen;
      tempMat.mul( thisFrame.worldToCamera );
      tempMat.fullInverse();
      tempMat.transpose();

      // Support using these matrices as float3x3 or float4x4...
      mShaderConsts->set( mMatWorldToScreenSC, tempMat, mMatWorldToScreenSC->getType() );

      // World space->screen space
      tempMat = thisFrame.cameraToScreen;
      tempMat.mul( thisFrame.worldToCamera );
      tempMat.transpose();

      // Support using these matrices as float3x3 or float4x4...
      mShaderConsts->set( mMatScreenToWorldSC, tempMat, mMatScreenToWorldSC->getType() );
   }

   if ( mMatPrevScreenToWorldSC->isValid() )
   {
      const PFXFrameState &lastFrame = PFXMGR->getLastFrameState();

      // Previous frame world space->screen space
      MatrixF tempMat = lastFrame.cameraToScreen;
      tempMat.mul( lastFrame.worldToCamera );
      tempMat.transpose();
      mShaderConsts->set( mMatPrevScreenToWorldSC, tempMat );
   }

   if ( mWaterColorSC->isValid() )
   {
      ColorF color( gClientSceneGraph->getWaterFogData().color );
      mShaderConsts->set( mWaterColorSC, color );
   }

   if ( mWaterFogDataSC->isValid() )
   {
      const WaterFogData &data = gClientSceneGraph->getWaterFogData();
      Point4F params( data.density, data.densityOffset, data.wetDepth, data.wetDarkening );
      mShaderConsts->set( mWaterFogDataSC, params );
   }

   if ( mAmbientColorSC->isValid() )
   {
      const ColorF &sunlight = gClientSceneGraph->getLightManager()->getSpecialLight(LightManager::slSunLightType)->getAmbient();
      Point3F ambientColor( sunlight.red, sunlight.green, sunlight.blue );

      mShaderConsts->set( mAmbientColorSC, ambientColor );
   }

   if ( mWaterFogPlaneSC->isValid() )
   {
      const PlaneF &plane = gClientSceneGraph->getWaterFogData().plane;
      mShaderConsts->set( mWaterFogPlaneSC, plane );
   }

   if ( mScreenSunPosSC->isValid() && state )
   {
      // Grab our projection matrix
      // from the frustum.
      Frustum frust = state->getFrustum();
      MatrixF proj( true );
      frust.getProjectionMatrix( &proj );

      // Grab the ScatterSky world matrix.
      MatrixF camMat = state->getCameraTransform();
      camMat.inverse();
      MatrixF tmp( true );
      tmp = camMat;
      tmp.setPosition( Point3F( 0, 0, 0 ) );

      Point3F sunPos( 0, 0, 0 );

      // Get the light manager and sun light object.
      LightManager *lm = state->getLightManager();
      LightInfo *sunLight = lm->getSpecialLight( LightManager::slSunLightType );

      // Grab the light direction and scale
      // by the ScatterSky radius to get the world
      // space sun position.
      const VectorF &lightDir = sunLight->getDirection();

      Point3F lightPos( lightDir.x * (6378.0f * 1000.0f),
                        lightDir.y * (6378.0f * 1000.0f),
                        lightDir.z * (6378.0f * 1000.0f) );

      // Get the screen space sun position.
      MathUtils::mProjectWorldToScreen( lightPos, &sunPos, GFX->getViewport(), tmp, proj );

      // And normalize it to the 0 to 1 range.
      sunPos.x /= (F32)GFX->getViewport().extent.x;
      sunPos.y /= (F32)GFX->getViewport().extent.y;

      mShaderConsts->set( mScreenSunPosSC, Point2F( sunPos.x, sunPos.y ) );
   }

   if ( mLightDirectionSC->isValid() && state )
   {
      LightManager *lm = state->getLightManager();
      LightInfo *sunLight = lm->getSpecialLight( LightManager::slSunLightType );

      const VectorF &lightDir = sunLight->getDirection();
      mShaderConsts->set( mLightDirectionSC, lightDir );
   }

   if ( mCameraForwardSC->isValid() && state )
   {
      const MatrixF &camMat = state->getCameraTransform();
      VectorF camFwd( 0, 0, 0 );

      camMat.getColumn( 1, &camFwd );

      mShaderConsts->set( mCameraForwardSC, camFwd );
   }

   if ( mAccumTimeSC->isValid() )
      mShaderConsts->set( mAccumTimeSC, MATMGR->getTotalTime() );

   if ( mDeltaTimeSC->isValid() )
      mShaderConsts->set( mDeltaTimeSC, MATMGR->getDeltaTime() );

   if ( mInvCameraMatSC->isValid() && state )
   {
      MatrixF mat = state->getCameraTransform();
      mat.inverse();
      mShaderConsts->set( mInvCameraMatSC, mat, mInvCameraMatSC->getType() );
   }

   // Set EffectConsts - specified from script

   // If our shader has reloaded since last frame we must mark all
   // EffectConsts dirty so they will be reset.
   if ( mShader->getReloadKey() != mShaderReloadKey )
   {
      mShaderReloadKey = mShader->getReloadKey();

      EffectConstTable::Iterator iter = mEffectConsts.begin();
      for ( ; iter != mEffectConsts.end(); iter++ )
         iter->value->mDirty = true;
   }

   // Doesn't look like anyone is using this anymore.
   // But if we do want to pass this info to script,
   // we should do so in the same way as I am doing below.
   /*
   Point2F texSizeScriptConst( 0, 0 );
   String buffer;
   if ( mActiveTextures[0] )
   {
      texSizeScriptConst.x = (F32)mActiveTextures[0]->getWidth();
      texSizeScriptConst.y = (F32)mActiveTextures[0]->getHeight();

      dSscanf( buffer.c_str(), "%g %g", texSizeScriptConst.x, texSizeScriptConst.y );
   }
   */
   
   if ( isMethod( "setShaderConsts" ) )
   {
      PROFILE_SCOPE( PostEffect_SetShaderConsts );

      // Pass some data about the current render state to script.
      // 
      // TODO: This is pretty messy... it should go away.  This info
      // should be available from some other script accessible method
      // or field which isn't PostEffect specific.
      //
      if ( state )
      {
         Con::setFloatVariable( "$Param::NearDist", state->getNearPlane() );
         Con::setFloatVariable( "$Param::FarDist", state->getFarPlane() );   
      }

      Con::executef( this, "setShaderConsts" );
   }   

   EffectConstTable::Iterator iter = mEffectConsts.begin();
   for ( ; iter != mEffectConsts.end(); iter++ )
      iter->value->setToBuffer( mShaderConsts );
}

void PostEffect::_setupTexture( U32 stage, GFXTexHandle &inputTex, const RectI *inTexViewport )
{
   const String &texFilename = mTexFilename[ stage ];

   GFXTexHandle theTex;
   MatTextureTarget *namedTarget = NULL;

   RectI viewport = GFX->getViewport();

   if ( texFilename.compare( "$inTex", 0, String::NoCase ) == 0 )
   {
      theTex = inputTex;

      if ( inTexViewport )
      {
         viewport = *inTexViewport;
      }
      else if ( theTex )
      {
         viewport.set( 0, 0, theTex->getWidth(), theTex->getHeight() );
      }
   }
   else if ( texFilename.compare( "$backBuffer", 0, String::NoCase ) == 0 )
   {
      theTex = PFXMGR->getBackBufferTex();
      if ( theTex )
         viewport.set( 0, 0, theTex->getWidth(), theTex->getHeight() );
   }
   else if ( texFilename.isNotEmpty() && texFilename[0] == '#' )
   {
      namedTarget = MatTextureTarget::findTargetByName( texFilename.c_str() + 1 );
      if ( namedTarget )
      {
         theTex = namedTarget->getTargetTexture( 0 );
         viewport = namedTarget->getTargetViewport();
      }      
   }
   else
   {
      theTex = mTextures[ stage ];
      if ( theTex )
         viewport.set( 0, 0, theTex->getWidth(), theTex->getHeight() );
   }

   mActiveTextures[ stage ] = theTex;
   mActiveNamedTarget[ stage ] = namedTarget;
   mActiveTextureViewport[ stage ] = viewport;

   if ( theTex.isValid() )
      GFX->setTexture( stage, theTex );
}

void PostEffect::_setupTransforms()
{
   // Set everything to identity.
   GFX->setWorldMatrix( MatrixF::Identity );
   GFX->setProjectionMatrix( MatrixF::Identity );
}

void PostEffect::_setupTarget( const SceneState *state, bool *outClearTarget )
{
   if ( mIsNamedTarget || mTargetName.compare( "$outTex", 0, String::NoCase ) == 0 )
   {
      // Size it relative to the texture of the first stage or
      // if NULL then use the current target.

      Point2I targetSize;

      // If we have an absolute target size then use that.
      if ( !mTargetSize.isZero() )
         targetSize = mTargetSize;

      // Else generate a relative size using the target scale.
      else if ( mActiveTextures[ 0 ] )
      {
         const Point3I &texSize = mActiveTextures[ 0 ]->getSize();

         targetSize.set(   texSize.x * mTargetScale.x,
                           texSize.y * mTargetScale.y );
      }
      else
      {
         GFXTarget *oldTarget = GFX->getActiveRenderTarget();
         const Point2I &oldTargetSize = oldTarget->getSize();

         targetSize.set(   oldTargetSize.x * mTargetScale.x,
                           oldTargetSize.y * mTargetScale.y );
      }

      // Make sure its at least 1x1.
      targetSize.setMax( Point2I::One );

      if (  !mIsNamedTarget ||
            !mTargetTex ||
            mTargetTex.getWidthHeight() != targetSize )
      {
         mTargetTex.set( targetSize.x, targetSize.y, mTargetFormat,
            &PostFxTargetProfile, "PostEffect::_setupTarget" );

         if ( mTargetClear == PFXTargetClear_OnCreate )
            *outClearTarget = true;

         mTargetRect.set( 0, 0, targetSize.x, targetSize.y );
      }
   }
   else
      mTargetTex = NULL;

   if ( mTargetClear == PFXTargetClear_OnDraw )
      *outClearTarget = true;

   if ( !mTarget && mTargetTex )
      mTarget = GFX->allocRenderToTextureTarget();
}

void PostEffect::_cleanTargets( bool recurse )
{
   mTargetTex = NULL;
   mTarget = NULL;

   if ( !recurse )
      return;

   // Clear the children too!
   for ( U32 i = 0; i < size(); i++ )
   {
      PostEffect *effect = (PostEffect*)(*this)[i];
      effect->_cleanTargets( true );
   }
}

void PostEffect::process(  const SceneState *state,
                           GFXTexHandle &inOutTex,
                           const RectI *inTexViewport )
{
   GFXDEBUGEVENT_SCOPE_EX( PostEffect_Process, ColorI::GREEN, avar("PostEffect: %s", getName()) );

   if ( mSkip || ( !mRequirementsMet && !mUpdateShader ) )
      return;

   // Skip out if we don't support reflection passes.
   if ( state && state->isReflectPass() && !mAllowReflectPass )
      return;

   if ( mOneFrameOnly && !mOnThisFrame )
      return;

   if ( isMethod( "preProcess" ) )
   {
      PROFILE_SCOPE( PostEffect_preProcess );
      Con::executef( this, "preProcess" );
   }  

   GFXTransformSaver saver;

   // Set the textures.
   for ( U32 i = 0; i < NumTextures; i++ )
      _setupTexture( i, inOutTex, inTexViewport );

   _setupStateBlock( state ) ;
   _setupTransforms();

   bool clearTarget = false;
   _setupTarget( state, &clearTarget );

   if ( mTargetTex )
   {
#ifdef TORQUE_OS_XENON
      // You may want to disable this functionality for speed reasons as it does
      // add some overhead. The upside is it makes things "just work". If you
      // re-work your post-effects properly, this is not needed.
      //
      // If this post effect doesn't alpha blend to the back-buffer, than preserve
      // the active render target contents so they are still around the next time
      // that render target activates
      if(!mStateBlockData->getState().blendEnable)
         GFX->getActiveRenderTarget()->preserve();
#endif
      GFX->pushActiveRenderTarget();
      mTarget->attachTexture( GFXTextureTarget::Color0, mTargetTex );
      GFX->setActiveRenderTarget( mTarget );
   }

   if ( clearTarget )
      GFX->clear( GFXClearTarget, mTargetClearColor, 1.f, 0 );

   // Do we have a shader that needs updating?
   if ( mUpdateShader )
   {
      mShader = NULL;
      mUpdateShader = false;

      // check requirements
      mRequirementsMet = checkRequirements();
      if ( mRequirementsMet )
      {
         ShaderData *shaderData;
         if ( Sim::findObject( mShaderName, shaderData ) )
         {
            // Gather macros specified on this PostEffect.
            Vector<GFXShaderMacro> macros( mShaderMacros );

            // Gather conditioner macros.
            for ( U32 i = 0; i < NumTextures; i++ )
            {
               if ( mActiveNamedTarget[i] )
                  mActiveNamedTarget[i]->getTargetShaderMacros( &macros );
            }

            mShader = shaderData->getShader( macros );
         }
      }
      else
      {
         // Clear the targets... we won't be rendering
         // again until the shader passes requirements. 
         _cleanTargets( true );
      }
   }

   // Setup the shader and constants.
   if ( mShader )
   {
      _setupConstants( state );

      GFX->setShader( mShader );
      GFX->setShaderConstBuffer( mShaderConsts );
   }
   else
      GFX->disableShaders();

   Frustum frustum;
   if ( state )
      frustum = state->getFrustum();
   else
   {
      // If we don't have a scene state then setup
      // a dummy frustum... you better not be depending
      // on this being related to the camera in any way.
      
      frustum.set( false, -0.1f, 0.1f, -0.1f, 0.1f, 0.1f, 100.0f );
   }

   GFXVertexBufferHandle<PFXVertex> vb;
   _updateScreenGeometry( frustum, &vb );

   // Draw it.
   GFX->setVertexBuffer( vb );
   GFX->drawPrimitive( GFXTriangleFan, 0, 2 );

   // Allow PostEffecVis to hook in.
   PFXVIS->onPFXProcessed( this );

   if ( mTargetTex )
   {
      mTarget->resolve();
      GFX->popActiveRenderTarget();
   }
   else
   {
      // We wrote to the active back buffer, so release
      // the current texture copy held by the manager.
      //
      // This ensures a new copy is made.
      PFXMGR->releaseBackBufferTex();
   }

   // Return and release our target texture.
   inOutTex = mTargetTex;
   if ( !mIsNamedTarget )
      mTargetTex = NULL;

   // Restore the transforms before the children
   // are processed as it screws up the viewport.
   saver.restore();

   // Now process my children.
   iterator i = begin();
   for ( ; i != end(); i++ )
   {
      PostEffect *effect = static_cast<PostEffect*>(*i);
      effect->process( state, inOutTex );
   }

   if ( mOneFrameOnly )
      mOnThisFrame = false;
}

bool PostEffect::_setIsEnabled( void* obj, const char* data )
{
   bool enabled = dAtob( data );
   if ( enabled )
      static_cast<PostEffect*>( obj )->enable();
   else
      static_cast<PostEffect*>( obj )->disable();

   // Always return false from a protected field.
   return false;
}

void PostEffect::enable()
{
   // Don't add TexGen PostEffects to the PostEffectManager!
   if ( mRenderTime == PFXTexGenOnDemand )
      return;

   // Ignore it if its already enabled.
   if ( mEnabled )
      return;

   mEnabled = true;

   // We cannot really enable the effect 
   // until its been registed.
   if ( !isProperlyAdded() )
      return;

   // If the enable callback returns 'false' then 
   // leave the effect disabled.
   const char* result = Con::executef( this, "onEnabled" );
   if ( result[0] && !dAtob( result ) )
   {
      mEnabled = false;
      return;
   }

   PFXMGR->_addEffect( this );
}

void PostEffect::disable()
{
   if ( !mEnabled )
      return;

   mEnabled = false;
   _cleanTargets( true );

   if ( isProperlyAdded() )
   {
      PFXMGR->_removeEffect( this );
      Con::executef( this, "onDisabled" );
   }
}

void PostEffect::reload()
{
   // Reload the shader if we have one or mark it
   // for updating when its processed next.
   if ( mShader )
      mShader->reload();
   else
      mUpdateShader = true;

   // Null stateblock so it is reloaded.
   mStateBlock = NULL;

   // Call reload on any children
   // this PostEffect may have.
   for ( U32 i = 0; i < size(); i++ )
   {
      PostEffect *effect = (PostEffect*)(*this)[i];
      effect->reload();
   }
}

void PostEffect::setShaderConst( const String &name, const String &val )
{
   PROFILE_SCOPE( PostEffect_SetShaderConst );

   EffectConstTable::Iterator iter = mEffectConsts.find( name );
   if ( iter == mEffectConsts.end() )
   {
      EffectConst *newConst = new EffectConst( name, val );
      iter = mEffectConsts.insertUnique( name, newConst );
   }

   iter->value->set( val );
}

F32 PostEffect::getAspectRatio() const
{
   const Point2I &rtSize = GFX->getActiveRenderTarget()->getSize();
   return (F32)rtSize.x / (F32)rtSize.y;
}

bool PostEffect::checkRequirements() const
{
   // Make sure that the requirements for this post effect can be met before
   // enabling it.
   U32 checkReqs = mPostEffectRequirements;
   PostEffect *parentEffect = dynamic_cast<PostEffect *>(getGroup());
   while(parentEffect != NULL && checkReqs == U32_MAX)
   {
      checkReqs = parentEffect->mPostEffectRequirements;
      parentEffect = dynamic_cast<PostEffect *>(parentEffect->getGroup());
   }

   if(checkReqs == U32_MAX)
   {
      Con::warnf("You should specify 'requirements' field for PostEffect '%s'. (You may need to move 'isEnabled' after the 'requirements' field in your singleton definition)", getName());
      checkReqs = 0;
   }

   // Now figure out if this effect can be enabled based on what it requires
   bool ret = true;   
// TODO... why is it just this part that needs to be 
// defined out?  I suspect this is actually unnessasary.
#ifndef TORQUE_DEDICATED 
   if(checkReqs & PostEffect::RequiresDepth)
   {
      MatTextureTarget *namedTarget = MatTextureTarget::findTargetByName( RenderPrePassMgr::BufferName );
      ret &= (namedTarget != NULL);
   }
   if(checkReqs & PostEffect::RequiresNormals)
   {
      // This is kind of a hack.
      MatTextureTarget *namedTarget = MatTextureTarget::findTargetByName( AdvancedLightBinManager::smBufferName );
      ret &= (namedTarget != NULL);
   }
   if(checkReqs & PostEffect::RequiresLightInfo)
   {
      MatTextureTarget *namedTarget = MatTextureTarget::findTargetByName( AdvancedLightBinManager::smBufferName );
      ret &= (namedTarget != NULL);
   }
#endif
   return ret;
}

bool PostEffect::dumpShaderDisassembly( String &outFilename ) const
{
   String data;

   if ( !mShader || !mShader->getDisassembly( data ) )
      return false;
         
   outFilename = FS::MakeUniquePath( "", "ShaderDisassembly", "txt" );

   FileStream *fstream = FileStream::createAndOpen( outFilename, Torque::FS::File::Write );
   if ( !fstream )
      return false;

   fstream->write( data );
   fstream->close();
   delete fstream;   

   return true;
}

void PostEffect::setShaderMacro( const String &name, const String &value )
{
   // Check to see if we already have this macro.
   Vector<GFXShaderMacro>::iterator iter = mShaderMacros.begin();
   for ( ; iter != mShaderMacros.end(); iter++ )
   {
      if ( iter->name == name )
      {
         if ( iter->value != value )
         {
            iter->value = value;
            mUpdateShader = true;
         }
         return;
      }
   }

   // Add a new macro.
   mShaderMacros.increment();
   mShaderMacros.last().name = name;
   mShaderMacros.last().value = value;
   mUpdateShader = true;
}

bool PostEffect::removeShaderMacro( const String &name )
{
   Vector<GFXShaderMacro>::iterator iter = mShaderMacros.begin();
   for ( ; iter != mShaderMacros.end(); iter++ )
   {
      if ( iter->name == name )
      {
         mShaderMacros.erase( iter );
         mUpdateShader = true;
         return true;
      }
   }

   return false;
}

void PostEffect::clearShaderMacros()
{
   if ( mShaderMacros.empty() )
      return;

   mShaderMacros.clear();
   mUpdateShader = true;
}

GFXTextureObject* PostEffect::getTargetTexture( U32 index ) const
{
   // A TexGen PostEffect will generate its texture now if it
   // has not already.
   if ( mRenderTime == PFXTexGenOnDemand )
      return const_cast<PostEffect*>( this )->_texGen();   
      
   return mTargetTex.getPointer();
}

GFXTextureObject* PostEffect::_texGen()
{
   if ( !mTargetTex || mUpdateShader )
   {
      GFXTexHandle chainTex;
      process( NULL, chainTex );
      
      // TODO: We should add a conditional copy
      // to a non-RT texture here to reduce the
      // amount of non-swappable RTs in use.      
   }

   return mTargetTex.getPointer();      
}

ConsoleMethod( PostEffect, reload, void, 2, 2,
   "Reloads the effect shader and textures." )
{
   return object->reload();
}

ConsoleMethod( PostEffect, enable, void, 2, 2,
   "Enables the effect." )
{
   object->enable();
}

ConsoleMethod( PostEffect, disable, void, 2, 2,
   "Disables the effect." )
{
   object->disable();
}

ConsoleMethod( PostEffect, toggle, bool, 2, 2,
   "Toggles the effect state returning true if we enable it." )
{
   if ( object->isEnabled() )
      object->disable();
   else
      object->enable();

   return object->isEnabled();
}

ConsoleMethod( PostEffect, isEnabled, bool, 2, 2,
   "Returns true if the effect is enabled." )
{
   return object->isEnabled();
}

ConsoleMethod( PostEffect, setShaderConst, void, 4, 4, "( String name, float value )" )
{
   object->setShaderConst( argv[2], argv[3] );
}

ConsoleMethod( PostEffect, getAspectRatio, F32, 2, 2, "Returns width over height aspect ratio of the backbuffer." )
{
   return object->getAspectRatio();
}

ConsoleMethod( PostEffect, dumpShaderDisassembly, const char*, 2, 2, "Dumps this PostEffect shader's disassembly to a temporary text file. Returns the fullpath of that file if successful." )
{
   String fileName;
   if ( !object->dumpShaderDisassembly( fileName ) )
      return NULL;

   char *buf = Con::getReturnBuffer(256);
   dStrcpy( buf, fileName.c_str() );

   return buf;
}

ConsoleMethod( PostEffect, setShaderMacro, void, 3, 4, "( string key, [string value] ) - add/set a shader macro." )
{
   if ( argc > 3 )
      object->setShaderMacro( argv[2], argv[3] );
   else
      object->setShaderMacro( argv[2] );
}

ConsoleMethod( PostEffect, removeShaderMacro, void, 3, 3, "( string key )" )
{
   object->removeShaderMacro( argv[2] );
}

ConsoleMethod( PostEffect, clearShaderMacros, void, 2, 2, "()" )
{
   object->clearShaderMacros();
}
