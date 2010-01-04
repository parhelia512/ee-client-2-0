//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "terrain/terrCellMaterial.h"

#include "terrain/terrData.h"
#include "terrain/terrCell.h"
#include "materials/materialFeatureTypes.h"
#include "materials/materialManager.h"
#include "terrain/terrFeatureTypes.h"
#include "terrain/terrMaterial.h"
#include "renderInstance/renderPrePassMgr.h"
#include "shaderGen/shaderGen.h"
#include "shaderGen/featureMgr.h"
#include "sceneGraph/sceneState.h"
#include "materials/sceneData.h"
#include "gfx/util/screenspace.h"

TerrainCellMaterial::TerrainCellMaterial()
   :  mCurrPass( 0 ),
      mTerrain( NULL ),
      mPrePassMat( NULL )
{
}

TerrainCellMaterial::~TerrainCellMaterial()
{
   SAFE_DELETE( mPrePassMat );
}

void TerrainCellMaterial::setTransformAndEye(   const MatrixF &modelXfm, 
                                                const MatrixF &viewXfm,
                                                const MatrixF &projectXfm,
                                                F32 farPlane )
{
   PROFILE_SCOPE( TerrainCellMaterial_SetTransformAndEye );

   MatrixF modelViewProj = projectXfm * viewXfm * modelXfm;
  
   MatrixF invViewXfm( viewXfm );
   invViewXfm.inverse();
   Point3F eyePos = invViewXfm.getPosition();
   
   MatrixF invModelXfm( modelXfm );
   invModelXfm.inverse();

   Point3F objEyePos = eyePos;
   invModelXfm.mulP( objEyePos );
   
   VectorF vEye = invViewXfm.getForwardVector();
   vEye.normalize( 1.0f / farPlane );

   for ( U32 i=0; i < mPasses.size(); i++ )
   {
      Pass &pass = mPasses[i];

      pass.consts->set( pass.modelViewProjConst, modelViewProj );

      if( pass.viewToObj->isValid() || pass.worldViewOnly->isValid() )
      {
         MatrixF worldViewOnly = viewXfm * modelXfm;

         if( pass.worldViewOnly->isValid() )
            pass.consts->set( pass.worldViewOnly, worldViewOnly );

         if( pass.viewToObj->isValid() )
         {
            worldViewOnly.affineInverse();
            pass.consts->set( pass.viewToObj, worldViewOnly);
         } 
      }

      pass.consts->set( pass.eyePosWorldConst, eyePos );
      pass.consts->set( pass.eyePosConst, objEyePos );

      pass.consts->set( pass.objTransConst, modelXfm );
      pass.consts->set( pass.worldToObjConst, invModelXfm );

      pass.consts->set( pass.vEyeConst, vEye );
   }
}

TerrainCellMaterial* TerrainCellMaterial::getPrePass()
{
   if ( !mPrePassMat )
   {
      mPrePassMat = new TerrainCellMaterial();
      mPrePassMat->init( mTerrain, mMaterials, true, mMaterials == 0 );
   }

   return mPrePassMat;
}

void TerrainCellMaterial::init(  TerrainBlock *block,
                                 U64 activeMaterials, 
                                 bool prePass,
                                 bool baseOnly )
{
   mTerrain = block;
   mMaterials = activeMaterials;

   Vector<MaterialInfo*> materials;

   for ( U32 i = 0; i < 64; i++ )
   {
      if ( !( mMaterials & ((U64)1 << i ) ) )
         continue;

      TerrainMaterial *mat = block->getMaterial( i );

      MaterialInfo *info = new MaterialInfo();
      info->layerId = i;
      info->mat = mat;
      materials.push_back( info );
   }

   mCurrPass = 0;
   mPasses.clear();

   // Ok... loop till we successfully generate all 
   // the shader passes for the materials.
   while ( materials.size() > 0 || baseOnly )
   {
      mPasses.increment();

      if ( !_createPass(   &materials, 
                           &mPasses.last(), 
                           mPasses.size() == 1, 
                           prePass, 
                           baseOnly ) )
      {
         Con::errorf( "TerrainCellMaterial::init - Failed to create pass!" );

         // The pass failed to be generated... give up.
         mPasses.last().materials.clear();
         mPasses.clear();
         for_each( materials.begin(), materials.end(), delete_pointer() );
         return;
      }

      if ( baseOnly )
         break;
   }

   // If we have a prepass then update it too.
   if ( mPrePassMat )
      mPrePassMat->init( mTerrain, mMaterials, true, baseOnly );
}

