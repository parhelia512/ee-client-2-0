//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "renderInstance/renderPrePassMgr.h"

#include "gfx/gfxTransformSaver.h"
#include "materials/sceneData.h"
#include "materials/materialManager.h"
#include "materials/materialFeatureTypes.h"
#include "core/util/safeDelete.h"
#include "shaderGen/featureMgr.h"
#include "shaderGen/HLSL/depthHLSL.h"
#include "shaderGen/GLSL/depthGLSL.h"
#include "shaderGen/conditionerFeature.h"
#include "shaderGen/shaderGenVars.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "gfx/gfxDebugEvent.h"
#include "materials/customMaterialDefinition.h"
#include "lighting/advanced/advancedLightManager.h"
#include "lighting/advanced/advancedLightBinManager.h"
#include "terrain/terrCell.h"
#include "renderInstance/renderTerrainMgr.h"
#include "terrain/terrCellMaterial.h"
#include "math/mathUtils.h"
#include "math/util/matrixSet.h"

const MatInstanceHookType PrePassMatInstanceHook::Type( "PrePass" );
const String RenderPrePassMgr::BufferName("prepass");
const RenderInstType RenderPrePassMgr::RIT_PrePass("PrePass");

IMPLEMENT_CONOBJECT(RenderPrePassMgr);


RenderPrePassMgr::RenderSignal& RenderPrePassMgr::getRenderSignal()
{
   static RenderSignal theSignal;
   return theSignal;
}


RenderPrePassMgr::RenderPrePassMgr( bool gatherDepth,
                                    GFXFormat format,
                                    const RenderInstType riType )
   :  Parent(  riType,
               0.01f,
               0.01f,
               format,
               Point2I( Parent::DefaultTargetSize, Parent::DefaultTargetSize),
               gatherDepth ? Parent::DefaultTargetChainLength : 0 ),
      mPrePassMatInstance( NULL )
{
   // We want a full-resolution buffer
   mTargetSizeType = RenderTexTargetBinManager::WindowSize;

   if(getTargetChainLength() > 0)
   {
      GFXShader::addGlobalMacro( "TORQUE_LINEAR_DEPTH" );
      MatTextureTarget::registerTarget( BufferName, this );
   }

   _registerFeatures();
}

RenderPrePassMgr::~RenderPrePassMgr()
{
   GFXShader::removeGlobalMacro( "TORQUE_LINEAR_DEPTH" );
   MatTextureTarget::unregisterTarget( BufferName, this );

   _unregisterFeatures();
   SAFE_DELETE( mPrePassMatInstance );
}

void RenderPrePassMgr::_registerFeatures()
{
#ifndef TORQUE_DEDICATED
   if(GFX->getAdapterType() == OpenGL)
   {
#ifdef TORQUE_OS_MAC
      FEATUREMGR->registerFeature(MFT_PrePassConditioner, new LinearEyeDepthConditioner(getTargetFormat()) );
#endif
   }
   else
   {
#ifndef TORQUE_OS_MAC
      FEATUREMGR->registerFeature(MFT_PrePassConditioner, new LinearEyeDepthConditioner(getTargetFormat()) );
#endif
   }
#endif
}

void RenderPrePassMgr::_unregisterFeatures()
{
   FEATUREMGR->unregisterFeature(MFT_PrePassConditioner);
}

ConditionerFeature *RenderPrePassMgr::getTargetConditioner() const
{
   return dynamic_cast<ConditionerFeature *>(FEATUREMGR->getByType(MFT_PrePassConditioner));
}

bool RenderPrePassMgr::setTargetSize(const Point2I &newTargetSize)
{
   bool ret = Parent::setTargetSize( newTargetSize );
   mTargetViewport = GFX->getViewport();
   return ret;
}

