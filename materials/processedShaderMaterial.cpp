//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/processedShaderMaterial.h"

#include "core/util/safeDelete.h"
#include "gfx/sim/cubemapData.h"
#include "gfx/gfxShader.h"
#include "gfx/genericConstBuffer.h"
#include "sceneGraph/sceneState.h"
#include "shaderGen/shaderFeature.h"
#include "shaderGen/shaderGenVars.h"
#include "shaderGen/featureMgr.h"
#include "shaderGen/shaderGen.h"
#include "materials/shaderData.h"
#include "materials/sceneData.h"
#include "materials/materialFeatureTypes.h"
#include "materials/materialManager.h"
#include "materials/shaderMaterialParameters.h"
#include "materials/matTextureTarget.h"
#include "gfx/util/screenspace.h"
#include "math/util/matrixSet.h"

///
/// ShaderConstHandles
///
void ShaderConstHandles::init( GFXShader *shader, ShaderData* sd /*=NULL*/ )
{
   mDiffuseColorSC = shader->getShaderConstHandle("$diffuseMaterialColor");
   mBumpMapTexSC = shader->getShaderConstHandle(ShaderGenVars::bumpMap);
   mLightMapTexSC = shader->getShaderConstHandle(ShaderGenVars::lightMap);
   mLightNormMapTexSC = shader->getShaderConstHandle(ShaderGenVars::lightNormMap);
   mCubeMapTexSC = shader->getShaderConstHandle(ShaderGenVars::cubeMap);
   mTexMatSC = shader->getShaderConstHandle(ShaderGenVars::texMat);
   mToneMapTexSC = shader->getShaderConstHandle(ShaderGenVars::toneMap);
   mSpecularColorSC = shader->getShaderConstHandle(ShaderGenVars::specularColor);
   mSpecularPowerSC = shader->getShaderConstHandle(ShaderGenVars::specularPower);
   mParallaxInfoSC = shader->getShaderConstHandle("$parallaxInfo");
   mFogDataSC = shader->getShaderConstHandle(ShaderGenVars::fogData);
   mFogColorSC = shader->getShaderConstHandle(ShaderGenVars::fogColor);
   mDetailScaleSC = shader->getShaderConstHandle(ShaderGenVars::detailScale);
   mVisiblitySC = shader->getShaderConstHandle(ShaderGenVars::visibility);
   mColorMultiplySC = shader->getShaderConstHandle(ShaderGenVars::colorMultiply);
   mAlphaTestValueSC = shader->getShaderConstHandle(ShaderGenVars::alphaTestValue);
   mModelViewProjSC = shader->getShaderConstHandle(ShaderGenVars::modelview);
   mWorldViewOnlySC = shader->getShaderConstHandle(ShaderGenVars::worldViewOnly);
   mWorldToCameraSC = shader->getShaderConstHandle(ShaderGenVars::worldToCamera);
   mWorldToObjSC = shader->getShaderConstHandle(ShaderGenVars::worldToObj);
   mViewToObjSC = shader->getShaderConstHandle(ShaderGenVars::viewToObj);
   mCubeTransSC = shader->getShaderConstHandle(ShaderGenVars::cubeTrans);
   mObjTransSC = shader->getShaderConstHandle(ShaderGenVars::objTrans);
   mCubeEyePosSC = shader->getShaderConstHandle(ShaderGenVars::cubeEyePos);
   mEyePosSC = shader->getShaderConstHandle(ShaderGenVars::eyePos);
   mEyePosWorldSC = shader->getShaderConstHandle(ShaderGenVars::eyePosWorld);
   m_vEyeSC = shader->getShaderConstHandle(ShaderGenVars::vEye);
   mEyeMatSC = shader->getShaderConstHandle(ShaderGenVars::eyeMat);
   mOneOverFarplane = shader->getShaderConstHandle(ShaderGenVars::oneOverFarplane);
   mAccumTimeSC = shader->getShaderConstHandle(ShaderGenVars::accumTime);
   mMinnaertConstantSC = shader->getShaderConstHandle(ShaderGenVars::minnaertConstant);
   mSubSurfaceParamsSC = shader->getShaderConstHandle(ShaderGenVars::subSurfaceParams);

   for (S32 i = 0; i < TEXTURE_STAGE_COUNT; ++i)
      mRTParamsSC[i] = shader->getShaderConstHandle( String::ToString( "$rtParams%d", i ) );

   // Clear any existing texture handles.
   dMemset( mTexHandlesSC, 0, sizeof( mTexHandlesSC ) );

   if(sd)
   {
      for (S32 i = 0; i < TEXTURE_STAGE_COUNT; ++i)
      {
         mTexHandlesSC[i] = shader->getShaderConstHandle(sd->getSamplerName(i));
      }
   }
}

///
/// ShaderRenderPassData
///
void ShaderRenderPassData::reset()
{
   Parent::reset();

   shader = NULL;

   for ( U32 i=0; i < featureShaderHandles.size(); i++ )
      delete featureShaderHandles[i];

   featureShaderHandles.clear();
}