bool TerrainCellMaterial::_createPass( Vector<MaterialInfo*> *materials, 
                                       Pass *pass, 
                                       bool firstPass,
                                       bool prePass,
                                       bool baseOnly )
{
   U32 matCount = materials->size();

   Vector<GFXTexHandle> normalMaps;

   // See if we're currently running under the
   // basic lighting manager.
   //
   // TODO: This seems ugly... we should trigger
   // features like this differently in the future.
   //
   bool useBLM = dStrcmp( gClientSceneGraph->getLightManager()->getId(), "BLM" ) == 0;

   // Loop till we create a valid shader!
   while( true )
   {
      FeatureSet features;
      features.addFeature( MFT_VertTransform );
      features.addFeature( MFT_TerrainBaseMap );
      features.addFeature( MFT_TerrainEmpty );

      if ( prePass )
      {
         features.addFeature( MFT_EyeSpaceDepthOut );
         features.addFeature( MFT_PrePassConditioner );
      }
      else
         features.addFeature( MFT_RTLighting );

      normalMaps.clear();
      pass->materials.clear();

      for ( U32 i=0; i < matCount && !baseOnly; i++ )
      {
         TerrainMaterial *mat = (*materials)[i]->mat;

         // We only include materials that 
         // have more than a base texture.
         if (  mat->getDetailSize() <= 0 ||
               mat->getDetailDistance() <= 0 ||
               mat->getDetailMap().isEmpty() )
            continue;

         features.addFeature( MFT_TerrainDetailMap, i );
         
         pass->materials.push_back( (*materials)[i] );
         normalMaps.increment();

         // Skip normal maps under basic lighting!
         if ( !useBLM && mat->getNormalMap().isNotEmpty() )
         {
            features.addFeature( MFT_TerrainNormalMap, i );

            normalMaps.last().set( mat->getNormalMap(), 
               &GFXDefaultStaticNormalMapProfile, "TerrainCellMaterial::_createPass() - NormalMap" );

            if ( normalMaps.last().getFormat() == GFXFormatDXT5 )
               features.addFeature( MFT_IsDXTnm, i );

            // Only allow parallax on 2.0 and above and when
            // side projection is disabled.
            if (  mat->getParallaxScale() > 0.0f &&
                  GFX->getPixelShaderVersion() >= 2.0f &&
                  !mat->useSideProjection() )
               features.addFeature( MFT_TerrainParallaxMap, i );
         }

         // Is this layer got side projection?
         if ( mat->useSideProjection() )
            features.addFeature( MFT_TerrainSideProject, i );
      }

      // Enable lightmaps and fogging if we're in BL.
      if ( useBLM )
      {
         features.addFeature( MFT_TerrainLightMap );
         features.addFeature( MFT_Fog );
      }
      
      // The additional passes need to be lerp blended into the
      // target to maintain the results of the previous passes.
      if ( !firstPass )
         features.addFeature( MFT_TerrainAdditive );

      MaterialFeatureData featureData;
      featureData.features = features;
      featureData.materialFeatures = features;

      // Check to see how many vertex shader output 
      // registers we're gonna need.
      U32 numTex = 0;
      U32 numTexReg = 0;
      for ( U32 i=0; i < features.getCount(); i++ )
      {
         S32 index;
         const FeatureType &type = features.getAt( i, &index );
         ShaderFeature* sf = FEATUREMGR->getByType( type );
         if ( !sf ) 
            continue;

         sf->setProcessIndex( index );
         ShaderFeature::Resources res = sf->getResources( featureData );

         numTex += res.numTex;
         numTexReg += res.numTexReg;
      }

      // Can we build the shader?
      //
      // NOTE: The 10 is sort of an abitrary SM 3.0 
      // limit.  Its really supposed to be 11, but that
      // always fails to compile so far.
      //
      if (  numTex < GFX->getNumSamplers() &&
            numTexReg <= 10 )
      {         
         // NOTE: We really shouldn't be getting errors building the shaders,
         // but we can generate more instructions than the ps_2_x will allow.
         //
         // There is no way to deal with this case that i know of other than
         // letting the compile fail then recovering by trying to build it
         // with fewer materials.
         //
         // We normally disabe the shader error logging so that the user 
         // isn't fooled into thinking there is a real bug.  That is until
         // we get down to a single material.  If a single material case
         // fails it means it cannot generate any passes at all!
         const bool logErrors = matCount == 1;
         GFXShader::setLogging( logErrors, true );

         pass->shader = SHADERGEN->getShader( featureData, getGFXVertexFormat<TerrVertex>(), NULL );
      }

      // If we got a shader then we can continue.
      if ( pass->shader )
         break;

      // If the material count is already 1 then this
      // is a real shader error... give up!
      if ( matCount <= 1 )
         return false;

      // If we failed we next try half the input materials
      // so that we can more quickly arrive at a valid shader.
      matCount -= matCount / 2;
   }

   // Setup the constant buffer.
   pass->consts = pass->shader->allocConstBuffer();

   // Prepare the basic constants.
   pass->modelViewProjConst = pass->shader->getShaderConstHandle( "$modelview" );
   pass->worldViewOnly = pass->shader->getShaderConstHandle( "$worldViewOnly" );
   pass->viewToObj = pass->shader->getShaderConstHandle( "$viewToObj" );
   pass->eyePosWorldConst = pass->shader->getShaderConstHandle( "$eyePosWorld" );
   pass->eyePosConst = pass->shader->getShaderConstHandle( "$eyePos" );
   pass->vEyeConst = pass->shader->getShaderConstHandle( "$vEye" );
   pass->layerSizeConst = pass->shader->getShaderConstHandle( "$layerSize" );
   pass->objTransConst = pass->shader->getShaderConstHandle( "$objTrans" );
   pass->worldToObjConst = pass->shader->getShaderConstHandle( "$worldToObj" );  
   pass->lightInfoBufferConst = pass->shader->getShaderConstHandle( "$lightInfoBuffer" );   
   pass->baseTexMapConst = pass->shader->getShaderConstHandle( "$baseTexMap" );
   pass->layerTexConst = pass->shader->getShaderConstHandle( "$layerTex" );
   pass->fogDataConst = pass->shader->getShaderConstHandle( "$fogData" );
   pass->fogColorConst = pass->shader->getShaderConstHandle( "$fogColor" );
   pass->lightMapTexConst = pass->shader->getShaderConstHandle( "$lightMapTex" );
   pass->oneOverTerrainSize = pass->shader->getShaderConstHandle( "$oneOverTerrainSize" );
   pass->squareSize = pass->shader->getShaderConstHandle( "$squareSize" );

   // NOTE: We're assuming rtParams0 here as we know its the only
   // render target we currently get in a terrain material and the
   // DeferredRTLightingFeatHLSL will always use 0.
   //
   // This could change in the future and we would need to fix
   // the ShaderFeature API to allow us to do this right.
   //
   pass->lightParamsConst = pass->shader->getShaderConstHandle( "$rtParams0" );

   // Now prepare the basic stateblock.
   GFXStateBlockDesc desc;
   if ( !firstPass )
   {
      desc.setBlend( true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha );

      // If this is the prepass then we don't want to
      // write to the last two color channels (where
      // depth is usually encoded).
      //
      // This trick works in combination with the 
      // MFT_TerrainAdditive feature to lerp the
      // output normal with the previous pass.
      //
      if ( prePass )
         desc.setColorWrites( true, true, false, false );
   }

   // We write to the zbuffer if this is a prepass
   // material or if the prepass is disabled.
   // We also write the zbuffer if we're using OpenGL, because in OpenGL the prepass
   // cannot share the same zbuffer as the backbuffer.
   desc.setZReadWrite( true,  !MATMGR->getPrePassEnabled() || 
                              GFX->getAdapterType() == OpenGL ||
                              prePass );

   desc.samplersDefined = true;
   if ( pass->baseTexMapConst->isValid() )
      desc.samplers[pass->baseTexMapConst->getSamplerRegister()] = GFXSamplerStateDesc::getWrapLinear();

   if ( pass->layerTexConst->isValid() )
      desc.samplers[pass->layerTexConst->getSamplerRegister()] = GFXSamplerStateDesc::getClampPoint();

   if ( pass->lightInfoBufferConst->isValid() )
      desc.samplers[pass->lightInfoBufferConst->getSamplerRegister()] = GFXSamplerStateDesc::getClampPoint();

   if ( pass->lightMapTexConst->isValid() )
      desc.samplers[pass->lightMapTexConst->getSamplerRegister()] = GFXSamplerStateDesc::getWrapLinear();

   // Finally setup the material specific shader 
   // constants and stateblock state.
   for ( U32 i=0; i < pass->materials.size(); i++ )
   {
      MaterialInfo *matInfo = pass->materials[i];

      matInfo->detailInfoVConst = pass->shader->getShaderConstHandle( avar( "$detailScaleAndFade%d", i ) );
      matInfo->detailInfoPConst = pass->shader->getShaderConstHandle( avar( "$detailIdStrengthParallax%d", i ) );

      matInfo->detailTexConst = pass->shader->getShaderConstHandle( avar( "$detailMap%d", i ) );
      if ( matInfo->detailTexConst->isValid() )
      {
         const S32 sampler = matInfo->detailTexConst->getSamplerRegister();

         desc.samplers[sampler] = GFXSamplerStateDesc::getWrapLinear();
         desc.samplers[sampler].magFilter = GFXTextureFilterLinear;
         desc.samplers[sampler].mipFilter = GFXTextureFilterLinear;
         desc.samplers[sampler].minFilter = GFXTextureFilterAnisotropic;
         desc.samplers[sampler].maxAnisotropy = 4;

         matInfo->detailTex.set( matInfo->mat->getDetailMap(), 
            &GFXDefaultStaticDiffuseProfile, "TerrainCellMaterial::_createPass() - DetailMap" );
      }

      matInfo->normalTexConst = pass->shader->getShaderConstHandle( avar( "$normalMap%d", i ) );
      if ( matInfo->normalTexConst->isValid() )
      {
         const S32 sampler = matInfo->normalTexConst->getSamplerRegister();

         desc.samplers[sampler] = GFXSamplerStateDesc::getWrapLinear();
         desc.samplers[sampler].magFilter = GFXTextureFilterLinear;
         desc.samplers[sampler].mipFilter = GFXTextureFilterLinear;
         desc.samplers[sampler].minFilter = GFXTextureFilterAnisotropic;
         desc.samplers[sampler].maxAnisotropy = 4;

         matInfo->normalTex = normalMaps[i];
      }
   }

   // Remove the materials we processed and leave the
   // ones that remain for the next pass.
   for ( U32 i=0; i < matCount; i++ )
   {
      MaterialInfo *matInfo = materials->first();
      if ( baseOnly || pass->materials.find_next( matInfo ) == -1 )
         delete matInfo;     
      materials->pop_front();
   }

   // If we're doing prepass it requires some 
   // special stencil settings for it to work.
   if ( prePass )
      desc.addDesc( RenderPrePassMgr::getOpaqueStenciWriteDesc( false ) );

   pass->stateBlock = GFX->createStateBlock( desc );

   GFXStateBlockDesc wireframe( desc );
   wireframe.fillMode = GFXFillWireframe;
   pass->wireframeStateBlock = GFX->createStateBlock( wireframe );

   desc.setCullMode( GFXCullCW );
   pass->reflectStateBlock = GFX->createStateBlock( desc );

   return true;
}