bool RenderPrePassMgr::_updateTargets()
{
   PROFILE_SCOPE(RenderPrePassMgr_updateTargets);

   bool ret = Parent::_updateTargets();
#ifndef TORQUE_DEDICATED
   // check for an output conditioner, and update it's format
   ConditionerFeature *outputConditioner = dynamic_cast<ConditionerFeature *>(FEATUREMGR->getByType(MFT_PrePassConditioner));

   if( outputConditioner && outputConditioner->setBufferFormat(mTargetFormat) )
   {
      // reload materials, the conditioner needs to alter the generated shaders
   }

   // Attach the light info buffer as a second render target, if there is
   // lightmapped geometry in the scene.
   MatTextureTarget *texTarget = MatTextureTarget::findTargetByName(AdvancedLightBinManager::smBufferName);
   if(texTarget)
   {
      AssertFatal(dynamic_cast<AdvancedLightBinManager *>(texTarget), "Bad buffer type!");
      AdvancedLightBinManager *lightBin = static_cast<AdvancedLightBinManager *>(texTarget);
      if(lightBin->MRTLightmapsDuringPrePass())
      {
         if(lightBin->isProperlyAdded())
         {
            // Update the size of the light bin target here. This will call _updateTargets
            // on the light bin
            ret &= lightBin->setTargetSize(mTargetSize);
            if(ret)
            {
               // Sanity check
               AssertFatal(lightBin->getTargetChainLength() == mTargetChainLength, "Target chain length mismatch");

               // Attach light info buffer to Color1 for each target in the chain
               for(U32 i = 0; i < mTargetChainLength; i++)
               {
                  GFXTexHandle lightInfoTex = lightBin->getTargetTexture(0, i);
                  mTargetChain[i]->attachTexture(GFXTextureTarget::Color1, lightInfoTex);
               }
            }
         }
      }
   }
#endif
   return ret;
}

void RenderPrePassMgr::_createPrePassMaterial()
{
   SAFE_DELETE(mPrePassMatInstance);

   const GFXVertexFormat *vertexFormat = getGFXVertexFormat<GFXVertexPNTTB>();

   MatInstance* prepassMat = static_cast<MatInstance*>(MATMGR->createMatInstance("AL_DefaultPrePassMaterial", vertexFormat));
   AssertFatal( prepassMat, "TODO: Handle this better." );
   mPrePassMatInstance = new PrePassMatInstance(prepassMat, this);
   mPrePassMatInstance->init( MATMGR->getDefaultFeatures(), vertexFormat);
   delete prepassMat;
}

void RenderPrePassMgr::setPrePassMaterial( PrePassMatInstance *mat )
{
   SAFE_DELETE(mPrePassMatInstance);
   mPrePassMatInstance = mat;
}

RenderBinManager::AddInstResult RenderPrePassMgr::addElement( RenderInst *inst )
{
   // Check for a custom refract type
   BaseMatInstance* matInst = getMaterial(inst);
   CustomMaterial* custMat = NULL;

   if( matInst )
      custMat = dynamic_cast<CustomMaterial*>(matInst->getMaterial());

   if ( (   inst->type == RenderPassManager::RIT_Mesh ||
            inst->type == RenderPassManager::RIT_Decal ||
            inst->type == RenderPassManager::RIT_Object ||
            inst->type == RenderPassManager::RIT_Terrain ||
            inst->type == RenderPassManager::RIT_Interior ) &&
         ( custMat == NULL || !custMat->mRefract ) )
   {
      internalAddElement( inst );

      if (  inst->type == RenderPassManager::RIT_Mesh ||
            inst->type == RenderPassManager::RIT_Decal ||
            inst->type == RenderPassManager::RIT_Interior )
      {
         MeshRenderInst *meshRI = static_cast<MeshRenderInst*>(inst);

         // Check for a Pre-Pass Mat Hook. If one doesn't exist, create it.
         if( meshRI->matInst && !meshRI->matInst->getHook( PrePassMatInstanceHook::Type ) )
         {
            MatInstance *m = reinterpret_cast<MatInstance *>(meshRI->matInst);
            if ( m )
               meshRI->matInst->addHook( new PrePassMatInstanceHook( m, this ) );
         }
      }

      return arAdded;
   }

   return arSkipped;
}

void RenderPrePassMgr::internalAddElement( RenderInst* inst )
{
   mElementList.increment();
   MainSortElem &elem = mElementList.last();
   elem.inst = inst;

   U32 originalKey = elem.key;

   // Ignore the default key, and instead sort front-to-back first for a pre-pass
   const F32 invSortDistSq = F32_MAX - inst->sortDistSq;
   elem.key = *((U32*)&invSortDistSq);

   // Next sort by pre-pass material, if applicable
   MeshRenderInst *meshRI = static_cast<MeshRenderInst*>(inst);
   if ( (   inst->type == RenderPassManager::RIT_Mesh ||
            inst->type == RenderPassManager::RIT_Interior ) && meshRI->matInst )
      elem.key2 = U32(meshRI->matInst->getHook( PrePassMatInstanceHook::Type ));
   else
      elem.key2 = originalKey;
}

