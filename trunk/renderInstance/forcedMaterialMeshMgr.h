//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _RENDERFORCEDMATMESHMGR_H_
#define _RENDERFORCEDMATMESHMGR_H_

#ifndef _RENDERMESHMGR_H_
#include "renderInstance/renderMeshMgr.h"
#endif

class Material;

/// Basically the same as RenderMeshMgr, but will override the material of the instance.
class ForcedMaterialMeshMgr : public RenderMeshMgr
{
   typedef RenderMeshMgr Parent;
public:
   ForcedMaterialMeshMgr();
   ForcedMaterialMeshMgr(RenderInstType riType, F32 renderOrder, F32 processAddOrder, BaseMatInstance* overrideMaterial);
   virtual ~ForcedMaterialMeshMgr();

   void setOverrideMaterial(BaseMatInstance* overrideMaterial);

   // RenderBinManager interface
   virtual void render(SceneState * state);

   DECLARE_CONOBJECT(ForcedMaterialMeshMgr);
   static void initPersistFields();
private:
   BaseMatInstance* mOverrideInstance;
   SimObjectPtr<Material> mOverrideMaterial;
   static bool _setOverrideMat( void *obj, const char *data );
};

#endif
