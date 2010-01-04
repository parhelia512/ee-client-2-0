//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SCENEGRAPH_H_
#define _SCENEGRAPH_H_

//Includes
#ifndef _MRECT_H_
#include "math/mRect.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _FOGSTRUCTS_H_
#include "sceneGraph/fogStructs.h"
#endif

class LightManager;
class SceneGraph;
class SceneState;
class NetConnection;
class RenderPassManager;
class TerrainBlock;


/// A signal used to notify of render passes.
/// @see SceneGraph
typedef Signal<void(SceneGraph*, const SceneState*)> SceneGraphRenderSignal;


/// The type of scene pass.
/// @see SceneGraph
/// @see SceneState
enum ScenePassType
{
   /// The regular diffuse scene pass.
   SPT_Diffuse,

   /// The scene pass made for reflection rendering.
   SPT_Reflect,

   /// The scene pass made for shadow map rendering.
   SPT_Shadow,

   /// A scene pass that isn't one of the other 
   /// predefined scene pass types.
   SPT_Other,
};


///
class SceneGraph
{
  public:
   SceneGraph(bool isClient);
   ~SceneGraph();

   /// @name SceneObject Management
   /// @{

   ///
   bool addObjectToScene(SceneObject*);
   void removeObjectFromScene(SceneObject*);
   void zoneInsert(SceneObject*);
   void zoneRemove(SceneObject*);

   Container* getContainer() const { return mIsClient ? &gClientContainer : &gServerContainer; }
   /// @}

 
   /// @name Zone management
   /// @{

   ///
   void registerZones(SceneObject*, U32 numZones);
   void unregisterZones(SceneObject*);
   SceneObject* getZoneOwner(const U32 zone);
   /// @}

   /// @name Rendering and Scope Management
   /// @{

   ///
   
   SceneState* createBaseState( ScenePassType passType, bool inverted = false );
   
   void renderScene( ScenePassType passType, U32 objectMask = -1 );

   void renderScene( SceneState *state, U32 objectMask = -1 );

   void scopeScene(const Point3F& scopePosition,
                   const F32      scopeDistance,
                   NetConnection* netConnection);

   /// Returns the currently active scene state.
   SceneState* getSceneState() const { return mSceneState; }
                        
   /// @}

   /// @name Fog/Visibility Management
   /// @{

   void setPostEffectFog( bool enable ) { mUsePostEffectFog = enable; }   
   bool usePostEffectFog() const { return mUsePostEffectFog; }

   /// Accessor for the FogData structure.
   const FogData& getFogData() const { return mFogData; }

   /// Sets the FogData structure.
   void setFogData( const FogData &data ) { mFogData = data; }

   /// Accessor for the WaterFogData structure.
   const WaterFogData& getWaterFogData() const { return mWaterFogData; }

   /// Sets the WaterFogData structure.
   void setWaterFogData( const WaterFogData &data ) { mWaterFogData = data; }

   /// Used by LevelInfo to set the default visible distance for
   /// rendering the scene.
   ///
   /// Note this should not be used to alter culling which is
   /// controlled by the active frustum when a SceneState is created.
   ///
   /// @see SceneState
   /// @see GameProcessCameraQuery
   /// @see LevelInfo
   void setVisibleDistance( F32 dist ) { mVisibleDistance = dist; }

   /// Returns the default visible distance for the scene.
   F32 getVisibleDistance() const { return mVisibleDistance; }

   /// Used by LevelInfo to set the default near clip plane 
   /// for rendering the scene.
   ///
   /// @see GameProcessCameraQuery
   /// @see LevelInfo
   void setNearClip( F32 nearClip ) { mNearClip = nearClip; }

   /// Returns the default near clip distance for the scene.
   F32 getNearClip() const { return mNearClip; }

   /// @}

   U32 getStateKey() const { return smStateKey; }

   U32 incStateKey() { return ++smStateKey; }

   //-------------------------------------- Private interface and data
  protected:

   static const U32 csmMaxTraversalDepth;
   static U32 smStateKey;

   /// The currently active scene state or NULL if we're
   /// not in the process of rendering.
   SceneState *mSceneState;

  public:

   /// This var is for cases where you need the "normal" camera position if you are
   /// in a reflection pass.  Used for correct fog calculations in reflections.
   Point3F mNormCamPos;

  protected:

   bool mIsClient;

   // NonClipProjection is the projection matrix without oblique frustum clipping
   // applied to it (in reflections)
   MatrixF mNonClipProj;

   F32 mInvVisibleDistance;

   U32  mCurrZoneEnd;
   U32  mNumActiveZones;
   