void RenderPrePassMgr::render( SceneState *state )
{
   PROFILE_SCOPE(RenderPrePassMgr_render);

   // NOTE: We don't early out here when the element list is
   // zero because we need the prepass to be cleared.

   // Automagically save & restore our viewport and transforms.
   GFXTransformSaver saver;

   GFXDEBUGEVENT_SCOPE( RenderPrePassMgr_Render, ColorI::RED );

   // Tell the superclass we're about to render
   const bool isRenderingToTarget = _onPreRender(state);


   // Clear all the buffers to white so that the
   // default depth is to the far plane.
   GFX->clear( GFXClearTarget | GFXClearZBuffer | GFXClearStencil, ColorI::WHITE, 1.0f, 0);

   // init loop data
   SceneGraphData sgData;

   if ( !mPrePassMatInstance )
      _createPrePassMaterial();

   // Restore transforms
   MatrixSet &matrixSet = getParentManager()->getMatrixSet();
   matrixSet.restoreSceneViewProjection();

   const MatrixF worldViewXfm = GFX->getWorldMatrix();

   // Set transforms for the default pre-pass material
   if( mPrePassMatInstance )
   {
      matrixSet.setWorld(MatrixF::Identity);
      mPrePassMatInstance->setTransforms(matrixSet, state);
   }

   // Signal start of pre-pass
   getRenderSignal().trigger( state, this, true );

   // Render mesh objects
   for( Vector< MainSortElem >::const_iterator itr = mElementList.begin();
      itr != mElementList.end(); itr++)
   {
      RenderInst *renderInst = itr->inst;

      if (  renderInst->type == RenderPassManager::RIT_Mesh ||
            renderInst->type == RenderPassManager::RIT_Decal ||
            renderInst->type == RenderPassManager::RIT_Interior )
      {
         MeshRenderInst* meshRI = static_cast<MeshRenderInst*>(renderInst);

         AssertFatal( meshRI->matInst->getHook( PrePassMatInstanceHook::Type ), "This should not happen." );
         AssertFatal( dynamic_cast<PrePassMatInstanceHook *>( meshRI->matInst->getHook( PrePassMatInstanceHook::Type ) ), "This should also not happen." );

         PrePassMatInstanceHook *prePassHook = static_cast<PrePassMatInstanceHook *>( meshRI->matInst->getHook( PrePassMatInstanceHook::Type ) );
         BaseMatInstance *mat = prePassHook->getPrePassMatInstance();

         // Set up SG data proper like, and flag that this is a pre-pass render
         setupSGData( meshRI, sgData );
         sgData.binType = SceneGraphData::PrePassBin;

         matrixSet.setWorld(*meshRI->objectToWorld);
         matrixSet.setView(*meshRI->worldToCamera);
         matrixSet.setProjection(*meshRI->projection);

         while( mat->setupPass(state, sgData) )
         {
            mat->setSceneInfo(state, sgData);
            mat->setTransforms(matrixSet, state);

            mat->setBuffers(meshRI->vertBuff, meshRI->primBuff);

            if ( meshRI->prim )
               GFX->drawPrimitive( *meshRI->prim );
            else
               GFX->drawPrimitive( meshRI->primBuffIndex );
         }
      }
      else if( renderInst->type == RenderPassManager::RIT_Terrain )
      {
         // TODO: Move to RenderTerrainMgr and use the signal.

         TerrainRenderInst *terrainRI = static_cast<TerrainRenderInst*>( renderInst );

         TerrainCellMaterial *mat = terrainRI->cellMat->getPrePass();

         GFX->setPrimitiveBuffer( terrainRI->primBuff );
         GFX->setVertexBuffer( terrainRI->vertBuff );

         mat->setTransformAndEye(   *terrainRI->objectToWorldXfm,
                                    worldViewXfm,
                                    GFX->getProjectionMatrix(),
                                    state->getFarPlane() );

         // The terrain doesn't need any scene graph data
         // in the the prepass... so just clear it.
         sgData.reset();
         sgData.binType = SceneGraphData::PrePassBin;
         sgData.wireframe = GFXDevice::getWireframe();

         while ( mat->setupPass( state, sgData ) )
            GFX->drawPrimitive( terrainRI->prim );
      }
      else if (   renderInst->type == RenderPassManager::RIT_Object &&
                  mPrePassMatInstance != NULL )
      {
         ObjectRenderInst *ri = static_cast<ObjectRenderInst*>(renderInst);
         if (ri->renderDelegate)
            ri->renderDelegate(ri, state, mPrePassMatInstance);
      }
   }

   // Signal end of pre-pass
   getRenderSignal().trigger( state, this, false );

   if(isRenderingToTarget)
      _onPostRender();
}