void TerrainCellMaterial::_updateMaterialConsts( Pass *pass )
{
   PROFILE_SCOPE( TerrainCellMaterial_UpdateMaterialConsts );

   for ( U32 j=0; j < pass->materials.size(); j++ )
   {
      MaterialInfo *matInfo = pass->materials[j];

      F32 detailSize = matInfo->mat->getDetailSize();
      F32 detailScale = 1.0f;
      if ( !mIsZero( detailSize ) )
         detailScale = mTerrain->getWorldBlockSize() / detailSize;

      // Scale the distance by the global scalar.
      const F32 distance = mTerrain->smDetailScale * matInfo->mat->getDetailDistance();

      // NOTE: The negation of the y scale is to make up for 
      // my mistake early in development and passing the wrong
      // y texture coord into the system.
      //
      // This negation fixes detail, normal, and parallax mapping
      // without harming the layer id blending code.
      //
      // Eventually we should rework this to correct this little
      // mistake, but there isn't really a hurry to.
      //
      Point4F detailScaleAndFade(   detailScale,
                                    -detailScale,
                                    distance, 
                                    0 );

      if ( !mIsZero( distance ) )
         detailScaleAndFade.w = 1.0f / distance;

      Point3F detailIdStrengthParallax( matInfo->layerId,
                                        matInfo->mat->getDetailStrength(),
                                        matInfo->mat->getParallaxScale() );

      pass->consts->set( matInfo->detailInfoVConst, detailScaleAndFade );
      pass->consts->set( matInfo->detailInfoPConst, detailIdStrengthParallax );
   }
}

