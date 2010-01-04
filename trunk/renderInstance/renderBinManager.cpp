//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "renderInstance/renderBinManager.h"
#include "console/consoleTypes.h"
#include "materials/matInstance.h"
#include "sceneGraph/sceneGraph.h"
//#include "scene/sceneReflectPass.h"

IMPLEMENT_CONOBJECT(RenderBinManager);

//-----------------------------------------------------------------------------
// RenderBinManager
//-----------------------------------------------------------------------------
RenderBinManager::RenderBinManager()
{
   VECTOR_SET_ASSOCIATION( mElementList );
   mElementList.reserve( 2048 );
   mRenderInstType = RenderPassManager::RIT_Custom;
   mRenderOrder = 1.0f;
   mProcessAddOrder = 1.0f;
   mParentManager = NULL;
}

RenderBinManager::RenderBinManager(const RenderInstType& ritype, F32 renderOrder, F32 processAddOrder)
{
   VECTOR_SET_ASSOCIATION( mElementList );
   mElementList.reserve( 2048 );
   mRenderInstType = ritype;
   mRenderOrder = renderOrder;
   mProcessAddOrder = processAddOrder;
   mParentManager = NULL;
}

void RenderBinManager::initPersistFields()
{
   addField("binType", TypeRealString, Offset(mRenderInstType.mName, RenderBinManager));
   addField("renderOrder", TypeF32, Offset(mRenderOrder, RenderBinManager));
   addField("processAddOrder", TypeF32, Offset(mProcessAddOrder, RenderBinManager));

   Parent::initPersistFields();
}

void RenderBinManager::onRemove()
{
   // Tell our parent to remove us when 
   // we're being unregistered.
   if ( mParentManager )
      mParentManager->removeManager( this );

   Parent::onRemove();
}

//-----------------------------------------------------------------------------
// addElement
//-----------------------------------------------------------------------------
RenderBinManager::AddInstResult RenderBinManager::addElement( RenderInst *inst )
{   
   if (inst->type != mRenderInstType)
      return arSkipped;

   internalAddElement(inst);

   return arAdded;
}

void RenderBinManager::internalAddElement(RenderInst* inst)
{
   mElementList.increment();
   MainSortElem &elem = mElementList.last();
   elem.inst = inst;
   elem.key = elem.key2 = 0;

   elem.key = inst->defaultKey;
   elem.key2 = inst->defaultKey2;
}

//-----------------------------------------------------------------------------
// clear
//-----------------------------------------------------------------------------
void RenderBinManager::clear()
{
   mElementList.clear();
}

//-----------------------------------------------------------------------------
// sort
//-----------------------------------------------------------------------------
void RenderBinManager::sort()
{
   dQsort( mElementList.address(), mElementList.size(), sizeof(MainSortElem), cmpKeyFunc);
}

//-----------------------------------------------------------------------------
// QSort callback function
//-----------------------------------------------------------------------------
S32 FN_CDECL RenderBinManager::cmpKeyFunc(const void* p1, const void* p2)
{
   const MainSortElem* mse1 = (const MainSortElem*) p1;
   const MainSortElem* mse2 = (const MainSortElem*) p2;

   S32 test1 = S32(mse2->key) - S32(mse1->key);

   return ( test1 == 0 ) ? S32(mse1->key2) - S32(mse2->key2) : test1;
}

void RenderBinManager::setupSGData( MeshRenderInst *ri, SceneGraphData &data )
{
   data.reset();
   data.setFogParams(mParentManager->getSceneManager()->getFogData());

   dMemcpy( data.lights, ri->lights, sizeof( data.lights ) );
   
   data.objTrans = *ri->objectToWorld;
   data.backBuffTex = ri->backBuffTex;
   data.cubemap = ri->cubemap;
   data.miscTex = ri->miscTex;
   data.reflectTex = ri->reflectTex;
   
   data.wireframe = GFXDevice::getWireframe();

   data.lightmap     = ri->lightmap;
   data.visibility = ri->visibility;

   data.materialHint = ri->materialHint;
}

ConsoleMethod(RenderBinManager, getBinType, const char*, 2, 2, "Returns the type of manager.")
{
   TORQUE_UNUSED(argc);
   TORQUE_UNUSED(argv);
   
   return object->getRenderInstType().getName();
}
