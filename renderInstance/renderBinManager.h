//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _RENDERBINMANAGER_H_
#define _RENDERBINMANAGER_H_

#ifndef _CONSOLEOBJECT_H_
#include "console/consoleObject.h"
#endif
#ifndef _RENDERPASSMANAGER_H_
#include "renderInstance/renderPassManager.h"
#endif
#ifndef _BASEMATINSTANCE_H_
#include "materials/baseMatInstance.h"
#endif
#ifndef _UTIL_DELEGATE_H_
#include "core/util/delegate.h"
#endif

class SceneState;


/// This delegate is used in derived RenderBinManager classes
/// to allow material instances to be overriden.
typedef Delegate<BaseMatInstance*(BaseMatInstance*)> MaterialOverrideDelegate;


/// The RenderBinManager manages and renders lists of MainSortElem, which
/// is a light wrapper around RenderInst.
class RenderBinManager : public SimObject
{
   typedef SimObject Parent;
public:
   struct MainSortElem
   {
      RenderInst *inst;
      U32 key;
      U32 key2;
   };

   // Returned by AddInst below
   enum AddInstResult
   {
      arAdded,       // We added this instance
      arSkipped,     // We didn't add this instance
      arStop         // Stop processing this instance
   };
public:
   RenderBinManager();
   RenderBinManager(const RenderInstType& ritype, F32 renderOrder, F32 processAddOrder);
   virtual ~RenderBinManager() {}
   
   // SimObject
   void onRemove();

   virtual AddInstResult addElement( RenderInst *inst );
   virtual void sort();
   virtual void render( SceneState *state ) {}
   virtual void clear();

   // Manager info
   F32 getProcessAddOrder() const { return mProcessAddOrder; }
   void setProcessAddOrder(F32 processAddOrder) { mProcessAddOrder = processAddOrder; }
   F32 getRenderOrder() const { return mRenderOrder; } 
   void setRenderOrder(F32 renderOrder) { mRenderOrder = renderOrder; }
   const RenderInstType& getRenderInstType() { return mRenderInstType; }
   RenderPassManager* getParentManager() const { return mParentManager; }
   void setParentManager(RenderPassManager* parentManager) { mParentManager = parentManager; }

   /// QSort callback function
   static S32 FN_CDECL cmpKeyFunc(const void* p1, const void* p2);

   DECLARE_CONOBJECT(RenderBinManager);
   static void initPersistFields();

   MaterialOverrideDelegate& getMatOverrideDelegate() { return mMatOverrideDelegate; }

protected:
   Vector< MainSortElem > mElementList; // List of our instances
   RenderInstType mRenderInstType; // What kind of render bin are we
   F32 mProcessAddOrder;   // Where in the list do we process RenderInstance additions?
   F32 mRenderOrder;       // Where in the list do we render?
   RenderPassManager* mParentManager; // What render pass manager is our parent?

   MaterialOverrideDelegate mMatOverrideDelegate;

   virtual void setupSGData(MeshRenderInst *ri, SceneGraphData &data );
   virtual bool newPassNeeded(BaseMatInstance* currMatInst, MeshRenderInst* ri);
   BaseMatInstance* getMaterial(RenderInst* inst) const;
   virtual void internalAddElement(RenderInst* inst);
};

// The bin is sorted by (see RenderBinManager::cmpKeyFunc)
//    1.  Material
//    2.  Manager specific key (vertex buffer address by default)
// This function is called on each item of the bin and basically detects any changes in conditions 1 or 2
inline bool RenderBinManager::newPassNeeded(BaseMatInstance* currMatInst, MeshRenderInst* ri)
{
   BaseMatInstance *matInst = ri->matInst;

   // If we have a delegate then we must let it
   // update the mat instance else the comparision
   // will always fail.
   if ( mMatOverrideDelegate )
      matInst = mMatOverrideDelegate( matInst );

   // We need a new pass if:
   //  1.  There's no Material Instance (old ff object?)
   //  2.  If the material differ 
   //  3.  If the vertex formats differ (materials with different vert formats can have different shaders).
   return   matInst == NULL || 
            matInst->getMaterial() != currMatInst->getMaterial() ||
            matInst->getVertexFormat() != currMatInst->getVertexFormat();
}

/// Utility function, gets the material from the RenderInst if available, otherwise, return NULL
inline BaseMatInstance* RenderBinManager::getMaterial(RenderInst* inst) const
{
   if (  inst->type == RenderPassManager::RIT_Mesh || 
         inst->type == RenderPassManager::RIT_Interior ||
         inst->type == RenderPassManager::RIT_Decal ||
         inst->type == RenderPassManager::RIT_Translucent )
      return static_cast<MeshRenderInst*>(inst)->matInst;         

   return NULL;      
}

#endif // _RENDERBINMANAGER_H_
