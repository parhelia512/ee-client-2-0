//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "platform/platform.h"
#include "lighting/shadowMap/lightShadowMap.h"

#include "lighting/shadowMap/blurTexture.h"
#include "lighting/shadowMap/shadowMapManager.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/gfxOcclusionQuery.h"
#include "materials/materialDefinition.h"
#include "materials/baseMatInstance.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
//#include "sceneGraph/container/sceneContainer.h"
#include "lighting/lightManager.h"
#include "math/mathUtils.h"
#include "shaderGen/shaderGenVars.h"
#include "core/util/safeDelete.h"
#include "core/stream/bitStream.h"
#include "math/mathIO.h"
#include "materials/shaderData.h"

// Remove this when the shader constants are reworked better
#include "lighting/advanced/advancedLightManager.h"
#include "lighting/advanced/advancedLightBinManager.h"

// TODO: Some cards (Justin's GeForce 7x series) barf on the integer format causing
// filtering artifacts. These can (sometimes) be resolved by switching the format
// to FP16 instead of Int16.
const GFXFormat LightShadowMap::ShadowMapFormat = GFXFormatR32F; // GFXFormatR8G8B8A8;

Vector<LightShadowMap*> LightShadowMap::smUsedShadowMaps;
Vector<LightShadowMap*> LightShadowMap::smShadowMaps;

GFX_ImplementTextureProfile( ShadowMapProfile,
                              GFXTextureProfile::DiffuseMap,
                              GFXTextureProfile::PreserveSize | 
                              GFXTextureProfile::RenderTarget |
                              GFXTextureProfile::Pooled,
                              GFXTextureProfile::None );

GFX_ImplementTextureProfile( ShadowMapZProfile,
                             GFXTextureProfile::DiffuseMap, 
                             GFXTextureProfile::PreserveSize | 
                             GFXTextureProfile::NoMipmap | 
                             GFXTextureProfile::ZTarget |
                             GFXTextureProfile::Pooled,
                             GFXTextureProfile::None );

//
// LightShadowMap, holds the shadow map and various other things for a light.
//
LightShadowMap::LightShadowMap( LightInfo *light )
   :  mWorldToLightProj( true ),
      mLight( light ),
      mTexSize( 0 ),
      mLastShader( NULL ),
      mLastUpdate( 0 ),
      mIsViewDependent( false ),
      mVizQuery( NULL ),
      mWasOccluded( false ),
      mLastScreenSize( 0.0f ),
      mLastPriority( 0.0f )
{
   GFXTextureManager::addEventDelegate( this, &LightShadowMap::_onTextureEvent );

   mTarget = GFX->allocRenderToTextureTarget();
   mVizQuery = GFX->createOcclusionQuery();

   smShadowMaps.push_back( this );
}

LightShadowMap::~LightShadowMap()
{
   mTarget = NULL;
   SAFE_DELETE( mVizQuery );   
   
   releaseTextures();

   smShadowMaps.remove( this );
   smUsedShadowMaps.remove( this );

   GFXTextureManager::removeEventDelegate( this, &LightShadowMap::_onTextureEvent );
}

void LightShadowMap::releaseAllTextures()
{
   PROFILE_SCOPE( LightShadowMap_ReleaseAllTextures );

   for ( U32 i=0; i < smShadowMaps.size(); i++ )
      smShadowMaps[i]->releaseTextures();
}

void LightShadowMap::releaseUnusedTextures()
{
   PROFILE_SCOPE( LightShadowMap_ReleaseUnusedTextures );

   const U32 currTime = Platform::getRealMilliseconds();
   const U32 purgeTime = 1000;

   for ( U32 i=0; i < smUsedShadowMaps.size(); )
   {
      LightShadowMap *lsm = smUsedShadowMaps[i];

      // If the shadow has not been updated for a while
      // then release its textures.
      if ( currTime > ( lsm->getLastUpdate() + purgeTime ) )
      {
         // Internally this will remove the map from the used
         // list, so don't increment the loop.
         lsm->releaseTextures();         
         continue;
      }

      i++;
   }
}

void LightShadowMap::_onTextureEvent( GFXTexCallbackCode code )
{
   if ( code == GFXZombify )
      releaseTextures();

   // We don't initialize here as we want the textures
   // to be reallocated when the shadow becomes visible.
}

