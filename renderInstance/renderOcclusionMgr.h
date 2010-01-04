//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _RENDEROCCLUSIONMGR_H_
#define _RENDEROCCLUSIONMGR_H_

#ifndef _RENDERBINMANAGER_H_
#include "renderInstance/renderBinManager.h"
#endif

//**************************************************************************
// RenderOcclusionMgr
//**************************************************************************
class RenderOcclusionMgr : public RenderBinManager
{
   typedef RenderBinManager Parent;
public:
   RenderOcclusionMgr();
   RenderOcclusionMgr(RenderInstType riType, F32 renderOrder, F32 processAddOrder);

   // RenderOcclusionMgr
   virtual void init();
   virtual void render(SceneState * state);

   // ConsoleObject
   static void consoleInit();
   static void initPersistFields();
   DECLARE_CONOBJECT(RenderOcclusionMgr);

protected:
   BaseMatInstance* mOverrideMat;
   GFXStateBlockRef mNormalSB;
   GFXStateBlockRef mTestSB;
   
   GFXStateBlockRef mDebugSB;
   static bool smDebugRender;

   GFXVertexBufferHandle<GFXVertexPC> mBoxBuff;
   GFXVertexBufferHandle<GFXVertexPC> mSphereBuff;
   U32 mSpherePrimCount;
};

#endif // _RENDEROCCLUSIONMGR_H_