///
/// ProcessedShaderMaterial
///
ProcessedShaderMaterial::ProcessedShaderMaterial()
   : mDefaultParameters( NULL )
{
   VECTOR_SET_ASSOCIATION( mShaderConstDesc );
   VECTOR_SET_ASSOCIATION( mParameterHandles );
}

ProcessedShaderMaterial::ProcessedShaderMaterial(Material &mat)
   : mDefaultParameters( NULL )
{
   VECTOR_SET_ASSOCIATION( mShaderConstDesc );
   VECTOR_SET_ASSOCIATION( mParameterHandles );
   mMaterial = &mat;
}

ProcessedShaderMaterial::~ProcessedShaderMaterial()
{
   SAFE_DELETE(mDefaultParameters);
   for (U32 i = 0; i < mParameterHandles.size(); i++)
      SAFE_DELETE(mParameterHandles[i]);
}

//
// Material init
//
bool ProcessedShaderMaterial::init( const FeatureSet &features, 
                                    const GFXVertexFormat *vertexFormat,
                                    const MatFeaturesDelegate &featuresDelegate )
{
   // Load our textures
   _setStageData();

   // Determine how many stages we use
   mMaxStages = getNumStages(); 
   mVertexFormat = vertexFormat;
   mFeatures.clear();

   for( U32 i=0; i<mMaxStages; i++ )
   {
      MaterialFeatureData fd;

      // Determine the features of this stage
      _determineFeatures( i, fd, features );

      // Let the delegate poke at the features.
      if ( featuresDelegate )
         featuresDelegate( this, i, fd, features );

      // Create the passes for this stage
      if ( fd.features.isNotEmpty() )
         if( !_createPasses( fd, i, features ) )
            return false;
   }

   _initRenderPassDataStateBlocks();
   _initMaterialParameters();
   mDefaultParameters =  allocMaterialParameters();
   setMaterialParameters(mDefaultParameters, 0);
   
   return true;
}

U32 ProcessedShaderMaterial::getNumStages()
{
   // Loops through all stages to determine how many stages we actually use
   U32 numStages = 0;

   U32 i;
   for( i=0; i<Material::MAX_STAGES; i++ )
   {
      // Assume stage is inactive
      bool stageActive = false;

      // Cubemaps only on first stage
      if( i == 0 )
      {
         // If we have a cubemap the stage is active
         if( mMaterial->mCubemapData || mMaterial->mDynamicCubemap )
         {
            numStages++;
            continue;
         }
      }

      // If we have a texture for the a feature the 
      // stage is active.
      if ( mStages[i].hasValidTex() )
         stageActive = true;

      // If this stage has specular lighting, it's active
      if (  mMaterial->mPixelSpecular[i] )
         stageActive = true;

      // If this stage has diffuse color, it's active
      if (mMaterial->mDiffuse[i].alpha > 0)
         stageActive = true;

      // If we have a Material that is vertex lit
      // then it may not have a texture
      if( mMaterial->mVertLit[i] )
         stageActive = true;

      // Increment the number of active stages
      numStages += stageActive;
   }

   return numStages;
}

