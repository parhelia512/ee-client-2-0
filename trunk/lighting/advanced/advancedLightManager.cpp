//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/advanced/advancedLightManager.h"

#include "lighting/advanced/advancedLightBinManager.h"
#include "lighting/advanced/advancedLightingFeatures.h"
#include "lighting/shadowMap/shadowMapManager.h"
#include "lighting/shadowMap/lightShadowMap.h"
#include "lighting/common/sceneLighting.h"
#include "lighting/common/lightMapParams.h"
#include "core/util/safeDelete.h"
#include "renderInstance/renderPrePassMgr.h"
#include "materials/materialManager.h"
#include "math/util/sphereMesh.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneState.h"


const String AdvancedLightManager::ConstantSpecularPowerSC( "$constantSpecularPower" );

AdvancedLightManager AdvancedLightManager::smSingleton;


AdvancedLightManager::AdvancedLightManager()
   :  LightManager( "Advanced Lighting", "ADVLM" )
{
   mLightBinManager = NULL;
   mLastShader = NULL;
   mAvailableSLInterfaces = NULL;
}

AdvancedLightManager::~AdvancedLightManager()
{
   mLastShader = NULL;
   mLastConstants = NULL;

   for (LightConstantMap::Iterator i = mConstantLookup.begin(); i != mConstantLookup.end(); i++)
   {
      if (i->value)
         SAFE_DELETE(i->value);
   }
   mConstantLookup.clear();
}

bool AdvancedLightManager::isCompatible() const
{
   // TODO: We need at least 3.0 shaders at the moment
   // but this should be relaxed to 2.0 soon.
   if ( GFX->getPixelShaderVersion() < 3.0 )
      return false;

   // TODO: Test for the necessary texture formats!

   return true;
}

void AdvancedLightManager::activate( SceneGraph *sceneManager )
{
   Parent::activate( sceneManager );

   GFXShader::addGlobalMacro( "TORQUE_ADVANCED_LIGHTING" );

   gClientSceneGraph->setPostEffectFog( true );

   SHADOWMGR->activate();

   // Find a target format that supports blending... 
   // we prefer the floating point format if it works.
   Vector<GFXFormat> formats;
   formats.push_back( GFXFormatR16G16B16A16F );
   formats.push_back( GFXFormatR16G16B16A16 );
   GFXFormat blendTargetFormat = GFX->selectSupportedFormat( &GFXDefaultRenderTargetProfile,
                                                         formats,
                                                         true,
                                                         true,
                                                         false );

   mLightBinManager = new AdvancedLightBinManager( this, SHADOWMGR, blendTargetFormat );
   mLightBinManager->assignName( "AL_LightBinMgr" );

   // First look for the prepass bin...
   RenderPrePassMgr *prePassBin = NULL;
   RenderPassManager *rpm = getSceneManager()->getRenderPass();
   for( U32 i = 0; i < rpm->getManagerCount(); i++ )
   {
      RenderBinManager *bin = rpm->getManager(i);
      if( bin->getRenderInstType() == RenderPrePassMgr::RIT_PrePass )
      {
         prePassBin = (RenderPrePassMgr*)bin;
         break;
      }
   }

   // If we didn't find the prepass bin then add one.
   if ( !prePassBin )
   {
      prePassBin = new RenderPrePassMgr( true, blendTargetFormat );
      prePassBin->registerObject();
      rpm->addManager( prePassBin );
      mPrePassRenderBin = prePassBin;
   }

   // Tell the material manager that prepass is enabled.
   MATMGR->setPrePassEnabled( true );

   // Insert our light bin manager.
   mLightBinManager->setRenderOrder( prePassBin->getRenderOrder() + 0.01f );
   rpm->addManager( mLightBinManager );

   AdvancedLightingFeatures::registerFeatures(mPrePassRenderBin->getTargetFormat(), mLightBinManager->getTargetFormat());

   // Last thing... let everyone know we're active.
   smActivateSignal.trigger( getId(), true );
}

