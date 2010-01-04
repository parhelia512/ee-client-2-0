//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _LIGHTSHADOWMAP_H_
#define _LIGHTSHADOWMAP_H_

#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _GFXTARGET_H_
#include "gfx/gfxTarget.h"
#endif
#ifndef _LIGHTINFO_H_
#include "lighting/lightInfo.h"
#endif
#ifndef _MATHUTIL_FRUSTUM_H_
#include "math/util/frustum.h"
#endif
#ifndef _MATTEXTURETARGET_H_
#include "materials/matTextureTarget.h"
#endif
#ifndef _SHADOW_COMMON_H_
#include "lighting/shadowMap/shadowCommon.h"
#endif
#ifndef _GFXSHADER_H_
#include "gfx/gfxShader.h"
#endif

class ShadowMapManager;
class SceneGraph;
class SceneState;
class BaseMatInstance;
class MaterialParameters;
class SharedShadowMapObjects;
struct SceneGraphData;
class GFXShaderConstBuffer;
class GFXShaderConstHandle;
class GFXShader;
class GFXOcclusionQuery;
class LightManager;


// Shader constant handle lookup
// This isn't broken up as much as it could be, we're mixing single light constants
// and pssm constants.
struct LightingShaderConstants
{
   bool mInit;

   GFXShaderRef mShader;

   GFXShaderConstHandle* mLightParamsSC;
   GFXShaderConstHandle* mLightSpotParamsSC;
   
   // NOTE: These are the shader constants used for doing 
   // lighting  during the forward pass.  Do not confuse 
   // these for the prepass lighting constants which are 
   // used from AdvancedLightBinManager.
   GFXShaderConstHandle *mLightPositionSC;
   GFXShaderConstHandle *mLightDiffuseSC;
   GFXShaderConstHandle *mLightAmbientSC;
   GFXShaderConstHandle *mLightInvRadiusSqSC;
   GFXShaderConstHandle *mLightSpotDirSC;
   GFXShaderConstHandle *mLightSpotAngleSC;

   GFXShaderConstHandle* mShadowMapSC;
   GFXShaderConstHandle* mShadowMapSizeSC;

   GFXShaderConstHandle* mRandomDirsConst;
   GFXShaderConstHandle* mShadowSoftnessConst;

   GFXShaderConstHandle* mWorldToLightProjSC;
   GFXShaderConstHandle* mViewToLightProjSC;

   GFXShaderConstHandle* mSplitStartSC;
   GFXShaderConstHandle* mSplitEndSC;
   GFXShaderConstHandle* mScaleXSC;
   GFXShaderConstHandle* mScaleYSC;
   GFXShaderConstHandle* mOffsetXSC;
   GFXShaderConstHandle* mOffsetYSC;
   GFXShaderConstHandle* mAtlasXOffsetSC;
   GFXShaderConstHandle* mAtlasYOffsetSC;
   GFXShaderConstHandle* mAtlasScaleSC;

   // fadeStartLength.x = Distance in eye space to start fading shadows
   // fadeStartLength.y = 1 / Length of fade
   GFXShaderConstHandle* mFadeStartLength;
   GFXShaderConstHandle* mFarPlaneScalePSSM;
   GFXShaderConstHandle* mOverDarkFactorPSSM;
   GFXShaderConstHandle* mSplitFade;

   // This should be moved too.
   GFXShaderConstHandle* mConstantSpecularPowerSC;

   GFXShaderConstHandle* mTapRotationTexSC;

   LightingShaderConstants();
   ~LightingShaderConstants();

   void init(GFXShader* buffer);

   void _onShaderReload();
};

typedef Map<GFXShader*, LightingShaderConstants*> LightConstantMap;


/// This represents everything we need to render
/// the shadowmap for one light.
class LightShadowMap : public MatTextureTarget
{
public:

   const static GFXFormat ShadowMapFormat;

public:

   LightShadowMap( LightInfo *light );

   virtual ~LightShadowMap();

   void render(   SceneGraph *sceneManager, 
                  const SceneState *diffuseState );

   U32 getLastUpdate() const { return mLastUpdate; }

   //U32 getLastVisible() const { return mLastVisible; }

   bool isViewDependent() const { return mIsViewDependent; }

   bool wasOccluded() const { return mWasOccluded; }

   void preLightRender();

   void postLightRender();

   void updatePriority( const SceneState *state, U32 currTimeMs );

   F32 getLastScreenSize() const { return mLastScreenSize; }

   F32 getLastPriority() const { return mLastPriority; }

   virtual bool hasShadowTex() const { return mShadowMapTex.isValid(); }

   virtual bool setTextureStage(const SceneGraphData& sgData, const U32 currTexFlag, 
      const U32 textureSlot, GFXShaderConstBuffer* shaderConsts, 
      GFXShaderConstHandle* shadowMapSC);