Frustum LightShadowMap::_getFrustum() const
{
   F32 left, right, bottom, top, nearPlane, farPlane;
   bool isOrtho;
   GFX->getFrustum(&left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho);

   MatrixF camMat(GFX->getWorldMatrix() );
   camMat.inverse();

   Frustum f;
   f.set( isOrtho, left, right, top, bottom, nearPlane, farPlane, camMat );
   return f;
}

void LightShadowMap::calcLightMatrices(MatrixF& outLightMatrix)
{
   // Create light matrix, set projection

   switch ( mLight->getType() )
   {
   case LightInfo::Vector :
      {
         Frustum viewFrustum(_getFrustum());

         const ShadowMapParams *p = mLight->getExtended<ShadowMapParams>();

         // Calculate the bonding box of the shadowed area 
         // we're interested in... this is the shadow box 
         // transformed by the frustum transform.
         Box3F viewBB( -p->shadowDistance, -p->shadowDistance, -p->shadowDistance,
                        p->shadowDistance, p->shadowDistance, p->shadowDistance );
         viewFrustum.getTransform().mul( viewBB );

         // Calculate a light "projection" matrix.
         MatrixF lightMatrix = MathUtils::createOrientFromDir(mLight->getDirection());
         outLightMatrix = lightMatrix;
         static MatrixF rotMat(EulerF( (M_PI_F / 2.0f), 0.0f, 0.0f));
         lightMatrix.mul( rotMat );

         // This is the box in lightspace
         Box3F lightViewBB(viewBB);
         lightMatrix.mul(lightViewBB);

         // Now, let's position our light based on the lightViewBB
         Point3F newLightPos(viewBB.getCenter());
         F32 sceneDepth = lightViewBB.maxExtents.z - lightViewBB.minExtents.z;
         newLightPos += mLight->getDirection() * ((-sceneDepth / 2.0f)-1.0f);         // -1 for the nearplane
         outLightMatrix.setPosition(newLightPos);

         // Update light info
         mLight->setRange( sceneDepth );
         mLight->setPosition( newLightPos );

         // Set our ortho projection
         F32 width = (lightViewBB.maxExtents.x - lightViewBB.minExtents.x) / 2.0f;
         F32 height = (lightViewBB.maxExtents.y - lightViewBB.minExtents.y) / 2.0f;

         width = getMax(width, height);

         GFX->setOrtho(-width, width, -width, width, 1.0f, sceneDepth, true);


         // TODO: Width * 2... really isn't that pixels being used as
         // meters?  Is a real physical metric of scene depth better?         
         //sm->setVisibleDistance(width * 2.0f);

#if 0
         DebugDrawer::get()->drawFrustum(viewFrustum, ColorF(1.0f, 0.0f, 0.0f));
         DebugDrawer::get()->drawBox(viewBB.minExtents, viewBB.maxExtents, ColorF(0.0f, 1.0f, 0.0f));
         DebugDrawer::get()->drawBox(lightViewBB.minExtents, lightViewBB.maxExtents, ColorF(0.0f, 0.0f, 1.0f));
         DebugDrawer::get()->drawBox(newLightPos - Point3F(1,1,1), newLightPos + Point3F(1,1,1), ColorF(1,1,0));
         DebugDrawer::get()->drawLine(newLightPos, newLightPos + mLight.mDirection*3.0f, ColorF(0,1,1));

         Point3F a(newLightPos);
         Point3F b(newLightPos);
         Point3F offset(width, height,0.0f);
         a -= offset;
         b += offset;
         DebugDrawer::get()->drawBox(a, b, ColorF(0.5f, 0.5f, 0.5f));
#endif
      }
      break;
   case LightInfo::Spot :
      {
         // Note: If a light is attached to a camera or something and mDirection is 0,0,-1 or 0,0,1 this transform won't match the camera because
         // we don't have the up vector from the mLight structure (I don't think this is a big deal).
         outLightMatrix = MathUtils::createOrientFromDir(mLight->getDirection());
         outLightMatrix.setPosition(mLight->getPosition());

         F32 fov = mLight->getOuterConeAngle();
         F32 range = mLight->getRange().x;
         GFX->setFrustum( fov, 1.0f, range * 0.01f, range );
      }
      break;
   default:
      AssertFatal(false, "Unsupported light type!");
   }
}

