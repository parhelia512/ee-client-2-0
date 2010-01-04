//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/advanced/advancedLightBinManager.h"

#include "lighting/advanced/advancedLightManager.h"
#include "lighting/advanced/advancedLightBufferConditioner.h"
#include "lighting/shadowMap/shadowMapManager.h"
#include "lighting/shadowMap/shadowMapPass.h"
#include "lighting/shadowMap/lightShadowMap.h"
#include "lighting/common/lightMapParams.h"
#include "renderInstance/renderPrePassMgr.h"
#include "gfx/gfxTransformSaver.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "materials/materialManager.h"
#include "materials/sceneData.h"
#include "core/util/safeDelete.h"
#include "core/util/rgb2luv.h"
#include "gfx/gfxDebugEvent.h"
#include "math/util/matrixSet.h"

const RenderInstType AdvancedLightBinManager::RIT_LightInfo( "LightInfo" );
const String AdvancedLightBinManager::smBufferName( "lightinfo" );

F32 AdvancedLightBinManager::smConstantSpecularPower = 12.0f;

ShadowFilterMode AdvancedLightBinManager::smShadowFilterMode = ShadowFilterMode_SoftShadowHighQuality;


// NOTE: The order here matches that of the LightInfo::Type enum.
const String AdvancedLightBinManager::smLightMatNames[] =
{
   "AL_PointLightMaterial",   // LightInfo::Point
   "AL_SpotLightMaterial",    // LightInfo::Spot
   "AL_VectorLightMaterial",  // LightInfo::Vector
   "",                        // LightInfo::Ambient
};

// NOTE: The order here matches that of the LightInfo::Type enum.
const GFXVertexFormat* AdvancedLightBinManager::smLightMatVertex[] =
{
   getGFXVertexFormat<AdvancedLightManager::LightVertex>(), // LightInfo::Point
   getGFXVertexFormat<AdvancedLightManager::LightVertex>(), // LightInfo::Spot
   getGFXVertexFormat<FarFrustumQuadVert>(),                // LightInfo::Vector
   NULL,                                                    // LightInfo::Ambient
};

// NOTE: The order here matches that of the ShadowType enum.
const String AdvancedLightBinManager::smShadowTypeMacro[] =
{
   "", // ShadowType_Spot
   "", // ShadowType_PSSM,
   "SHADOW_PARABOLOID",                   // ShadowType_Paraboloid,
   "SHADOW_DUALPARABOLOID_SINGLE_PASS",   // ShadowType_DualParaboloidSinglePass,
   "SHADOW_DUALPARABOLOID",               // ShadowType_DualParaboloid,
   "SHADOW_CUBE",                         // ShadowType_CubeMap,
};


IMPLEMENT_CONOBJECT(AdvancedLightBinManager);
 
AdvancedLightBinManager::AdvancedLightBinManager( AdvancedLightManager *lm /* = NULL */, 
                                                 ShadowMapManager *sm /* = NULL */, 
                                                 GFXFormat lightBufferFormat /* = GFXFormatR8G8B8A8 */ )
   :  RenderTexTargetBinManager( RIT_LightInfo, 1.0f, 1.0f, lightBufferFormat ), 
      mNumLightsCulled(0), 
      mLightManager(lm), 
      mShadowManager(sm),
      mConditioner(NULL)
{
   // Create an RGB conditioner
   mConditioner = new AdvancedLightBufferConditioner( getTargetFormat(), 
                                                      AdvancedLightBufferConditioner::RGB );

   // We want a full-resolution buffer
   mTargetSizeType = RenderTexTargetBinManager::WindowSize;

   mMRTLightmapsDuringPrePass = false;

   MatTextureTarget::registerTarget( smBufferName, this );
}

//------------------------------------------------------------------------------

AdvancedLightBinManager::~AdvancedLightBinManager()
{
   MatTextureTarget::unregisterTarget( smBufferName, this );

   deleteLightMaterials();

   SAFE_DELETE(mConditioner);
}

RenderBinManager::AddInstResult AdvancedLightBinManager::addElement(RenderInst *)
{
   return RenderBinManager::arSkipped;
}

