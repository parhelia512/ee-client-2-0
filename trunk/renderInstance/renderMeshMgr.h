//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _RENDERMESHMGR_H_
#define _RENDERMESHMGR_H_

#ifndef _RENDERBINMANAGER_H_
#include "renderInstance/renderBinManager.h"
#endif

//**************************************************************************
// RenderMeshMgr
//**************************************************************************
class RenderMeshMgr : public RenderBinManager
{
   typedef RenderBinManager Parent;
public:
   RenderMeshMgr();
   RenderMeshMgr(RenderInstType riType, F32 renderOrder, F32 processAddOrder);   

   // RenderBinManager interface
   virtual void init();
   virtual void render(SceneState * state);
   AddInstResult addElement( RenderInst *inst );

   // ConsoleObject interface
   static void initPersistFields();
   DECLARE_CONOBJECT(RenderMeshMgr);
protected:
   GFXStateBlockRef mNormalSB;
   GFXStateBlockRef mReflectSB;

   void construct();
};

#endif // _RENDERMESHMGR_H_
