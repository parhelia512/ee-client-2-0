//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/lightManager.h"

#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/util/safeDelete.h"
#include "console/sim.h"
#include "console/simSet.h"
#include "sceneGraph/sceneGraph.h"
#include "materials/materialManager.h"
#include "materials/sceneData.h"
#include "lighting/lightInfo.h"
#include "lighting/lightingInterfaces.h"
#include "T3D/gameConnection.h"
#include "gfx/gfxStringEnumTranslate.h"


Signal<void(const char*,bool)> LightManager::smActivateSignal;


LightManager::LightManager( const char *name, const char *id )
   :  mName( name ),
      mId( id ),
      mIsActive( false ),      
      mSceneManager( NULL ),
      mDefaultLight( NULL ),
      mAvailableSLInterfaces( NULL ),
      mCullPos( Point3F::Zero )
{ 
   _getLightManagers().insert( mName, this );

   dMemset( &mSpecialLights, 0, sizeof( mSpecialLights ) );
}

LightManager::~LightManager() 
{
   _getLightManagers().erase( mName );
   SAFE_DELETE( mAvailableSLInterfaces );
   SAFE_DELETE( mDefaultLight );
}

LightManagerMap& LightManager::_getLightManagers()
{
   static LightManagerMap lightManagerMap;
   return lightManagerMap;
}

LightManager* LightManager::findByName( const char *name )
{
   LightManagerMap &lightManagers = _getLightManagers();

   LightManagerMap::Iterator iter = lightManagers.find( name );
   if ( iter != lightManagers.end() )
      return iter->value;

   return NULL;
}

void LightManager::getLightManagerNames( String *outString )
{
   LightManagerMap &lightManagers = _getLightManagers();
   LightManagerMap::Iterator iter = lightManagers.begin();
   for ( ; iter != lightManagers.end(); iter++ )
      *outString += iter->key + "\t";

   // TODO!
   //outString->rtrim();
}

LightInfo* LightManager::createLightInfo()
{
   LightInfo *outLight = new LightInfo;

   LightManagerMap &lightManagers = _getLightManagers();
   LightManagerMap::Iterator iter = lightManagers.begin();
   for ( ; iter != lightManagers.end(); iter++ )
   {
      LightManager *lm = iter->value;
      lm->_addLightInfoEx( outLight );
   }

   return outLight;
}

void LightManager::initLightFields()
{
   LightManagerMap &lightManagers = _getLightManagers();

   LightManagerMap::Iterator iter = lightManagers.begin();
   for ( ; iter != lightManagers.end(); iter++ )
   {
      LightManager *lm = iter->value;
      lm->_initLightFields();
   }
}

void LightManager::activate( SceneGraph *sceneManager )
{
   AssertFatal( sceneManager, "LightManager::activate() - Got null scene manager!" );
   AssertFatal( mIsActive == false, "LightManager::activate() - Already activated!" );

   mIsActive = true;
   mSceneManager = sceneManager;

   Con::executef("onLightManagerActivate", getName());
}

void LightManager::deactivate()
{
   AssertFatal( mIsActive == true, "LightManager::deactivate() - Already deactivated!" );

   if( Sim::getRootGroup() ) // To protect against shutdown.
      Con::executef("onLightManagerDeactivate", getName());

   mIsActive = false;
   mSceneManager = NULL;

   // Just in case... make sure we're all clear.
   unregisterAllLights();
}

LightInfo* LightManager::getDefaultLight()
{
   // The sun is always our default light when
   // when its registered.
   if ( mSpecialLights[ LightManager::slSunLightType ] )
      return mSpecialLights[ LightManager::slSunLightType ];

   // Else return a dummy special light.
   if ( !mDefaultLight )
      mDefaultLight = createLightInfo();
   return mDefaultLight;
}

LightInfo* LightManager::getSpecialLight( LightManager::SpecialLightTypesEnum type, bool useDefault )
{
   if ( mSpecialLights[type] )
      return mSpecialLights[type];

   if ( useDefault )
      return getDefaultLight();

   return NULL;
}

void LightManager::setSpecialLight( LightManager::SpecialLightTypesEnum type, LightInfo *light )
{
   if ( light && type == slSunLightType )
   {
      // The sun must be specially positioned and ranged
      // so that it can be processed like a point light 
      // in the stock light shader used by Basic Lighting.
      
      light->setPosition( mCullPos - ( light->getDirection() * 10000.0f ) );
      light->setRange( 2000000.0f );
   }

   mSpecialLights[type] = light;
   registerGlobalLight( light, NULL );
}