bool AdvancedLightBinManager::setTargetSize(const Point2I &newTargetSize)
{
   bool ret = Parent::setTargetSize( newTargetSize );

   // We require the viewport to match the default.
   mTargetViewport = GFX->getViewport();

   return ret;
}

ConditionerFeature *AdvancedLightBinManager::getTargetConditioner() const
{
   return const_cast<AdvancedLightBinManager *>(this)->mConditioner;
}

RenderBinManager::AddInstResult AdvancedLightBinManager::addLight( LightInfo *light )
{
   // Get the light type.
   const LightInfo::Type lightType = light->getType();

   AssertFatal(   lightType == LightInfo::Point || 
                  lightType == LightInfo::Spot, "Bogus light type." );

   // Find a shadow map for this light, if it has one
   LightShadowMap *lsm = light->getExtended<ShadowMapParams>()->getShadowMap();

   // Get the right shadow type.
   ShadowType shadowType = ShadowType_None;
   if (  light->getCastShadows() && 
         lsm && lsm->hasShadowTex() &&
         !ShadowMapPass::smDisableShadows )
      shadowType = lsm->getShadowType();

   // Add the entry
   LightBinEntry lEntry;
   lEntry.lightInfo = light;
   lEntry.shadowMap = lsm;
   lEntry.lightMaterial = _getLightMaterial( lightType, shadowType );

   if( lightType == LightInfo::Spot )
      lEntry.vertBuffer = mLightManager->getConeMesh( lEntry.numPrims, lEntry.primBuffer );
   else
      lEntry.vertBuffer = mLightManager->getSphereMesh( lEntry.numPrims, lEntry.primBuffer );

   // If it's a point light, push front, spot 
   // light, push back. This helps batches.
   Vector<LightBinEntry> &curBin = mLightBin;
   if ( light->getType() == LightInfo::Point )
      curBin.push_front( lEntry );
   else
      curBin.push_back( lEntry );

   return arAdded;
}

void AdvancedLightBinManager::clear()
{
#define BIN_DEBUG_SPEW
#ifdef BIN_DEBUG_SPEW
   bool bSpewBinInfo = Con::getBoolVariable( "$spewBins" );

   if( bSpewBinInfo )
   {
      Con::printf( "Advanced Light Bin Spew:" );
      Con::printf( "%d lights.", mLightBin.size() );
      Con::printf( "%d lights culled.", mNumLightsCulled );

      Con::setBoolVariable( "$spewBins", false );
   }
#endif

   mLightBin.clear();
   mNumLightsCulled = 0;
}