void AdvancedLightManager::deactivate()
{
   Parent::deactivate();

   GFXShader::removeGlobalMacro( "TORQUE_ADVANCED_LIGHTING" );

   MatTextureTarget::unregisterTarget( "AL_ShadowVizTexture", NULL );

   // Release our bin manager... it will take care of
   // removing itself from the render passes.
   if( mLightBinManager )
   {
      mLightBinManager->MRTLightmapsDuringPrePass(false);
      mLightBinManager->deleteObject();
   }
   mLightBinManager = NULL;

   if ( mPrePassRenderBin )
      mPrePassRenderBin->deleteObject();
   mPrePassRenderBin = NULL;

   SHADOWMGR->deactivate();

   mLastShader = NULL;
   mLastConstants = NULL;

   for (LightConstantMap::Iterator i = mConstantLookup.begin(); i != mConstantLookup.end(); i++)
   {
      if (i->value)
         SAFE_DELETE(i->value);
   }
   mConstantLookup.clear();

   mSphereGeometry = NULL;
   mSphereIndices = NULL;
   mConeGeometry = NULL;
   mConeIndices = NULL;

   AdvancedLightingFeatures::unregisterFeatures();

   // Now let everyone know we've deactivated.
   smActivateSignal.trigger( getId(), false );
}

void AdvancedLightManager::_addLightInfoEx( LightInfo *lightInfo )
{
   lightInfo->addExtended( new ShadowMapParams( lightInfo ) );
   lightInfo->addExtended( new LightMapParams( lightInfo ) );
}


