//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _LIGHTMANAGER_H_
#define _LIGHTMANAGER_H_

#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/str.h"
#endif
#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif
#ifndef _LIGHTINFO_H_
#include "lighting/lightInfo.h"
#endif

class SimObject;
class LightManager;
class Material;
class ProcessedMaterial;
class SceneGraph;
struct SceneGraphData;
class Point3F;
class SphereF;
class AvailableSLInterfaces;
struct LightReceiver;
class GFXShaderConstBuffer;
class GFXShaderConstHandle;
class ShaderConstHandles;
class SceneState;

///
typedef Map<String,LightManager*> LightManagerMap;


class LightManager
{
public:

   enum SpecialLightTypesEnum
   {
      slSunLightType,
      slSpecialLightTypesCount
   };

   LightManager( const char *name, const char *id );

   virtual ~LightManager();

   ///
   static void initLightFields();

   /// 
   static LightInfo* createLightInfo();

   ///
   static LightManager* findByName( const char *name );

   /// Returns a tab seperated list of available light managers.
   static void getLightManagerNames( String *outString );

   /// The light manager activation signal.
   static Signal<void(const char*,bool)> smActivateSignal;

   /// Return an id string used to load different versions of light manager
   /// specific assets.  It shoud be short, contain no spaces, and be safe 
   /// for filename use.
   const char* getName() const { return mName.c_str(); }

   /// Return an id string used to load different versions of light manager
   /// specific assets.  It shoud be short, contain no spaces, and be safe 
   /// for filename use.
   const char* getId() const { return mId.c_str(); }

   // Returns the scene manager passed at activation.
   SceneGraph* getSceneManager() { return mSceneManager; }

   // Should return true if this light manager is compatible
   // on the current platform and GFX device.
   virtual bool isCompatible() const = 0;

   // Called when the lighting manager should become active
   virtual void activate( SceneGraph *sceneManager );

   // Called when we don't want the light manager active (should clean up)
   virtual void deactivate();

   // Returns the active scene lighting interface for this light manager.  
   virtual AvailableSLInterfaces* getSceneLightingInterface();

   // Returns a "default" light info that callers should not free.  Used for instances where we don't actually care about 
   // the light (for example, setting default data for SceneGraphData)
   virtual LightInfo* getDefaultLight();

   /// Returns the special light or the default light if useDefault is true.
   /// @see getDefaultLight
   virtual LightInfo* getSpecialLight( SpecialLightTypesEnum type, 
                                       bool useDefault = true );

   /// Set a special light type.
   virtual void setSpecialLight( SpecialLightTypesEnum type, LightInfo *light );

   // registered before scene traversal...
   virtual void registerGlobalLight( LightInfo *light, SimObject *obj );
   virtual void unregisterGlobalLight( LightInfo *light );

   // registered per object...
   virtual void registerLocalLight( LightInfo *light );
   virtual void unregisterLocalLight( LightInfo *light );

   virtual void registerGlobalLights( const Frustum *frustum, bool staticlighting );
   virtual void unregisterAllLights();

   /// Returns all unsorted and un-scored lights (both global and local).
   virtual void getAllUnsortedLights( LightInfoList &list );

   /// For the terrain.  Finds the best
   /// lights in the viewing area based on distance to camera.
   virtual void setupLights(  LightReceiver *obj, 
                              const Point3F &camerapos,
                              const Point3F &cameradir, 
                              F32 viewdist,
                              U32 maxLights = 4 );

   /// Finds the best lights that overlap with the bounds.
   /// @see getBestLights
   virtual void setupLights(  LightReceiver *obj, 
                              const SphereF &bounds,
                              U32 maxLights = 4 );

   /// This returns the best lights as gathered by the
   /// previous setupLights() calls.
   /// @see setupLights
   virtual void getBestLights( LightInfo** outLights, U32 maxLights );   

   /// Clears the best lights list and all associated data.
   virtual void resetLights();

   /// Sets shader constants / textures for light infos
   virtual void setLightInfo( ProcessedMaterial *pmat, 
                              const Material *mat, 
                              const SceneGraphData &sgData, 
                              const SceneState *state,
                              U32 pass, 
                              GFXShaderConstBuffer *shaderConsts ) = 0;

   /// Allows us to set textures during the Material::setTextureStage call, return true if we've done work.
   virtual bool setTextureStage( const SceneGraphData &sgData, 
                                 const U32 currTexFlag, 
                                 const U32 textureSlot, 
                                 GFXShaderConstBuffer *shaderConsts, 
                                 ShaderConstHandles *handles ) = 0;

   /// Called when the static scene lighting (aka lightmaps) should be computed.
   virtual bool lightScene( const char* callback, const char* param );

   /// Returns true if this light manager is active
   virtual bool isActive() const { return mIsActive; }

protected:

   /// This helper function sets the shader constansts
   /// for the stock 4 light forward lighting code.
   static void _update4LightConsts( const SceneGraphData &sgData,
                                    GFXShaderConstHandle *lightPositionSC,
                                    GFXShaderConstHandle *lightDiffuseSC,
                                    GFXShaderConstHandle *lightAmbientSC,
                                    GFXShaderConstHandle *lightInvRadiusSqSC,
                                    GFXShaderConstHandle *lightSpotDirSC,
                                    GFXShaderConstHandle *lightSpotAngleSC,
                                    GFXShaderConstBuffer *shaderConsts );

   /// A dummy default light used when no lights
   /// happen to be registered with the manager.
   LightInfo *mDefaultLight;
  
   /// The list of global registered lights which is
   /// initialized before the scene is rendered.
   LightInfoList mRegisteredLights;

   /// The registered special light list.
   LightInfo *mSpecialLights[slSpecialLightTypesCount];

   /// The sorted list of the best lights.
   /// @see setupLights, getBestLights
	LightInfoList mBestLights;

   /// The root culling position used for
   /// special sun light placement.
   /// @see setSpecialLight
   Point3F mCullPos;

   /// The scene lighting interfaces for 
   /// lightmap generation.
   AvailableSLInterfaces *mAvailableSLInterfaces;

   ///
   void _scoreLights( const SphereF &bounds );

   ///
   static S32 _lightScoreCmp( LightInfo* const *a, LightInfo* const *b );

   /// Attaches any LightInfoEx data for this manager 
   /// to the light info object.
   virtual void _addLightInfoEx( LightInfo *lightInfo ) = 0;

   ///
   virtual void _initLightFields() = 0;

   /// Returns the static light manager map.
   static LightManagerMap& _getLightManagers();
  
   /// The constant light manager name initialized
   /// in the constructor.
   const String mName;

   /// The constant light manager identifier initialized
   /// in the constructor.
   const String mId;

   /// Is true if this light manager has been activated.
   bool mIsActive;

   /// The scene graph the light manager is associated with.
   SceneGraph *mSceneManager;
};

#endif // _LIGHTMANAGER_H_