void LightManager::registerGlobalLights( const Frustum *frustum, bool staticLighting )
{
   PROFILE_SCOPE( LightManager_RegisterGlobalLights );

   // TODO: We need to work this out...
   //
   // 1. Why do we register and unregister lights on every 
   //    render when they don't often change... shouldn't we
   //    just register once and keep them?
   // 
   // 2. If we do culling of lights should this happen as part
   //    of registration or somewhere else?
   //

   // Grab the lights to process.
   Vector<SceneObject*> activeLights;
   const U32 lightMask = LightObjectType;
   
   if ( staticLighting || !frustum )
   {
      // We're processing static lighting or want all the lights
      // in the container registerd...  so no culling.
      getSceneManager()->getContainer()->findObjectList( lightMask, &activeLights );
   }
   else
   {
      // Cull the lights using the frustum.
      getSceneManager()->getContainer()->findObjectList( *frustum, lightMask, &activeLights );

      // Store the culling position for sun placement
      // later... see setSpecialLight.
      mCullPos = frustum->getPosition();

      // HACK: Make sure the control object always gets 
      // processed as lights mounted to it don't change
      // the shape bounds and can often get culled.

      GameConnection *conn = GameConnection::getConnectionToServer();
      if ( conn->getControlObject() )
      {
         GameBase *conObject = conn->getControlObject();
         activeLights.push_back_unique( conObject );
      }
   }

   // Let the lights register themselves.
   for ( U32 i = 0; i < activeLights.size(); i++ )
   {
      ISceneLight *lightInterface = dynamic_cast<ISceneLight*>( activeLights[i] );
      if ( lightInterface )
         lightInterface->submitLights( this, staticLighting );
   }
}

void LightManager::registerGlobalLight( LightInfo *light, SimObject *obj )
{
   AssertFatal( !mRegisteredLights.contains( light ), 
      "LightManager::registerGlobalLight - This light is already registered!" );

   mRegisteredLights.push_back( light );
}

void LightManager::unregisterGlobalLight( LightInfo *light )
{
   mRegisteredLights.unregisterLight( light );

   // If this is the sun... clear the special light too.
   if ( light == mSpecialLights[slSunLightType] )
      dMemset( mSpecialLights, 0, sizeof( mSpecialLights ) );
}

void LightManager::registerLocalLight( LightInfo *light )
{
   // TODO: What should we do here?
}

void LightManager::unregisterLocalLight( LightInfo *light )
{
   // TODO: What should we do here?
}

void LightManager::unregisterAllLights()
{
   dMemset( mSpecialLights, 0, sizeof( mSpecialLights ) );
   mRegisteredLights.clear();
   mBestLights.clear();
}

void LightManager::getAllUnsortedLights( LightInfoList &list )
{
   list.merge( mRegisteredLights );
}

void LightManager::setupLights(  LightReceiver *obj, 
                                 const Point3F &camerapos,
                                 const Point3F &cameradir, 
                                 F32 viewdist,
                                 U32 maxLights )
{
   SphereF bounds( camerapos, viewdist );
   _scoreLights( bounds );

   // Limit the maximum.
   if ( mBestLights.size() > maxLights )
      mBestLights.setSize( maxLights );
}

void LightManager::setupLights(  LightReceiver *obj, 
                                 const SphereF &bounds,
                                 U32 maxLights )
{
   _scoreLights( bounds );

   // Limit the maximum.
   if ( mBestLights.size() > maxLights )
      mBestLights.setSize( maxLights );
}

void LightManager::getBestLights( LightInfo** outLights, U32 maxLights )
{
   PROFILE_SCOPE( LightManager_GetBestLights );

   // How many lights can we get... we don't allow 
   // more than 4 for forward lighting.
   U32 lights = getMin( (U32)mBestLights.size(), getMin( (U32)4, maxLights ) );

   // Copy them over.
   for ( U32 i = 0; i < lights; i++ )
   {
      LightInfo *light = mBestLights[i];

      // If the score reaches zero then we got to
      // the end of the valid lights for this object.
      if ( light->getScore() <= 0.0f )
         break;

      outLights[i] = light;
   }
}

void LightManager::resetLights()
{
   mBestLights.clear();
}

