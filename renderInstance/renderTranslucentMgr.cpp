//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "renderInstance/renderTranslucentMgr.h"

#include "materials/sceneData.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneObject.h"
#include "sceneGraph/sceneState.h"
#include "materials/matInstance.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDebugEvent.h"
#include "renderInstance/renderParticleMgr.h"
#include "math/util/matrixSet.h"

#define HIGH_NUM ((U32(-1)/2) - 1)

IMPLEMENT_CONOBJECT(RenderTranslucentMgr);


RenderTranslucentMgr::RenderTranslucentMgr()
   :  RenderBinManager( RenderPassManager::RIT_Custom, 1.0f, 1.0f ), mParticleRenderMgr(NULL)
{

}

RenderTranslucentMgr::~RenderTranslucentMgr()
{

}

void RenderTranslucentMgr::setupSGData(MeshRenderInst *ri, SceneGraphData &data )
{
   Parent::setupSGData(ri, data);

   data.backBuffTex = NULL;
   data.cubemap = NULL;   
   data.lightmap = NULL;
}

RenderBinManager::AddInstResult RenderTranslucentMgr::addElement( RenderInst *inst )
{
   // See if we support this instance type.
   if (  inst->type != RenderPassManager::RIT_ObjectTranslucent && 
         inst->type != RenderPassManager::RIT_Translucent &&
         inst->type != RenderPassManager::RIT_Particle)
      return RenderBinManager::arSkipped;

   // See if this instance is translucent.
   if (!inst->translucentSort)
      return RenderBinManager::arSkipped;

   BaseMatInstance* matInst = getMaterial(inst);
   bool translucent = (!matInst || matInst->getMaterial()->isTranslucent());
   if (!translucent)
      return RenderBinManager::arSkipped;

   // We made it this far, add the instance.
   mElementList.increment();
   MainSortElem& elem = mElementList.last();
   elem.inst = inst;   

   // Override the instances default key to be the sort distance. All 
   // the pointer dereferencing is in there to prevent us from losing
   // information when converting to a U32.
   elem.key = *((U32*)&inst->sortDistSq);

   AssertFatal( inst->defaultKey != 0, "RenderTranslucentMgr::addElement() - Got null sort key... did you forget to set it?" );

   // Then use the instances primary key as our secondary key
   elem.key2 = inst->defaultKey;
   
   // We are the only thing to handle translucent "things" right now.
   return RenderBinManager::arStop; 
}

GFXStateBlockRef RenderTranslucentMgr::_getStateBlock( U8 transFlag )
{
   if ( mStateBlocks[transFlag].isValid() )
      return mStateBlocks[transFlag];

   GFXStateBlockDesc d;

   d.cullDefined = true;
   d.cullMode = GFXCullNone;
   d.blendDefined = true;
   d.blendEnable = true;
   d.blendSrc = (GFXBlend)((transFlag >> 4) & 0x0f); 
   d.blendDest = (GFXBlend)(transFlag & 0x0f);
   d.alphaDefined = true;
   
   // See http://www.garagegames.com/mg/forums/result.thread.php?qt=81397
   d.alphaTestEnable = (d.blendSrc == GFXBlendSrcAlpha && (d.blendDest == GFXBlendInvSrcAlpha || d.blendDest == GFXBlendOne));
   d.alphaTestRef = 1;
   d.alphaTestFunc = GFXCmpGreaterEqual;
   d.zDefined = true;
   d.zWriteEnable = false;
   d.samplersDefined = true;
   d.samplers[0] = GFXSamplerStateDesc::getClampLinear();
   d.samplers[0].alphaOp = GFXTOPModulate;
   d.samplers[0].alphaArg1 = GFXTATexture;
   d.samplers[0].alphaArg2 = GFXTADiffuse;

   mStateBlocks[transFlag] = GFX->createStateBlock(d);
   return mStateBlocks[transFlag];
}