void ProcessedShaderMaterial::_determineFeatures(  U32 stageNum, 
                                                   MaterialFeatureData &fd, 
                                                   const FeatureSet &features )
{
   PROFILE_SCOPE( ProcessedShaderMaterial_DetermineFeatures );

   AssertFatal(GFX->getPixelShaderVersion() > 0.0 , "Cannot create a shader material if we don't support shaders");

   bool lastStage = stageNum == (mMaxStages-1);

   // First we add all the features which the 
   // material has defined.

   if ( mMaterial->isTranslucent() )
   {
      // Note: This is for decal blending into the prepass
      // for AL... it probably needs to be made clearer.
      if (  mMaterial->mTranslucentBlendOp == Material::LerpAlpha &&
            mMaterial->mTranslucentZWrite )
         fd.features.addFeature( MFT_IsTranslucentZWrite );
      else
         fd.features.addFeature( MFT_IsTranslucent );
   }

   if ( mMaterial->mAlphaTest )
      fd.features.addFeature( MFT_AlphaTest );

   if ( mMaterial->mEmissive[stageNum] )
      fd.features.addFeature( MFT_IsEmissive );

   if ( mMaterial->mExposure[stageNum] == 2 )
      fd.features.addFeature( MFT_IsExposureX2 ); 

   if ( mMaterial->mExposure[stageNum] == 4 )
      fd.features.addFeature( MFT_IsExposureX4 );

   if ( mMaterial->mAnimFlags[stageNum] )
      fd.features.addFeature( MFT_TexAnim );

   if ( !mMaterial->mEmissive[stageNum] )
   {
      fd.features.addFeature( MFT_RTLighting );

      // Only allow pixel specular if we have 
      // realtime lighting enabled.
      if ( mMaterial->mPixelSpecular[stageNum] )
         fd.features.addFeature( MFT_PixSpecular );
   }

   if ( mMaterial->mVertLit[stageNum] )
      fd.features.addFeature( MFT_VertLit );
   
   // cubemaps only available on stage 0 for now - bramage   
   if ( stageNum < 1 && 
         (  (  mMaterial->mCubemapData && mMaterial->mCubemapData->mCubemap ) ||
               mMaterial->mDynamicCubemap ) )
   fd.features.addFeature( MFT_CubeMap );

   fd.features.addFeature( MFT_Visibility );

   if ( mMaterial->mColorMultiply[stageNum].alpha > 0 )
      fd.features.addFeature( MFT_ColorMultiply );

   if (  lastStage && 
         (  !gClientSceneGraph->usePostEffectFog() ||
            fd.features.hasFeature( MFT_IsTranslucent ) ) )
      fd.features.addFeature( MFT_Fog );

   if ( mMaterial->mMinnaertConstant[stageNum] > 0.0f )
      fd.features.addFeature( MFT_MinnaertShading );

   if ( mMaterial->mSubSurface[stageNum] )
      fd.features.addFeature( MFT_SubSurface );

   // Grab other features like normal maps, base texture, etc..
   fd.features.merge( mStages[stageNum].getFeatureSet() );

   if ( fd.features[ MFT_NormalMap ] )
   {
      // If we have bump we gotta have a normal 
      // and tangent in our vertex format.
      if ( !mVertexFormat->hasNormalAndTangent() )
         fd.features.removeFeature( MFT_NormalMap );
      else
      {
         // If we have a DXT5 texture we can only assume its a DXTnm if
         // per-pixel specular is disabled... else we get bad results.
         if (  !fd.features[MFT_PixSpecular] && 
               mStages[stageNum].getTex( MFT_NormalMap )->mFormat == GFXFormatDXT5 )
            fd.features.addFeature( MFT_IsDXTnm );
      }
   }

   // If specular map is enabled, make sure that per-pixel specular is as well
   if( !fd.features[ MFT_RTLighting ] )
      fd.features.removeFeature( MFT_SpecularMap );

   if( fd.features[ MFT_SpecularMap ] )
   {
      fd.features.addFeature( MFT_PixSpecular );

      // Check for an alpha channel on the specular map. If it has one (and it
      // has values less than 255) than the artist has put the gloss map into
      // the alpha channel.
      if( mStages[stageNum].getTex( MFT_SpecularMap )->mHasTransparency )
         fd.features.addFeature( MFT_GlossMap );
   }

   // Only allow parallax if we have a normal map,
   // we're not using DXTnm, and we're above SM 2.0.
   if (  mMaterial->mParallaxScale[stageNum] > 0.0f &&
         fd.features[ MFT_NormalMap ] &&
         !fd.features[ MFT_IsDXTnm ] &&
         GFX->getPixelShaderVersion() >= 2.0f )
      fd.features.addFeature( MFT_Parallax );

   // Without a base texture try using diffuse color.
   if (!fd.features[MFT_DiffuseMap])
   {
      if ( mMaterial->mDiffuse[stageNum].alpha > 0 )
         fd.features.addFeature( MFT_DiffuseColor );

      fd.features.removeFeature( MFT_OverlayMap );
   }

   // If lightmaps or tonemaps are enabled or we 
   // don't have a second UV set then we cannot 
   // use the overlay texture.
   if (  fd.features[MFT_LightMap] || 
         fd.features[MFT_ToneMap] || 
         mVertexFormat->getTexCoordCount() < 2 )
      fd.features.removeFeature( MFT_OverlayMap );

   // If tonemaps are enabled don't use lightmap
   if ( fd.features[MFT_ToneMap] || mVertexFormat->getTexCoordCount() < 2 )
      fd.features.removeFeature( MFT_LightMap );

   // Don't allow tonemaps if we don't have a second UV set
   if ( mVertexFormat->getTexCoordCount() < 2 )
      fd.features.removeFeature( MFT_ToneMap );

   // Always add the HDR output feature.  
   //
   // It will be filtered out if it was disabled 
   // for this material creation below.
   //
   // Also the shader code will evaluate to a nop
   // if HDR is not enabled in the scene.
   //
   fd.features.addFeature( MFT_HDROut );

   // Allow features to add themselves.
   for ( U32 i = 0; i < FEATUREMGR->getFeatureCount(); i++ )
   {
      const FeatureInfo &info = FEATUREMGR->getAt( i );
      info.feature->determineFeature(  mMaterial, 
                                       mVertexFormat, 
                                       stageNum, 
                                       *info.type, 
                                       features, 
                                       &fd );
   }

   // Now disable any features that were 
   // not part of the input feature handle.
   fd.features.filter( features );
}

