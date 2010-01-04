//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "renderInstance/forcedMaterialMeshMgr.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/gfxDebugEvent.h"
#include "materials/sceneData.h"
#include "materials/materialManager.h"
#include "materials/materialDefinition.h"
#include "console/consoleTypes.h"
#include "math/util/matrixSet.h"

IMPLEMENT_CONOBJECT(ForcedMaterialMeshMgr);

ForcedMaterialMeshMgr::ForcedMaterialMeshMgr()
{
   mOverrideInstance = NULL;
   mOverrideMaterial = NULL;
}

ForcedMaterialMeshMgr::ForcedMaterialMeshMgr(RenderInstType riType, F32 renderOrder, F32 processAddOrder, BaseMatInstance* overrideMaterial)
: RenderMeshMgr(riType, renderOrder, processAddOrder)
{
   mOverrideInstance = overrideMaterial;
   mOverrideMaterial = NULL;
}

void ForcedMaterialMeshMgr::setOverrideMaterial(BaseMatInstance* overrideMaterial)
{
   SAFE_DELETE(mOverrideInstance);
   mOverrideInstance = overrideMaterial;
}

ForcedMaterialMeshMgr::~ForcedMaterialMeshMgr()
{
   setOverrideMaterial(NULL);
}

void ForcedMaterialMeshMgr::initPersistFields()
{
   addProtectedField("material", TypeSimObjectPtr, Offset(mOverrideMaterial, ForcedMaterialMeshMgr),
      &_setOverrideMat, &defaultProtectedGetFn, "Material used to draw all meshes in the render bin.");

   Parent::initPersistFields();
}

void ForcedMaterialMeshMgr::render(SceneState * state)
{
   PROFILE_SCOPE(ForcedMaterialMeshMgr_render);

   if(!mOverrideInstance && mOverrideMaterial.isValid())
   {
      mOverrideInstance = mOverrideMaterial->createMatInstance();
      mOverrideInstance->init( MATMGR->getDefaultFeatures(), getGFXVertexFormat<GFXVertexPNTBT>() );
   }

   // Early out if nothing to draw.
   if(!mElementList.size() || !mOverrideInstance)
      return;

   GFXDEBUGEVENT_SCOPE(ForcedMaterialMeshMgr_Render, ColorI::RED);

   // Automagically save & restore our viewport and transforms.
   GFXTransformSaver saver;

   // init loop data
   SceneGraphData sgData;   
   MeshRenderInst *ri = static_cast<MeshRenderInst*>(mElementList[0].inst);
   setupSGData( ri, sgData );

   while (mOverrideInstance->setupPass(state, sgData))
   {   
      for( U32 j=0; j<mElementList.size(); j++)
      {
         MeshRenderInst* passRI = static_cast<MeshRenderInst*>(mElementList[j].inst);
         if(passRI->primBuff->getPointer()->mPrimitiveArray[passRI->primBuffIndex].numVertices < 1)
            continue;

         getParentManager()->getMatrixSet().setWorld(*passRI->objectToWorld);
         getParentManager()->getMatrixSet().setView(*passRI->worldToCamera);
         getParentManager()->getMatrixSet().setProjection(*passRI->projection);
         mOverrideInstance->setTransforms(getParentManager()->getMatrixSet(), state);

         mOverrideInstance->setBuffers(passRI->vertBuff, passRI->primBuff);
         GFX->drawPrimitive( passRI->primBuffIndex );                  
      }
   }
}

bool ForcedMaterialMeshMgr::_setOverrideMat( void *obj, const char *data )
{
   // Clear out the instance and let the assignment take care of the rest.
   ForcedMaterialMeshMgr &mgr = *reinterpret_cast<ForcedMaterialMeshMgr *>(obj);
   mgr.setOverrideMaterial(NULL);
   return true;
}