void RenderTranslucentMgr::render( SceneState *state )
{
   PROFILE_SCOPE(RenderTranslucentMgr_render);

   // Early out if nothing to draw.
   if(!mElementList.size())
      return;

   GFXDEBUGEVENT_SCOPE(RenderTranslucentMgr_Render, ColorI::BLUE);

   // Find the particle render manager (if we don't have it)
   if(mParticleRenderMgr == NULL)
   {
      RenderPassManager *rpm = state->getRenderPass();
      for( U32 i = 0; i < rpm->getManagerCount(); i++ )
      {
         RenderBinManager *bin = rpm->getManager(i);
         if( bin->getRenderInstType() == RenderParticleMgr::RIT_Particles )
         {
            mParticleRenderMgr = reinterpret_cast<RenderParticleMgr *>(bin);
            break;
         }
      }
   }

   GFXTransformSaver saver;   

   SceneGraphData sgData;

   GFXVertexBuffer * lastVB = NULL;
   GFXPrimitiveBuffer * lastPB = NULL;

   // Restore transforms
   MatrixSet &matrixSet = getParentManager()->getMatrixSet();
   matrixSet.restoreSceneViewProjection();

   U32 binSize = mElementList.size();
   for( U32 j=0; j<binSize; )
   {
      RenderInst *baseRI = mElementList[j].inst;
      
      U32 matListEnd = j;

      // render these separately...
      if ( baseRI->type == RenderPassManager::RIT_ObjectTranslucent )
      {
         ObjectRenderInst* objRI = static_cast<ObjectRenderInst*>(baseRI);
         objRI->renderDelegate( objRI, state, NULL );         

         lastVB = NULL;
         lastPB = NULL;
         j++;
         continue;
      }
      else if ( baseRI->type == RenderPassManager::RIT_Particle )
      {
         ParticleRenderInst *ri = static_cast<ParticleRenderInst*>(baseRI);

         // Tell Particle RM to draw the system. (This allows the particle render manager
         // to manage drawing offscreen particle systems, and allows the systems
         // to be composited back into the scene with proper translucent
         // sorting order)
         mParticleRenderMgr->renderInstance(ri, state);

         lastVB = NULL;    // no longer valid, null it
         lastPB = NULL;    // no longer valid, null it

         j++;
         continue;
      }
      else if ( baseRI->type == RenderPassManager::RIT_Translucent )
      {
         MeshRenderInst* ri = static_cast<MeshRenderInst*>(baseRI);
         BaseMatInstance *mat = ri->matInst;

         // .ifl?
         if( !mat )
         {
            GFX->setStateBlock( _getStateBlock( ri->transFlags ) );

            GFX->pushWorldMatrix();

            GFX->setWorldMatrix(*ri->objectToWorld );

            GFX->setTexture( 0, ri->miscTex );
            GFX->setPrimitiveBuffer( *ri->primBuff );
            GFX->setVertexBuffer( *ri->vertBuff );
            GFX->disableShaders();
            GFX->setupGenericShaders( GFXDevice::GSModColorTexture );
            GFX->drawPrimitive( ri->primBuffIndex );

            GFX->popWorldMatrix();

            lastVB = NULL;    // no longer valid, null it
            lastPB = NULL;    // no longer valid, null it

            j++;
            continue;
         }

         setupSGData( ri, sgData );

         bool firstmatpass = true;
         while( mat->setupPass( state, sgData ) )
         {
           U32 a;
           for( a=j; a<binSize; a++ )
           {
              RenderInst* nextRI = mElementList[a].inst;
              if ( nextRI->type != RenderPassManager::RIT_Translucent )
                 break;

              MeshRenderInst *passRI = static_cast<MeshRenderInst*>(nextRI);

              // if new matInst is null or different, break
              // The visibility check prevents mesh elements with different visibility from being
              // batched together. This can happen when visibility is animated in the dts model.
              if (newPassNeeded(mat, passRI) || passRI->visibility != ri->visibility)
                break;

              // Z sorting and stuff is still not working in this mgr...
              setupSGData( passRI, sgData );
              mat->setSceneInfo(state, sgData);
              matrixSet.setWorld(*passRI->objectToWorld);
              matrixSet.setView(*passRI->worldToCamera);
              matrixSet.setProjection(*passRI->projection);
              mat->setTransforms(matrixSet, state);
              mat->setBuffers(passRI->vertBuff, passRI->primBuff);

              // draw it
              if ( passRI->prim )   
                 GFX->drawPrimitive( *passRI->prim );   
              else  
                 GFX->drawPrimitive( passRI->primBuffIndex );              
           }

           matListEnd = a;
           firstmatpass = false;
         }

         // force increment if none happened, otherwise go to end of batch
         j = ( j == matListEnd ) ? j+1 : matListEnd;
      }
   }
}