void AdvancedLightBinManager::render(SceneState * state)
{
   PROFILE_SCOPE( AdvancedLightManager_Render );

   // Automagically save & restore our viewport and transforms.
   GFXTransformSaver saver;

   if( !mLightManager )
      return;

   // Get the sunlight. If there's no sun, and no lights in the bins, no draw
   LightInfo *sunLight = mLightManager->getSpecialLight( LightManager::slSunLightType );
   if( !sunLight && mLightBin.empty() )
      return;

   GFXDEBUGEVENT_SCOPE( AdvancedLightBinManager_Render, ColorI::RED );

   // Tell the superclass we're about to render
   if ( !_onPreRender( state ) )
      return;

   // Clear as long as there isn't MRT population of light buffer with lightmap data
   if ( !MRTLightmapsDuringPrePass() )
      GFX->clear(GFXClearTarget, ColorI(0, 0, 0, 0), 1.0f, 0);

   // Restore transforms
   MatrixSet &matrixSet = getParentManager()->getMatrixSet();
   matrixSet.restoreSceneViewProjection();

   const MatrixF &worldToCameraXfm = matrixSet.getWorldToCamera();

   // Set up the SG Data
   SceneGraphData sgData;

   // There are cases where shadow rendering is disabled.
   const bool disableShadows = state->isReflectPass() || ShadowMapPass::smDisableShadows;

   // Pick the right material for rendering the sunlight... we only
   // cast shadows when its enabled and we're not in a reflection.
   LightMaterialInfo *vectorMatInfo;
   if (  sunLight &&
         sunLight->getCastShadows() &&
         !disableShadows &&
         sunLight->getExtended<ShadowMapParams>() )
      vectorMatInfo = _getLightMaterial( LightInfo::Vector, ShadowType_PSSM );
   else
      vectorMatInfo = _getLightMaterial( LightInfo::Vector, ShadowType_None );

   // Initialize and set the per-frame parameters after getting
   // the vector light material as we use lazy creation.
   _setupPerFrameParameters( state->getCameraPosition() );
   
   // Draw sunlight/ambient
   if ( sunLight && vectorMatInfo )
   {
      GFXDEBUGEVENT_SCOPE( AdvancedLightBinManager_Render_Sunlight, ColorI::RED );

      // Set up SG data
      setupSGData( sgData, sunLight );
      vectorMatInfo->setLightParameters( sunLight, worldToCameraXfm );

      // Set light holds the active shadow map.       
      mShadowManager->setLightShadowMapForLight( sunLight );

      // Set geometry
      GFX->setVertexBuffer( mFarFrustumQuadVerts );
      GFX->setPrimitiveBuffer( NULL );

      // Render the material passes
      while( vectorMatInfo->matInstance->setupPass( state, sgData ) )
      {
         vectorMatInfo->matInstance->setSceneInfo( state, sgData );
         vectorMatInfo->matInstance->setTransforms( matrixSet, state );
         GFX->drawPrimitive( GFXTriangleFan, 0, 2 );
      }
   }

   // Blend the lights in the bin to the light buffer
   for( LightBinIterator itr = mLightBin.begin(); itr != mLightBin.end(); itr++ )
   {
      LightBinEntry& curEntry = *itr;
      LightInfo *curLightInfo = curEntry.lightInfo;
      LightMaterialInfo *curLightMat = curEntry.lightMaterial;
      const U32 numPrims = curEntry.numPrims;
      const U32 numVerts = curEntry.vertBuffer->mNumVerts;

      // Skip lights which won't affect the scene.
      if ( !curLightMat || curLightInfo->getBrightness() <= 0.001f )
         continue;

      GFXDEBUGEVENT_SCOPE( AdvancedLightBinManager_Render_Light, ColorI::RED );

      setupSGData( sgData, curLightInfo );
      curLightMat->setLightParameters( curLightInfo, worldToCameraXfm );
      mShadowManager->setLightShadowMap( curEntry.shadowMap );

      // Let the shadow know we're about to render from it.
      if ( curEntry.shadowMap )
         curEntry.shadowMap->preLightRender();

      // Set geometry
      GFX->setVertexBuffer( curEntry.vertBuffer );
      GFX->setPrimitiveBuffer( curEntry.primBuffer );

      // Render the material passes
      while( curLightMat->matInstance->setupPass( state, sgData ) )
      {
         // Set transforms
         matrixSet.setWorld(sgData.objTrans);
         curLightMat->matInstance->setTransforms(matrixSet, state);
         curLightMat->matInstance->setSceneInfo(state, sgData);

         if(curEntry.primBuffer)
            GFX->drawIndexedPrimitive(GFXTriangleList, 0, 0, numVerts, 0, numPrims);
         else
            GFX->drawPrimitive(GFXTriangleList, 0, numPrims);
      }

      // Tell it we're done rendering.
      if ( curEntry.shadowMap )
         curEntry.shadowMap->postLightRender();
   }

   // Set NULL for active shadow map (so nothing gets confused)
   mShadowManager->setLightShadowMap(NULL);
   GFX->setVertexBuffer( NULL );
   GFX->setPrimitiveBuffer( NULL );

   // Finish up the rendering
   _onPostRender();
}

