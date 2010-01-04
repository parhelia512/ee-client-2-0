//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/shadowMapPass.h"

#include "lighting/shadowMap/lightShadowMap.h"
#include "lighting/shadowMap/shadowMapManager.h"
#include "lighting/shadowMap/shadowMatHook.h"
#include "lighting/lightManager.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "renderInstance/renderPassManager.h"
#include "renderInstance/renderObjectMgr.h"
#include "renderInstance/renderMeshMgr.h"
#include "renderInstance/renderTerrainMgr.h"
#include "core/util/safeDelete.h"
#include "console/consoleTypes.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDebugEvent.h"
#include "platform/platformTimer.h"


const String ShadowMapPass::PassTypeName("ShadowMap");

U32 ShadowMapPass::smActiveShadowMaps = 0;
U32 ShadowMapPass::smUpdatedShadowMaps = 0;
U32 ShadowMapPass::smShadowMapsDrawCalls = 0;
U32 ShadowMapPass::smShadowMapPolyCount = 0;
U32 ShadowMapPass::smRenderTargetChanges = 0;
U32 ShadowMapPass::smShadowPoolTexturesCount = 0.;
F32 ShadowMapPass::smShadowPoolMemory = 0.0f;

bool ShadowMapPass::smDisableShadows = false;

/// We have a default 8ms render budget for shadow rendering.
U32 ShadowMapPass::smRenderBudgetMs = 8;

ShadowMapPass::ShadowMapPass(LightManager* lightManager, ShadowMapManager* shadowManager)
{
   mLightManager = lightManager;
   mShadowManager = shadowManager;
   mShadowRPM = new ShadowRenderPassManager();
   mShadowRPM->assignName( "ShadowRenderPassManager" );
   mShadowRPM->registerObject();
   Sim::getRootGroup()->addObject( mShadowRPM );

   // Setup our render pass manager

   RenderBinManager *renderBin = new RenderMeshMgr(RenderPassManager::RIT_Mesh, 0.3f, 0.3f);
   renderBin->getMatOverrideDelegate().bind( this, &ShadowMapPass::_overrideDelegate );
   mShadowRPM->addManager( renderBin );

   renderBin = new RenderMeshMgr( RenderPassManager::RIT_Interior, 0.4f, 0.4f );
   renderBin->getMatOverrideDelegate().bind( this, &ShadowMapPass::_overrideDelegate );
   mShadowRPM->addManager( renderBin );

   //mRenderObjMgr = new RenderObjectMgr();
   //mRenderObjMgr->getMatOverrideDelegate().bind( this, &ShadowMapPass::_overrideDelegate );
   //mShadowRPM->addManager(mRenderObjMgr);

   renderBin = new RenderTerrainMgr(0.5f, 0.5f);
   renderBin->getMatOverrideDelegate().bind( this, &ShadowMapPass::_overrideDelegate );
   mShadowRPM->addManager( renderBin );

   mActiveLights = 0;

   mTimer = PlatformTimer::create();

   Con::addVariable( "$ShadowStats::activeMaps", TypeS32, &smActiveShadowMaps );
   Con::addVariable( "$ShadowStats::updatedMaps", TypeS32, &smUpdatedShadowMaps );
   Con::addVariable( "$ShadowStats::drawCalls", TypeS32, &smShadowMapsDrawCalls );
   Con::addVariable( "$ShadowStats::polyCount", TypeS32, &smShadowMapPolyCount );
   Con::addVariable( "$ShadowStats::rtChanges", TypeS32, &smRenderTargetChanges );
   Con::addVariable( "$ShadowStats::poolTexCount", TypeS32, &smShadowPoolTexturesCount );
   Con::addVariable( "$ShadowStats::poolTexMemory", TypeF32, &smShadowPoolMemory );

   Con::addVariable( "$ShadowMap::disableShadows", TypeBool, &smDisableShadows );
}

ShadowMapPass::~ShadowMapPass()
{
   SAFE_DELETE( mTimer );

   if ( mShadowRPM )
      mShadowRPM->deleteObject();
}

