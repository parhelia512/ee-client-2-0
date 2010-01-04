//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TERRCELLMATERIAL_H_
#define _TERRCELLMATERIAL_H_

#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _MATTEXTURETARGET_H_
#include "materials/matTextureTarget.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _GFXSHADER_H_
#include "gfx/gfxShader.h"
#endif
#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif


class SceneState;
struct SceneGraphData;
class TerrainMaterial;
class TerrainBlock;


/// This is a complex material which holds one or more
/// optimized shaders for rendering a single cell.
class TerrainCellMaterial
{
protected:

   class MaterialInfo
   {
   public:

      MaterialInfo()
      {
      }

      ~MaterialInfo() 
      {
      }

      TerrainMaterial *mat;
      U32 layerId;

      GFXShaderConstHandle *detailTexConst;
      GFXTexHandle detailTex;

      GFXShaderConstHandle *normalTexConst;
      GFXTexHandle normalTex;

      GFXShaderConstHandle *detailInfoVConst;
      GFXShaderConstHandle *detailInfoPConst;
   };

   class Pass
   {
   public:

      Pass() 
         :  shader( NULL )                     
      {
      }

      ~Pass() 
      {
         for ( U32 i=0; i < materials.size(); i++ )
            delete materials[i];
      }

      Vector<MaterialInfo*> materials;

      ///
      GFXShader *shader;

      GFXShaderConstBufferRef consts;

      GFXStateBlockRef stateBlock;
      GFXStateBlockRef reflectStateBlock;
      GFXStateBlockRef wireframeStateBlock;

      GFXShaderConstHandle *modelViewProjConst;
      GFXShaderConstHandle *worldViewOnly;
      GFXShaderConstHandle *viewToObj;

      GFXShaderConstHandle *eyePosWorldConst;
      GFXShaderConstHandle *eyePosConst;

      GFXShaderConstHandle *objTransConst;
      GFXShaderConstHandle *worldToObjConst;
      GFXShaderConstHandle *vEyeConst;

      GFXShaderConstHandle *layerSizeConst;
      GFXShaderConstHandle *lightParamsConst;
      GFXShaderConstHandle *lightInfoBufferConst;

      GFXShaderConstHandle *baseTexMapConst;
      GFXShaderConstHandle *layerTexConst;

      GFXShaderConstHandle *lightMapTexConst;

      GFXShaderConstHandle *squareSize;
      GFXShaderConstHandle *oneOverTerrainSize;

      GFXShaderConstHandle *fogDataConst;
      GFXShaderConstHandle *fogColorConst;
   };

   TerrainBlock *mTerrain;

   U64 mMaterials;

   Vector<Pass> mPasses;

   U32 mCurrPass;

   GFXTexHandle mBaseMapTexture;

   GFXTexHandle mLayerMapTexture;

   MatTextureTargetRef mLightInfoTarget;

   /// The prepass material for this material.
   TerrainCellMaterial *mPrePassMat;


   bool _createPass( Vector<MaterialInfo*> *materials, 
                     Pass *pass, 
                     bool firstPass,
                     bool prePass,
                     bool baseOnly );

   void _updateMaterialConsts( Pass *pass );

public:
   
   TerrainCellMaterial();
   ~TerrainCellMaterial();

   void init(  TerrainBlock *block, 
               U64 activeMaterials,
               bool prePass = false,
               bool baseOnly = false );

   /// Returns a prepass material from this material.
   TerrainCellMaterial* getPrePass();

   void setTransformAndEye(   const MatrixF &modelXfm, 
                              const MatrixF &viewXfm,
                              const MatrixF &projectXfm,
                              F32 farPlane );


   ///
   bool setupPass(   const SceneState *state,
                     const SceneGraphData &sceneData );

};

#endif // _TERRCELLMATERIAL_H_