bool ProcessedShaderMaterial::_createPasses( MaterialFeatureData &stageFeatures, U32 stageNum, const FeatureSet &features )
{
   // Creates passes for the given stage
   ShaderRenderPassData passData;
   U32 texIndex = 0;

   for( U32 i=0; i < FEATUREMGR->getFeatureCount(); i++ )
   {
      const FeatureInfo &info = FEATUREMGR->getAt( i );
      if ( !stageFeatures.features.hasFeature( *info.type ) ) 
         continue;

      U32 numTexReg = info.feature->getResources( passData.mFeatureData ).numTexReg;

      // adds pass if blend op changes for feature
      _setPassBlendOp( info.feature, passData, texIndex, stageFeatures, stageNum, features );

      // Add pass if num tex reg is going to be too high
      if( passData.mNumTexReg + numTexReg > GFX->getNumSamplers() )
      {
         if( !_addPass( passData, texIndex, stageFeatures, stageNum, features ) )
            return false;
         _setPassBlendOp( info.feature, passData, texIndex, stageFeatures, stageNum, features );
      }

      passData.mNumTexReg += numTexReg;
      passData.mFeatureData.features.addFeature( *info.type );
      info.feature->setTexData( mStages[stageNum], stageFeatures, passData, texIndex );

      // Add pass if tex units are maxed out
      if( texIndex > GFX->getNumSamplers() )
      {
         if( !_addPass( passData, texIndex, stageFeatures, stageNum, features ) )
            return false;
         _setPassBlendOp( info.feature, passData, texIndex, stageFeatures, stageNum, features );
      }
   }

   const FeatureSet &passFeatures = passData.mFeatureData.codify();
   if ( passFeatures.isNotEmpty() )
   {
      mFeatures.merge( passFeatures );
      if(  !_addPass( passData, texIndex, stageFeatures, stageNum, features ) )
      {
         mFeatures.clear();
         return false;
      }
   }

   return true;
} 

void ProcessedShaderMaterial::_initMaterialParameters()
{   
   // Cleanup anything left first.
   SAFE_DELETE( mDefaultParameters );
   for ( U32 i = 0; i < mParameterHandles.size(); i++ )
      SAFE_DELETE( mParameterHandles[i] );

   // Gather the shaders as they all need to be 
   // passed to the ShaderMaterialParameterHandles.
   Vector<GFXShader*> shaders;
   shaders.setSize( mPasses.size() );
   for ( U32 i = 0; i < mPasses.size(); i++ )
      shaders[i] = _getRPD(i)->shader;

   // Run through each shader and prepare its constants.
   for ( U32 i = 0; i < mPasses.size(); i++ )
   {
      const Vector<GFXShaderConstDesc>& desc = shaders[i]->getShaderConstDesc();

      Vector<GFXShaderConstDesc>::const_iterator p = desc.begin();
      for ( ; p != desc.end(); p++ )
      {
         // Add this to our list of shader constants
         GFXShaderConstDesc d(*p);
         mShaderConstDesc.push_back(d);

         ShaderMaterialParameterHandle* smph = new ShaderMaterialParameterHandle(d.name, shaders);
         mParameterHandles.push_back(smph);
      }
   }
}

bool ProcessedShaderMaterial::_addPass( ShaderRenderPassData &rpd, 
                                       U32 &texIndex, 
                                       MaterialFeatureData &fd,
                                       U32 stageNum,
                                       const FeatureSet &features )
{
   // Set number of textures, stage, glow, etc.
   rpd.mNumTex = texIndex;
   rpd.mStageNum = stageNum;
   rpd.mGlow |= mMaterial->mGlow[stageNum];

   // Copy over features
   rpd.mFeatureData.materialFeatures = fd.features;

   // Generate shader
   GFXShader::setLogging( true, true );
   rpd.shader = SHADERGEN->getShader( rpd.mFeatureData, mVertexFormat, &mUserMacros );
   if( !rpd.shader )
      return false;
   rpd.shaderHandles.init( rpd.shader );   

   // If a pass glows, we glow
   if( rpd.mGlow )
      mHasGlow = true;
 
   ShaderRenderPassData *newPass = new ShaderRenderPassData( rpd );
   mPasses.push_back( newPass );

   // Give each active feature a chance to create specialized shader consts.
   for( U32 i=0; i < FEATUREMGR->getFeatureCount(); i++ )
   {
      const FeatureInfo &info = FEATUREMGR->getAt( i );
      if ( !fd.features.hasFeature( *info.type ) ) 
         continue;

      ShaderFeatureConstHandles *fh = info.feature->createConstHandles( rpd.shader );
      if ( fh )
         newPass->featureShaderHandles.push_back( fh );
   }

   rpd.reset();
   texIndex = 0;
   
   return true;
}

void ProcessedShaderMaterial::_setPassBlendOp( ShaderFeature *sf,
                                              ShaderRenderPassData &passData,
                                              U32 &texIndex,
                                              MaterialFeatureData &stageFeatures,
                                              U32 stageNum,
                                              const FeatureSet &features )
{
   if( sf->getBlendOp() == Material::None )
   {
      return;
   }

   // set up the current blend operation for multi-pass materials
   if( mPasses.size() > 0)
   {
      // If passData.numTexReg is 0, this is a brand new pass, so set the
      // blend operation to the first feature.
      if( passData.mNumTexReg == 0 )
      {
         passData.mBlendOp = sf->getBlendOp();
      }
      else
      {
         // numTegReg is more than zero, if this feature
         // doesn't have the same blend operation, then
         // we need to create yet another pass 
         if( sf->getBlendOp() != passData.mBlendOp && mPasses[mPasses.size()-1]->mStageNum == stageNum)
         {
            _addPass( passData, texIndex, stageFeatures, stageNum, features );
            passData.mBlendOp = sf->getBlendOp();
         }
      }
   }
} 

