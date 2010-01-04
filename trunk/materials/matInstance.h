//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _MATINSTANCE_H_
#define _MATINSTANCE_H_

#ifndef _MATERIALDEFINITION_H_
#include "materials/materialDefinition.h"
#endif
#ifndef _BASEMATINSTANCE_H_
#include "materials/baseMatInstance.h"
#endif
#ifndef _SCENEDATA_H_
#include "materials/sceneData.h"
#endif
#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif

class GFXShader;
class GFXCubemap;
class ShaderFeature;
class MatInstanceParameterHandle;
class MatInstParameters;
class ProcessedMaterial;


///
class MatInstance : public BaseMatInstance
{   
public:
   virtual ~MatInstance();

   // BaseMatInstance
   virtual bool init(   const FeatureSet &features, 
                        const GFXVertexFormat *vertexFormat );
   virtual bool reInit();
   virtual void addStateBlockDesc(const GFXStateBlockDesc& desc);
   virtual void addShaderMacro( const String &name, const String &value );
   virtual MaterialParameters* allocMaterialParameters();
   virtual void setMaterialParameters(MaterialParameters* param); 
   virtual MaterialParameters* getMaterialParameters();
   virtual MaterialParameterHandle* getMaterialParameterHandle(const String& name);
   virtual bool setupPass(SceneState *, const SceneGraphData &sgData );
   virtual void setTransforms(const MatrixSet &matrixSet, SceneState *state);
   virtual void setSceneInfo(SceneState *, const SceneGraphData& sgData);
   virtual void setTextureStages(SceneState * state, const SceneGraphData &sgData );
   virtual void setBuffers(GFXVertexBufferHandleBase* vertBuffer, GFXPrimitiveBufferHandle* primBuffer);
   virtual Material *getMaterial() { return mMaterial; }
   virtual bool hasGlow();
   virtual U32 getCurPass() { return getMax( mCurPass, 0 ); }
   virtual U32 getCurStageNum();
   virtual RenderPassData *getPass(U32 pass);   
   virtual const GFXVertexFormat* getVertexFormat() const { return mVertexFormat; }
   virtual const FeatureSet& getFeatures() const;
   virtual const FeatureSet& getRequestedFeatures() const { return mFeatureList; }
   virtual void dumpShaderInfo() const;

   ProcessedMaterial *getProcessedMaterial() const { return mProcessedMaterial; }

protected:

   friend class Material;

   /// Create a material instance by reference to a Material.
   MatInstance( Material &mat );

   virtual bool processMaterial();
   virtual ProcessedMaterial* getShaderMaterial();

   Material* mMaterial;
   ProcessedMaterial* mProcessedMaterial;

   /// The features requested at material creation time.
   FeatureSet mFeatureList;

   /// The vertex format on which this material will render.
   const GFXVertexFormat *mVertexFormat;

   S32               mCurPass;
   U32               mMaxStages;

   GFXStateBlockDesc mUserDefinedState;

   Vector<GFXShaderMacro> mUserMacros;
   
   Vector<MatInstanceParameterHandle*> mCurrentHandles;
   Vector<MatInstParameters*> mCurrentParameters;
   MatInstParameters* mActiveParameters;
   MatInstParameters* mDefaultParameters;
private:
   void construct();  
};

//
// MatInstParameters 
//
class MatInstParameters : public MaterialParameters
{
public:
   MatInstParameters();
   MatInstParameters(MaterialParameters* matParams);
   virtual ~MatInstParameters();
   
   void loadParameters(ProcessedMaterial* pmat);

   /// Returns our list of shader constants, the material can get this and just set the constants it knows about
   virtual const Vector<GFXShaderConstDesc>& getShaderConstDesc() const;

   /// @name Set shader constant values
   /// @{
   /// Actually set shader constant values
   /// @param name Name of the constant, this should be a name contained in the array returned in getShaderConstDesc,
   /// if an invalid name is used, it is ignored.
   virtual void set(MaterialParameterHandle* handle, const F32 f);
   virtual void set(MaterialParameterHandle* handle, const Point2F& fv);
   virtual void set(MaterialParameterHandle* handle, const Point3F& fv);
   virtual void set(MaterialParameterHandle* handle, const Point4F& fv);
   virtual void set(MaterialParameterHandle* handle, const ColorF& fv);
   virtual void set(MaterialParameterHandle* handle, const S32 f);
   virtual void set(MaterialParameterHandle* handle, const Point2I& fv);
   virtual void set(MaterialParameterHandle* handle, const Point3I& fv);
   virtual void set(MaterialParameterHandle* handle, const Point4I& fv);
   virtual void set(MaterialParameterHandle* handle, const AlignedArray<F32>& fv);
   virtual void set(MaterialParameterHandle* handle, const AlignedArray<Point2F>& fv);
   virtual void set(MaterialParameterHandle* handle, const AlignedArray<Point3F>& fv);
   virtual void set(MaterialParameterHandle* handle, const AlignedArray<Point4F>& fv);
   virtual void set(MaterialParameterHandle* handle, const AlignedArray<S32>& fv);
   virtual void set(MaterialParameterHandle* handle, const AlignedArray<Point2I>& fv);
   virtual void set(MaterialParameterHandle* handle, const AlignedArray<Point3I>& fv);
   virtual void set(MaterialParameterHandle* handle, const AlignedArray<Point4I>& fv);
   virtual void set(MaterialParameterHandle* handle, const MatrixF& mat, const GFXShaderConstType matrixType = GFXSCT_Float4x4);
   virtual void set(MaterialParameterHandle* handle, const MatrixF* mat, const U32 arraySize, const GFXShaderConstType matrixType = GFXSCT_Float4x4);
   virtual U32 getAlignmentValue(const GFXShaderConstType constType);
private:
   MaterialParameters* mParameters;
   bool mOwnParameters;
};


#endif // _MATINSTANCE_H_