void LightShadowMap::releaseTextures()
{
   mShadowMapTex = NULL;
   mLastUpdate = 0;
   smUsedShadowMaps.remove( this );
}

GFXTextureObject* LightShadowMap::_getDepthTarget( U32 width, U32 height )
{
   // Get a depth texture target from the pooled profile
   // which is returned as a temporary.
   GFXTexHandle depthTex( width, height, GFXFormatD24S8, &ShadowMapZProfile, 
      "LightShadowMap::_getDepthTarget()" );

   return depthTex;
}

bool LightShadowMap::setTextureStage(  const SceneGraphData& sgData, 
                                       const U32 currTexFlag,
                                       const U32 textureSlot, 
                                       GFXShaderConstBuffer* shaderConsts,
                                       GFXShaderConstHandle* shadowMapSC )
{
   switch (currTexFlag)
   {
   case Material::DynamicLight:
      {
         if(shaderConsts && shadowMapSC->isValid())
            shaderConsts->set(shadowMapSC, (S32)textureSlot);
         GFX->setTexture(textureSlot, mShadowMapTex);
         return true;
      }
      break;
   }
   return false;
}

void LightShadowMap::render(  SceneGraph *sceneManager, 
                              const SceneState *diffuseState )
{

   _render( sceneManager, diffuseState );

   // Add it to the used list unless we're been updated.
   if ( !mLastUpdate )
   {
      AssertFatal( !smUsedShadowMaps.contains( this ), "LightShadowMap::render - Used shadow map inserted twice!" );
      smUsedShadowMaps.push_back( this );
   }

   mLastUpdate = Platform::getRealMilliseconds();
}

void LightShadowMap::preLightRender()
{
   if ( mVizQuery )
   {
      mWasOccluded = mVizQuery->getStatus( true ) == GFXOcclusionQuery::Occluded;
      mVizQuery->begin();
   }
}

void LightShadowMap::postLightRender()
{
   if ( mVizQuery )
      mVizQuery->end();
}

void LightShadowMap::updatePriority( const SceneState *state, U32 currTimeMs )
{
   F32 dist = SphereF( mLight->getPosition(), mLight->getRange().x ).distanceTo( state->getCameraPosition() );
   mLastScreenSize = state->projectRadius( dist, mLight->getRange().x );

   F32 timeSinceLastUpdate = currTimeMs - mLastUpdate;
   mLastPriority =   ( 1.0f - mClampF( mLastScreenSize / 600, 0.0f, 1.0f ) ) +
                     timeSinceLastUpdate;
}

S32 QSORT_CALLBACK LightShadowMap::cmpPriority( LightShadowMap *const *lsm1, LightShadowMap *const *lsm2 )
{
   F32 diff = (*lsm1)->getLastPriority() - (*lsm2)->getLastPriority(); 
   return diff > 0.0f ? -1 : diff < 0.0f ? 1 : 0;
}


LightingShaderConstants::LightingShaderConstants() 
   :  mInit( false ),
      mShader( NULL ),
      mLightParamsSC(NULL), 
      mLightSpotParamsSC(NULL), 
      mLightPositionSC(NULL),
      mLightDiffuseSC(NULL), 
      mLightAmbientSC(NULL), 
      mLightInvRadiusSqSC(NULL),
      mLightSpotDirSC(NULL),
      mLightSpotAngleSC(NULL),
      mShadowMapSC(NULL), 
      mShadowMapSizeSC(NULL), 
      mRandomDirsConst(NULL),
      mShadowSoftnessConst(NULL), 
      mWorldToLightProjSC(NULL), 
      mViewToLightProjSC(NULL),
      mSplitStartSC(NULL), 
      mSplitEndSC(NULL), 
      mScaleXSC(NULL), 
      mScaleYSC(NULL),
      mOffsetXSC(NULL), 
      mOffsetYSC(NULL), 
      mAtlasXOffsetSC(NULL), 
      mAtlasYOffsetSC(NULL),
      mAtlasScaleSC(NULL), 
      mFadeStartLength(NULL), 
      mFarPlaneScalePSSM(NULL),
      mOverDarkFactorPSSM(NULL), 
      mSplitFade(NULL), 
      mConstantSpecularPowerSC(NULL),
      mTapRotationTexSC(NULL)
{
}