AdvancedLightBinManager::LightMaterialInfo* AdvancedLightBinManager::_getLightMaterial( LightInfo::Type lightType, ShadowType shadowType )
{
   PROFILE_SCOPE( AdvancedLightBinManager_GetLightMaterial );

   // Build the key.
   const LightMatKey key( lightType, shadowType );

   // See if we've already built this one.
   LightMatTable::Iterator iter = mLightMaterials.find( key );
   if ( iter != mLightMaterials.end() )
      return iter->value;
   
   // If we got here we need to build a material for 
   // this light+shadow combination.

   LightMaterialInfo *info = NULL;

   // First get the light material name and make sure
   // this light has a material in the first place.
   const String &lightMatName = smLightMatNames[ lightType ];
   if ( lightMatName.isNotEmpty() )
   {
      Vector<GFXShaderMacro> shadowMacros;

      // Setup the shadow type macros for this material.
      if ( shadowType == ShadowType_None )
         shadowMacros.push_back( GFXShaderMacro( "NO_SHADOW" ) );
      else
      {
         shadowMacros.push_back( GFXShaderMacro( smShadowTypeMacro[ shadowType ] ) );

         // Do we need to do shadow filtering?
         if ( smShadowFilterMode != ShadowFilterMode_None )
         {
            shadowMacros.push_back( GFXShaderMacro( "SOFTSHADOW" ) );
           
            const F32 SM = GFX->getPixelShaderVersion();
            if ( SM >= 3.0f && smShadowFilterMode == ShadowFilterMode_SoftShadowHighQuality )
               shadowMacros.push_back( GFXShaderMacro( "SOFTSHADOW_HIGH_QUALITY" ) );
         }
      }
   
      // Now create the material info object.
      info = new LightMaterialInfo( lightMatName, smLightMatVertex[ lightType ], shadowMacros );
   }

   // Push this into the map and return it.
   mLightMaterials.insertUnique( key, info );
   return info;
}

void AdvancedLightBinManager::deleteLightMaterials()
{
   LightMatTable::Iterator iter = mLightMaterials.begin();
   for ( ; iter != mLightMaterials.end(); iter++ )
      delete iter->value;
      
   mLightMaterials.clear();
}

void AdvancedLightBinManager::_setupPerFrameParameters( const Point3F &eyePos )
{
   PROFILE_SCOPE( AdvancedLightBinManager_SetupPerFrameParameters );

   F32 frustLeft, frustRight, frustBottom, frustTop, zNear, zFar;
   bool isOrtho;
   GFX->getFrustum( &frustLeft, &frustRight, &frustBottom, &frustTop, &zNear, &zFar, &isOrtho );

   const F32 zFarOverNear = zFar / zNear;

   MatrixF camMat(GFX->getWorldMatrix());
   camMat.inverse();

   mFrustum.set( isOrtho, frustLeft, frustRight, frustTop, frustBottom, zNear, zFar, camMat);
   mViewSpaceFrustum.set( isOrtho, frustLeft, frustRight, frustTop, frustBottom, zNear, zFar, MatrixF::Identity );

   const Point3F* wsFrutumPoints = mFrustum.getPoints();
   const Point3F* vsFrustumPoints = mViewSpaceFrustum.getPoints();

   PlaneF farPlane(wsFrutumPoints[Frustum::FarBottomLeft], wsFrutumPoints[Frustum::FarTopLeft], wsFrutumPoints[Frustum::FarTopRight]);
   PlaneF vsFarPlane(vsFrustumPoints[Frustum::FarBottomLeft], vsFrustumPoints[Frustum::FarTopLeft], vsFrustumPoints[Frustum::FarTopRight]);

   // Parameters calculated, assign them to the materials
   LightMatTable::Iterator iter = mLightMaterials.begin();
   for ( ; iter != mLightMaterials.end(); iter++ )
   {
      if ( iter->value )
         iter->value->setViewParameters(zNear, zFar, eyePos, farPlane, vsFarPlane);
   }

   // Now build the quad for drawing full-screen vector light
   // passes.... this is a volitile VB and updates every frame.

   //static const U32 sRemap[] = { 0, 1, 2, 3 }; // For normal culling
   static const U32 sRemap[] = { 0, 3, 2, 1 }; // For reverse culling

   mFarFrustumQuadVerts.set( GFX, 4 );
   FarFrustumQuadVert *verts = mFarFrustumQuadVerts.lock();

      verts[sRemap[0]].point.set( wsFrutumPoints[Frustum::FarBottomLeft] - eyePos );
      verts[sRemap[0]].normal.set( frustLeft * zFarOverNear, zFar, frustBottom * zFarOverNear );
      verts[sRemap[0]].texCoord.set( 0.0f, 0.0f );

      verts[sRemap[1]].point.set( wsFrutumPoints[Frustum::FarTopLeft] - eyePos );
      verts[sRemap[1]].normal.set( frustLeft * zFarOverNear, zFar, frustTop * zFarOverNear );
      verts[sRemap[1]].texCoord.set( 0.0f, 1.0f );

      verts[sRemap[2]].point.set( wsFrutumPoints[Frustum::FarTopRight] - eyePos );
      verts[sRemap[2]].normal.set( frustRight * zFarOverNear, zFar, frustTop * zFarOverNear );
      verts[sRemap[2]].texCoord.set( 1.0f, 1.0f );

      verts[sRemap[3]].point.set( wsFrutumPoints[Frustum::FarBottomRight] - eyePos );
      verts[sRemap[3]].normal.set( frustRight * zFarOverNear, zFar, frustBottom * zFarOverNear);
      verts[sRemap[3]].texCoord.set( 1.0f, 0.0f );

   mFarFrustumQuadVerts.unlock();
}

