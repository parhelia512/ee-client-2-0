//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/basic/basicLightManager.h"

#include "platform/platformTimer.h"
#include "console/simSet.h"
#include "console/consoleTypes.h"
#include "core/util/safeDelete.h"
#include "materials/processedMaterial.h"
#include "shaderGen/shaderFeature.h"
#include "lighting/basic/basicSceneObjectLightingPlugin.h"
#include "shaderGen/shaderGenVars.h"
#include "gfx/gfxShader.h"
#include "materials/sceneData.h"
#include "materials/materialParameters.h"
#include "materials/materialManager.h"
#include "materials/materialFeatureTypes.h"
#include "math/util/frustum.h"
#include "sceneGraph/sceneObject.h"
#include "renderInstance/renderPrePassMgr.h"
#include "shaderGen/featureMgr.h"
#include "shaderGen/HLSL/shaderFeatureHLSL.h"
#include "shaderGen/HLSL/bumpHLSL.h"
#include "shaderGen/HLSL/pixSpecularHLSL.h"
#include "lighting/shadowMap/shadowMatHook.h"


#ifdef TORQUE_OS_MAC
#include "shaderGen/GLSL/shaderFeatureGLSL.h"
#include "shaderGen/GLSL/bumpGLSL.h"
#include "shaderGen/GLSL/pixSpecularGLSL.h"
#endif

U32 BasicLightManager::smActiveShadowPlugins = 0;
U32 BasicLightManager::smShadowsUpdated = 0;
U32 BasicLightManager::smElapsedUpdateMs = 0;

F32 BasicLightManager::smProjectedShadowFilterDistance = 40.0f;

// Dummy used to force initialization!
BasicLightManager *foo = BLM;

static S32 QSORT_CALLBACK comparePluginScores( const void *a, const void *b )
{
   const BasicSceneObjectLightingPlugin *A = *((BasicSceneObjectLightingPlugin**)a);
   const BasicSceneObjectLightingPlugin *B = *((BasicSceneObjectLightingPlugin**)b);     
   
   F32 dif = B->getScore() - A->getScore();
   return (S32)mFloor( dif );
}

BasicLightManager::BasicLightManager()
   : LightManager( "Basic Lighting", "BLM" ),
     mLastShader(NULL),
     mLastConstants(NULL)
{
   mTimer = PlatformTimer::create();
}

BasicLightManager::~BasicLightManager()
{
   mLastShader = NULL;
   mLastConstants = NULL;

   for (LightConstantMap::Iterator i = mConstantLookup.begin(); i != mConstantLookup.end(); i++)
   {
      if (i->value)
         SAFE_DELETE(i->value);
   }
   mConstantLookup.clear();

   if (mTimer)
      SAFE_DELETE( mTimer );
}

bool BasicLightManager::isCompatible() const
{
   // As long as we have some shaders this works.
   return GFX->getPixelShaderVersion() > 1.0;
}