const GFXStateBlockDesc & RenderPrePassMgr::getOpaqueStenciWriteDesc( bool lightmappedGeometry /*= true*/ )
{
   static bool sbInit = false;
   static GFXStateBlockDesc sOpaqueStaticLitStencilWriteDesc;
   static GFXStateBlockDesc sOpaqueDynamicLitStencilWriteDesc;

   if(!sbInit)
   {
      sbInit = true;

      // Build the static opaque stencil write/test state block descriptions
      sOpaqueStaticLitStencilWriteDesc.stencilDefined = true;
      sOpaqueStaticLitStencilWriteDesc.stencilEnable = true;
      sOpaqueStaticLitStencilWriteDesc.stencilWriteMask = 0x03;
      sOpaqueStaticLitStencilWriteDesc.stencilMask = 0x03;
      sOpaqueStaticLitStencilWriteDesc.stencilRef = RenderPrePassMgr::OpaqueStaticLitMask;
      sOpaqueStaticLitStencilWriteDesc.stencilPassOp = GFXStencilOpReplace;
      sOpaqueStaticLitStencilWriteDesc.stencilFailOp = GFXStencilOpKeep;
      sOpaqueStaticLitStencilWriteDesc.stencilZFailOp = GFXStencilOpKeep;
      sOpaqueStaticLitStencilWriteDesc.stencilFunc = GFXCmpAlways;

      // Same only dynamic
      sOpaqueDynamicLitStencilWriteDesc = sOpaqueStaticLitStencilWriteDesc;
      sOpaqueDynamicLitStencilWriteDesc.stencilRef = RenderPrePassMgr::OpaqueDynamicLitMask;
   }

   return (lightmappedGeometry ? sOpaqueStaticLitStencilWriteDesc : sOpaqueDynamicLitStencilWriteDesc);
}