void LightManager::_scoreLights( const SphereF &bounds )
{
   PROFILE_SCOPE( LightManager_ScoreLights );

   // Get all the lights.
   mBestLights.clear();
   getAllUnsortedLights( mBestLights );

   // Grab the sun to test for it.
   LightInfo *sun = getSpecialLight( slSunLightType );

   const Point3F lumDot( 0.2125f, 0.7154f, 0.0721f );

   for ( U32 i=0; i < mBestLights.size(); i++ )
   {
      // Get the light.
      LightInfo *light = mBestLights[i];

      F32 luminace = 0.0f;
      F32 dist = 0.0f;
      F32 weight = 0.0f;

      const bool isSpot = light->getType() == LightInfo::Spot;
      const bool isPoint = light->getType() == LightInfo::Point;

      if ( isPoint || isSpot )
      {
         // Get the luminocity.
         luminace = mDot( light->getColor(), lumDot ) * light->getBrightness();

         // Get the distance to the light... score it 1 to 0 near to far.
         F32 lenSq = ( bounds.center - light->getPosition() ).lenSquared();

         F32 radiusSq = light->getRange().x + bounds.radius;
         radiusSq *= radiusSq;

         F32 distSq = radiusSq - lenSq;
         
         if ( distSq > 0.0f )
            dist = mClampF( distSq / ( 1000.0f * 1000.0f ), 0.0f, 1.0f );

         // TODO: This culling is broken... it culls spotlights 
         // that are actually visible.
         if ( false && isSpot && dist > 0.0f )
         {
            // TODO: I cannot test to see if we're within
            // the cone without a more detailed test... so
            // just reject if we're behind the spot direction.

            Point3F toCenter = bounds.center - light->getPosition();
            F32 angDot = mDot( toCenter, light->getDirection() );
            if ( angDot < 0.0f )
               dist = 0.0f;
         }

         weight = light->getPriority();
      }
      else
      {
         // The sun always goes first 
         // regardless of the settings.
         if ( light == sun )
         {
            weight = F32_MAX;
            dist = 1.0f;
            luminace = 1.0f;
         }
         else
         {
            // TODO: When we have multiple directional 
            // lights we should score them here.
         }
      }
      
      // TODO: Manager ambient lights here too!

      light->setScore( luminace * weight * dist );
   }

   // Sort them!
   mBestLights.sort( _lightScoreCmp );
}

S32 LightManager::_lightScoreCmp( LightInfo* const *a, LightInfo* const *b )
{
   F32 diff = (*a)->getScore() - (*b)->getScore();
   return diff < 0 ? 1 : diff > 0 ? -1 : 0;
}

void LightManager::_update4LightConsts(   const SceneGraphData &sgData,
                                          GFXShaderConstHandle *lightPositionSC,
                                          GFXShaderConstHandle *lightDiffuseSC,
                                          GFXShaderConstHandle *lightAmbientSC,
                                          GFXShaderConstHandle *lightInvRadiusSqSC,
                                          GFXShaderConstHandle *lightSpotDirSC,
                                          GFXShaderConstHandle *lightSpotAngleSC,
                                          GFXShaderConstBuffer *shaderConsts )
{
   PROFILE_SCOPE( LightManager_Update4LightConsts );

   // Skip over gathering lights if we don't have to!
   if (  lightPositionSC->isValid() || 
         lightDiffuseSC->isValid() ||
         lightInvRadiusSqSC->isValid() ||
         lightSpotDirSC->isValid() ||
         lightSpotAngleSC->isValid() )
   {
      // NOTE: We haven't ported the lighting shaders on OSX
      // to the optimized HLSL versions.
      #ifdef TORQUE_OS_MAC
         static AlignedArray<Point3F> lightPositions( 4, sizeof( Point4F ) );
      #else
         static AlignedArray<Point4F> lightPositions( 3, sizeof( Point4F ) );
         static AlignedArray<Point4F> lightSpotDirs( 3, sizeof( Point4F ) );
      #endif               
      static AlignedArray<Point4F> lightColors( 4, sizeof( Point4F ) );
      static Point4F lightInvRadiusSq;
      static Point4F lightSpotAngle;
      F32 range;
      
      // Need to clear the buffers so that we don't leak
      // lights from previous passes or have NaNs.
      dMemset( lightPositions.getBuffer(), 0, lightPositions.getBufferSize() );
      dMemset( lightColors.getBuffer(), 0, lightColors.getBufferSize() );
      lightInvRadiusSq = Point4F::Zero;
      lightSpotAngle.set( -1.0f, -1.0f, -1.0f, -1.0f );
      
      // Gather the data for the first 4 lights.
      const LightInfo *light;
      for ( U32 i=0; i < 4; i++ )
      {
         light = sgData.lights[i];
         if ( !light )            
            break;

         #ifdef TORQUE_OS_MAC

            lightPositions[i] = light->getPosition();

         #else
      
            // The light positions and spot directions are 
            // in SoA order to make optimal use of the GPU.
            const Point3F &lightPos = light->getPosition();
            lightPositions[0][i] = lightPos.x;
            lightPositions[1][i] = lightPos.y;
            lightPositions[2][i] = lightPos.z;

            const VectorF &lightDir = light->getDirection();
            lightSpotDirs[0][i] = lightDir.x;
            lightSpotDirs[1][i] = lightDir.y;
            lightSpotDirs[2][i] = lightDir.z;
            
            if ( light->getType() == LightInfo::Spot )
               lightSpotAngle[i] = mCos( mDegToRad( light->getOuterConeAngle() / 2.0f ) ); 

         #endif            

         // Prescale the light color by the brightness to 
         // avoid doing this in the shader.
         lightColors[i] = Point4F(light->getColor()) * light->getBrightness();

         // We need 1 over range^2 here.
         range = light->getRange().x;
         lightInvRadiusSq[i] = 1.0f / ( range * range );
      }

      shaderConsts->set( lightPositionSC, lightPositions );   
      shaderConsts->set( lightDiffuseSC, lightColors );
      shaderConsts->set( lightInvRadiusSqSC, lightInvRadiusSq );

      #ifndef TORQUE_OS_MAC

         shaderConsts->set( lightSpotDirSC, lightSpotDirs );
         shaderConsts->set( lightSpotAngleSC, lightSpotAngle );

      #endif
   }

   // Setup the ambient lighting from the first 
   // light which is the directional light if 
   // one exists at all in the scene.
   if ( lightAmbientSC->isValid() )
   {
      ColorF ambient( ColorF::BLACK );
      if ( sgData.lights[0] )
         ambient = sgData.lights[0]->getAmbient();
      shaderConsts->set( lightAmbientSC, ambient );
   }
}

