//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _FEATUREMGR_H_
#define _FEATUREMGR_H_

#ifndef _TSINGLETON_H_
#include "core/util/tSingleton.h"
#endif 
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif 

class FeatureType;
class ShaderFeature;

/// Used by the feature manager.
struct FeatureInfo
{
   const FeatureType *type;
   ShaderFeature *feature;
};


///
class FeatureMgr
{
protected:

   bool mNeedsSort;

   typedef Vector<FeatureInfo> FeatureInfoVector;

   FeatureInfoVector mFeatures;

   static S32 QSORT_CALLBACK _featureInfoCompare( const FeatureInfo *a, const FeatureInfo *b );

public:
   
   FeatureMgr();
   ~FeatureMgr();

   /// Returns the count of registered features.
   U32 getFeatureCount() const { return mFeatures.size(); }

   /// Returns the feature info at the index.
   const FeatureInfo& getAt( U32 index );

   /// 
   ShaderFeature* getByType( const FeatureType &type );

   // Allows other systems to add features.  index is 
   // the enum in GFXMaterialFeatureData.
   void registerFeature(   const FeatureType &type, 
                           ShaderFeature *feature );

   // Unregister a feature.
   void unregisterFeature( const FeatureType &type );


   /// Removes all features.
   void unregisterAll();
};

// Helper for accessing the feature manager singleton.
#define FEATUREMGR Singleton<FeatureMgr>::instance()

#endif // FEATUREMGR