void BasicLightManager::activate( SceneGraph *sceneManager )
{
   Parent::activate( sceneManager );

   if( GFX->getAdapterType() == OpenGL )
   {
      #ifdef TORQUE_OS_MAC
         FEATUREMGR->registerFeature( MFT_LightMap, new LightmapFeatGLSL );
         FEATUREMGR->registerFeature( MFT_ToneMap, new TonemapFeatGLSL );
         FEATUREMGR->registerFeature( MFT_NormalMap, new BumpFeatGLSL );
         FEATUREMGR->registerFeature( MFT_RTLighting, new RTLightingFeatGLSL );
         FEATUREMGR->registerFeature( MFT_PixSpecular, new PixelSpecularGLSL );
      #endif
   }
   else
   {
      #ifndef TORQUE_OS_MAC
         FEATUREMGR->registerFeature( MFT_LightMap, new LightmapFeatHLSL );
         FEATUREMGR->registerFeature( MFT_ToneMap, new TonemapFeatHLSL );
         FEATUREMGR->registerFeature( MFT_NormalMap, new BumpFeatHLSL );
         FEATUREMGR->registerFeature( MFT_RTLighting, new RTLightingFeatHLSL );
         FEATUREMGR->registerFeature( MFT_PixSpecular, new PixelSpecularHLSL );
      #endif
   }

   FEATUREMGR->unregisterFeature( MFT_MinnaertShading );
   FEATUREMGR->unregisterFeature( MFT_SubSurface );

   // First look for the prepass bin...
   RenderPrePassMgr *prePassBin = NULL;
   RenderPassManager *rpm = getSceneManager()->getRenderPass();
   for( U32 i = 0; i < rpm->getManagerCount(); i++ )
   {
      RenderBinManager *bin = rpm->getManager(i);
      if( bin->getRenderInstType() == RenderPrePassMgr::RIT_PrePass )
      {
         prePassBin = (RenderPrePassMgr*)bin;
         break;
      }
   }

   /*
   // If you would like to use forward shading, and have a linear depth pre-pass
   // than un-comment this code block.
   if ( !prePassBin )
   {
      Vector<GFXFormat> formats;
      formats.push_back( GFXFormatR32F );
      formats.push_back( GFXFormatR16F );
      formats.push_back( GFXFormatR8G8B8A8 );
      GFXFormat linearDepthFormat = GFX->selectSupportedFormat( &GFXDefaultRenderTargetProfile,
         formats,
         true,
         false );

      // Uncomment this for a no-color-write z-fill pass. 
      //linearDepthFormat = GFXFormat_COUNT;

      prePassBin = new RenderPrePassMgr( linearDepthFormat != GFXFormat_COUNT, linearDepthFormat );
      prePassBin->registerObject();
      rpm->addManager( prePassBin );
   }
   */
   mPrePassRenderBin = prePassBin;

   // If there is a prepass bin
   MATMGR->setPrePassEnabled( mPrePassRenderBin.isValid() );
   gClientSceneGraph->setPostEffectFog( mPrePassRenderBin.isValid() && mPrePassRenderBin->getTargetChainLength() > 0  );

   // Tell the material manager that we don't use prepass.
   MATMGR->setPrePassEnabled( false );

   GFXShader::addGlobalMacro( "TORQUE_BASIC_LIGHTING" );

   // Hook into the SceneGraph prerender signal.
   sceneManager->getPreRenderSignal().notify( this, &BasicLightManager::_onPreRender );

   // Last thing... let everyone know we're active.
   smActivateSignal.trigger( getId(), true );

   Con::addVariable( "$BasicLightManagerStats::activePlugins", TypeS32, &smActiveShadowPlugins );
   Con::addVariable( "$BasicLightManagerStats::shadowsUpdated", TypeS32, &smShadowsUpdated );
   Con::addVariable( "$BasicLightManagerStats::elapsedUpdateMs", TypeS32, &smElapsedUpdateMs );

   Con::addVariable( "$BasicLightManager::shadowFilterDistance", TypeF32, &smProjectedShadowFilterDistance );

   // Get our BL projected shadow render pass manager
   RenderPassManager *projectedShadowRPM = NULL;
   if ( !Sim::findObject( "BL_ProjectedShadowRPM", projectedShadowRPM ) )
      return;
   
   // Get the first (and only) render bin
   RenderBinManager *meshMgr = projectedShadowRPM->getManager( 0 );
   if ( !meshMgr )
      return;

   // Set up the material override delegate on the render bin
   meshMgr->getMatOverrideDelegate().bind( this, &BasicLightManager::_shadowMaterialOverride );
}

void BasicLightManager::deactivate()
{
   Parent::deactivate();

   mLastShader = NULL;
   mLastConstants = NULL;

   for (LightConstantMap::Iterator i = mConstantLookup.begin(); i != mConstantLookup.end(); i++)
   {
      if (i->value)
         SAFE_DELETE(i->value);
   }
   mConstantLookup.clear();

   if ( mPrePassRenderBin )
      mPrePassRenderBin->deleteObject();
   mPrePassRenderBin = NULL;

   GFXShader::removeGlobalMacro( "TORQUE_BASIC_LIGHTING" );

   // Remove us from the prerender signal.
   getSceneManager()->getPreRenderSignal().remove( this, &BasicLightManager::_onPreRender );

   // Now let everyone know we've deactivated.
   smActivateSignal.trigger( getId(), false );
}

void BasicLightManager::_onPreRender( SceneGraph *sceneManger, const SceneState *state )
{
   // Update all our shadow plugins here!
   Vector<BasicSceneObjectLightingPlugin*> *pluginInsts = BasicSceneObjectLightingPlugin::getPluginInstances();

   Vector<BasicSceneObjectLightingPlugin*>::const_iterator pluginIter = (*pluginInsts).begin();
   for ( ; pluginIter != (*pluginInsts).end(); pluginIter++ )
   {
      BasicSceneObjectLightingPlugin *plugin = *pluginIter;
      plugin->updateShadow( (SceneState*)state );
   }

   U32 pluginCount = (*pluginInsts).size();

   // Sort them by the score.
   dQsort( (*pluginInsts).address(), pluginCount, sizeof(BasicSceneObjectLightingPlugin*), comparePluginScores );

   mTimer->getElapsedMs();
   mTimer->reset();
   U32 numUpdated = 0;
   U32 targetMs = 5;

   S32 updateMs = 0;

   // NOTE: This is a hack to work around the state key
   // system and allow prepRenderImage to be called directly
   // on a SceneObject without going thru regular traversal.
   //
   // See ProjectedShadow::_renderToTexture.
   //
   sceneManger->incStateKey();

   pluginIter = (*pluginInsts).begin();
   for ( ; pluginIter != (*pluginInsts).end(); pluginIter++ )
   {
      BasicSceneObjectLightingPlugin *plugin = *pluginIter;

      // If we run out of update time then stop.
      updateMs = mTimer->getElapsedMs();
      if ( updateMs >= targetMs )
         break;

      // NOTE! Fix this all up to past const SceneState!
      plugin->renderShadow( (SceneState*)state );
      numUpdated++;
   }

   smShadowsUpdated = numUpdated;
   smActiveShadowPlugins = pluginCount;
   smElapsedUpdateMs = updateMs;
}