void AdvancedLightBinManager::setupSGData( SceneGraphData &data, LightInfo *light )
{
   data.reset();
   data.setFogParams(mParentManager->getSceneManager()->getFogData());

   data.lights[0] = light;   

   if ( light )
   {
      if ( light->getType() == LightInfo::Point )
      {
         data.objTrans = light->getTransform();

         // The point light volume gets some flat spots along
         // the perimiter mostly visible in the constant and 
         // quadradic falloff modes.
         //
         // To account for them slightly increase the scale 
         // instead of greatly increasing the polycount.

         data.objTrans.scale( light->getRange() * 1.01f );
      }
      else if ( light->getType() == LightInfo::Spot )
      {
         data.objTrans = light->getTransform();

         // Rotate it to face down the -y axis.
         MatrixF scaleRotateTranslate( EulerF( M_PI_F / -2.0f, 0.0f, 0.0f ) );

         // Calculate the radius based on the range and angle.
         F32 range = light->getRange().x;
         F32 radius = range * mSin( mDegToRad( light->getOuterConeAngle() ) * 0.5f );

         // NOTE: This fudge makes the cone a little bigger
         // to remove the facet egde of the cone geometry.
         radius *= 1.1f;

         // Use the scale to distort the cone to
         // match our radius and range.
         scaleRotateTranslate.scale( Point3F( radius, radius, range ) );

         // Apply the transform and set the position.
         data.objTrans *= scaleRotateTranslate;
         data.objTrans.setPosition( light->getPosition() );
      }
      else
         data.objTrans.identity();
   }
   else
      data.objTrans.identity();
}

void AdvancedLightBinManager::MRTLightmapsDuringPrePass( bool val )
{
   // Do not enable if the GFX device can't do MRT's
   if(GFX->getNumRenderTargets() < 2)
      val = false;

   if(mMRTLightmapsDuringPrePass != val)
   {
      mMRTLightmapsDuringPrePass = val;

      // Reload materials to cause a feature recalculation on prepass materials
      if(mLightManager->isActive())
         MATMGR->flushAndReInitInstances();

      // Force the pre-pass manager to update its targets
      MatTextureTarget *texTarget = MatTextureTarget::findTargetByName(RenderPrePassMgr::BufferName);
      if(texTarget != NULL)
      {
         AssertFatal(dynamic_cast<RenderPrePassMgr *>(texTarget), "Bad buffer type!");
         RenderPrePassMgr *prepass = static_cast<RenderPrePassMgr *>(texTarget);
         prepass->updateTargets();
      }
   }
}

AdvancedLightBinManager::LightMaterialInfo::LightMaterialInfo( const String &matName,
                                                               const GFXVertexFormat *vertexFormat,
                                                               const Vector<GFXShaderMacro> &macros )
