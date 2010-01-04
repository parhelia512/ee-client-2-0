//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/processedMaterial.h"

#include "materials/sceneData.h"
#include "materials/materialParameters.h"
#include "materials/matTextureTarget.h"
#include "materials/materialFeatureTypes.h"
#include "materials/materialManager.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/sim/cubemapData.h"


//-----------------------------------------------------------------------------
// RenderPassData
//-----------------------------------------------------------------------------
RenderPassData::RenderPassData()
{
   reset();
}

void RenderPassData::reset()
{
   for( U32 i = 0; i < Material::MAX_TEX_PER_PASS; ++ i )
      destructInPlace( &mTexSlot[ i ] );

   dMemset( &mTexSlot, 0, sizeof(mTexSlot) );
   dMemset( &mTexType, 0, sizeof(mTexType) );

   mCubeMap = NULL;
   mNumTex = mNumTexReg = mStageNum = 0;
   mGlow = false;
   mBlendOp = Material::None;

   mFeatureData.clear();

   for (U32 i = 0; i < STATE_MAX; i++)
      mRenderStates[i] = NULL;
}

//-----------------------------------------------------------------------------
// ProcessedMaterial
//-----------------------------------------------------------------------------
ProcessedMaterial::ProcessedMaterial()
:  mMaterial( NULL ),
   mCurrentParams( NULL ),
   mHasSetStageData( false ),
   mHasGlow( false ),   
   mMaxStages( 0 ),
   mVertexFormat( NULL )
{
   VECTOR_SET_ASSOCIATION( mPasses );
}

ProcessedMaterial::~ProcessedMaterial()
{
   for_each( mPasses.begin(), mPasses.end(), delete_pointer() );
}

void ProcessedMaterial::_setBlendState(Material::BlendOp blendOp, GFXStateBlockDesc& desc )
{
   switch( blendOp )
   {
   case Material::Add:
      {
         desc.blendSrc = GFXBlendOne;
         desc.blendDest = GFXBlendOne;
         break;
      }
   case Material::AddAlpha:
      {
         desc.blendSrc = GFXBlendSrcAlpha;
         desc.blendDest = GFXBlendOne;
         break;
      }
   case Material::Mul:
      {
         desc.blendSrc = GFXBlendDestColor;
         desc.blendDest = GFXBlendZero;
         break;
      }
   case Material::LerpAlpha:
      {
         desc.blendSrc = GFXBlendSrcAlpha;
         desc.blendDest = GFXBlendInvSrcAlpha;
         break;
      }

   default:
      {
         // default to LerpAlpha
         desc.blendSrc = GFXBlendSrcAlpha;
         desc.blendDest = GFXBlendInvSrcAlpha;
         break;
      }
   }
}

void ProcessedMaterial::setBuffers(GFXVertexBufferHandleBase* vertBuffer, GFXPrimitiveBufferHandle* primBuffer)
{
   GFX->setVertexBuffer( *vertBuffer );
   GFX->setPrimitiveBuffer( *primBuffer );
}

String ProcessedMaterial::_getTexturePath(const String& filename)
{
   // if '/', then path is specified, use it.
   if( filename.find('/') != String::NPos )
   {
      return filename;
   }

   // otherwise, construct path
   return mMaterial->getPath() + filename;
}

GFXTexHandle ProcessedMaterial::_createTexture( const char* filename, GFXTextureProfile *profile)
{
   return GFXTexHandle( _getTexturePath(filename), profile, avar("%s() - NA (line %d)", __FUNCTION__, __LINE__) );
}

void ProcessedMaterial::addStateBlockDesc(const GFXStateBlockDesc& sb)
{
   mUserDefined = sb;
}