AvailableSLInterfaces* LightManager::getSceneLightingInterface()
{
   if ( !mAvailableSLInterfaces )
      mAvailableSLInterfaces = new AvailableSLInterfaces();

   return mAvailableSLInterfaces;
}

bool LightManager::lightScene( const char* callback, const char* param )
{
   BitSet32 flags = 0;

   if ( param )
   {
      if ( !dStricmp( param, "forceAlways" ) )
         flags.set( SceneLighting::ForceAlways );
      else if ( !dStricmp(param, "forceWritable" ) )
         flags.set( SceneLighting::ForceWritable );
   }

   // The SceneLighting object will delete itself 
   // once the lighting process is complete.   
   SceneLighting* sl = new SceneLighting( getSceneLightingInterface() );
   return sl->lightScene( callback, flags );
}

ConsoleFunctionGroupBegin( LightManager, "Functions for working with the light managers." );

ConsoleFunction( setLightManager, bool, 1, 3, "setLightManager( string lmName )\n"
   "Finds and activates the named light manager." )
{
   return gClientSceneGraph->setLightManager( argv[1] );
}

ConsoleFunction(lightScene, bool, 1, 3, "(script_function completeCallback=NULL, string mode=\"\")"
               "Relight the scene.\n\n"
               "If mode is \"forceAlways\", the lightmaps will be regenerated regardless of whether "
               "lighting cache files can be written to. If mode is \"forceWritable\", then the lightmaps "
               "will be regenerated only if the lighting cache files can be written.")
{
   LightManager * lm = gClientSceneGraph->getLightManager();

   if (lm)
   {
      if (argc > 1)
         return lm->lightScene(argv[1], argv[2]);
      else
         return lm->lightScene(argv[1], NULL);
   } 
   else
      return false;
}

ConsoleFunction( getLightManagerNames, const char*, 1, 1, 
   "Returns a tab seperated list of light manager names." )
{
   String names;
   LightManager::getLightManagerNames( &names );

   char *result = Con::getReturnBuffer( names.length() + 1 );
   dStrcpy( result, names.c_str() );
   return result;
}

ConsoleFunction( getActiveLightManager, const char*, 1, 1, 
   "Returns the active light manager name." )
{
   LightManager *lm = gClientSceneGraph->getLightManager();
   if ( !lm )
      return NULL;

   return lm->getName();
}

ConsoleFunction( resetLightManager, void, 1, 1, 
   "Deactivates and then activates the currently active light manager." )
{
   LightManager *lm = gClientSceneGraph->getLightManager();
   if ( lm )
   {
      lm->deactivate();
      lm->activate( gClientSceneGraph );
   }
}

ConsoleFunction( getBestHDRFormat, const char*, 1, 1, 
   "Returns the best texture GFXFormat for storage of HDR data." )
{
   // TODO: Maybe expose GFX::selectSupportedFormat() so that this
   // specialized method can be moved to script.

   // Figure out the best HDR format.  This is the smallest
   // format which supports blending and filtering.
   Vector<GFXFormat> formats;
   formats.push_back( GFXFormatR10G10B10A2 );
   formats.push_back( GFXFormatR16G16B16A16F );
   formats.push_back( GFXFormatR16G16B16A16 );    
   GFXFormat format = GFX->selectSupportedFormat(  &GFXDefaultRenderTargetProfile,
                                                   formats, 
                                                   true,
                                                   true,
                                                   true );
                                                   
   return Con::getData( TypeEnum, &format, 0, &gTextureFormatEnumTable );
}

ConsoleFunctionGroupEnd( LightManager );