void AdvancedLightManager::_initLightFields()
{
   static EnumTable::Enums gShadowTypeEnums[] =
   {
      { ShadowType_Spot,                     "Spot" },
      { ShadowType_PSSM,                     "PSSM" },
      { ShadowType_Paraboloid,               "Paraboloid" },
      { ShadowType_DualParaboloidSinglePass, "DualParaboloidSinglePass" },
      { ShadowType_DualParaboloid,           "DualParaboloid" },
      { ShadowType_CubeMap,                  "CubeMap" },
   };

   static EnumTable smShadowTypeTable( ShadowType_Count, gShadowTypeEnums );


   #define DEFINE_LIGHT_FIELD( var, type, enum_ )                             \
   static inline const char* _get##var##Field( void *obj, const char *data )  \
   {                                                                          \
      ShadowMapParams *p = _getShadowMapParams( obj );                        \
      if ( p )                                                                \
         return Con::getData( type, &p->var, 0, enum_ );                      \
      else                                                                    \
         return "";                                                           \
   }                                                                          \
                                                                              \
   static inline bool _set##var##Field( void *obj, const char *data )         \
   {                                                                          \
      ShadowMapParams *p = _getShadowMapParams( obj );                        \
      if ( p )                                                                \
      {                                                                       \
         Con::setData( type, &p->var, 0, 1, &data, enum_ );                   \
         p->_validate();                                                       \
      }                                                                       \
      return false;                                                           \
   }                                                                          \

   #define DEFINE_LIGHTMAP_FIELD( var, type, enum_ )                          \
   static inline const char* _get##var##Field( void *obj, const char *data )  \
   {                                                                          \
      LightMapParams *p = _getLightMapParams( obj );                        \
      if ( p )                                                                \
         return Con::getData( type, &p->var, 0, enum_ );                      \
      else                                                                    \
         return "";                                                           \
   }                                                                          \
   \
   static inline bool _set##var##Field( void *obj, const char *data )         \
   {                                                                          \
      LightMapParams *p = _getLightMapParams( obj );                        \
      if ( p )                                                                \
      {                                                                       \
         Con::setData( type, &p->var, 0, 1, &data, enum_ );                   \
      }                                                                       \
      return false;                                                           \
   }                                                                          \

   #define ADD_LIGHT_FIELD( field, type, var, enum_, desc )                \
   ConsoleObject::addProtectedField( field, type, 0,                       \
      &Dummy::_set##var##Field, &Dummy::_get##var##Field, 1, enum_, desc ) \

   // Our dummy adaptor class which we hide in here
   // to keep from poluting the global namespace.
   class Dummy
   {
   protected:

      static inline ShadowMapParams* _getShadowMapParams( void *obj )
      {
         ISceneLight *sceneLight = dynamic_cast<ISceneLight*>( (SimObject*)obj );
         if ( sceneLight )
         {
            LightInfo *lightInfo = sceneLight->getLight();                           
            if ( lightInfo )
               return lightInfo->getExtended<ShadowMapParams>();
         }
         return NULL;
      }

      static inline LightMapParams* _getLightMapParams( void *obj )
      {
         ISceneLight *sceneLight = dynamic_cast<ISceneLight*>( (SimObject*)obj );
         if ( sceneLight )
         {
            LightInfo *lightInfo = sceneLight->getLight();                           
            if ( lightInfo )
               return lightInfo->getExtended<LightMapParams>();
         }
         return NULL;
      }

   public:

      DEFINE_LIGHT_FIELD( attenuationRatio, TypePoint3F, NULL );
      DEFINE_LIGHT_FIELD( shadowType, TypeEnum, &smShadowTypeTable );
      DEFINE_LIGHT_FIELD( texSize, TypeS32, NULL );
      DEFINE_LIGHT_FIELD( numSplits, TypeS32, NULL );
      DEFINE_LIGHT_FIELD( logWeight, TypeF32, NULL );
      DEFINE_LIGHT_FIELD( overDarkFactor, TypePoint4F, NULL);
      DEFINE_LIGHT_FIELD( shadowDistance, TypeF32, NULL );
      DEFINE_LIGHT_FIELD( shadowSoftness, TypeF32, NULL );
      DEFINE_LIGHT_FIELD( fadeStartDist, TypeF32, NULL );
      DEFINE_LIGHT_FIELD( lastSplitTerrainOnly, TypeBool, NULL );     
      DEFINE_LIGHT_FIELD( splitFadeDistances, TypePoint4F, NULL );

      DEFINE_LIGHTMAP_FIELD( representedInLightmap, TypeBool, NULL );
      DEFINE_LIGHTMAP_FIELD( shadowDarkenColor, TypeColorF, NULL );
      DEFINE_LIGHTMAP_FIELD( includeLightmappedGeometryInShadow, TypeBool, NULL );
   };

   ConsoleObject::addGroup( "Advanced Lighting" );

      ADD_LIGHT_FIELD( "attenuationRatio", TypePoint3F, attenuationRatio, NULL,
         "The proportions of constant, linear, and quadratic attenuation to use for "
         "the falloff for point and spot lights." );

      ADD_LIGHT_FIELD( "shadowType", TypeEnum, shadowType, &smShadowTypeTable,
         "The type of shadow to use on this light." );

      ADD_LIGHT_FIELD( "texSize", TypeS32, texSize, NULL, 
         "The texture size of the shadow map." );

      ADD_LIGHT_FIELD( "overDarkFactor", TypePoint4F, overDarkFactor, NULL,
         "The ESM shadow darkening factor");

      ADD_LIGHT_FIELD( "shadowDistance", TypeF32, shadowDistance, NULL,
         "The distance from the camera to extend the PSSM shadow." );

      ADD_LIGHT_FIELD( "shadowSoftness", TypeF32, shadowSoftness, NULL,
         "" );

      ADD_LIGHT_FIELD( "numSplits", TypeS32, numSplits, NULL,
         "The logrithmic PSSM split distance factor." );

      ADD_LIGHT_FIELD( "logWeight", TypeF32, logWeight, NULL,
         "The logrithmic PSSM split distance factor." );      

      ADD_LIGHT_FIELD( "fadeStartDistance", TypeF32, fadeStartDist, NULL,
         "Start fading shadows out at this distance.  0 = auto calculate this distance.");

      ADD_LIGHT_FIELD( "lastSplitTerrainOnly", TypeBool, lastSplitTerrainOnly, NULL,
         "This toggles only terrain being rendered to the last split of a PSSM shadow map.");

      ADD_LIGHT_FIELD( "splitFadeDistances", TypePoint4F, splitFadeDistances, NULL,
         "Distances (in world space units) that the shadows will fade between PSSM splits");

   ConsoleObject::endGroup( "Advanced Lighting" );

   ConsoleObject::addGroup( "Advanced Lighting Lightmap" );

      ADD_LIGHT_FIELD( "representedInLightmap", TypeBool, representedInLightmap, NULL,
         "This light is represented in lightmaps (static light, default: false)");

      ADD_LIGHT_FIELD( "shadowDarkenColor", TypeColorF, shadowDarkenColor, NULL,
         "The color that should be used to multiply-blend dynamic shadows onto lightmapped geometry (ignored if 'representedInLightmap' is false)");

      ADD_LIGHT_FIELD( "includeLightmappedGeometryInShadow", TypeBool, includeLightmappedGeometryInShadow, NULL,
         "This light should render lightmapped geometry during its shadow-map update (ignored if 'representedInLightmap' is false)");

   ConsoleObject::endGroup( "Advanced Lighting Lightmap" );

   #undef DEFINE_LIGHT_FIELD
   #undef ADD_LIGHT_FIELD
}

void AdvancedLightManager::setLightInfo(  ProcessedMaterial *pmat, 
                                          const Material *mat, 
                                          const SceneGraphData &sgData,
                                          const SceneState *state,
                                          U32 pass, 
                                          GFXShaderConstBuffer *shaderConsts)
{
   // Skip this if we're rendering from the prepass bin.
   if ( sgData.binType == SceneGraphData::PrePassBin )
      return;

   PROFILE_SCOPE(AdvancedLightManager_setLightInfo);

   LightingShaderConstants *lsc = getLightingShaderConstants(shaderConsts);

   LightShadowMap *lsm = SHADOWMGR->getCurrentShadowMap();

   LightInfo *light;
   if ( lsm )
      light = lsm->getLightInfo();
   else
   {
      light = sgData.lights[0];
      if ( !light )
         light = getDefaultLight();
   }

   // NOTE: If you encounter a crash from this point forward
   // while setting a shader constant its probably because the
   // mConstantLookup has bad shaders/constants in it.
   //
   // This is a known crash bug that can occur if materials/shaders
   // are reloaded and the light manager is not reset.
   //
   // We should look to fix this by clearing the table.

   // Update the forward shading light constants.
   _update4LightConsts( sgData,
                        lsc->mLightPositionSC,
                        lsc->mLightDiffuseSC,
                        lsc->mLightAmbientSC,
                        lsc->mLightInvRadiusSqSC,
                        lsc->mLightSpotDirSC,
                        lsc->mLightSpotAngleSC,
                        shaderConsts );

   if ( lsc->mConstantSpecularPowerSC->isValid() )
      shaderConsts->set( lsc->mConstantSpecularPowerSC, AdvancedLightBinManager::getConstantSpecularPower() );

   if ( lsm )
   {
      if (  lsc->mWorldToLightProjSC->isValid() )
         shaderConsts->set(   lsc->mWorldToLightProjSC, 
                              lsm->getWorldToLightProj(), 
                              lsc->mWorldToLightProjSC->getType() );

      if (  lsc->mViewToLightProjSC->isValid() )
      {
         // TODO: Should not do this mul here probably
         shaderConsts->set(   lsc->mViewToLightProjSC, 
                              lsm->getWorldToLightProj() * state->getCameraTransform(), 
                              lsc->mViewToLightProjSC->getType() );
      }

      if ( lsc->mShadowMapSizeSC->isValid() )
         shaderConsts->set( lsc->mShadowMapSizeSC, 1.0f / (F32)lsm->getTexSize() );

      // Do this last so that overrides can properly override parameters previously set
      lsm->setShaderParameters(shaderConsts, lsc);
   }
}

void AdvancedLightManager::registerGlobalLight(LightInfo *light, SimObject *obj)
{
   Parent::registerGlobalLight( light, obj );

   // Pass the volume lights to the bin manager.
   if (  mLightBinManager &&
         (  light->getType() == LightInfo::Point ||
            light->getType() == LightInfo::Spot ) )
      mLightBinManager->addLight( light );
}

void AdvancedLightManager::unregisterAllLights()
{
   Parent::unregisterAllLights();

   if ( mLightBinManager )
      mLightBinManager->clear();
}

bool AdvancedLightManager::setTextureStage(  const SceneGraphData &sgData,
                                             const U32 currTexFlag,
                                             const U32 textureSlot, 
                                             GFXShaderConstBuffer *shaderConsts, 
                                             ShaderConstHandles *handles )
{
   LightShadowMap* lsm = SHADOWMGR->getCurrentShadowMap();

   // Assign Shadowmap, if it exists
   LightingShaderConstants* lsc = getLightingShaderConstants(shaderConsts);
   if ( !lsc )
      return false;

   if(lsm)
      return lsm->setTextureStage( sgData, currTexFlag, textureSlot, shaderConsts, lsc->mShadowMapSC );

   // HACK: We should either be setting a 2x2 white texture
   // or be using a shader that doesn't sample the shadow maps!
      
   switch (currTexFlag)
   {
      case Material::DynamicLight:
      {
         if(shaderConsts && lsc->mShadowMapSC->isValid())
            shaderConsts->set(lsc->mShadowMapSC, (S32)textureSlot);
         GFX->setTexture( textureSlot, NULL );
         return true;
      }
   };

   return false;
}

LightingShaderConstants* AdvancedLightManager::getLightingShaderConstants(GFXShaderConstBuffer* buffer)
{
   if ( !buffer )
      return NULL;

   PROFILE_SCOPE( AdvancedLightManager_GetLightingShaderConstants );

   GFXShader* shader = buffer->getShader();

   // Check to see if this is the same shader, we'll get hit repeatedly by
   // the same one due to the render bin loops.
   if ( mLastShader.getPointer() != shader )
   {
      LightConstantMap::Iterator iter = mConstantLookup.find(shader);   
      if ( iter != mConstantLookup.end() )
      {
         mLastConstants = iter->value;
      } 
      else 
      {     
         LightingShaderConstants* lsc = new LightingShaderConstants();
         mConstantLookup[shader] = lsc;

         mLastConstants = lsc;      
      }

      // Set our new shader
      mLastShader = shader;
   }

   // Make sure that our current lighting constants are initialized
   if (!mLastConstants->mInit)
      mLastConstants->init(shader);

   return mLastConstants;
}

GFXVertexBufferHandle<AdvancedLightManager::LightVertex> AdvancedLightManager::getSphereMesh(U32 &outNumPrimitives, GFXPrimitiveBuffer *&outPrimitives)
{
   static SphereMesh sSphereMesh;

   if( mSphereGeometry.isNull() )
   {
      const SphereMesh::TriangleMesh * sphereMesh = sSphereMesh.getMesh(3);
      S32 numPoly = sphereMesh->numPoly;
      mSpherePrimitiveCount = 0;
      mSphereGeometry.set(GFX, numPoly*3, GFXBufferTypeStatic);
      mSphereGeometry.lock();
      S32 vertexIndex = 0;
      
      for (S32 i=0; i<numPoly; i++)
      {
         mSpherePrimitiveCount++;

         mSphereGeometry[vertexIndex].point = sphereMesh->poly[i].pnt[0];
         mSphereGeometry[vertexIndex].color = ColorI::WHITE;
         vertexIndex++;

         mSphereGeometry[vertexIndex].point = sphereMesh->poly[i].pnt[1];
         mSphereGeometry[vertexIndex].color = ColorI::WHITE;
         vertexIndex++;

         mSphereGeometry[vertexIndex].point = sphereMesh->poly[i].pnt[2];
         mSphereGeometry[vertexIndex].color = ColorI::WHITE;
         vertexIndex++;
      }
      mSphereGeometry.unlock();
   }

   outNumPrimitives = mSpherePrimitiveCount;
   outPrimitives = NULL; // For now
   return mSphereGeometry;
}

GFXVertexBufferHandle<AdvancedLightManager::LightVertex> AdvancedLightManager::getConeMesh(U32 &outNumPrimitives, GFXPrimitiveBuffer *&outPrimitives )
{
   static const Point2F circlePoints[] = 
   {
      Point2F(0.707107f, 0.707107f),
      Point2F(0.923880f, 0.382683f),
      Point2F(1.000000f, 0.000000f),
      Point2F(0.923880f, -0.382684f),
      Point2F(0.707107f, -0.707107f),
      Point2F(0.382683f, -0.923880f),
      Point2F(0.000000f, -1.000000f),
      Point2F(-0.382683f, -0.923880f),
      Point2F(-0.707107f, -0.707107f),
      Point2F(-0.923880f, -0.382684f),
      Point2F(-1.000000f, 0.000000f),
      Point2F(-0.923879f, 0.382684f),
      Point2F(-0.707107f, 0.707107f),
      Point2F(-0.382683f, 0.923880f),
      Point2F(0.000000f, 1.000000f),
      Point2F(0.382684f, 0.923879f)
   };
   const S32 numPoints = sizeof(circlePoints)/sizeof(Point2F);

   if ( mConeGeometry.isNull() )
   {
      mConeGeometry.set(GFX, numPoints + 1, GFXBufferTypeStatic);
      mConeGeometry.lock();

      mConeGeometry[0].point = Point3F(0.0f,0.0f,0.0f);

      for (S32 i=1; i<numPoints + 1; i++)
      {
         S32 imod = (i - 1) % numPoints;
         mConeGeometry[i].point = Point3F(circlePoints[imod].x,circlePoints[imod].y, -1.0f);
         mConeGeometry[i].color = ColorI::WHITE;
      }
      mConeGeometry.unlock();

      mConePrimitiveCount = numPoints * 2 - 1;

      // Now build the index buffer
      mConeIndices.set(GFX, mConePrimitiveCount * 3, mConePrimitiveCount, GFXBufferTypeStatic);

      U16 *idx = NULL;
      mConeIndices.lock( &idx );
      // Build the cone
      U32 idxIdx = 0;
      for( int i = 1; i < numPoints + 1; i++ )
      {
         idx[idxIdx++] = 0; // Triangles on cone start at top point
         idx[idxIdx++] = i;
         idx[idxIdx++] = ( i + 1 > numPoints ) ? 1 : i + 1;
      }

      // Build the bottom of the cone (reverse winding order)
      for( int i = 1; i < numPoints - 1; i++ )
      {
         idx[idxIdx++] = 1;
         idx[idxIdx++] = i + 2;
         idx[idxIdx++] = i + 1;
      }
      mConeIndices.unlock();
   }

   outNumPrimitives = mConePrimitiveCount;
   outPrimitives = mConeIndices.getPointer();
   return mConeGeometry;
}

LightShadowMap* AdvancedLightManager::findShadowMapForObject( SimObject *object )
{
   if ( !object )
   {
      MatTextureTarget::unregisterTarget( "AL_ShadowVizTexture", NULL );
      return NULL;
   }

   ISceneLight *sceneLight = dynamic_cast<ISceneLight*>( object );
   if ( !sceneLight || !sceneLight->getLight() )
      return NULL;

   return sceneLight->getLight()->getExtended<ShadowMapParams>()->getShadowMap();
}

ConsoleFunction( setShadowVizLight, const char*, 2, 2, "" )
{
   MatTextureTarget::unregisterTarget( "AL_ShadowVizTexture", NULL );

   AdvancedLightManager *lm = dynamic_cast<AdvancedLightManager*>( gClientSceneGraph->getLightManager() );
   if ( lm )
   {
      SimObject *object;
      Sim::findObject( argv[1], object );
      LightShadowMap *lightShadowMap = lm->findShadowMapForObject( object );
      if ( lightShadowMap && lightShadowMap->getTargetTexture(0) )
      {
         if ( MatTextureTarget::registerTarget( "AL_ShadowVizTexture", lightShadowMap ) )
         {
            GFXTextureObject *texObject = lightShadowMap->getTargetTexture(0);
            const Point3I &size = texObject->getSize();
            F32 aspect = (F32)size.x / (F32)size.y;

            char *result = Con::getReturnBuffer( 64 );
            dSprintf( result, 64, "%d %d %g", size.x, size.y, aspect ); 
            return result;
         }
      }
   }

   return 0;
}