LightingShaderConstants::~LightingShaderConstants()
{
   if (mShader.isValid())
   {
      mShader->getReloadSignal().remove( this, &LightingShaderConstants::_onShaderReload );
      mShader = NULL;
   }
}

void LightingShaderConstants::init(GFXShader* shader)
{
   if (mShader.getPointer() != shader)
   {
      if (mShader.isValid())
         mShader->getReloadSignal().remove( this, &LightingShaderConstants::_onShaderReload );

      mShader = shader;
      mShader->getReloadSignal().notify( this, &LightingShaderConstants::_onShaderReload );
   }

   mLightParamsSC = shader->getShaderConstHandle("$lightParams");
   mLightSpotParamsSC = shader->getShaderConstHandle("$lightSpotParams");

   // NOTE: These are the shader constants used for doing lighting 
   // during the forward pass.  Do not confuse these for the prepass
   // lighting constants which are used from AdvancedLightBinManager.
   mLightPositionSC = shader->getShaderConstHandle( ShaderGenVars::lightPosition );
   mLightDiffuseSC = shader->getShaderConstHandle( ShaderGenVars::lightDiffuse );
   mLightAmbientSC = shader->getShaderConstHandle( ShaderGenVars::lightAmbient );
   mLightInvRadiusSqSC = shader->getShaderConstHandle( ShaderGenVars::lightInvRadiusSq );
   mLightSpotDirSC = shader->getShaderConstHandle( ShaderGenVars::lightSpotDir );
   mLightSpotAngleSC = shader->getShaderConstHandle( ShaderGenVars::lightSpotAngle );

   mShadowMapSC = shader->getShaderConstHandle("$shadowMap");
   mShadowMapSizeSC = shader->getShaderConstHandle("$shadowMapSize");

   mShadowSoftnessConst = shader->getShaderConstHandle("$shadowSoftness");

   mWorldToLightProjSC = shader->getShaderConstHandle("$worldToLightProj");
   mViewToLightProjSC = shader->getShaderConstHandle("$viewToLightProj");

   mSplitStartSC = shader->getShaderConstHandle("$splitDistStart");
   mSplitEndSC = shader->getShaderConstHandle("$splitDistEnd");
   mScaleXSC = shader->getShaderConstHandle("$scaleX");
   mScaleYSC = shader->getShaderConstHandle("$scaleY");
   mOffsetXSC = shader->getShaderConstHandle("$offsetX");
   mOffsetYSC = shader->getShaderConstHandle("$offsetY");
   mAtlasXOffsetSC = shader->getShaderConstHandle("$atlasXOffset");
   mAtlasYOffsetSC = shader->getShaderConstHandle("$atlasYOffset");
   mAtlasScaleSC = shader->getShaderConstHandle("$atlasScale");

   mFadeStartLength = shader->getShaderConstHandle("$fadeStartLength");
   mFarPlaneScalePSSM = shader->getShaderConstHandle("$farPlaneScalePSSM");

   mOverDarkFactorPSSM = shader->getShaderConstHandle("$overDarkPSSM");
   mSplitFade = shader->getShaderConstHandle("$splitFade");

   mConstantSpecularPowerSC = shader->getShaderConstHandle( AdvancedLightManager::ConstantSpecularPowerSC );

   mTapRotationTexSC = shader->getShaderConstHandle( "$gTapRotationTex" );

   mInit = true;
}

void LightingShaderConstants::_onShaderReload()
{
   if (mShader.isValid())
      init( mShader );
}


const LightInfoExType ShadowMapParams::Type( "ShadowMapParams" );

ShadowMapParams::ShadowMapParams( LightInfo *light ) 
   :  mLight( light ),
      mShadowMap( NULL )
{
   attenuationRatio.set( 0.0f, 1.0f, 1.0f );
   shadowType = ShadowType_Spot;
   overDarkFactor.set(2000.0f, 1000.0f, 500.0f, 100.0f);
   numSplits = 4;
   logWeight = 0.91f;
   texSize = 512;
   shadowDistance = 400.0f;
   shadowSoftness = 0.15f;
   fadeStartDist = 0.0f;
   splitFadeDistances.set(10.0f, 20.0f, 30.0f, 40.0f);
   lastSplitTerrainOnly = false;
   _validate();
}

