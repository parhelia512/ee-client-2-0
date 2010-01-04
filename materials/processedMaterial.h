//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _MATERIALS_PROCESSEDMATERIAL_H_
#define _MATERIALS_PROCESSEDMATERIAL_H_

#ifndef _MATERIALDEFINITION_H_
#include "materials/materialDefinition.h"
#endif
#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif
#ifndef _MATTEXTURETARGET_H_
#include "materials/matTextureTarget.h"
#endif

class ShaderFeature;
class MaterialParameters;
class MaterialParameterHandle;
class SceneState;
class GFXVertexBufferHandleBase;
class GFXPrimitiveBufferHandle;
class MatrixSet;


/// This contains the common data needed to render a pass.
struct RenderPassData
{
public:

   struct TexSlotT
   {
      /// This is the default type of texture which 
      /// is valid with most texture types.
      /// @see mTexType
      GFXTexHandle texObject;

      /// Only valid when the texture type is set 
      /// to Material::TexTarget.
      /// @see mTexType
     MatTextureTargetRef texTarget;

   } mTexSlot[Material::MAX_TEX_PER_PASS];

   U32 mTexType[Material::MAX_TEX_PER_PASS];

   /// The cubemap to use when the texture type is
   /// set to Material::Cube.
   /// @see mTexType
   GFXCubemap *mCubeMap;

   U32 mNumTex;

   U32 mNumTexReg;

   MaterialFeatureData mFeatureData;

   bool mGlow;

   Material::BlendOp mBlendOp;

   U32 mStageNum;

   /// State permutations, used to index into 
   /// the render states array.
   /// @see mRenderStates
   enum 
   {
      STATE_REFLECT = 1,
      STATE_TRANSLUCENT = 2,
      STATE_GLOW = 4,      
      STATE_WIREFRAME = 8,
      STATE_MAX = 16
   };

   ///
   GFXStateBlockRef mRenderStates[STATE_MAX];

   RenderPassData();

   virtual ~RenderPassData() { reset(); }

   virtual void reset();
};

/// This is an abstract base class which provides the external
/// interface all subclasses must implement. This interface
/// primarily consists of setting state.  Pass creation
/// is implementation specific, and internal, thus it is
/// not in this base class.
class ProcessedMaterial
{
public:
   ProcessedMaterial();
   virtual ~ProcessedMaterial();

   /// @name State setting functions
   ///
   /// @{

   virtual void addStateBlockDesc(const GFXStateBlockDesc& sb);

   /// Set the user defined shader macros.
   virtual void setShaderMacros( const Vector<GFXShaderMacro> &macros ) { mUserMacros = macros; }

   /// Sets the textures needed for rendering the current pass
   virtual void setTextureStages(SceneState *, const SceneGraphData &sgData, U32 pass ) = 0;

   /// Sets the transformation matrix, i.e. Model * View * Projection
   virtual void setTransforms(const MatrixSet &matrixSet, SceneState *state, const U32 pass) = 0;
   
   /// Sets the scene info like lights for the given pass.
   virtual void setSceneInfo(SceneState *, const SceneGraphData& sgData, U32 pass) = 0;

   /// Sets the given vertex and primitive buffers so we can render geometry
   virtual void setBuffers(GFXVertexBufferHandleBase* vertBuffer, GFXPrimitiveBufferHandle* primBuffer); 

   /// @}

   /// Initializes us (eg. loads textures, creates passes, generates shaders)
   virtual bool init(   const FeatureSet& features, 
                        const GFXVertexFormat *vertexFormat,
                        const MatFeaturesDelegate &featuresDelegate ) = 0;

   /// Sets up the given pass.  Returns true if the pass was set up, false if there was an error or if
   /// the specified pass is out of bounds.
   virtual bool setupPass(SceneState *, const SceneGraphData& sgData, U32 pass) = 0;

   // Material parameter methods
   virtual MaterialParameters* allocMaterialParameters() = 0;
   virtual MaterialParameters* getDefaultMaterialParameters() = 0;
   virtual void setMaterialParameters(MaterialParameters* param, S32 pass) { mCurrentParams = param; }; 
   virtual MaterialParameters* getMaterialParameters() { return mCurrentParams; }
   virtual MaterialParameterHandle* getMaterialParameterHandle(const String& name) = 0;