BaseMatInstance* BasicLightManager::_shadowMaterialOverride( BaseMatInstance *inMat )
{
   // See if we have an existing material hook.
   ShadowMaterialHook *hook = static_cast<ShadowMaterialHook*>( inMat->getHook( ShadowMaterialHook::Type ) );
   if ( !hook )
   {
      // Create a hook and initialize it using the incoming material.
      hook = new ShadowMaterialHook;
      hook->init( inMat ); 
      inMat->addHook( hook );
   }

   return hook->getShadowMat( ShadowType_Spot );
}

BasicLightManager::LightingShaderConstants::LightingShaderConstants()
   :  mInit( false ),
      mShader( NULL ),
      mLightPosition( NULL ),
      mLightDiffuse( NULL ),
      mLightAmbient( NULL ),
      mLightInvRadiusSq( NULL ),
      mLightSpotDir( NULL ),
      mLightSpotAngle( NULL )
{
}

BasicLightManager::LightingShaderConstants::~LightingShaderConstants()
{
   if (mShader.isValid())
   {
      mShader->getReloadSignal().remove( this, &LightingShaderConstants::_onShaderReload );
      mShader = NULL;
   }
}

void BasicLightManager::LightingShaderConstants::init(GFXShader* shader)
{
   if (mShader.getPointer() != shader)
   {
      if (mShader.isValid())
         mShader->getReloadSignal().remove( this, &LightingShaderConstants::_onShaderReload );

      mShader = shader;
      mShader->getReloadSignal().notify( this, &LightingShaderConstants::_onShaderReload );
   }

   mLightPosition = shader->getShaderConstHandle( ShaderGenVars::lightPosition );
   mLightDiffuse = shader->getShaderConstHandle( ShaderGenVars::lightDiffuse);
   mLightInvRadiusSq = shader->getShaderConstHandle( ShaderGenVars::lightInvRadiusSq );
   mLightAmbient = shader->getShaderConstHandle( ShaderGenVars::lightAmbient );   
   mLightSpotDir = shader->getShaderConstHandle( ShaderGenVars::lightSpotDir );
   mLightSpotAngle = shader->getShaderConstHandle( ShaderGenVars::lightSpotAngle );

   mInit = true;
}

void BasicLightManager::LightingShaderConstants::_onShaderReload()
{
   if (mShader.isValid())
      init( mShader );
}

void BasicLightManager::setLightInfo(  ProcessedMaterial* pmat, 
                                       const Material* mat, 
                                       const SceneGraphData& sgData, 
                                       const SceneState *state,
                                       U32 pass, 
                                       GFXShaderConstBuffer* shaderConsts ) 
{
   PROFILE_SCOPE( BasicLightManager_SetLightInfo );

   GFXShader *shader = shaderConsts->getShader();

   // Check to see if this is the same shader.  Since we
   // sort by material we should get hit repeatedly by the
   // same one.  This optimization should save us many 
   // hash table lookups.
   if ( mLastShader.getPointer() != shader )
   {
      LightConstantMap::Iterator iter = mConstantLookup.find(shader);   
      if ( iter != mConstantLookup.end() )
      {
         mLastConstants = iter->value;
      } 
      else 
      {     
         LightingShaderConstants* lsc = new LightingShaderConstants();
         mConstantLookup[shader] = lsc;

         mLastConstants = lsc;      
      }

      // Set our new shader
      mLastShader = shader;
   }

   // Make sure that our current lighting constants are initialized
   if (!mLastConstants->mInit)
      mLastConstants->init(shader);

   // NOTE: If you encounter a crash from this point forward
   // while setting a shader constant its probably because the
   // mConstantLookup has bad shaders/constants in it.
   //
   // This is a known crash bug that can occur if materials/shaders
   // are reloaded and the light manager is not reset.
   //
   // We should look to fix this by clearing the table.

   _update4LightConsts( sgData,
                        mLastConstants->mLightPosition,
                        mLastConstants->mLightDiffuse,
                        mLastConstants->mLightAmbient,
                        mLastConstants->mLightInvRadiusSq,
                        mLastConstants->mLightSpotDir,
                        mLastConstants->mLightSpotAngle,
                        shaderConsts );
}