void ShadowMapPass::render(   SceneGraph *sceneManager, 
                              const SceneState *diffuseState, 
                              U32 objectMask )
{
   PROFILE_SCOPE( ShadowMapPass_Render );

   // Prep some shadow rendering stats.
   smActiveShadowMaps = 0;
   smUpdatedShadowMaps = 0;
   GFXDeviceStatistics stats;
   stats.start( GFX->getDeviceStatistics() );

   // NOTE: The lights were already registered by SceneGraph.

   // Update mLights
   mLights.clear();
   mLightManager->getAllUnsortedLights( mLights );
   mActiveLights = mLights.size();

   // Start tracking time here so we can stop
   // when we've gone over the shadow update 
   // frame budget.
   const U32 currTime = Platform::getRealMilliseconds();

   // First do a loop thru the lights setting up the shadow
   // info array for this pass.
   Vector<LightShadowMap*> shadowMaps;
   shadowMaps.reserve( mActiveLights );
   for ( U32 i = 0; i < mActiveLights; i++ )
   {
      ShadowMapParams *params = mLights[i]->getExtended<ShadowMapParams>();

      // Before we do anything... skip lights without shadows.      
      if ( !mLights[i]->getCastShadows() || smDisableShadows )
         continue;

      LightShadowMap *lsm = params->getOrCreateShadowMap();

      // First check the visiblity query... if it wasn't 
      // visible skip it.
      if ( lsm->wasOccluded() )
         continue;

      // Any shadow that is visible is counted as being 
      // active regardless if we update it or not.
      ++smActiveShadowMaps;

      lsm->updatePriority( diffuseState, currTime );

      // Do lod... but only on view independent shadows.
      if ( !lsm->isViewDependent() )
      {
         F32 lodScale = lsm->getLastScreenSize() / 600.0f;         
         if ( lodScale < 0.25f )
            continue;

         U32 msDelta = mClampU( currTime - lsm->getLastUpdate(), 1, U32_MAX );
         if ( ( msDelta * mPow( lodScale, 2.0f ) ) < 2 )
            continue;
      }

      shadowMaps.push_back( lsm );
   }

   // Now sort the shadow info by priority.
   shadowMaps.sort( LightShadowMap::cmpPriority );

   GFXDEBUGEVENT_SCOPE( ShadowMapPass_Render, ColorI::RED );

   // Ok, let's render out the shadow maps.
   sceneManager->pushRenderPass( mShadowRPM );

   // Use a timer for tracking our shadow rendering 
   // budget to ensure a high precision results.
   mTimer->getElapsedMs();
   mTimer->reset();

   for ( U32 i = 0; i < shadowMaps.size(); i++ )
   {
      LightShadowMap *lsm = shadowMaps[i];

      // For material override delegate below.
      mActiveShadowType = lsm->getShadowType();

      {
         GFXDEBUGEVENT_SCOPE( ShadowMapPass_Render_Shadow, ColorI::RED );

         mShadowManager->setLightShadowMap( lsm );
         lsm->render( sceneManager, diffuseState );
         ++smUpdatedShadowMaps;
      }

      // See if we're over our frame budget for shadow 
      // updates... give up completely in that case.
      if ( mTimer->getElapsedMs() > smRenderBudgetMs )
         break;
   }

   // Cleanup old unused textures.
   LightShadowMap::releaseUnusedTextures();

   // Update the stats.
   stats.end( GFX->getDeviceStatistics() );
   smShadowMapsDrawCalls = stats.mDrawCalls;
   smShadowMapPolyCount = stats.mPolyCount;
   smRenderTargetChanges = stats.mRenderTargetChanges;
   smShadowPoolTexturesCount = ShadowMapProfile.getStats().activeCount;
   smShadowPoolMemory = ( ShadowMapProfile.getStats().activeBytes / 1024.0f ) / 1024.0f;

   // The NULL here is importaint as having it around
   // will cause extra work in AdvancedLightManager::setLightInfo()/
   mShadowManager->setLightShadowMap( NULL );

   sceneManager->popRenderPass();
}

BaseMatInstance* ShadowMapPass::_overrideDelegate( BaseMatInstance *inMat )
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

   return hook->getShadowMat( mActiveShadowType );
}

void ShadowRenderPassManager::addInst( RenderInst *inst )
{
   if(inst->type == RIT_Mesh || inst->type == RIT_Interior )
   {
      MeshRenderInst *meshRI = static_cast<MeshRenderInst *>(inst);
      if(meshRI != NULL && meshRI->matInst)
      {
         // TODO: Should castsShadows() override isTranslucent() ? 
         //       This would mess some things up I think.
         if(!meshRI->matInst->getMaterial()->castsShadows() ||
            meshRI->matInst->getMaterial()->isTranslucent())
         {
            // Do not add this instance, return here and avoid the default behavior
            // of calling up to Parent::addInst()
            return;
         }
      }
   }

   Parent::addInst(inst);
}