   LightInfo* getLightInfo() { return mLight; }

   virtual void setShaderParameters(GFXShaderConstBuffer* params, LightingShaderConstants* lsc) = 0;

   U32 getTexSize() const { return mTexSize; }

   const MatrixF& getWorldToLightProj() const { return mWorldToLightProj; }

   static GFXTextureObject* _getDepthTarget( U32 width, U32 height );

   virtual ShadowType getShadowType() const = 0;

   // Cleanup texture resources
   virtual void releaseTextures();

   // MatTextureTarget
   virtual GFXTextureObject* getTargetTexture( U32 mrtIndex ) const { return mShadowMapTex.getPointer(); }
   virtual const RectI& getTargetViewport() const { return RectI::One; }
   virtual void setupSamplerState( GFXSamplerStateDesc *desc ) const { TORQUE_UNUSED( desc ); }
   virtual ConditionerFeature* getTargetConditioner() const { return NULL; }

   static void releaseAllTextures();

   static void releaseUnusedTextures();

   ///
   static S32 QSORT_CALLBACK cmpPriority( LightShadowMap *const *lsm1, LightShadowMap *const *lsm2 );

protected:

   /// All the shadow maps in the system.
   static Vector<LightShadowMap*> smShadowMaps;

   /// All the shadow maps that have been reciently rendered to.
   static Vector<LightShadowMap*> smUsedShadowMaps;

   virtual void _render(   SceneGraph *sceneManager, 
                           const SceneState *diffuseState ) = 0;

   /// If true the shadow is view dependent and cannot
   /// be skipped if visible and within active range.
   bool mIsViewDependent;

   /// The time this shadow was last updated.
   U32 mLastUpdate;

   /// The time this shadow/light was last updated.
   U32 mLastCull;

   /// The shadow occlusion query used when the light is
   /// rendered to determin if any pixel of it is visible.
   GFXOcclusionQuery *mVizQuery;

   /// If true the light was occluded by geometry the
   /// last frame it was updated.
   //the last frame.
   bool mWasOccluded;

   F32 mLastScreenSize;

   F32 mLastPriority;

   MatrixF mWorldToLightProj;

   GFXTextureTargetRef mTarget;
   U32 mTexSize;
   GFXTexHandle mShadowMapTex;

   // The light we are rendering.
   LightInfo *mLight;   

   // Used for blur
   GFXShader* mLastShader;
   GFXShaderConstHandle* mBlurBoundaries;

   // Calculate view matrices and set proper projection with GFX
   virtual void calcLightMatrices(MatrixF& outLightMatrix);

   Frustum _getFrustum() const;

   /// The callback used to get texture events.
   /// @see GFXTextureManager::addEventDelegate
   void _onTextureEvent( GFXTexCallbackCode code );  
};

GFX_DeclareTextureProfile( ShadowMapProfile );
GFX_DeclareTextureProfile( ShadowMapZProfile );


class ShadowMapParams : public LightInfoEx
{
public:

   ShadowMapParams( LightInfo *light );
   virtual ~ShadowMapParams();

   /// The LightInfoEx hook type.
   static const LightInfoExType Type;

   // LightInfoEx
   virtual void set( const LightInfoEx *ex );
   virtual const LightInfoExType& getType() const { return Type; }
   virtual void packUpdate( BitStream *stream ) const;
   virtual void unpackUpdate( BitStream *stream );

   LightShadowMap* getShadowMap() { return mShadowMap; }

   LightShadowMap* getOrCreateShadowMap();

   // Validates the parameters after a field is changed.
   void _validate();

protected:

   void _initShadowMap();

   ///
   LightShadowMap *mShadowMap;

   LightInfo *mLight;

public:

   // We're leaving these public for easy access 
   // for console protected fields.

   /// @name Shadow Map
   /// @{
   
   ///
   U32 texSize;

   /// @}

   Point3F attenuationRatio;

   /// @name Point Lights
   /// @{

   ///
   ShadowType shadowType;

   /// @}

   /// @name Exponential Shadow Map Parameters
   /// @{
   Point4F overDarkFactor;
   /// @}   

   /// @name Parallel Split Shadow Map
   /// @{

   ///
   F32 shadowDistance;  

   ///
   F32 shadowSoftness;

   /// The number of splits in the shadow map.
   U32 numSplits;

   /// 
   F32 logWeight;

   /// At what distance do we start fading the shadows out completely
   F32 fadeStartDist;

   /// This toggles only terrain being visible in the last
   /// split of a PSSM shadow map.
   bool lastSplitTerrainOnly;

   /// Distances (in world space units) that the shadows will fade between
   /// PSSM splits
   Point4F splitFadeDistances;

   /// @}
};

#endif // _LIGHTSHADOWMAP_H_
