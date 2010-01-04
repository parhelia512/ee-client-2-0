//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MATERIALS_PROCESSEDSHADERMATERIAL_H_
#define _MATERIALS_PROCESSEDSHADERMATERIAL_H_

#ifndef _MATERIALS_PROCESSEDMATERIAL_H_
#include "processedMaterial.h"
#endif
#ifndef _BASEMATINSTANCE_H_
#include "materials/baseMatInstance.h"
#endif
#ifndef _GFXSHADER_H_
#include "gfx/gfxShader.h"
#endif

class GenericConstBufferLayout;
class ShaderData;
class LightInfo;
class ShaderMaterialParameterHandle;
class ShaderFeatureConstHandles;


class ShaderConstHandles
{
public:
   GFXShaderConstHandle* mDiffuseColorSC;
   GFXShaderConstHandle* mBumpMapTexSC;
   GFXShaderConstHandle* mLightMapTexSC;
   GFXShaderConstHandle* mLightNormMapTexSC;
   GFXShaderConstHandle* mCubeMapTexSC;
   GFXShaderConstHandle* mFogMapTexSC;
   GFXShaderConstHandle* mToneMapTexSC;
   GFXShaderConstHandle* mTexMatSC;
   GFXShaderConstHandle* mSpecularColorSC;
   GFXShaderConstHandle* mSpecularPowerSC;
   GFXShaderConstHandle* mParallaxInfoSC;
   GFXShaderConstHandle* mFogDataSC;
   GFXShaderConstHandle* mFogColorSC;   
   GFXShaderConstHandle* mDetailScaleSC;
   GFXShaderConstHandle* mVisiblitySC;
   GFXShaderConstHandle* mColorMultiplySC;
   GFXShaderConstHandle* mAlphaTestValueSC;
   GFXShaderConstHandle* mModelViewProjSC;
   GFXShaderConstHandle* mWorldViewOnlySC;     
   GFXShaderConstHandle* mWorldToCameraSC;
   GFXShaderConstHandle* mWorldToObjSC;         
   GFXShaderConstHandle* mViewToObjSC;         
   GFXShaderConstHandle* mCubeTransSC;
   GFXShaderConstHandle* mObjTransSC;
   GFXShaderConstHandle* mCubeEyePosSC;
   GFXShaderConstHandle* mEyePosSC;
   GFXShaderConstHandle* mEyePosWorldSC;
   GFXShaderConstHandle* m_vEyeSC;
   GFXShaderConstHandle* mEyeMatSC;
   GFXShaderConstHandle* mOneOverFarplane;
   GFXShaderConstHandle* mAccumTimeSC;
   GFXShaderConstHandle* mMinnaertConstantSC;
   GFXShaderConstHandle* mSubSurfaceParamsSC;

   GFXShaderConstHandle* mTexHandlesSC[TEXTURE_STAGE_COUNT];
   GFXShaderConstHandle* mRTParamsSC[TEXTURE_STAGE_COUNT];

   void init( GFXShader* shader, ShaderData* sd = NULL );
};

class ShaderRenderPassData : public RenderPassData
{
   typedef RenderPassData Parent;

public:

   GFXShaderRef         shader;
   ShaderConstHandles   shaderHandles;
   Vector<ShaderFeatureConstHandles*> featureShaderHandles;

   virtual void reset();
};

class ProcessedShaderMaterial : public ProcessedMaterial
{
   typedef ProcessedMaterial Parent;
public:

   ProcessedShaderMaterial();
   ProcessedShaderMaterial(Material &mat);
   ~ProcessedShaderMaterial();

   virtual void dumpMaterialInfo();

   /// @name Render state setting
   ///
   /// @{

   /// Binds all of the necessary textures for the given pass
   virtual void setTextureStages(SceneState *, const SceneGraphData &sgData, U32 pass );

   /// Sets the transformation matrices
   virtual void setTransforms(const MatrixSet &matrixSet, SceneState *state, const U32 pass);

   /// Sets the scene info for a given pass
   virtual void setSceneInfo(SceneState *, const SceneGraphData& sgData, U32 pass);

   /// @}

   virtual bool init(   const FeatureSet &features, 
                        const GFXVertexFormat *vertexFormat,
                        const MatFeaturesDelegate &featuresDelegate );

   /// Sets up the given pass for rendering
   virtual bool setupPass(SceneState *, const SceneGraphData& sgData, U32 pass);

   /// Cleans up the given pass (or will anyways).
   virtual void cleanup(U32 pass);

   /// Returns a shader constant block
   virtual MaterialParameters* allocMaterialParameters();    
   virtual MaterialParameters* getDefaultMaterialParameters() { return mDefaultParameters; }
   
   virtual MaterialParameterHandle* getMaterialParameterHandle(const String& name);

   /// Gets the number of stages we're using (not to be confused with the number of passes!)
   virtual U32 getNumStages();

protected:
   Vector<GFXShaderConstDesc> mShaderConstDesc;
   MaterialParameters* mDefaultParameters;
   Vector<ShaderMaterialParameterHandle*> mParameterHandles;

   /// @name Internal functions
   ///
   /// @{

   /// Adds a pass for the given stage.
   virtual bool _addPass( ShaderRenderPassData &rpd, 
      U32 &texIndex, 
      MaterialFeatureData &fd,
      U32 stageNum,
      const FeatureSet &features);

   /// Chooses a blend op for the given pass
   virtual void _setPassBlendOp( ShaderFeature *sf,
      ShaderRenderPassData &passData,
      U32 &texIndex,
      MaterialFeatureData &stageFeatures,
      U32 stageNum,
      const FeatureSet &features);

   /// Creates passes for the given stage
   virtual bool _createPasses( MaterialFeatureData &fd, U32 stageNum, const FeatureSet &features );

   /// Fills in the MaterialFeatureData for the given stage
   virtual void _determineFeatures( U32 stageNum, 
                                    MaterialFeatureData &fd, 
                                    const FeatureSet &features );

   /// Do we have a cubemap on pass?
   virtual bool _hasCubemap(U32 pass);

   /// Used by setTextureTransforms
   F32 _getWaveOffset( U32 stage );

   /// Sets texture transformation matrices for texture animations such as scale and wave
   virtual void _setTextureTransforms(const U32 pass);

   /// Sets all of the necessary shader constants for the given pass
   virtual void _setShaderConstants(SceneState *, const SceneGraphData &sgData, U32 pass);

   /// @}

   void _setPrimaryLightConst(const LightInfo* light, const MatrixF& objTrans, const U32 stageNum);

   /// This is here to deal with the differences between ProcessedCustomMaterials and ProcessedShaderMaterials.
   virtual GFXShaderConstBuffer* _getShaderConstBuffer(const U32 pass);
   virtual ShaderConstHandles* _getShaderConstHandles(const U32 pass);

   ///
   virtual void _initMaterialParameters();

   ShaderRenderPassData* _getRPD(const U32 pass) { return static_cast<ShaderRenderPassData*>(mPasses[pass]); }
};

#endif
