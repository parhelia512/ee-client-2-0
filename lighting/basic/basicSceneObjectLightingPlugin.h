//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif

class ShadowBase;

class BasicSceneObjectLightingPlugin : public SceneObjectLightingPlugin
{
private:

   ShadowBase* mShadow;
   SceneObject* mParentObject;

   static Vector<BasicSceneObjectLightingPlugin*> smPluginInstances;
   
public:
   BasicSceneObjectLightingPlugin(SceneObject* parent);
   ~BasicSceneObjectLightingPlugin();

   static Vector<BasicSceneObjectLightingPlugin*>* getPluginInstances() { return &smPluginInstances; }

   static void cleanupPluginInstances();
   static void resetAll();

   const F32 getScore() const;

   // Called from BasicLightManager
   virtual void updateShadow( SceneState *state );
   virtual void renderShadow( SceneState *state );

   // Called by statics
   virtual U32  packUpdate(SceneObject* obj, U32 checkMask, NetConnection *conn, U32 mask, BitStream *stream) { return 0; }
   virtual void unpackUpdate(SceneObject* obj, NetConnection *conn, BitStream *stream) { }

   virtual void reset();
};

class BasicSceneObjectPluginFactory
{
protected:

   /// Called from the light manager on activation.
   /// @see LightManager::addActivateCallback
   void _onLMActivate( const char *lm, bool enable );
   
   void _onDecalManagerClear();

   void removeLightPlugin(SceneObject* obj);
   void addLightPlugin(SceneObject* obj);
   void addToExistingObjects();
public:
   BasicSceneObjectPluginFactory();
};