:  matInstance(NULL),
   zNearFarInvNearFar(NULL),
   farPlane(NULL), 
   vsFarPlane(NULL),
   negFarPlaneDotEye(NULL),
   lightPosition(NULL), 
   lightDirection(NULL), 
   lightColor(NULL), 
   lightAttenuation(NULL),
   lightRange(NULL), 
   lightAmbient(NULL), 
   lightTrilight(NULL), 
   lightSpotParams(NULL)
{   
   Material *mat = MATMGR->getMaterialDefinitionByName( matName );
   if ( !mat )
      return;

   matInstance = new LightMatInstance( *mat );

   for ( U32 i=0; i < macros.size(); i++ )
      matInstance->addShaderMacro( macros[i].name, macros[i].value );

   matInstance->init( MATMGR->getDefaultFeatures(), vertexFormat );

   lightDirection = matInstance->getMaterialParameterHandle("$lightDirection");
   lightAmbient = matInstance->getMaterialParameterHandle("$lightAmbient");
   lightTrilight = matInstance->getMaterialParameterHandle("$lightTrilight");
   lightSpotParams = matInstance->getMaterialParameterHandle("$lightSpotParams");
   lightAttenuation = matInstance->getMaterialParameterHandle("$lightAttenuation");
   lightRange = matInstance->getMaterialParameterHandle("$lightRange");
   lightPosition = matInstance->getMaterialParameterHandle("$lightPosition");
   farPlane = matInstance->getMaterialParameterHandle("$farPlane");
   vsFarPlane = matInstance->getMaterialParameterHandle("$vsFarPlane");
   negFarPlaneDotEye = matInstance->getMaterialParameterHandle("$negFarPlaneDotEye");
   zNearFarInvNearFar = matInstance->getMaterialParameterHandle("$zNearFarInvNearFar");
   lightColor = matInstance->getMaterialParameterHandle("$lightColor");
   lightBrightness = matInstance->getMaterialParameterHandle("$lightBrightness");
   constantSpecularPower = matInstance->getMaterialParameterHandle(AdvancedLightManager::ConstantSpecularPowerSC);
}

AdvancedLightBinManager::LightMaterialInfo::~LightMaterialInfo()
{
   SAFE_DELETE(matInstance);
}

void AdvancedLightBinManager::LightMaterialInfo::setViewParameters(  const F32 _zNear, 
                                                                     const F32 _zFar, 
                                                                     const Point3F &_eyePos, 
                                                                     const PlaneF &_farPlane,
                                                                     const PlaneF &_vsFarPlane)
{
   MaterialParameters *matParams = matInstance->getMaterialParameters();

   if ( farPlane->isValid() )
      matParams->set( farPlane, *((const Point4F *)&_farPlane) );

   if ( vsFarPlane->isValid() )
      matParams->set( vsFarPlane, *((const Point4F *)&_vsFarPlane) );

   if ( negFarPlaneDotEye->isValid() )
   {
      // -dot( farPlane, eyePos )
      const F32 negFarPlaneDotEyeVal = -( mDot( *((const Point3F *)&_farPlane), _eyePos ) + _farPlane.d );
      matParams->set( negFarPlaneDotEye, negFarPlaneDotEyeVal );
   }

   if ( zNearFarInvNearFar->isValid() )
      matParams->set( zNearFarInvNearFar, Point4F( _zNear, _zFar, 1.0f / _zNear, 1.0f / _zFar ) );
}