ShadowMapParams::~ShadowMapParams()
{
   SAFE_DELETE( mShadowMap );
}

void ShadowMapParams::_validate()
{
   switch ( mLight->getType() )
   {
      case LightInfo::Spot:
         shadowType = ShadowType_Spot;
         break;

      case LightInfo::Vector:
         shadowType = ShadowType_PSSM;
         break;

      case LightInfo::Point:
         if ( shadowType < ShadowType_Paraboloid )
            shadowType = ShadowType_DualParaboloidSinglePass;
         break;
      
      default:
         break;
   }

   if ( mLight->getType() == LightInfo::Vector )
   {
      numSplits = mClamp( numSplits, 1, 4 );
      
      // Limit the total texture size for the PSSM to
      // 4096... so use the split count to decide what
      // the size of a single split can be.
      U32 max = 4096;
      if ( numSplits == 2 || numSplits == 4 )
         max = 2048;
      if ( numSplits == 3 )
         max = 1024;

      texSize = mClamp( texSize, 32, max );
   }
   else
   {
      texSize = mClamp( texSize, 32, 4096 );
      numSplits = 1;
   }
}

#include "lighting/shadowMap/singleLightShadowMap.h"
#include "lighting/shadowMap/pssmLightShadowMap.h"
#include "lighting/shadowMap/cubeLightShadowMap.h"
#include "lighting/shadowMap/dualParaboloidLightShadowMap.h"

LightShadowMap* ShadowMapParams::getOrCreateShadowMap()
{
   if ( mShadowMap )
      return mShadowMap;

   if ( !mLight->getCastShadows() )
      return NULL;

   switch ( mLight->getType() )
   {
      case LightInfo::Spot:
         mShadowMap = new SingleLightShadowMap( mLight );
         break;

      case LightInfo::Vector:
         mShadowMap = new PSSMLightShadowMap( mLight );
         break;

      case LightInfo::Point:

         if ( shadowType == ShadowType_CubeMap )
            mShadowMap = new CubeLightShadowMap( mLight );
         else if ( shadowType == ShadowType_Paraboloid )
            mShadowMap = new ParaboloidLightShadowMap( mLight );
         else
            mShadowMap = new DualParaboloidLightShadowMap( mLight );
         break;
   
      default:
         break;
   }

   return mShadowMap;
}

void ShadowMapParams::set( const LightInfoEx *ex )
{
   // TODO: Do we even need this?
}

void ShadowMapParams::packUpdate( BitStream *stream ) const
{
   // HACK: We need to work out proper parameter 
   // validation when any field changes on the light.

   ((ShadowMapParams*)this)->_validate();

   stream->writeInt( shadowType, 8 );
   
   mathWrite( *stream, attenuationRatio );
   
   stream->write( texSize );

   stream->write( numSplits );
   stream->write( logWeight );

   mathWrite(*stream, overDarkFactor);

   stream->write( fadeStartDist );
   stream->writeFlag( lastSplitTerrainOnly );

   mathWrite(*stream, splitFadeDistances);

   stream->write( shadowDistance );

   stream->write( shadowSoftness );   
}

void ShadowMapParams::unpackUpdate( BitStream *stream )
{
   ShadowType newType = (ShadowType)stream->readInt( 8 );
   if ( shadowType != newType )
   {
      // If the shadow type changes delete the shadow
      // map so it can be reallocated on the next render.
      shadowType = newType;
      SAFE_DELETE( mShadowMap );
   }

   mathRead( *stream, &attenuationRatio );

   stream->read( &texSize );

   stream->read( &numSplits );
   stream->read( &logWeight );
   mathRead(*stream, &overDarkFactor);

   stream->read( &fadeStartDist );
   lastSplitTerrainOnly = stream->readFlag();

   mathRead(*stream, &splitFadeDistances);

   stream->read( &shadowDistance );   

   stream->read( &shadowSoftness );
}