const GFXStateBlockDesc & RenderPrePassMgr::getOpaqueStencilTestDesc()
{
   static bool sbInit = false;
   static GFXStateBlockDesc sOpaqueStencilTestDesc;

   if(!sbInit)
   {
      // Build opaque test
      sbInit = true;
      sOpaqueStencilTestDesc.stencilDefined = true;
      sOpaqueStencilTestDesc.stencilEnable = true;
      sOpaqueStencilTestDesc.stencilWriteMask = 0xFE;
      sOpaqueStencilTestDesc.stencilMask = 0x03;
      sOpaqueStencilTestDesc.stencilRef = 0;
      sOpaqueStencilTestDesc.stencilPassOp = GFXStencilOpKeep;
      sOpaqueStencilTestDesc.stencilFailOp = GFXStencilOpKeep;
      sOpaqueStencilTestDesc.stencilZFailOp = GFXStencilOpKeep;
      sOpaqueStencilTestDesc.stencilFunc = GFXCmpLess;
   }

   return sOpaqueStencilTestDesc;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


ProcessedPrePassMaterial::ProcessedPrePassMaterial( Material& mat, const RenderPrePassMgr *prePassMgr )
: Parent(mat), mPrePassMgr(prePassMgr)
{

}

void ProcessedPrePassMaterial::_determineFeatures( U32 stageNum,
                                                   MaterialFeatureData &fd,
                                                   const FeatureSet &features )
{
   Parent::_determineFeatures( stageNum, fd, features );

   // Find this for use down below...
   MatTextureTarget *texTarget = MatTextureTarget::findTargetByName(AdvancedLightBinManager::smBufferName);
   bool bEnableMRTLightmap = false;
   if(texTarget)
   {
      AssertFatal(dynamic_cast<AdvancedLightBinManager *>(texTarget), "Bad buffer type!");
      AdvancedLightBinManager *lightBin = static_cast<AdvancedLightBinManager *>(texTarget);
      bEnableMRTLightmap = lightBin->MRTLightmapsDuringPrePass();
   }

   // If this material has a lightmap or tonemap (texture or baked vertex color),
   // it must be static. Otherwise it is dynamic.
   mIsLightmappedGeometry = ( fd.features.hasFeature( MFT_ToneMap ) ||
                              fd.features.hasFeature( MFT_LightMap ) ||
                              fd.features.hasFeature( MFT_VertLit ) ||
                              ( bEnableMRTLightmap && fd.features.hasFeature( MFT_IsTranslucent ) ||
                                                      fd.features.hasFeature( MFT_IsTranslucentZWrite ) ) );

   // Integrate proper opaque stencil write state
   mUserDefined.addDesc( mPrePassMgr->getOpaqueStenciWriteDesc( mIsLightmappedGeometry ) );

   FeatureSet newFeatures;

   // These are always on for prepass.
   newFeatures.addFeature( MFT_EyeSpaceDepthOut );
   newFeatures.addFeature( MFT_PrePassConditioner );

#ifndef TORQUE_DEDICATED

   for ( U32 i=0; i < fd.features.getCount(); i++ )
   {
      const FeatureType &type = fd.features.getAt( i );

      // Turn on the diffuse texture only if we
      // have alpha test.
      if ( type == MFT_AlphaTest )
      {
         newFeatures.addFeature( MFT_AlphaTest );
         newFeatures.addFeature( MFT_DiffuseMap );
      }

      else if ( type == MFT_IsTranslucentZWrite )
      {
         newFeatures.addFeature( MFT_IsTranslucentZWrite );
         newFeatures.addFeature( MFT_DiffuseMap );
      }

      // Always allow these.
      else if (   type == MFT_IsDXTnm ||
                  type == MFT_TexAnim ||
                  type == MFT_NormalMap ||
                  type == MFT_AlphaTest ||
                  type == MFT_Parallax )
         newFeatures.addFeature( type );

      // Add any transform features.
      else if (   type.getGroup() == MFG_PreTransform ||
                  type.getGroup() == MFG_Transform ||
                  type.getGroup() == MFG_PostTransform )
         newFeatures.addFeature( type );
   }

   // If there is lightmapped geometry support, add the MRT light buffer features
   if(bEnableMRTLightmap)
   {
      // If this material has a lightmap, pass it through, and flag it to
      // send it's output to RenderTarget1
      if( fd.features.hasFeature( MFT_ToneMap ) )
      {
         newFeatures.addFeature( MFT_ToneMap );
         newFeatures.addFeature( MFT_LightbufferMRT );
      }
      else if( fd.features.hasFeature( MFT_LightMap ) )
      {
         newFeatures.addFeature( MFT_LightMap );
         newFeatures.addFeature( MFT_LightbufferMRT );
      }
      else if( fd.features.hasFeature( MFT_VertLit ) )
      {
         // Flag un-tone-map if necesasary
         if( fd.features.hasFeature( MFT_DiffuseMap ) )
            newFeatures.addFeature( MFT_VertLitTone );

         newFeatures.addFeature( MFT_VertLit );
         newFeatures.addFeature( MFT_LightbufferMRT );
      }
      else
      {
         // If this object isn't lightmapped, add a zero-output feature to it
         newFeatures.addFeature( MFT_RenderTarget1_Zero );
      }
   }

#endif

   // Set the new features.
   fd.features = newFeatures;
}

U32 ProcessedPrePassMaterial::getNumStages()
{
   // Return 1 stage so this material gets processed for sure
   return 1;
}

void ProcessedPrePassMaterial::addStateBlockDesc(const GFXStateBlockDesc& desc)
{
   GFXStateBlockDesc prePassStateBlock = desc;

   // Adjust color writes if this is a pure z-fill pass
   const bool pixelOutEnabled = mPrePassMgr->getTargetChainLength() > 0;
   if ( !pixelOutEnabled )
   {
      prePassStateBlock.colorWriteDefined = true;
      prePassStateBlock.colorWriteRed = pixelOutEnabled;
      prePassStateBlock.colorWriteGreen = pixelOutEnabled;
      prePassStateBlock.colorWriteBlue = pixelOutEnabled;
      prePassStateBlock.colorWriteAlpha = pixelOutEnabled;
   }

   // Never allow the alpha test state when rendering
   // the prepass as we use the alpha channel for the
   // depth information... MFT_AlphaTest will handle it.
   prePassStateBlock.alphaDefined = true;
   prePassStateBlock.alphaTestEnable = false;

   // If we're translucent then we're doing prepass blending
   // which never writes to the depth channels.
   const bool isTranslucent = getMaterial()->isTranslucent();
   if ( isTranslucent )
   {
      prePassStateBlock.setBlend( true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha );
      prePassStateBlock.setColorWrites( true, true, false, false );
   }

   // Enable z reads, but only enable zwrites if we're not translucent.
   prePassStateBlock.setZReadWrite( true, isTranslucent ? false : true );

   // Pass to parent
   Parent::addStateBlockDesc(prePassStateBlock);
}

PrePassMatInstance::PrePassMatInstance(MatInstance* root, const RenderPrePassMgr *prePassMgr)
: Parent(*root->getMaterial()), mPrePassMgr(prePassMgr)
{
   mFeatureList = root->getRequestedFeatures();
   mVertexFormat = root->getVertexFormat();
}

PrePassMatInstance::~PrePassMatInstance()
{
}

ProcessedMaterial* PrePassMatInstance::getShaderMaterial()
{
   return new ProcessedPrePassMaterial(*mMaterial, mPrePassMgr);
}

bool PrePassMatInstance::init( const FeatureSet &features,
                               const GFXVertexFormat *vertexFormat )
{
   return Parent::init( features, vertexFormat );
}

PrePassMatInstanceHook::PrePassMatInstanceHook( MatInstance *baseMatInst,
                                                const RenderPrePassMgr *prePassMgr )
   : mHookedPrePassMatInst(NULL), mPrePassManager(prePassMgr)
{
   // If the material is a custom material then
   // hope that using DefaultPrePassMaterial gives
   // them a good prepass.
   if ( dynamic_cast<CustomMaterial*>(baseMatInst->getMaterial()) )
   {
      MatInstance* dummyInst = static_cast<MatInstance*>( MATMGR->createMatInstance( "AL_DefaultPrePassMaterial", baseMatInst->getVertexFormat() ) );

      mHookedPrePassMatInst = new PrePassMatInstance( dummyInst, prePassMgr );
      mHookedPrePassMatInst->init( dummyInst->getRequestedFeatures(), baseMatInst->getVertexFormat());

      delete dummyInst;
      return;
   }

   mHookedPrePassMatInst = new PrePassMatInstance(baseMatInst, prePassMgr);
   mHookedPrePassMatInst->init(baseMatInst->getRequestedFeatures(), baseMatInst->getVertexFormat());
}

PrePassMatInstanceHook::~PrePassMatInstanceHook()
{
   SAFE_DELETE(mHookedPrePassMatInst);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void LinearEyeDepthConditioner::processPix( Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd )
{
   // find depth
   ShaderFeature *depthFeat = FEATUREMGR->getByType( MFT_EyeSpaceDepthOut );
   AssertFatal( depthFeat != NULL, "No eye space depth feature found!" );

   Var *depth = (Var*) LangElement::find(depthFeat->getOutputVarName());
   AssertFatal( depth, "Something went bad with ShaderGen. The depth should be already generated by the EyeSpaceDepthOut feature." );

   MultiLine *meta = new MultiLine;

   meta->addStatement( assignOutput( depth ) );

   output = meta;
}

Var *LinearEyeDepthConditioner::_conditionOutput( Var *unconditionedOutput, MultiLine *meta )
{
   Var *retVar = NULL;

   String fracMethodName = (GFX->getAdapterType() == OpenGL) ? "fract" : "frac";

   switch(getBufferFormat())
   {
   case GFXFormatR8G8B8A8:
      retVar = new Var;
      retVar->setType("float4");
      retVar->setName("_ppDepth");
      meta->addStatement( new GenOp( "   // depth conditioner: packing to rgba\r\n" ) );
      meta->addStatement( new GenOp(
         avar( "   @ = %s(@ * (255.0/256) * float4(1, 255, 255 * 255, 255 * 255 * 255));\r\n", fracMethodName.c_str() ),
         new DecOp(retVar), unconditionedOutput ) );
      break;
   default:
      retVar = unconditionedOutput;
      meta->addStatement( new GenOp( "   // depth conditioner: no conditioning\r\n" ) );
      break;
   }

   AssertFatal( retVar != NULL, avar( "Cannot condition output to buffer format: %s", GFXStringTextureFormat[getBufferFormat()] ) );
   return retVar;
}

Var *LinearEyeDepthConditioner::_unconditionInput( Var *conditionedInput, MultiLine *meta )
{
   String float4Typename = (GFX->getAdapterType() == OpenGL) ? "vec4" : "float4";

   Var *retVar = conditionedInput;
   if(getBufferFormat() != GFXFormat_COUNT)
   {
      retVar = new Var;
      retVar->setType(float4Typename.c_str());
      retVar->setName("_ppDepth");
      meta->addStatement( new GenOp( avar( "   @ = %s(0, 0, 1, 1);\r\n", float4Typename.c_str() ), new DecOp(retVar) ) );

      switch(getBufferFormat())
      {
      case GFXFormatR32F:
      case GFXFormatR16F:
         meta->addStatement( new GenOp( "   // depth conditioner: float texture\r\n" ) );
         meta->addStatement( new GenOp( "   @.w = @.r;\r\n", retVar, conditionedInput ) );
         break;

      case GFXFormatR8G8B8A8:
         meta->addStatement( new GenOp( "   // depth conditioner: unpacking from rgba\r\n" ) );
         meta->addStatement( new GenOp(
            avar( "   @.w = dot(@ * (256.0/255), %s(1, 1 / 255, 1 / (255 * 255), 1 / (255 * 255 * 255)));\r\n", float4Typename.c_str() )
            , retVar, conditionedInput ) );
         break;
      default:
         AssertFatal(false, "LinearEyeDepthConditioner::_unconditionInput - Unrecognized buffer format");
      }
   }

   return retVar;
}

Var* LinearEyeDepthConditioner::printMethodHeader( MethodType methodType, const String &methodName, Stream &stream, MultiLine *meta )
{
   const bool isCondition = ( methodType == ConditionerFeature::ConditionMethod );

   Var *retVal = NULL;

   // The uncondition method inputs are changed
   if( isCondition )
      retVal = Parent::printMethodHeader( methodType, methodName, stream, meta );
   else
   {
      Var *methodVar = new Var;
      methodVar->setName(methodName);
      if (GFX->getAdapterType() == OpenGL)
         methodVar->setType("vec4");
      else
         methodVar->setType("inline float4");
      DecOp *methodDecl = new DecOp(methodVar);

      Var *prepassSampler = new Var;
      prepassSampler->setName("prepassSamplerVar");
      prepassSampler->setType("sampler2D");
      DecOp *prepassSamplerDecl = new DecOp(prepassSampler);

      Var *screenUV = new Var;
      screenUV->setName("screenUVVar");
      if (GFX->getAdapterType() == OpenGL)
         screenUV->setType("vec2");
      else
         screenUV->setType("float2");
      DecOp *screenUVDecl = new DecOp(screenUV);

      Var *bufferSample = new Var;
      bufferSample->setName("bufferSample");
      if (GFX->getAdapterType() == OpenGL)
         bufferSample->setType("vec4");
      else
         bufferSample->setType("float4");
      DecOp *bufferSampleDecl = new DecOp(bufferSample);

      meta->addStatement( new GenOp( "@(@, @)\r\n", methodDecl, prepassSamplerDecl, screenUVDecl ) );

      meta->addStatement( new GenOp( "{\r\n" ) );

      meta->addStatement( new GenOp( "   // Sampler g-buffer\r\n" ) );

      // The linear depth target has no mipmaps, so use tex2dlod when
      // possible so that the shader compiler can optimize.
      meta->addStatement( new GenOp( "   #if TORQUE_SM >= 30\r\n" ) );
      if (GFX->getAdapterType() == OpenGL)
         meta->addStatement( new GenOp( "    @ = texture2DLod(@, @, 0); \r\n", bufferSampleDecl, prepassSampler, screenUV) );
      else
         meta->addStatement( new GenOp( "      @ = tex2Dlod(@, float4(@,0,0));\r\n", bufferSampleDecl, prepassSampler, screenUV ) );
      meta->addStatement( new GenOp( "   #else\r\n" ) );
      if (GFX->getAdapterType() == OpenGL)
         meta->addStatement( new GenOp( "    @ = texture2D(@, @);\r\n", bufferSampleDecl, prepassSampler, screenUV) );
      else
         meta->addStatement( new GenOp( "      @ = tex2D(@, @);\r\n", bufferSampleDecl, prepassSampler, screenUV ) );
      meta->addStatement( new GenOp( "   #endif\r\n\r\n" ) );

      // We don't use this way of passing var's around, so this should cause a crash
      // if something uses this improperly
      retVal = bufferSample;
   }

   return retVal;
}