void AdvancedLightBinManager::LightMaterialInfo::setLightParameters( const LightInfo *lightInfo, const MatrixF &worldViewOnly )
{
   MaterialParameters *matParams = matInstance->getMaterialParameters();

   // Set color in the right format, set alpha to the luminance value for the color.
   ColorF col = lightInfo->getColor();

   // TODO: The specularity control of the light
   // is being scaled by the overall lumiance.
   //
   // Not sure if this may be the source of our
   // bad specularity results maybe?
   //

   const Point3F colorToLumiance( 0.3576f, 0.7152f, 0.1192f );
   F32 lumiance = mDot(*((const Point3F *)&lightInfo->getColor()), colorToLumiance );
   col.alpha *= lumiance;

   matParams->set( lightColor, col );
   matParams->set( lightBrightness, lightInfo->getBrightness() );
   matParams->set( constantSpecularPower, AdvancedLightBinManager::getConstantSpecularPower() );

   switch( lightInfo->getType() )
   {
   case LightInfo::Vector:
      {
         VectorF lightDir = lightInfo->getDirection();
         worldViewOnly.mulV(lightDir);
         lightDir.normalize();
         matParams->set( lightDirection, lightDir );

         // Set small number for alpha since it represents existing specular in
         // the vector light. This prevents a divide by zero.
         ColorF ambientColor = lightInfo->getAmbient();
         ambientColor.alpha = 0.00001f;
         matParams->set( lightAmbient, ambientColor );

         // If no alt color is specified, set it to the average of
         // the ambient and main color to avoid artifacts.
         //
         // TODO: Trilight disabled until we properly implement it
         // in the light info!
         //
         //ColorF lightAlt = lightInfo->getAltColor();
         ColorF lightAlt( ColorF::BLACK ); // = lightInfo->getAltColor();
         if ( lightAlt.red == 0.0f && lightAlt.green == 0.0f && lightAlt.blue == 0.0f )
            lightAlt = (lightInfo->getColor() + lightInfo->getAmbient()) / 2.0f;

         ColorF trilightColor = lightAlt;
         matParams->set(lightTrilight, trilightColor);
      }
      break;

   case LightInfo::Spot:
      {
         const F32 outerCone = lightInfo->getOuterConeAngle();
         const F32 innerCone = getMin( lightInfo->getInnerConeAngle(), outerCone );
         const F32 outerCos = mCos( mDegToRad( outerCone / 2.0f ) );
         const F32 innerCos = mCos( mDegToRad( innerCone / 2.0f ) );
         Point4F spotParams(  outerCos, 
                              innerCos - outerCos, 
                              mCos( mDegToRad( outerCone ) ), 
                              0.0f );

         matParams->set( lightSpotParams, spotParams );

         VectorF lightDir = lightInfo->getDirection();
         worldViewOnly.mulV(lightDir);
         lightDir.normalize();
         matParams->set( lightDirection, lightDir );
      }
      // Fall through

   case LightInfo::Point:
   {
      const F32 radius = lightInfo->getRange().x;
      matParams->set( lightRange, radius );

      Point3F lightPos;
      worldViewOnly.mulP(lightInfo->getPosition(), &lightPos);
      matParams->set( lightPosition, lightPos );

      // Get the attenuation falloff ratio and normalize it.
      Point3F attenRatio = lightInfo->getExtended<ShadowMapParams>()->attenuationRatio;
      F32 total = attenRatio.x + attenRatio.y + attenRatio.z;
      if ( total > 0.0f )
         attenRatio /= total;

      Point2F attenParams( ( 1.0f / radius ) * attenRatio.y,
                           ( 1.0f / ( radius * radius ) ) * attenRatio.z );

      matParams->set( lightAttenuation, attenParams );
      break;
   }

   default:
      AssertFatal( false, "Bad light type!" );
      break;
   }
}

bool LightMatInstance::setupPass( SceneState *state, const SceneGraphData &sgData )
{
   // Go no further if the material failed to initialize properly.
   if (  !mProcessedMaterial || 
         mProcessedMaterial->getNumPasses() == 0 )
      return false;

   // Fetch the lightmap params
   const LightMapParams *lmParams = sgData.lights[0]->getExtended<LightMapParams>();
   
   // If no Lightmap params, let parent handle it
   if(lmParams == NULL)
      return Parent::setupPass(state, sgData);

   // Defaults
   bool bRetVal = true;

   // What render pass is this...
   if(mCurPass == -1)
   {
      // First pass, reset this flag
      mInternalPass = false;

      // Pass call to parent
      bRetVal = Parent::setupPass(state, sgData);
   }
   else
   {
      // If this light is represented in a lightmap, it has already done it's 
      // job for non-lightmapped geometry. Now render the lightmapped geometry
      // pass (specular + shadow-darkening)
      if(!mInternalPass && lmParams->representedInLightmap)
         mInternalPass = true;
      else
         return Parent::setupPass(state, sgData);
   }

   // Set up the shader constants we need to...
   if(mLightMapParamsSC->isValid())
   {
      // If this is an internal pass, special case the parameters
      if(mInternalPass)
      {
         AssertFatal( lmParams->shadowDarkenColor.alpha == -1.0f, "Assumption failed, check unpack code!" );
         getMaterialParameters()->set( mLightMapParamsSC, lmParams->shadowDarkenColor );
      }
      else
         getMaterialParameters()->set( mLightMapParamsSC, ColorF::WHITE );
   }

   // Now override stateblock with our own
   if(!mInternalPass)
   {
      // If this is not an internal pass, and this light is represented in lightmaps
      // than only effect non-lightmapped geometry for this pass
      if(lmParams->representedInLightmap)
         GFX->setStateBlock(mLitState[StaticLightNonLMGeometry]);
      else // This is a normal, dynamic light.
         GFX->setStateBlock(mLitState[DynamicLight]);
      
   }
   else // Internal pass, this is the add-specular/multiply-darken-color pass
      GFX->setStateBlock(mLitState[StaticLightLMGeometry]);

   return bRetVal;
}

