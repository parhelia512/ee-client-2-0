//-----------------------------------------------------------------------------
// Copyright (C) Sickhead Games, LLC
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "renderInstance/renderTerrainMgr.h"

#include "platform/profiler.h"
#include "sceneGraph/sceneGraph.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDrawUtil.h"
#include "gfx/gfxDebugEvent.h"
#include "materials/shaderData.h"
#include "materials/matInstance.h"
#include "sceneGraph/sceneState.h"
#include "console/consoleTypes.h"
#include "terrain/terrCell.h"
#include "terrain/terrCellMaterial.h"
#include "math/util/matrixSet.h"

bool RenderTerrainMgr::smRenderWireframe = false;
bool RenderTerrainMgr::smRenderCellBounds = false;
S32 RenderTerrainMgr::smForcedDetailShader = -1;

S32 RenderTerrainMgr::smCellsRendered = 0;
S32 RenderTerrainMgr::smOverrideCells = 0;
S32 RenderTerrainMgr::smDrawCalls = 0;


IMPLEMENT_CONOBJECT(RenderTerrainMgr);

RenderTerrainMgr::RenderTerrainMgr()
   :  RenderBinManager( RenderPassManager::RIT_Terrain, 1.0f, 1.0f )
{
}

RenderTerrainMgr::RenderTerrainMgr( F32 renderOrder, F32 processAddOrder )
   :  RenderBinManager( RenderPassManager::RIT_Terrain, renderOrder, processAddOrder )
{
}

RenderTerrainMgr::~RenderTerrainMgr()
{
}

void RenderTerrainMgr::initPersistFields()
{
   Con::addVariable( "RenderTerrainMgr::renderWireframe", TypeBool, &smRenderWireframe );
   Con::addVariable( "RenderTerrainMgr::renderCellBounds", TypeBool, &smRenderCellBounds );
   Con::addVariable( "RenderTerrainMgr::forceDetailShader", TypeS32, &smForcedDetailShader );

   // For stats.
   GFXDevice::getDeviceEventSignal().notify( &RenderTerrainMgr::_clearStats );
   Con::addVariable( "$TerrainBlock::cellsRendered", TypeS32, &smCellsRendered );
   Con::addVariable( "$TerrainBlock::overrideCells", TypeS32, &smOverrideCells );
   Con::addVariable( "$TerrainBlock::drawCalls", TypeS32, &smDrawCalls );

   Parent::initPersistFields();
}

bool RenderTerrainMgr::_clearStats( GFXDevice::GFXDeviceEventType type )
{
   if ( type == GFXDevice::deStartOfFrame )
   {
      smCellsRendered = 0;
      smOverrideCells = 0;
      smDrawCalls = 0;
   }

   return true;
}

void RenderTerrainMgr::internalAddElement( RenderInst *inst_ )
{
   TerrainRenderInst *inst = static_cast<TerrainRenderInst*>( inst_ );

   mInstVector.push_back( inst );
}

void RenderTerrainMgr::sort()
{
   // TODO: We could probably sort this in some
   // manner to improve terrain rendering perf.
}

void RenderTerrainMgr::clear()
{
   mInstVector.clear();
}

void RenderTerrainMgr::render( SceneState *state )
{
   if ( mInstVector.empty() )
      return;

   PROFILE_SCOPE( RenderTerrainMgr_Render );

   GFXTransformSaver saver;

   // Prepare the common scene graph data.
   SceneGraphData sgData;
   sgData.setFogParams( state->getSceneManager()->getFogData() );
   sgData.objTrans.identity();
   sgData.visibility = 1.0f;
   sgData.wireframe = smRenderWireframe || GFXDevice::getWireframe();

   // Restore transforms
   MatrixSet &matrixSet = getParentManager()->getMatrixSet();
   matrixSet.restoreSceneViewProjection();

   GFXDEBUGEVENT_SCOPE( RenderTerrainMgr_Render, ColorI::GREEN );

   const MatrixF worldViewXfm = matrixSet.getWorldToCamera();
   const MatrixF &projXfm = matrixSet.getCameraToScreen();

   const U32 binSize = mInstVector.size();

   // If we have an override delegate then do a special loop!
   if ( !mMatOverrideDelegate.empty() )
   {      
      PROFILE_SCOPE( RenderTerrainMgr_Render_OverrideMat );

      BaseMatInstance *overideMat = mMatOverrideDelegate( mInstVector[0]->mat );
      while ( overideMat && overideMat->setupPass( state, sgData ) )
      {
         for ( U32 i=0; i < binSize; i++ )
         {
            smOverrideCells++;

            const TerrainRenderInst *inst = mInstVector[i];

            GFX->setPrimitiveBuffer( inst->primBuff );
            GFX->setVertexBuffer( inst->vertBuff );

            matrixSet.setWorld(*inst->objectToWorldXfm);

            overideMat->setSceneInfo( state, sgData );
            overideMat->setTransforms( matrixSet, state );

            GFX->drawPrimitive( inst->prim );
         }
      }

      return;
   }

   // Do the detail map passes.
   for ( U32 i=0; i < binSize; i++ )
   {
      const TerrainRenderInst *inst = mInstVector[i];

      TerrainCellMaterial *mat = inst->cellMat;

      GFX->setPrimitiveBuffer( inst->primBuff );
      GFX->setVertexBuffer( inst->vertBuff );

      ++smCellsRendered;

      mat->setTransformAndEye(   *inst->objectToWorldXfm,
                                 worldViewXfm,
                                 projXfm,
                                 state->getFarPlane() );

      sgData.objTrans = *inst->objectToWorldXfm;
      dMemcpy( sgData.lights, inst->lights, sizeof( sgData.lights ) );

      while ( mat->setupPass( state, sgData ) )
      {
         ++smDrawCalls;
         GFX->drawPrimitive( inst->prim );
      }
   }
}