//
// Runtime / rendering
//
bool ProcessedShaderMaterial::setupPass( SceneState *state, const SceneGraphData &sgData, U32 pass )
{
   PROFILE_SCOPE( ProcessedShaderMaterial_SetupPass );

   // Make sure we have the pass
   if(pass >= mPasses.size())
      return false;

   _setRenderState( state, sgData, pass );

   // Set shaders
   ShaderRenderPassData* rpd = _getRPD(pass);
   if( rpd->shader )
   {
      GFX->setShader( rpd->shader );
      GFX->setShaderConstBuffer(_getShaderConstBuffer(pass));      
      _setShaderConstants(state, sgData, pass);       
   }
   else
   {
      GFX->disableShaders();
      GFX->setShaderConstBuffer(NULL);
   } 

   // Set our textures
   setTextureStages( state, sgData, pass );
   _setTextureTransforms(pass);

   return true;
}

void ProcessedShaderMaterial::cleanup(U32 pass)
{
   // Cleanup is dumb... we waste time clearing stuff
   // that will be re-applied on the next draw when we
   // sort by material.

   Parent::cleanup(pass);
}

void ProcessedShaderMaterial::setTextureStages( SceneState *state, const SceneGraphData &sgData, U32 pass )
{
   PROFILE_SCOPE( ProcessedShaderMaterial_SetTextureStages );

   LightManager *lm = state->getLightManager();   
   ShaderConstHandles *handles = _getShaderConstHandles(pass);

   // Set all of the textures we need to render the give pass.
#ifdef TORQUE_DEBUG
   AssertFatal( pass<mPasses.size(), "Pass out of bounds" );
#endif

   RenderPassData *rpd = mPasses[pass];
   GFXShaderConstBuffer* shaderConsts = _getShaderConstBuffer(pass);
   MatTextureTarget *texTarget;
   GFXTextureObject *texObject; 

   for( U32 i=0; i<rpd->mNumTex; i++ )
   {
      U32 currTexFlag = rpd->mTexType[i];
      if (!lm || !lm->setTextureStage(sgData, currTexFlag, i, shaderConsts, handles))
      {
         switch( currTexFlag )
         {
         // If the flag is unset then assume its just
         // a regular texture to set... nothing special.
         case 0:
            if( mMaterial->isIFL() && sgData.miscTex )
            {
               GFX->setTexture( i, sgData.miscTex );
               break;
            }
            // Fall thru to the next case.
              
         default:
            GFX->setTexture(i, rpd->mTexSlot[i].texObject);
            break;

         case Material::NormalizeCube:
            GFX->setCubeTexture(i, Material::GetNormalizeCube());
            break;

         case Material::Lightmap:
            GFX->setTexture( i, sgData.lightmap );
            break;

         case Material::ToneMapTex:
            shaderConsts->set(handles->mToneMapTexSC, (S32)i);
            GFX->setTexture(i, rpd->mTexSlot[i].texObject);
            break;

         case Material::Cube:
            //shaderConsts->set(handles->mCubeMapTexSC, (S32)i);
            GFX->setCubeTexture( i, rpd->mCubeMap );
            break;

         case Material::SGCube:
            GFX->setCubeTexture( i, sgData.cubemap );
            break;

         case Material::BackBuff:
            GFX->setTexture( i, sgData.backBuffTex );
            break;
            
         case Material::TexTarget:
            {
               texTarget = rpd->mTexSlot[i].texTarget;
               if ( !texTarget )
               {
                  GFX->setTexture( i, NULL );
                  break;
               }
            
               texObject = texTarget->getTargetTexture( 0 );

               // If no texture is available then map the default 2x2
               // black texture to it.  This at least will ensure that
               // we get consistant behavior across GPUs and platforms.
               if ( !texObject )
                  texObject = GFXTexHandle::ZERO;

               if ( handles->mRTParamsSC[i]->isValid() && texObject )
               {
                  const Point3I &targetSz = texObject->getSize();
                  const RectI &targetVp = texTarget->getTargetViewport();
                  Point4F rtParams;

                  ScreenSpace::RenderTargetParameters(targetSz, targetVp, rtParams);

                  shaderConsts->set(handles->mRTParamsSC[i], rtParams);
               }

               GFX->setTexture( i, texObject );
               break;
            }
         }
      }
   }
}