/// Creates the default state block templates, used by initStateBlocks
void ProcessedMaterial::_initStateBlockTemplates(GFXStateBlockDesc& stateTranslucent, GFXStateBlockDesc& stateGlow, GFXStateBlockDesc& stateReflect)
{
   // Translucency   
   stateTranslucent.blendDefined = true;
   stateTranslucent.blendEnable = mMaterial->mTranslucentBlendOp != Material::None;
   _setBlendState(mMaterial->mTranslucentBlendOp, stateTranslucent);
   stateTranslucent.zDefined = true;
   stateTranslucent.zWriteEnable = mMaterial->mTranslucentZWrite;   
   stateTranslucent.alphaDefined = true;
   stateTranslucent.alphaTestEnable = mMaterial->mAlphaTest;
   stateTranslucent.alphaTestRef = mMaterial->mAlphaRef;
   stateTranslucent.alphaTestFunc = GFXCmpGreaterEqual;
   stateTranslucent.samplersDefined = true;
   stateTranslucent.samplers[0].textureColorOp = GFXTOPModulate;
   stateTranslucent.samplers[0].alphaOp = GFXTOPModulate;   
   stateTranslucent.samplers[0].alphaArg1 = GFXTATexture;
   stateTranslucent.samplers[0].alphaArg2 = GFXTADiffuse;   

   // Glow   
   stateGlow.zDefined = true;
   stateGlow.zWriteEnable = false;

   // Reflect   
   stateReflect.cullDefined = true;
   stateReflect.cullMode = mMaterial->mDoubleSided ? GFXCullNone : GFXCullCW;
}

/// Creates the default state blocks for each RenderPassData item
void ProcessedMaterial::_initRenderPassDataStateBlocks()
{
   for (U32 pass = 0; pass < mPasses.size(); pass++)
   {
      RenderPassData* rpd = mPasses[pass];
      _initRenderStateStateBlocks(rpd->mBlendOp, rpd->mNumTex, rpd->mTexType, rpd->mRenderStates);
   }
}

/// Does the base render state block setting, normally per pass
void ProcessedMaterial::_initPassStateBlock(const Material::BlendOp blendOp, U32 numTex, const U32 texFlags[Material::MAX_TEX_PER_PASS], GFXStateBlockDesc& result)
{
   if (blendOp != Material::None)
   {
      result.blendDefined = true;
      result.blendEnable = true;
      _setBlendState(blendOp, result);
   }

   if (mMaterial->isDoubleSided())
   {
      result.cullDefined = true;
      result.cullMode = GFXCullNone;         
   }

   if(mMaterial->mAlphaTest)
   {
      result.alphaDefined = true;
      result.alphaTestEnable = mMaterial->mAlphaTest;
      result.alphaTestRef = mMaterial->mAlphaRef;
      result.alphaTestFunc = GFXCmpGreaterEqual;
   }

   result.samplersDefined = true;
   MatTextureTarget *texTarget;

   for( U32 i=0; i < numTex; i++ )
   {      
      U32 currTexFlag = texFlags[i];

      switch( currTexFlag )
      {
         case 0:
            result.samplers[i].textureColorOp = GFXTOPModulate;
            result.samplers[i].addressModeU = GFXAddressWrap;
            result.samplers[i].addressModeV = GFXAddressWrap;
            break;

         case Material::TexTarget:
         {
            texTarget = mPasses[0]->mTexSlot[i].texTarget;
            if ( texTarget )
               texTarget->setupSamplerState( &result.samplers[i] );
            break;
         }
      }
   }

   // The prepass will take care of writing to the 
   // zbuffer, so we don't have to by default.
   // The prepass can't write to the backbuffer's zbuffer in OpenGL.
   if ( MATMGR->getPrePassEnabled() && !GFX->getAdapterType() == OpenGL )
      result.setZReadWrite( result.zEnable, false );

   result.addDesc(mUserDefined);
}