bool LightMatInstance::init( const FeatureSet &features, const GFXVertexFormat *vertexFormat )
{
   bool success = Parent::init(features, vertexFormat);
   
   // If the initialization failed don't continue.
   if ( !success || !mProcessedMaterial || mProcessedMaterial->getNumPasses() == 0 )   
      return false;

   mLightMapParamsSC = getMaterialParameterHandle("$lightMapParams");

   // Grab the state block for the first render pass (since this mat instance
   // inserts a pass after the first pass)
   AssertFatal(mProcessedMaterial->getNumPasses() > 0, "No passes created! Ohnoes");
   const RenderPassData *rpd = mProcessedMaterial->getPass(0);
   AssertFatal(rpd, "No render pass data!");
   AssertFatal(rpd->mRenderStates[0], "No render state 0!");
   
   // Get state block desc for normal (not wireframe, not translucent, not glow, etc)
   // render state
   GFXStateBlockDesc litState = rpd->mRenderStates[0]->getDesc();

   // Create state blocks for each of the 3 possible combos in setupPass

   //DynamicLight State: This will effect lightmapped and non-lightmapped geometry
   // in the same way.
   litState.separateAlphaBlendDefined = true;
   litState.separateAlphaBlendEnable = false;
   litState.stencilMask = RenderPrePassMgr::OpaqueDynamicLitMask | RenderPrePassMgr::OpaqueStaticLitMask;
   mLitState[DynamicLight] = GFX->createStateBlock(litState);

   // StaticLightNonLMGeometry State: This will treat non-lightmapped geometry
   // in the usual way, but will not effect lightmapped geometry.
   litState.separateAlphaBlendDefined = true;
   litState.separateAlphaBlendEnable = false;
   litState.stencilMask = RenderPrePassMgr::OpaqueDynamicLitMask;
   mLitState[StaticLightNonLMGeometry] = GFX->createStateBlock(litState);

   // StaticLightLMGeometry State: This will add specular information (alpha) but
   // multiply-darken color information. 
   litState.blendDest = GFXBlendSrcColor;
   litState.blendSrc = GFXBlendZero;
   litState.stencilMask = RenderPrePassMgr::OpaqueStaticLitMask;
   litState.separateAlphaBlendDefined = true;
   litState.separateAlphaBlendEnable = true;
   litState.separateAlphaBlendSrc = GFXBlendOne;
   litState.separateAlphaBlendDest = GFXBlendOne;
   litState.separateAlphaBlendOp = GFXBlendOpAdd;
   mLitState[StaticLightLMGeometry] = GFX->createStateBlock(litState);

   return true;
}

ConsoleFunction( setShadowFilterMode, void, 2, 2, 
   "Sets the shadow filtering mode as None, SoftShadow, or SoftShadowHighQuality" )
{   
   ShadowFilterMode mode = ShadowFilterMode_None;
   if ( dStricmp( argv[1], "SoftShadow" ) == 0 )
      mode = ShadowFilterMode_SoftShadow;
   else if ( dStricmp( argv[1], "SoftShadowHighQuality" ) == 0 )
      mode = ShadowFilterMode_SoftShadowHighQuality;

   if ( mode != AdvancedLightBinManager::smShadowFilterMode )
   {
      AdvancedLightBinManager::smShadowFilterMode = mode;

      AdvancedLightBinManager *lightBin = NULL;
      if ( Sim::findObject( "AL_LightBinMgr", lightBin ) && lightBin )
         lightBin->deleteLightMaterials();
   }
}