void ProcessedShaderMaterial::_setTextureTransforms(const U32 pass)
{
   PROFILE_SCOPE( ProcessedShaderMaterial_SetTextureTransforms );

   ShaderConstHandles* handles = _getShaderConstHandles(pass);
   if (handles->mTexMatSC->isValid())
   {   
      MatrixF texMat( true );

      mMaterial->updateTimeBasedParams();
      F32 waveOffset = _getWaveOffset( pass ); // offset is between 0.0 and 1.0

      // handle scroll anim type
      if(  mMaterial->mAnimFlags[pass] & Material::Scroll )
      {
         if( mMaterial->mAnimFlags[pass] & Material::Wave )
         {
            Point3F scrollOffset;
            scrollOffset.x = mMaterial->mScrollDir[pass].x * waveOffset;
            scrollOffset.y = mMaterial->mScrollDir[pass].y * waveOffset;
            scrollOffset.z = 1.0;

            texMat.setColumn( 3, scrollOffset );
         }
         else
         {
            Point3F offset( mMaterial->mScrollOffset[pass].x, 
               mMaterial->mScrollOffset[pass].y, 
               1.0 );

            texMat.setColumn( 3, offset );
         }

      }

      // handle rotation
      if( mMaterial->mAnimFlags[pass] & Material::Rotate )
      {
         if( mMaterial->mAnimFlags[pass] & Material::Wave )
         {
            F32 rotPos = waveOffset * M_2PI;
            texMat.set( EulerF( 0.0, 0.0, rotPos ) );
            texMat.setColumn( 3, Point3F( 0.5, 0.5, 0.0 ) );

            MatrixF test( true );
            test.setColumn( 3, Point3F( mMaterial->mRotPivotOffset[pass].x, 
               mMaterial->mRotPivotOffset[pass].y,
               0.0 ) );
            texMat.mul( test );
         }
         else
         {
            texMat.set( EulerF( 0.0, 0.0, mMaterial->mRotPos[pass] ) );

            texMat.setColumn( 3, Point3F( 0.5, 0.5, 0.0 ) );

            MatrixF test( true );
            test.setColumn( 3, Point3F( mMaterial->mRotPivotOffset[pass].x, 
               mMaterial->mRotPivotOffset[pass].y,
               0.0 ) );
            texMat.mul( test );
         }
      }

      // Handle scale + wave offset
      if(  mMaterial->mAnimFlags[pass] & Material::Scale &&
         mMaterial->mAnimFlags[pass] & Material::Wave )
      {
         F32 wOffset = fabs( waveOffset );

         texMat.setColumn( 3, Point3F( 0.5, 0.5, 0.0 ) );

         MatrixF temp( true );
         temp.setRow( 0, Point3F( wOffset,  0.0,  0.0 ) );
         temp.setRow( 1, Point3F( 0.0,  wOffset,  0.0 ) );
         temp.setRow( 2, Point3F( 0.0,  0.0,  wOffset ) );
         temp.setColumn( 3, Point3F( -wOffset * 0.5, -wOffset * 0.5, 0.0 ) );
         texMat.mul( temp );
      }

      // handle sequence
      if( mMaterial->mAnimFlags[pass] & Material::Sequence )
      {
         U32 frameNum = (U32)(MATMGR->getTotalTime() * mMaterial->mSeqFramePerSec[pass]);
         F32 offset = frameNum * mMaterial->mSeqSegSize[pass];

         Point3F texOffset = texMat.getPosition();
         texOffset.x += offset;
         texMat.setPosition( texOffset );
      }

      GFXShaderConstBuffer* shaderConsts = _getShaderConstBuffer(pass);
      shaderConsts->set(handles->mTexMatSC, texMat);
   }
}

//--------------------------------------------------------------------------
// Get wave offset for texture animations using a wave transform
//--------------------------------------------------------------------------
F32 ProcessedShaderMaterial::_getWaveOffset( U32 stage )
{
   switch( mMaterial->mWaveType[stage] )
   {
   case Material::Sin:
      {
         return mMaterial->mWaveAmp[stage] * mSin( M_2PI * mMaterial->mWavePos[stage] );
         break;
      }

   case Material::Triangle:
      {
         F32 frac = mMaterial->mWavePos[stage] - mFloor( mMaterial->mWavePos[stage] );
         if( frac > 0.0 && frac <= 0.25 )
         {
            return mMaterial->mWaveAmp[stage] * frac * 4.0;
         }

         if( frac > 0.25 && frac <= 0.5 )
         {
            return mMaterial->mWaveAmp[stage] * ( 1.0 - ((frac-0.25)*4.0) );
         }

         if( frac > 0.5 && frac <= 0.75 )
         {
            return mMaterial->mWaveAmp[stage] * (frac-0.5) * -4.0;
         }

         if( frac > 0.75 && frac <= 1.0 )
         {
            return -mMaterial->mWaveAmp[stage] * ( 1.0 - ((frac-0.75)*4.0) );
         }

         break;
      }

   case Material::Square:
      {
         F32 frac = mMaterial->mWavePos[stage] - mFloor( mMaterial->mWavePos[stage] );
         if( frac > 0.0 && frac <= 0.5 )
         {
            return 0.0;
         }
         else
         {
            return mMaterial->mWaveAmp[stage];
         }
         break;
      }

   }

   return 0.0;
}

