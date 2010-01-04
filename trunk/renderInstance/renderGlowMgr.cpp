//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "renderInstance/renderGlowMgr.h"

#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "materials/sceneData.h"
#include "materials/matInstance.h"
#include "materials/materialFeatureTypes.h"
#include "materials/processedMaterial.h"
#include "postFx/postEffect.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDebugEvent.h"
#include "math/util/matrixSet.h"

IMPLEMENT_CONOBJECT( RenderGlowMgr );


const MatInstanceHookType RenderGlowMgr::GlowMaterialHook::Type( "Glow" );


RenderGlowMgr::GlowMaterialHook::GlowMaterialHook( BaseMatInstance *matInst )
   : mGlowMatInst( NULL )
{
   mGlowMatInst = (MatInstance*)matInst->getMaterial()->createMatInstance();
   mGlowMatInst->getFeaturesDelegate().bind( &GlowMaterialHook::_overrideFeatures );
   mGlowMatInst->init(  matInst->getRequestedFeatures(), 
                        matInst->getVertexFormat() );
}

RenderGlowMgr::GlowMaterialHook::~GlowMaterialHook()
{
   SAFE_DELETE( mGlowMatInst );
}

void RenderGlowMgr::GlowMaterialHook::_overrideFeatures( ProcessedMaterial *mat,
                                                         U32 stageNum,
                                                         MaterialFeatureData &fd, 
                                                         const FeatureSet &features )
{
   // If this isn't a glow pass... then add the glow mask feature.
   if (  mat->getMaterial() && 
         !mat->getMaterial()->mGlow[stageNum] )
      fd.features.addFeature( MFT_GlowMask );

   // Don't allow fog or HDR encoding on 
   // the glow materials.
   fd.features.removeFeature( MFT_Fog );
   fd.features.removeFeature( MFT_HDROut );
}

RenderGlowMgr::RenderGlowMgr()
   : RenderTexTargetBinManager(  RenderPassManager::RIT_Mesh, 
                                 1.0f, 
                                 1.0f,
                                 GFXFormatR8G8B8A8,
                                 Point2I( 512, 512 ) )
{
   mTargetSizeType = WindowSize;
   MatTextureTarget::registerTarget( "glowbuffer", this );
}

RenderGlowMgr::~RenderGlowMgr()
{
   MatTextureTarget::unregisterTarget( "glowbuffer", this );   
}

bool RenderGlowMgr::isGlowEnabled()
{
   if ( !mGlowEffect )
      mGlowEffect = dynamic_cast<PostEffect*>( Sim::findObject( "GlowPostFx" ) );

   return mGlowEffect && mGlowEffect->isEnabled();
}

RenderBinManager::AddInstResult RenderGlowMgr::addElement( RenderInst *inst )
{
   // Skip out if we don't have the glow post 
   // effect enabled at this time.
   if ( !isGlowEnabled() )
      return RenderBinManager::arSkipped;

   // TODO: We need to get the scene state here in a more reliable
   // manner so we can skip glow in a non-diffuse render pass.
   //if ( !mParentManager->getSceneManager()->getSceneState()->isDiffusePass() )
      //return RenderBinManager::arSkipped;
      
   BaseMatInstance* matInst = getMaterial(inst);
   bool hasGlow = matInst && matInst->hasGlow();
   if ( !hasGlow )   
      return RenderBinManager::arSkipped;

   internalAddElement(inst);

   return RenderBinManager::arAdded;
}

void RenderGlowMgr::render( SceneState *state )
{
   PROFILE_SCOPE( RenderGlowMgr_Render );

   // Don't allow non-diffuse passes.
   if ( !state->isDiffusePass() )
      return;

   GFXDEBUGEVENT_SCOPE( RenderGlowMgr_Render, ColorI::GREEN );

   GFXTransformSaver saver;

   // Tell the superclass we're about to render, preserve contents
   const bool isRenderingToTarget = _onPreRender( state, true );

   // Clear all the buffers to black.
   GFX->clear( GFXClearTarget, ColorI::BLACK, 1.0f, 0);

   // Restore transforms
   MatrixSet &matrixSet = getParentManager()->getMatrixSet();
   matrixSet.restoreSceneViewProjection();

   // init loop data
   SceneGraphData sgData;
   U32 binSize = mElementList.size();

   for( U32 j=0; j<binSize; )
   {
      MeshRenderInst *ri = static_cast<MeshRenderInst*>(mElementList[j].inst);

      setupSGData( ri, sgData );
      sgData.binType = SceneGraphData::GlowBin;

      BaseMatInstance *mat = ri->matInst;
      GlowMaterialHook *hook = mat->getHook<GlowMaterialHook>();
      if ( !hook )
      {
         hook = new GlowMaterialHook( ri->matInst );
         ri->matInst->addHook( hook );
      }
      BaseMatInstance *glowMat = hook->getMatInstance();

      U32 matListEnd = j;

      while( glowMat && glowMat->setupPass( state, sgData ) )
      {
         U32 a;
         for( a=j; a<binSize; a++ )
         {
            MeshRenderInst *passRI = static_cast<MeshRenderInst*>(mElementList[a].inst);

            if (newPassNeeded(mat, passRI))
               break;

            matrixSet.setWorld(*passRI->objectToWorld);
            matrixSet.setView(*passRI->worldToCamera);
            matrixSet.setProjection(*passRI->projection);
            glowMat->setTransforms(matrixSet, state);

            glowMat->setSceneInfo(state, sgData);
            glowMat->setBuffers(passRI->vertBuff, passRI->primBuff);

            if ( passRI->prim )
               GFX->drawPrimitive( *passRI->prim );
            else
               GFX->drawPrimitive( passRI->primBuffIndex );
         }
         matListEnd = a;
      }

      // force increment if none happened, otherwise go to end of batch
      j = ( j == matListEnd ) ? j+1 : matListEnd;
   }

   // Finish up.
   if ( isRenderingToTarget )
      _onPostRender();
}