bool TerrainCellMaterial::setupPass(   const SceneState *state, 
                                       const SceneGraphData &sceneData )
{
   PROFILE_SCOPE( TerrainCellMaterial_SetupPass );

   if ( mCurrPass >= mPasses.size() )
   {
      mCurrPass = 0;
      return false;
   }

   Pass &pass = mPasses[mCurrPass];

   _updateMaterialConsts( &pass );

   if ( pass.baseTexMapConst->isValid() )
      GFX->setTexture( pass.baseTexMapConst->getSamplerRegister(), mTerrain->mBaseTex.getPointer() );

   if ( pass.layerTexConst->isValid() )
      GFX->setTexture( pass.layerTexConst->getSamplerRegister(), mTerrain->mLayerTex.getPointer() );

   if ( pass.lightMapTexConst->isValid() )
      GFX->setTexture( pass.lightMapTexConst->getSamplerRegister(), mTerrain->getLightMapTex() );

   if ( sceneData.wireframe )
      GFX->setStateBlock( pass.wireframeStateBlock );
   else if ( state->isReflectPass() )
      GFX->setStateBlock( pass.reflectStateBlock );
   else
      GFX->setStateBlock( pass.stateBlock );

   GFX->setShader( pass.shader );
   GFX->setShaderConstBuffer( pass.consts );

   // Let the light manager prepare any light stuff it needs.
   state->getLightManager()->setLightInfo(   NULL,
                                             NULL,
                                             sceneData,
                                             state,
                                             mCurrPass,
                                             pass.consts );

   for ( U32 i=0; i < pass.materials.size(); i++ )
   {
      MaterialInfo *matInfo = pass.materials[i];

      if ( matInfo->detailTexConst->isValid() )
         GFX->setTexture( matInfo->detailTexConst->getSamplerRegister(), matInfo->detailTex );
      if ( matInfo->normalTexConst->isValid() )
         GFX->setTexture( matInfo->normalTexConst->getSamplerRegister(), matInfo->normalTex );
   }

   pass.consts->set( pass.layerSizeConst, (F32)mTerrain->mLayerTex.getWidth() );

   if ( pass.oneOverTerrainSize->isValid() )
   {
      F32 oneOverTerrainSize = 1.0f / mTerrain->getWorldBlockSize();
      pass.consts->set( pass.oneOverTerrainSize, oneOverTerrainSize );
   }

   if ( pass.squareSize->isValid() )
      pass.consts->set( pass.squareSize, mTerrain->getSquareSize() );

   if ( pass.fogDataConst->isValid() )
   {
      Point3F fogData;
      fogData.x = sceneData.fogDensity;
      fogData.y = sceneData.fogDensityOffset;
      fogData.z = sceneData.fogHeightFalloff;     
      pass.consts->set( pass.fogDataConst, fogData );
   }

   pass.consts->set( pass.fogColorConst, sceneData.fogColor );

   if (  pass.lightInfoBufferConst->isValid() &&
         pass.lightParamsConst->isValid() )
   {
      if ( !mLightInfoTarget )
         mLightInfoTarget = MatTextureTarget::findTargetByName( "lightinfo" );

      GFXTextureObject *texObject = mLightInfoTarget->getTargetTexture( 0 );
      GFX->setTexture( pass.lightInfoBufferConst->getSamplerRegister(), texObject );

      const Point3I &targetSz = texObject->getSize();
      const RectI &targetVp = mLightInfoTarget->getTargetViewport();
      Point4F rtParams;

      ScreenSpace::RenderTargetParameters(targetSz, targetVp, rtParams);

      pass.consts->set( pass.lightParamsConst, rtParams );
   }

   ++mCurrPass;
   return true;
}
