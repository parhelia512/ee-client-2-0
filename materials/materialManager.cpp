//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/materialManager.h"

#include "materials/matInstance.h"
#include "materials/materialFeatureTypes.h"
#include "lighting/lightManager.h"
#include "core/util/safeDelete.h"
#include "shaderGen/shaderGen.h"


MaterialManager::MaterialManager()
{
   VECTOR_SET_ASSOCIATION( mMatInstanceList );

   mDt = 0.0f; 
   mAccumTime = 0.0f; 
   mLastTime = 0; 
   mWarningInst = NULL;

   GFXDevice::getDeviceEventSignal().notify( this, &MaterialManager::_handleGFXEvent );

   // Make sure we get activation signals
   // and that we're the last to get them.
   LightManager::smActivateSignal.notify( this, &MaterialManager::_onLMActivate, 9999 );

   mMaterialSet = NULL;

   mUsingPrePass = false;
}

MaterialManager::~MaterialManager()
{
   GFXDevice::getDeviceEventSignal().remove( this, &MaterialManager::_handleGFXEvent );  
   LightManager::smActivateSignal.remove( this, &MaterialManager::_onLMActivate );

   SAFE_DELETE( mWarningInst );

#ifndef TORQUE_SHIPPING
   DebugMaterialMap::Iterator itr = mMeshDebugMaterialInsts.begin();

   for ( ; itr != mMeshDebugMaterialInsts.end(); itr++ )
      delete itr->value;
#endif
}

void MaterialManager::_onLMActivate( const char *lm, bool activate )
{
   if ( !activate )
      return;

   // Since the light manager usually swaps shadergen features
   // and changes system wide shader defines we need to completely
   // flush and rebuild all the material instances.

   flushAndReInitInstances();
}

Material * MaterialManager::allocateAndRegister(const String &objectName, const String &mapToName)
{
   Material *newMat = new Material();

   if ( mapToName.isNotEmpty() )
      newMat->mMapTo = mapToName;

   bool registered = newMat->registerObject(objectName );
   AssertFatal( registered, "Unable to register material" );

   if (registered)
      Sim::getRootGroup()->addObject( newMat );
   else
   {
      delete newMat;
      newMat = NULL;
   }

   return newMat;
}

Material * MaterialManager::getMaterialDefinitionByName(const String &matName)
{
   // Get the material
   Material * foundMat;

   if(!Sim::findObject(matName, foundMat))
   {
      Con::errorf("MaterialManager: Unable to find material '%s'", matName.c_str());
      return NULL;
   }

   return foundMat;
}

BaseMatInstance* MaterialManager::createMatInstance(const String &matName)
{
   BaseMaterialDefinition* mat = NULL;
   if (Sim::findObject(matName, mat))
      return mat->createMatInstance();

   return NULL;
}

BaseMatInstance* MaterialManager::createMatInstance(  const String &matName, 
                                                      const GFXVertexFormat *vertexFormat )
{
   return createMatInstance( matName, getDefaultFeatures(), vertexFormat );
}

BaseMatInstance* MaterialManager::createMatInstance(  const String &matName, 
                                                      const FeatureSet& features, 
                                                      const GFXVertexFormat *vertexFormat )
{
   BaseMatInstance* mat = createMatInstance(matName);
   if (mat)
   {
      mat->init( features, vertexFormat );
      return mat;
   }

   return NULL;
}

BaseMatInstance  * MaterialManager::createWarningMatInstance()
{
   Material *warnMat = static_cast<Material*>(Sim::findObject("WarningMaterial"));

   BaseMatInstance   *warnMatInstance = NULL;

   if( warnMat != NULL )
   {
      warnMatInstance = warnMat->createMatInstance();

      GFXStateBlockDesc desc;
      desc.setCullMode(GFXCullNone);
      warnMatInstance->addStateBlockDesc(desc);

      warnMatInstance->init(  getDefaultFeatures(), 
                              getGFXVertexFormat<GFXVertexPNTTB>() );
   }

   return warnMatInstance;
}