/// Creates the default state blocks for a list of render states
void ProcessedMaterial::_initRenderStateStateBlocks(const Material::BlendOp blendOp, U32 numTex, const U32 texFlags[Material::MAX_TEX_PER_PASS], GFXStateBlockRef renderStates[RenderPassData::STATE_MAX])
{
   GFXStateBlockDesc stateTranslucent;
   GFXStateBlockDesc stateGlow;
   GFXStateBlockDesc stateReflect;
   GFXStateBlockDesc statePass;

   _initStateBlockTemplates(stateTranslucent, stateGlow, stateReflect);
   _initPassStateBlock(blendOp, numTex, texFlags, statePass);

   // Ok, we've got our templates set up, let's combine them together based on state and
   // create our state blocks.
   for (U32 i = 0; i < RenderPassData::STATE_MAX; i++)
   {
      GFXStateBlockDesc stateFinal;

      if (i & RenderPassData::STATE_REFLECT)
         stateFinal.addDesc(stateReflect);
      if (i & RenderPassData::STATE_TRANSLUCENT)
         stateFinal.addDesc(stateTranslucent);
      if (i & RenderPassData::STATE_GLOW)
         stateFinal.addDesc(stateGlow);

      stateFinal.addDesc(statePass);

      if (i & RenderPassData::STATE_WIREFRAME)
         stateFinal.fillMode = GFXFillWireframe;

      GFXStateBlockRef sb = GFX->createStateBlock(stateFinal);
      renderStates[i] = sb;
   }   
}

U32 ProcessedMaterial::_getRenderStateIndex( const SceneState *sceneState, 
                                             const SceneGraphData &sgData )
{
   // Based on what the state of the world is, get our render state block
   U32 currState = 0;

   if ( sgData.binType == SceneGraphData::GlowBin )
      currState |= RenderPassData::STATE_GLOW;

   if ( sceneState && sceneState->isReflectPass() )
      currState |= RenderPassData::STATE_REFLECT;

   if( mMaterial->isTranslucent() || sgData.visibility < 1.0f )
      currState |= RenderPassData::STATE_TRANSLUCENT;

   if ( sgData.wireframe )
      currState |= RenderPassData::STATE_WIREFRAME;

   return currState;
}

void ProcessedMaterial::_setRenderState(  const SceneState *state, 
                                          const SceneGraphData& sgData, 
                                          U32 pass )
{   
   // Make sure we have the pass
   if ( pass >= mPasses.size() )
      return;

   U32 currState = _getRenderStateIndex( state, sgData );

   GFX->setStateBlock(mPasses[pass]->mRenderStates[currState]);   
}


