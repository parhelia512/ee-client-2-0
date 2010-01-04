//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "lighting/basic/basicSceneObjectLightingPlugin.h"

//#include "lighting/basic/basicLightManager.h"
#include "lighting/common/projectedShadow.h"
#include "T3D/shapeBase.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "ts/tsRenderState.h"
#include "gfx/sim/cubemapData.h"
#include "sceneGraph/reflector.h"
#include "T3D/decal/decalManager.h"

static const U32 shadowObjectTypeMask = PlayerObjectType | CorpseObjectType | ItemObjectType | VehicleObjectType;
Vector<BasicSceneObjectLightingPlugin*> BasicSceneObjectLightingPlugin::smPluginInstances;

BasicSceneObjectLightingPlugin::BasicSceneObjectLightingPlugin(SceneObject* parent)
   : mParentObject( parent )
{
   mShadow = NULL;

   // Stick us on the list.
   smPluginInstances.push_back( this );
}

BasicSceneObjectLightingPlugin::~BasicSceneObjectLightingPlugin()
{
   SAFE_DELETE( mShadow );
   
   // Delete us from the list.
   smPluginInstances.remove( this );
}

void BasicSceneObjectLightingPlugin::reset()
{
   SAFE_DELETE( mShadow );
}

void BasicSceneObjectLightingPlugin::cleanupPluginInstances()
{
   for (U32 i = 0; i < smPluginInstances.size(); i++)
   {
      BasicSceneObjectLightingPlugin *plug = smPluginInstances[i];
      smPluginInstances.remove( plug );
      delete plug;
      i--;
   }
   
   smPluginInstances.clear();
}

void BasicSceneObjectLightingPlugin::resetAll()
{
   for( U32 i = 0, num = smPluginInstances.size(); i < num; ++ i )
      smPluginInstances[ i ]->reset();
}

const F32 BasicSceneObjectLightingPlugin::getScore() const
{ 
   return mShadow ? mShadow->getScore() : 0.0f; 
}

void BasicSceneObjectLightingPlugin::updateShadow( SceneState *state )
{
   if ( !mShadow )
      mShadow = new ProjectedShadow( mParentObject );

   mShadow->update( state );
}

void BasicSceneObjectLightingPlugin::renderShadow( SceneState *state )
{
   // hack until new scenegraph in place
   GFXTransformSaver ts;
   
   TSRenderState rstate;
   rstate.setSceneState(state);

   F32 camDist = (state->getCameraPosition() - mParentObject->getRenderPosition()).len();
   
   // Make sure the shadow wants to be rendered
   if( mShadow->shouldRender( state ) )
   {
      // Render! (and note the time)      
      mShadow->render( camDist, rstate );
   }
}

BasicSceneObjectPluginFactory p_BasicSceneObjectPluginFactory;

BasicSceneObjectPluginFactory::BasicSceneObjectPluginFactory()
{
   LightManager::smActivateSignal.notify( this, &BasicSceneObjectPluginFactory::_onLMActivate );
}

void BasicSceneObjectPluginFactory::_onLMActivate( const char *lm, bool enable )
{
   // Skip over signals that are not from
   // the basic light manager.
   if ( dStricmp( lm, "BLM" ) != 0 )
      return;

   if ( enable )
   {
      SceneObject::smSceneObjectAdd.notify(this, &BasicSceneObjectPluginFactory::addLightPlugin);
      SceneObject::smSceneObjectRemove.notify(this, &BasicSceneObjectPluginFactory::removeLightPlugin);
      gDecalManager->getClearDataSignal().notify( this, &BasicSceneObjectPluginFactory::_onDecalManagerClear );
      addToExistingObjects();
   } 
   else 
   {
      SceneObject::smSceneObjectAdd.remove(this, &BasicSceneObjectPluginFactory::addLightPlugin);
      SceneObject::smSceneObjectRemove.remove(this, &BasicSceneObjectPluginFactory::removeLightPlugin);
      gDecalManager->getClearDataSignal().remove( this, &BasicSceneObjectPluginFactory::_onDecalManagerClear );
      BasicSceneObjectLightingPlugin::cleanupPluginInstances();
   }
}

void BasicSceneObjectPluginFactory::_onDecalManagerClear()
{
   BasicSceneObjectLightingPlugin::resetAll();
}

void BasicSceneObjectPluginFactory::removeLightPlugin( SceneObject *obj )
{
   // Grab the plugin instance.
   SceneObjectLightingPlugin *lightPlugin = obj->getLightingPlugin();

   // Delete it, which will also remove it
   // from the static list of plugin instances.
   if ( lightPlugin )
   {
      delete lightPlugin;
      obj->setLightingPlugin( NULL );
   }
}

void BasicSceneObjectPluginFactory::addLightPlugin(SceneObject* obj)
{
   bool serverObj = obj->isServerObject();
   bool isShadowType = (obj->getType() & shadowObjectTypeMask);

   if ( !isShadowType || serverObj )
      return;

   obj->setLightingPlugin(new BasicSceneObjectLightingPlugin(obj));
}

// Some objects may not get cleaned up during mission load/free, so add our
// plugin to existing scene objects
void BasicSceneObjectPluginFactory::addToExistingObjects()
{
   SimpleQueryList sql;  
   gClientContainer.findObjects( shadowObjectTypeMask, SimpleQueryList::insertionCallback, &sql);
   for (SceneObject** i = sql.mList.begin(); i != sql.mList.end(); i++)
      addLightPlugin(*i);
}
                                                                                                         