// Gets the global warning material instance, callers should not free this copy
BaseMatInstance * MaterialManager::getWarningMatInstance()
{
   if (!mWarningInst)
      mWarningInst = createWarningMatInstance();

   return mWarningInst;
}

#ifndef TORQUE_SHIPPING
BaseMatInstance * MaterialManager::createMeshDebugMatInstance(const ColorF &meshColor)
{
   String  meshDebugStr = String::ToString( "Torque_MeshDebug_%d", meshColor.getRGBAPack() );

   Material *debugMat;
   if (!Sim::findObject(meshDebugStr,debugMat))
   {
      debugMat = allocateAndRegister( meshDebugStr );

      debugMat->mDiffuse[0] = meshColor;
      debugMat->mEmissive[0] = true;
   }

   BaseMatInstance   *debugMatInstance = NULL;

   if( debugMat != NULL )
   {
      debugMatInstance = debugMat->createMatInstance();

      GFXStateBlockDesc desc;
      desc.setCullMode(GFXCullNone);
      desc.fillMode = GFXFillWireframe;
      debugMatInstance->addStateBlockDesc(desc);

      // Disable fog and other stuff.
      FeatureSet debugFeatures;
      debugFeatures.addFeature( MFT_DiffuseColor );
      debugMatInstance->init( debugFeatures, getGFXVertexFormat<GFXVertexPCN>() );
   }

   return debugMatInstance;
}

// Gets the global material instance for a given color, callers should not free this copy
BaseMatInstance *MaterialManager::getMeshDebugMatInstance(const ColorF &meshColor)
{
   DebugMaterialMap::Iterator itr = mMeshDebugMaterialInsts.find( meshColor.getRGBAPack() );

   BaseMatInstance   *inst  = NULL;

   if ( itr == mMeshDebugMaterialInsts.end() )
      inst = createMeshDebugMatInstance( meshColor );
   else
      inst = itr->value;

   mMeshDebugMaterialInsts.insert( meshColor.getRGBAPack(), inst );

   return inst;
}
#endif

void MaterialManager::mapMaterial(const String & textureName, const String & materialName)
{
   if (getMapEntry(textureName).isNotEmpty())
   {
      if (!textureName.equal("unmapped_mat", String::NoCase))
         Con::warnf(ConsoleLogEntry::General, "Warning, overwriting material for: %s", textureName.c_str());
   }

   mMaterialMap[String::ToLower(textureName)] = materialName;
}

String MaterialManager::getMapEntry(const String & textureName) const
{
   MaterialMap::ConstIterator iter = mMaterialMap.find(String::ToLower(textureName));
   if ( iter == mMaterialMap.end() )
      return String();
   return iter->value;
}

void MaterialManager::flushAndReInitInstances()
{
   // First we flush all the shader gen shaders which will
   // invalidate all GFXShader* to them.
   SHADERGEN->flushProceduralShaders();   

   // First do a pass deleting all hooks as they can contain
   // materials themselves.  This means we have to restart the
   // loop every time we delete any hooks... lame.
   Vector<BaseMatInstance*>::iterator iter = mMatInstanceList.begin();
   while ( iter != mMatInstanceList.end() )
   {
      if ( (*iter)->deleteAllHooks() != 0 )
      {
         // Restart the loop.
         iter = mMatInstanceList.begin();
         continue;
      }

      iter++;
   }

   // Now do a pass re-initializing materials.
   iter = mMatInstanceList.begin();
   for ( ; iter != mMatInstanceList.end(); iter++ )
      (*iter)->reInit();
}

// Used in the materialEditor. This flushes the material preview object so it can be reloaded easily.
void MaterialManager::flushInstance( BaseMaterialDefinition *target )
{
   Vector<BaseMatInstance*>::iterator iter = mMatInstanceList.begin();
   while ( iter != mMatInstanceList.end() )
   {
      if ( (*iter)->getMaterial() == target )
      {
		  (*iter)->deleteAllHooks();
		  return;
      }
	  iter++;
   }
}