void ProcessedMaterial::_setStageData()
{
   // Only do this once
   if ( mHasSetStageData ) 
      return;
   mHasSetStageData = true;

   U32 i;

   // Load up all the textures for every possible stage
   for( i=0; i<Material::MAX_STAGES; i++ )
   {
      // DiffuseMap
      if( mMaterial->mDiffuseMapFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_DiffuseMap, _createTexture( mMaterial->mDiffuseMapFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_DiffuseMap ))
            mMaterial->logError("Failed to load diffuse map %s for stage %i", _getTexturePath(mMaterial->mDiffuseMapFilename[i]).c_str(), i);
      }
      else if( mMaterial->mBaseTexFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_DiffuseMap, _createTexture( mMaterial->mBaseTexFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_DiffuseMap ))
            mMaterial->logError("Failed to load diffuse map %s for stage %i", _getTexturePath(mMaterial->mBaseTexFilename[i]).c_str(), i);
      }

      // OverlayMap
      if( mMaterial->mOverlayMapFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_OverlayMap, _createTexture( mMaterial->mOverlayMapFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_OverlayMap ))
            mMaterial->logError("Failed to load overlay map %s for stage %i", _getTexturePath(mMaterial->mOverlayMapFilename[i]).c_str(), i);
      }
      else if( mMaterial->mOverlayTexFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_OverlayMap, _createTexture( mMaterial->mOverlayTexFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_OverlayMap ))
            mMaterial->logError("Failed to load overlay map %s for stage %i", _getTexturePath(mMaterial->mOverlayTexFilename[i]).c_str(), i);
      }

      // LightMap
      if( mMaterial->mLightMapFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_LightMap, _createTexture( mMaterial->mLightMapFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_LightMap ))
            mMaterial->logError("Failed to load light map %s for stage %i", _getTexturePath(mMaterial->mLightMapFilename[i]).c_str(), i);
      }

      // ToneMap
      if( mMaterial->mToneMapFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_ToneMap, _createTexture( mMaterial->mToneMapFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_ToneMap ))
            mMaterial->logError("Failed to load tone map %s for stage %i", _getTexturePath(mMaterial->mToneMapFilename[i]).c_str(), i);
      }

      // DetailMap
      if( mMaterial->mDetailMapFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_DetailMap, _createTexture( mMaterial->mDetailMapFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_DetailMap ))
            mMaterial->logError("Failed to load detail map %s for stage %i", _getTexturePath(mMaterial->mDetailMapFilename[i]).c_str(), i);
      }
      else if( mMaterial->mDetailTexFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_DetailMap, _createTexture( mMaterial->mDetailTexFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_DetailMap ))
            mMaterial->logError("Failed to load detail map %s for stage %i", _getTexturePath(mMaterial->mDetailTexFilename[i]).c_str(), i);
      }

      // NormalMap
      if( mMaterial->mNormalMapFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_NormalMap, _createTexture( mMaterial->mNormalMapFilename[i], &GFXDefaultStaticNormalMapProfile ) );
         if(!mStages[i].getTex( MFT_NormalMap ))
            mMaterial->logError("Failed to load normal map %s for stage %i", _getTexturePath(mMaterial->mNormalMapFilename[i]).c_str(), i);
      }
      else if( mMaterial->mBumpTexFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_NormalMap, _createTexture( mMaterial->mBumpTexFilename[i], &GFXDefaultStaticNormalMapProfile ) );
         if(!mStages[i].getTex( MFT_NormalMap ))
            mMaterial->logError("Failed to load normal map %s for stage %i", _getTexturePath(mMaterial->mBumpTexFilename[i]).c_str(), i);
      }

      // SpecularMap
      if( mMaterial->mSpecularMapFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_SpecularMap, _createTexture( mMaterial->mSpecularMapFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_SpecularMap ))
            mMaterial->logError("Failed to load specular map %s for stage %i", _getTexturePath(mMaterial->mSpecularMapFilename[i]).c_str(), i);
      }

      // EnironmentMap
      if( mMaterial->mEnvMapFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_EnvMap, _createTexture( mMaterial->mEnvMapFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_EnvMap ))
            mMaterial->logError("Failed to load environment map %s for stage %i", _getTexturePath(mMaterial->mEnvMapFilename[i]).c_str(), i);
      }
      else if( mMaterial->mEnvTexFilename[i].isNotEmpty() )
      {
         mStages[i].setTex( MFT_EnvMap, _createTexture( mMaterial->mEnvTexFilename[i], &GFXDefaultStaticDiffuseProfile ) );
         if(!mStages[i].getTex( MFT_EnvMap ))
            mMaterial->logError("Failed to load environment map %s for stage %i", _getTexturePath(mMaterial->mEnvTexFilename[i]).c_str(), i);
      }
   }

	mMaterial->mCubemapData = dynamic_cast<CubemapData*>(Sim::findObject( mMaterial->mCubemapName ));
	if( !mMaterial->mCubemapData )
		mMaterial->mCubemapData = NULL;
		
		
   // If we have a cubemap put it on stage 0 (cubemaps only supported on stage 0)
   if( mMaterial->mCubemapData )
   {
      mMaterial->mCubemapData->createMap();
      mStages[0].setCubemap( mMaterial->mCubemapData->mCubemap ); 
      if ( !mStages[0].getCubemap() )
         mMaterial->logError("Failed to load cubemap");
   }
}

void ProcessedMaterial::cleanup(U32 pass)
{   
}
