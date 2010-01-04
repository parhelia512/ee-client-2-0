//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "renderInstance/renderMeshMgr.h"

#include "console/consoleTypes.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "materials/sceneData.h"
#include "materials/processedMaterial.h"
#include "materials/materialManager.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxDebugEvent.h"
#include "math/util/matrixSet.h"

//**************************************************************************
// RenderMeshMgr
//**************************************************************************
IMPLEMENT_CONOBJECT(RenderMeshMgr);

RenderMeshMgr::RenderMeshMgr()
: RenderBinManager(RenderPassManager::RIT_Mesh, 1.0f, 1.0f)
{
}

RenderMeshMgr::RenderMeshMgr(RenderInstType riType, F32 renderOrder, F32 processAddOrder)
   : RenderBinManager(riType, renderOrder, processAddOrder)
{
}

void RenderMeshMgr::init()
{
   GFXStateBlockDesc d;

   d.cullDefined = true;
   d.cullMode = GFXCullCCW;
   d.samplersDefined = true;
   d.samplers[0] = GFXSamplerStateDesc::getWrapLinear();

   mNormalSB = GFX->createStateBlock(d);

   d.cullMode = GFXCullCW;
   mReflectSB = GFX->createStateBlock(d);
}

void RenderMeshMgr::initPersistFields()
{
   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------
// add element
//-----------------------------------------------------------------------------
RenderBinManager::AddInstResult RenderMeshMgr::addElement( RenderInst *inst )
{
   if (inst->type != mRenderInstType)
      return RenderBinManager::arSkipped;

   // If this instance is translucent handle it in RenderTranslucentMgr
   if (inst->translucentSort)
      return RenderBinManager::arSkipped;

   AssertFatal( inst->defaultKey != 0, "RenderMeshMgr::addElement() - Got null sort key... did you forget to set it?" );

   internalAddElement(inst);

   return RenderBinManager::arAdded;
}

//-----------------------------------------------------------------------------
// render
//-----------------------------------------------------------------------------
void RenderMeshMgr::render(SceneState * state)
{
   PROFILE_SCOPE(RenderMeshMgr_render);

   // Early out if nothing to draw.
   if(!mElementList.size())
      return;


   GFXDEBUGEVENT_SCOPE( RenderMeshMgr_Render, ColorI::GREEN );

   // Automagically save & restore our viewport and transforms.
   GFXTransformSaver saver;

   // Restore transforms
   MatrixSet &matrixSet = getParentManager()->getMatrixSet();
   matrixSet.restoreSceneViewProjection();

   // init loop data
   GFXVertexBuffer * lastVB = NULL;
   GFXPrimitiveBuffer * lastPB = NULL;

   GFXTextureObject *lastLM = NULL;
   GFXCubemap *lastCubemap = NULL;
   GFXTextureObject *lastReflectTex = NULL;
   GFXTextureObject *lastMiscTex = NULL;

   SceneGraphData sgData;
   U32 binSize = mElementList.size();

   for( U32 j=0; j<binSize; )
   {
      MeshRenderInst *ri = static_cast<MeshRenderInst*>(mElementList[j].inst);

      setupSGData( ri, sgData );
      BaseMatInstance *mat = ri->matInst;

      // .ifl?
      if( !mat )
      {
         // Deal with reflect pass.
         if ( state->isReflectPass() )
            GFX->setStateBlock(mReflectSB);
         else
            GFX->setStateBlock(mNormalSB);

         GFX->pushWorldMatrix();
         GFX->setWorldMatrix(*ri->objectToWorld);

         GFX->setTexture( 0, ri->miscTex );
         GFX->setPrimitiveBuffer(*ri->primBuff );
         GFX->setVertexBuffer(*ri->vertBuff );
         GFX->disableShaders();
         GFX->setupGenericShaders( GFXDevice::GSModColorTexture );
         GFX->drawPrimitive( ri->primBuffIndex );

         GFX->popWorldMatrix();

         lastVB = NULL;    // no longer valid, null it
         lastPB = NULL;    // no longer valid, null it

         j++;
         continue;
      }

      // If we have an override delegate then give it a 
      // chance to swap the material with another.
      if ( mMatOverrideDelegate )
      {
         mat = mMatOverrideDelegate( mat );
         if ( !mat )
         {
            j++;
            continue;
         }
      }

      if( !mat )
         mat = MATMGR->getWarningMatInstance();


      U32 matListEnd = j;
      lastMiscTex = sgData.miscTex;

      while( mat && mat->setupPass(state, sgData ) )
      {
         U32 a;
         for( a=j; a<binSize; a++ )
         {
            MeshRenderInst *passRI = static_cast<MeshRenderInst*>(mElementList[a].inst);

            if (newPassNeeded(mat, passRI) || lastMiscTex != passRI->miscTex )
            {
               lastLM = NULL;  // pointer no longer valid after setupPass() call
               break;
            }

            matrixSet.setWorld(*passRI->objectToWorld);
            matrixSet.setView(*passRI->worldToCamera);
            matrixSet.setProjection(*passRI->projection);
            mat->setTransforms(matrixSet, state);

            setupSGData( passRI, sgData );
            mat->setSceneInfo(state, sgData);

            mat->setBuffers(passRI->vertBuff, passRI->primBuff);

            // TODO: This could proably be done in a cleaner way.
            //
            // This section of code is dangerous, it overwrites the
            // lightmap values in sgData.  This could be a problem when multiple
            // render instances use the same multi-pass material.  When
            // the first pass is done, setupPass() is called again on
            // the material, but the lightmap data has been changed in
            // sgData to the lightmaps in the last renderInstance rendered.

            // This section sets the lightmap data for the current batch.
            // For the first iteration, it sets the same lightmap data,
            // however the redundancy will be caught by GFXDevice and not
            // actually sent to the card.  This is done for simplicity given
            // the possible condition mentioned above.  Better to set always
            // than to get bogged down into special case detection.
            //-------------------------------------
            bool dirty = false;

            // set the lightmaps if different
            if( passRI->lightmap && passRI->lightmap != lastLM )
            {
               sgData.lightmap = passRI->lightmap;
               lastLM = passRI->lightmap;
               dirty = true;
            }

            // set the cubemap if different.
            if ( passRI->cubemap != lastCubemap )
            {
               sgData.cubemap = passRI->cubemap;
               lastCubemap = passRI->cubemap;
               dirty = true;
            }

            if ( passRI->reflectTex != lastReflectTex )
            {
               sgData.reflectTex = passRI->reflectTex;
               lastReflectTex = passRI->reflectTex;
               dirty = true;
            }

			         if ( dirty )
               mat->setTextureStages( state, sgData );

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
}