void MaterialManager::reInitInstance( BaseMaterialDefinition *target )
{
   Vector<BaseMatInstance*>::iterator iter = mMatInstanceList.begin();
   for ( ; iter != mMatInstanceList.end(); iter++ )
   {
      if ( (*iter)->getMaterial() == target )
         (*iter)->reInit();
   }
}

void MaterialManager::updateTime()
{
   U32 curTime = Sim::getCurrentTime();
   if(curTime > mLastTime)
   {
      mDt =  (curTime - mLastTime) / 1000.0f;
      mLastTime = curTime;
      mAccumTime += mDt;
   }
   else
      mDt = 0.0f;
}

SimSet * MaterialManager::getMaterialSet()
{
   if(!mMaterialSet)
      mMaterialSet = static_cast<SimSet*>( Sim::findObject( "MaterialSet" ) );

   AssertFatal( mMaterialSet, "MaterialSet not found" );
   return mMaterialSet;
}

void MaterialManager::dumpMaterialInstances( BaseMaterialDefinition *target ) const
{
   if ( !mMatInstanceList.size() )
      return;

   if ( target )
      Con::printf( "--------------------- %s MatInstances ---------------------", target->getName() );
   else
      Con::printf( "--------------------- MatInstances %d ---------------------", mMatInstanceList.size() );

   for( U32 i=0; i<mMatInstanceList.size(); i++ )
   {
      BaseMatInstance *inst = mMatInstanceList[i];
      
      if ( target && inst->getMaterial() != target )
         continue;

      inst->dumpShaderInfo();
   }

   Con::printf( "---------------------- Dump complete ----------------------");
}

void MaterialManager::_track( MatInstance *matInstance )
{
   mMatInstanceList.push_back( matInstance );
}

void MaterialManager::_untrack( MatInstance *matInstance )
{
   mMatInstanceList.remove( matInstance );
}

void MaterialManager::recalcFeaturesFromPrefs()
{
   mDefaultFeatures.clear();
   FeatureType::addDefaultTypes( &mDefaultFeatures );

   mExclusionFeatures.setFeature(   MFT_NormalMap, 
                                    Con::getBoolVariable( "$pref::Video::disableNormalmapping", false ) );

   mExclusionFeatures.setFeature(   MFT_PixSpecular,
                                    Con::getBoolVariable( "$pref::Video::disablePixSpecular", false ) );

   mExclusionFeatures.setFeature(   MFT_CubeMap, 
                                    Con::getBoolVariable( "$pref::Video::disableCubemapping", false ) );
}

bool MaterialManager::_handleGFXEvent(GFXDevice::GFXDeviceEventType event)
{
   switch (event)
   {
   case GFXDevice::deInit :
      recalcFeaturesFromPrefs();
      break;
   case GFXDevice::deDestroy :
      SAFE_DELETE(mWarningInst);
      break;
   default :
      break;
   }
   return true;
}

ConsoleFunction( reInitMaterials, void, 1, 1, 
   "Flushes all the procedural shaders and re-initializes all the active materials instances." )
{
   MATMGR->flushAndReInitInstances();
}

ConsoleFunction( addMaterialMapping, void, 3, 3, "(string texName, string matName)\n"
   "Set up a material to texture mapping.")
{
   MATMGR->mapMaterial(argv[1],argv[2]);
}

ConsoleFunction( recalcFeaturesFromPrefs, void, 1, 1, 
   "Enables/disable shader features based on pref settings.")
{
   MATMGR->recalcFeaturesFromPrefs();
}

ConsoleFunction( getMaterialMapping, const char*, 2, 2, "(string texName)\n"
   "Gets the name of the material mapped to this texture.")
{
   return MATMGR->getMapEntry(argv[1]).c_str();
}

ConsoleFunction( dumpMaterialInstances, void, 1, 1, 
   "Dumps a formatted list of currently allocated material instances to the console." )
{
   MATMGR->dumpMaterialInstances();
}

ConsoleFunction( getMapEntry, const char *, 2, 2, 
   "getMapEntry( String ) Returns the material name via the materialList mapTo entry" )
{
	return MATMGR->getMapEntry( String(argv[1]) );
}