void ProcessedShaderMaterial::_setShaderConstants(SceneState * state, const SceneGraphData &sgData, U32 pass)
{
   PROFILE_SCOPE( ProcessedShaderMaterial_SetShaderConstants );

   GFXShaderConstBuffer* shaderConsts = _getShaderConstBuffer(pass);
   ShaderConstHandles* handles = _getShaderConstHandles(pass);
   U32 stageNum = getStageFromPass(pass);

   // this is OK for now, will need to change later to support different
   // specular values per pass in custom materials
   //-------------------------   
   if ( handles->mSpecularColorSC->isValid() )
      shaderConsts->set(handles->mSpecularColorSC, mMaterial->mSpecular[stageNum]);   

   if ( handles->mSpecularPowerSC->isValid() )
      shaderConsts->set(handles->mSpecularPowerSC, mMaterial->mSpecularPower[stageNum]);

   if ( handles->mParallaxInfoSC->isValid() )
      shaderConsts->set(handles->mParallaxInfoSC, mMaterial->mParallaxScale[stageNum]);   

   if ( handles->mMinnaertConstantSC->isValid() )
      shaderConsts->set(handles->mMinnaertConstantSC, mMaterial->mMinnaertConstant[stageNum]);

   if ( handles->mSubSurfaceParamsSC->isValid() )
   {
      Point4F subSurfParams;
      dMemcpy( &subSurfParams, &mMaterial->mSubSurfaceColor[stageNum], sizeof(ColorF) );
      subSurfParams.w = mMaterial->mSubSurfaceRolloff[stageNum];
      shaderConsts->set(handles->mSubSurfaceParamsSC, subSurfParams);
   }

   // fog
   if ( handles->mFogDataSC->isValid() )
   {
      Point3F fogData;
      fogData.x = sgData.fogDensity;
      fogData.y = sgData.fogDensityOffset;
      fogData.z = sgData.fogHeightFalloff;     
      shaderConsts->set( handles->mFogDataSC, fogData );
   }
   if ( handles->mFogColorSC->isValid() )
      shaderConsts->set(handles->mFogColorSC, sgData.fogColor);

   // set detail scale
   if ( handles->mDetailScaleSC->isValid() )
      shaderConsts->set(handles->mDetailScaleSC, mMaterial->mDetailScale[stageNum]);

   // Visibility
   if ( handles->mVisiblitySC->isValid() )
      shaderConsts->set(handles->mVisiblitySC, sgData.visibility);

   // Diffuse
   if ( handles->mDiffuseColorSC->isValid() )
      shaderConsts->set(handles->mDiffuseColorSC, mMaterial->mDiffuse[stageNum]);

   // Color multiply
   if ( handles->mColorMultiplySC->isValid() )
      if (mMaterial->mColorMultiply[stageNum].alpha > 0.0f)
         shaderConsts->set(handles->mColorMultiplySC, mMaterial->mColorMultiply[stageNum]);      

   if ( handles->mAlphaTestValueSC->isValid() )
      shaderConsts->set( handles->mAlphaTestValueSC, mClampF( (F32)mMaterial->mAlphaRef / 255.0f, 0.0f, 1.0f ) );      

   if( handles->mOneOverFarplane->isValid() )
   {
      const F32 &invfp = 1.0f / state->getFarPlane();
      Point4F oneOverFP(invfp, invfp, invfp, invfp);
      shaderConsts->set( handles->mOneOverFarplane, oneOverFP );
   }

   if ( handles->mAccumTimeSC->isValid() )   
      shaderConsts->set( handles->mAccumTimeSC, MATMGR->getTotalTime() );
}

bool ProcessedShaderMaterial::_hasCubemap(U32 pass)
{
   // Only support cubemap on the first stage
   if( mPasses[pass]->mStageNum > 0 )
      return false;

   if( mPasses[pass]->mCubeMap )
      return true;

   return false;
}

void ProcessedShaderMaterial::setTransforms(const MatrixSet &matrixSet, SceneState *state, const U32 pass)
{   
   GFXShaderConstBuffer* shaderConsts = _getShaderConstBuffer(pass);
   ShaderConstHandles* handles = _getShaderConstHandles(pass);
   
   if ( handles->mModelViewProjSC->isValid() )
      shaderConsts->set(handles->mModelViewProjSC, matrixSet.getWorldViewProjection());

   if ( handles->mCubeTransSC->isValid() )
   {   
      if( _hasCubemap(pass) || mMaterial->mDynamicCubemap)
      {
         MatrixF cubeTrans = matrixSet.getObjectToWorld();
         cubeTrans.setPosition( Point3F( 0.0, 0.0, 0.0 ) );                  
         shaderConsts->set(handles->mCubeTransSC, cubeTrans, GFXSCT_Float3x3);      
      }   
   }

   if ( handles->mObjTransSC->isValid() )
      shaderConsts->set(handles->mObjTransSC, matrixSet.getObjectToWorld());      

   if ( handles->mWorldToObjSC->isValid() )
      shaderConsts->set(handles->mWorldToObjSC, matrixSet.getWorldToObject());

   if ( handles->mWorldToCameraSC->isValid() )
      shaderConsts->set( handles->mWorldToCameraSC, matrixSet.getWorldToCamera() );

   if ( handles->mWorldViewOnlySC->isValid() )
      shaderConsts->set( handles->mWorldViewOnlySC, matrixSet.getObjectToCamera() );

   if ( handles->mViewToObjSC->isValid() )
      shaderConsts->set( handles->mViewToObjSC, matrixSet.getCameraToObject() );


   // vEye
   if( handles->m_vEyeSC->isValid() )
   {
      // vEye is the direction the camera is pointing, with length 1 / zFar
      Point3F vEye;
      matrixSet.getCameraToWorld().getColumn( 1, &vEye );
      vEye.normalize( 1.0f / state->getFarPlane() );
      shaderConsts->set( handles->m_vEyeSC, vEye );
   }
}