   /// Cleans up the state and resources set by the given pass.
   virtual void cleanup(U32 pass);

   /// Returns the pass data for the given pass.
   RenderPassData* getPass(U32 pass)
   {
      if(pass >= mPasses.size())
         return NULL;
      return mPasses[pass];
   }

   /// Returns the pass data for the given pass (const version)
   const RenderPassData* getPass(U32 pass) const
   {
      return getPass(pass);
   }

   /// Returns the number of stages we're rendering (not to be confused with the number of passes).
   virtual U32 getNumStages() = 0;

   /// Returns the number of passes we are rendering (not to be confused with the number of stages).
   U32 getNumPasses()
   {
      return mPasses.size();
   }

   /// Returns true if any pass glows
   bool hasGlow() 
   { 
      return mHasGlow; 
   }

   /// Gets the stage number for a pass
   U32 getStageFromPass(U32 pass) const
   {
      if(pass >= mPasses.size())
         return 0;
      return mPasses[pass]->mStageNum;
   }

   /// Returns the active features in use by this material.
   /// @see BaseMatInstance::getFeatures
   const FeatureSet& getFeatures() const { return mFeatures; }

   /// Dump shader info, or FF texture info?
   virtual void dumpMaterialInfo() { }

   /// Returns the source material.
   Material* getMaterial() const { return mMaterial; }

protected:

   /// Our passes.
   Vector<RenderPassData*> mPasses;

   /// The active features in use by this material.
   FeatureSet mFeatures;

   /// The material which we are processing.
   Material* mMaterial;

   MaterialParameters* mCurrentParams;

   /// Material::StageData is used here because the shader 
   /// generator throws a fit if it's passed anything else.
   Material::StageData mStages[Material::MAX_STAGES];

   /// If we've already loaded the stage data
   bool mHasSetStageData;

   /// If we glow
   bool mHasGlow;

   /// Number of stages (not to be confused with number of passes)
   U32 mMaxStages;

   /// The vertex format on which this material will render.
   const GFXVertexFormat *mVertexFormat;

   ///  Set by addStateBlockDesc, should be considered 
   /// when initPassStateBlock is called.
   GFXStateBlockDesc mUserDefined;   

   /// The user defined macros to pass to the 
   /// shader initialization.
   Vector<GFXShaderMacro> mUserMacros;

   /// Loads all the textures for all of the stages in the Material
   virtual void _setStageData();

   /// Sets the blend state for rendering   
   void _setBlendState(Material::BlendOp blendOp, GFXStateBlockDesc& desc );

   /// Returns the path the material will attempt to load for a given texture filename.
   String _getTexturePath(const String& filename);

   /// Loads the texture located at _getTexturePath(filename) and gives it the specified profile
   GFXTexHandle _createTexture( const char *filename, GFXTextureProfile *profile );

   /// @name State blocks
   ///
   /// @{

   /// Creates the default state block templates, used by initStateBlocks
   virtual void _initStateBlockTemplates(GFXStateBlockDesc& stateTranslucent, GFXStateBlockDesc& stateGlow, GFXStateBlockDesc& stateReflect);

   /// Does the base render state block setting, normally per pass
   virtual void _initPassStateBlock(const Material::BlendOp blendOp, U32 numTex, const U32 texFlags[Material::MAX_TEX_PER_PASS], GFXStateBlockDesc& result);

   /// Creates the default state blocks for a list of render states
   virtual void _initRenderStateStateBlocks(const Material::BlendOp blendOp, U32 numTex, const U32 texFlags[Material::MAX_TEX_PER_PASS], GFXStateBlockRef renderStates[RenderPassData::STATE_MAX]);

   /// Creates the default state blocks for each RenderPassData item
   virtual void _initRenderPassDataStateBlocks();

   /// This returns the index into the renderState array based on the sgData passed in.
   virtual U32 _getRenderStateIndex(   const SceneState *state, 
                                       const SceneGraphData &sgData );

   /// Activates the correct mPasses[currPass].renderState based on scene graph info
   virtual void _setRenderState( const SceneState *state, 
                                 const SceneGraphData &sgData, 
                                 U32 pass );
   /// @
};

#endif