   FogData mFogData;

   WaterFogData mWaterFogData;   

   bool mUsePostEffectFog;

   F32 mVisibleDistance;

   F32 mNearClip;

   Vector<RenderPassManager*> mRenderPassStack;

   LightManager* mLightManager;

   TerrainBlock* mCurrTerrain;

   void            addRefPoolBlock();
   SceneObjectRef* allocateObjectRef();
   void            freeObjectRef(SceneObjectRef*);
   SceneObjectRef*         mFreeRefPool;
   Vector<SceneObjectRef*> mRefPoolBlocks;
   static const U32        csmRefPoolBlockSize;

   /// @see setDisplayTargetResolution
   Point2I mDisplayTargetResolution;

   ///
   void _setDiffuseRenderPass();

  public:

   /// @name dtr Display Target Resolution
   ///
   /// Some rendering must be targeted at a specific display resolution.
   /// This display resolution is distinct from the current RT's size
   /// (such as when rendering a reflection to a texture, for instance)
   /// so we store the size at which we're going to display the results of
   /// the current render.
   ///
   /// @{

   ///
   void setDisplayTargetResolution(const Point2I &size);
   const Point2I &getDisplayTargetResolution() const;

   /// @}

   // Returns the current active light manager.
   LightManager* getLightManager();

   /// Finds the light manager by name and activates it.
   bool setLightManager( const char *lmName );

   /// Returns the current render pass manager on the stack.
   RenderPassManager* getRenderPass() const 
   {
      AssertFatal( !mRenderPassStack.empty(), "SceneGraph::getRenderManager() - The stack is empty!" );        
      return mRenderPassStack.last(); 
   }

   void pushRenderPass( RenderPassManager *rpm );

   void popRenderPass();

   TerrainBlock* getCurrentTerrain()      { return mCurrTerrain; }

   // NonClipProjection is the projection matrix without oblique frustum clipping
   // applied to it (in reflections)
   void setNonClipProjection( const MatrixF &proj ) { mNonClipProj = proj; }
   const MatrixF& getNonClipProjection() const { return mNonClipProj; }

   static SceneGraphRenderSignal& getPreRenderSignal() 
   { 
      static SceneGraphRenderSignal theSignal;
      return theSignal;
   }

   static SceneGraphRenderSignal& getPostRenderSignal() 
   { 
      static SceneGraphRenderSignal theSignal;
      return theSignal;
   }

   // Object database for zone managers
  protected:
   struct ZoneManager {
      SceneObject* obj;
      U32          zoneRangeStart;
      U32          numZones;
   };
   Vector<ZoneManager>     mZoneManagers;

   /// Zone Lists
   ///
   /// @note The object refs in this are somewhat singular in that the object pointer does not
   ///  point to a referenced object, but the owner of that zone...
   Vector<SceneObjectRef*> mZoneLists;

protected:

   /// Deactivates the previous light manager and activates the new one.
   bool _setLightManager( LightManager *lm );

   void _traverseSceneTree( SceneState *state );

   void _buildSceneTree(   SceneState *state,
                           U32 objectMask = (U32)-1,
                           SceneObject *baseObject = NULL,
                           U32 baseZone = 0,
                           U32 currDepth = 0 );

   void treeTraverseVisit(SceneObject*, SceneState*, const U32);

   void compactZonesCheck();
   bool alreadyManagingZones(SceneObject*) const;

public:
   void findZone(const Point3F&, SceneObject*&, U32&);
protected:

   void rezoneObject(SceneObject*);
};

extern SceneGraph* gClientSceneGraph;
extern SceneGraph* gServerSceneGraph;


inline SceneObjectRef* SceneGraph::allocateObjectRef()
{
   if (mFreeRefPool == NULL) {
      addRefPoolBlock();
   }
   AssertFatal(mFreeRefPool!=NULL, "Error, should always have a free reference here!");

   SceneObjectRef* ret = mFreeRefPool;
   mFreeRefPool = mFreeRefPool->nextInObj;

   ret->nextInObj = NULL;
   return ret;
}

inline void SceneGraph::freeObjectRef(SceneObjectRef* trash)
{
   trash->nextInBin = NULL;
   trash->prevInBin = NULL;
   trash->nextInObj = mFreeRefPool;
   mFreeRefPool     = trash;
}

inline SceneObject* SceneGraph::getZoneOwner(const U32 zone)
{
   AssertFatal(zone < mCurrZoneEnd, "Error, out of bounds zone selected!");

   return mZoneLists[zone]->object;
}


#endif //_SCENEGRAPH_H_