void ProcessedShaderMaterial::setSceneInfo(SceneState * state, const SceneGraphData& sgData, U32 pass)
{
   GFXShaderConstBuffer* shaderConsts = _getShaderConstBuffer(pass);
   ShaderConstHandles* handles = _getShaderConstHandles(pass);

   // Set cubemap stuff here (it's convenient!)
   const Point3F &eyePosWorld = state->getCameraPosition();
   if ( handles->mCubeEyePosSC->isValid() )
   {
      if(_hasCubemap(pass) || mMaterial->mDynamicCubemap)
      {
         Point3F cubeEyePos = eyePosWorld - sgData.objTrans.getPosition();
         shaderConsts->set(handles->mCubeEyePosSC, cubeEyePos);      
      }
   }

   shaderConsts->set(handles->mEyePosWorldSC, eyePosWorld);   

   if ( handles->mEyePosSC->isValid() )
   {
      MatrixF tempMat = sgData.objTrans;
      tempMat.inverse();
      Point3F eyepos;
      tempMat.mulP( eyePosWorld, &eyepos );
      shaderConsts->set(handles->mEyePosSC, eyepos);   
   }

   if ( handles->mEyeMatSC->isValid() )   
      shaderConsts->set(handles->mEyeMatSC, state->getCameraTransform());   

   // Now give the features a chance.
   ShaderRenderPassData *rpd = _getRPD( pass );
   for ( U32 i=0; i < rpd->featureShaderHandles.size(); i++ )
      rpd->featureShaderHandles[i]->setConsts( state, sgData, shaderConsts );

   LightManager* lm = state ? state->getLightManager() : NULL;
   if (lm)
      lm->setLightInfo(this, mMaterial, sgData, state, pass, shaderConsts);
}

MaterialParameters* ProcessedShaderMaterial::allocMaterialParameters()
{
   ShaderMaterialParameters* smp = new ShaderMaterialParameters();
   Vector<GFXShaderConstBufferRef> buffers( __FILE__, __LINE__ );
   buffers.setSize(mPasses.size());
   for (U32 i = 0; i < mPasses.size(); i++)
      buffers[i] = _getRPD(i)->shader->allocConstBuffer();
   // smp now owns these buffers.
   smp->setBuffers(mShaderConstDesc, buffers);
   return smp;   
}

MaterialParameterHandle* ProcessedShaderMaterial::getMaterialParameterHandle(const String& name)
{
   // Search our list
   for (U32 i = 0; i < mParameterHandles.size(); i++)
   {
      if (mParameterHandles[i]->getName().equal(name))
         return mParameterHandles[i];
   }
   
   // If we didn't find it, we have to add it to support shader reloading.

   Vector<GFXShader*> shaders;
   shaders.setSize(mPasses.size());
   for (U32 i = 0; i < mPasses.size(); i++)
      shaders[i] = _getRPD(i)->shader;

   ShaderMaterialParameterHandle* smph = new ShaderMaterialParameterHandle( name, shaders );
   mParameterHandles.push_back(smph);

   return smph;
}

/// This is here to deal with the differences between ProcessedCustomMaterials and ProcessedShaderMaterials.
GFXShaderConstBuffer* ProcessedShaderMaterial::_getShaderConstBuffer( const U32 pass )
{   
   if (pass < mPasses.size())
   {
      return static_cast<ShaderMaterialParameters*>(mCurrentParams)->getBuffer(pass);
   }
   return NULL;
}

ShaderConstHandles* ProcessedShaderMaterial::_getShaderConstHandles(const U32 pass)
{
   if (pass < mPasses.size())
   {
      return &_getRPD(pass)->shaderHandles;
   }
   return NULL;
}

void ProcessedShaderMaterial::dumpMaterialInfo()
{
   for ( U32 i = 0; i < getNumPasses(); i++ )
   {
      const ShaderRenderPassData *passData = _getRPD( i );

      if ( passData == NULL )
         continue;

      const GFXShader      *shader = passData->shader;

      if ( shader == NULL )
         Con::printf( "  [%i] [NULL shader]", i );
      else
         Con::printf( "  [%i] %s", i, shader->describeSelf().c_str() );
   }
}
