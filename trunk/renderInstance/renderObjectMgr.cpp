//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "renderObjectMgr.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneObject.h"

IMPLEMENT_CONOBJECT(RenderObjectMgr);

RenderObjectMgr::RenderObjectMgr()
: RenderBinManager(RenderPassManager::RIT_Object, 1.0f, 1.0f)
{
   mOverrideMat = NULL;
}

RenderObjectMgr::RenderObjectMgr(RenderInstType riType, F32 renderOrder, F32 processAddOrder)
 : RenderBinManager(riType, renderOrder, processAddOrder)
{  
   mOverrideMat = NULL;
}

void RenderObjectMgr::initPersistFields()
{
   Parent::initPersistFields();
}

void RenderObjectMgr::setOverrideMaterial(BaseMatInstance* overrideMat)
{ 
   mOverrideMat = overrideMat; 
}

//-----------------------------------------------------------------------------
// render objects
//-----------------------------------------------------------------------------
void RenderObjectMgr::render( SceneState *state )
{
   PROFILE_SCOPE(RenderObjectMgr_render);

   // Early out if nothing to draw.
   if(!mElementList.size())
      return;

   for( U32 i=0; i<mElementList.size(); i++ )
   {
      ObjectRenderInst *ri = static_cast<ObjectRenderInst*>(mElementList[i].inst);
      if ( ri->renderDelegate )
         ri->renderDelegate( ri, state, mOverrideMat );      
   }
}