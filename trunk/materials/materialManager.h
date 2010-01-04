//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _MATERIAL_MGR_H_
#define _MATERIAL_MGR_H_

#ifndef _MATERIALDEFINITION_H_
#include "materials/materialDefinition.h"
#endif
#ifndef _GFXDEVICE_H_
#include "gfx/gfxDevice.h"
#endif
#ifndef _TSINGLETON_H_
#include "core/util/tSingleton.h"
#endif

class SimSet;
class MatInstance;

class MaterialManager : public ManagedSingleton<MaterialManager>
{
public:
   MaterialManager();
   ~MaterialManager();

   // ManagedSingleton
   static const char* getSingletonName() { return "MaterialManager"; }

   Material * allocateAndRegister(const String &objectName, const String &mapToName = String());
   Material * getMaterialDefinitionByName(const String &matName);
   SimSet * getMaterialSet();   

   // map textures to materials
   void mapMaterial(const String & textureName, const String & materialName);
   String getMapEntry(const String & textureName) const;

   // Return instance of named material caller is responsible for memory
   BaseMatInstance * createMatInstance( const String &matName );

   // Create a BaseMatInstance with the default feature flags. 
   BaseMatInstance * createMatInstance( const String &matName, const GFXVertexFormat *vertexFormat );
   BaseMatInstance * createMatInstance( const String &matName, const FeatureSet &features, const GFXVertexFormat *vertexFormat );

   /// The default feature set for materials.
   const FeatureSet& getDefaultFeatures() const { return mDefaultFeatures; }

   /// The feature exclusion list for disabling features.
   const FeatureSet& getExclusionFeatures() const { return mExclusionFeatures; }

   void recalcFeaturesFromPrefs();

   /// Allocate and return an instance of special materials.  Caller is responsible for the memory.
   BaseMatInstance * createWarningMatInstance();

   /// Gets the global warning material instance, callers should not free this copy
   BaseMatInstance * getWarningMatInstance();

   /// Set the prepass enabled state.
   void setPrePassEnabled( bool enabled ) { mUsingPrePass = enabled; }

   /// Get the prepass enabled state.
   bool getPrePassEnabled() const { return mUsingPrePass; }

#ifndef TORQUE_SHIPPING

   // Allocate and return an instance of mesh debugging materials.  Caller is responsible for the memory.
   BaseMatInstance * createMeshDebugMatInstance(const ColorF &meshColor);

   // Gets the global material instance for a given color, callers should not free this copy
   BaseMatInstance * getMeshDebugMatInstance(const ColorF &meshColor);

#endif

   void dumpMaterialInstances( BaseMaterialDefinition *target = NULL ) const;

   void updateTime();
   F32 getTotalTime() const { return mAccumTime; }
   F32 getDeltaTime() const { return mDt; }
   U32 getLastUpdateTime() const { return mLastTime; }

   /// Flushes all the procedural shaders and re-initializes all
   /// the active materials instances.
   void flushAndReInitInstances();

   // Flush the instance
   void flushInstance( BaseMaterialDefinition *target );
   /// Re-initializes the material instances for a specific target material.   
   void reInitInstance( BaseMaterialDefinition *target );

protected:

   // MatInstance tracks it's instances here
   friend class MatInstance;
   void _track(MatInstance*);
   void _untrack(MatInstance*);

   /// @see LightManager::smActivateSignal
   void _onLMActivate( const char *lm, bool activate );

   bool _handleGFXEvent(GFXDevice::GFXDeviceEventType event);

   SimSet* mMaterialSet;
   Vector<BaseMatInstance*> mMatInstanceList;

   /// The default material features.
   FeatureSet mDefaultFeatures;

   /// The feature exclusion set.
   FeatureSet mExclusionFeatures;

   // material map
   typedef Map<String, String> MaterialMap;
   MaterialMap mMaterialMap;

   bool mUsingPrePass;

   // time tracking
   F32 mDt;
   F32 mAccumTime;
   U32 mLastTime;

   BaseMatInstance* mWarningInst;

#ifndef TORQUE_SHIPPING
   typedef Map<U32, BaseMatInstance *>  DebugMaterialMap;
   DebugMaterialMap  mMeshDebugMaterialInsts;
#endif

};

/// Helper for accessing MaterialManager singleton.
#define MATMGR MaterialManager::instance()

#endif // _MATERIAL_MGR_H_
