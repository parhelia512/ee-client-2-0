//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/shadowMatHook.h"

#include "materials/materialManager.h"
#include "materials/customMaterialDefinition.h"
#include "materials/materialFeatureTypes.h"

#include "sceneGraph/sceneState.h"

const MatInstanceHookType ShadowMaterialHook::Type( "ShadowMap" );

ShadowMaterialHook::ShadowMaterialHook()
{
   dMemset( mShadowMat, 0, sizeof( mShadowMat ) );
}

ShadowMaterialHook::~ShadowMaterialHook()
{
   for ( U32 i = 0; i < ShadowType_Count; i++ )
      SAFE_DELETE( mShadowMat[i] );
}

void ShadowMaterialHook::init( BaseMatInstance *inMat )
{
   // Tweak the feature data to include just what we need.
   FeatureSet features;
   features.addFeature( MFT_VertTransform );
   features.addFeature( MFT_DiffuseMap );
   features.addFeature( MFT_TexAnim );
   features.addFeature( MFT_AlphaTest );

   Material *shadowMat = (Material*)inMat->getMaterial();
   if ( dynamic_cast<CustomMaterial*>( shadowMat ) )
   {
      // This is a custom material... who knows what it really does, but
      // if it wasn't already filtered out of the shadow render then just
      // give it some default depth out material.
      shadowMat = MATMGR->getMaterialDefinitionByName( "AL_DefaultShadowMaterial" );
   }

   // By default we want to disable some states
   // that the material might enable for us.
   GFXStateBlockDesc forced;
   forced.setBlend( false );
   forced.setAlphaTest( false );

   // We should force on zwrite as the prepass
   // will disable it by default.
   forced.setZReadWrite( true, true );
   
   // TODO: Should we render backfaces for 
   // shadows or does the ESM take care of 
   // all our acne issues?
   //forced.setCullMode( GFXCullCW );

   // Vector, and spotlights use the same shadow material.
   BaseMatInstance *newMat = new ShadowMatInstance( shadowMat );
   newMat->getFeaturesDelegate().bind( &ShadowMaterialHook::_overrideFeatures );
   newMat->addStateBlockDesc( forced );
   newMat->init( features, inMat->getVertexFormat() );
   mShadowMat[ShadowType_Spot] = newMat;

   newMat = new ShadowMatInstance( shadowMat );
   newMat->getFeaturesDelegate().bind( &ShadowMaterialHook::_overrideFeatures );
   forced.setCullMode( GFXCullCW );   
   newMat->addStateBlockDesc( forced );
   forced.cullDefined = false;
   newMat->addShaderMacro( "CUBE_SHADOW_MAP", "" );
   newMat->init( features, inMat->getVertexFormat() );
   mShadowMat[ShadowType_CubeMap] = newMat;
   
   // A dual paraboloid shadow rendered in a single draw call.
   features.addFeature( MFT_ParaboloidVertTransform );
   features.addFeature( MFT_IsSinglePassParaboloid );
   features.removeFeature( MFT_VertTransform );
   newMat = new ShadowMatInstance( shadowMat );
   GFXStateBlockDesc noCull( forced );
   noCull.setCullMode( GFXCullNone );
   newMat->addStateBlockDesc( noCull );
   newMat->getFeaturesDelegate().bind( &ShadowMaterialHook::_overrideFeatures );
   newMat->init( features, inMat->getVertexFormat() );
   mShadowMat[ShadowType_DualParaboloidSinglePass] = newMat;

   // Regular dual paraboloid shadow.
   features.addFeature( MFT_ParaboloidVertTransform );
   features.removeFeature( MFT_IsSinglePassParaboloid );
   features.removeFeature( MFT_VertTransform );
   newMat = new ShadowMatInstance( shadowMat );
   newMat->addStateBlockDesc( forced );
   newMat->getFeaturesDelegate().bind( &ShadowMaterialHook::_overrideFeatures );
   newMat->init( features, inMat->getVertexFormat() );
   mShadowMat[ShadowType_DualParaboloid] = newMat;

   /*
   // A single paraboloid shadow.
   newMat = new ShadowMatInstance( startMatInstance );
   GFXStateBlockDesc noCull;
   noCull.setCullMode( GFXCullNone );
   newMat->addStateBlockDesc( noCull );
   newMat->getFeaturesDelegate().bind( &ShadowMaterialHook::_overrideFeatures );
   newMat->init( features, globalFeatures, inMat->getVertexFormat() );
   mShadowMat[ShadowType_DualParaboloidSinglePass] = newMat;
   */
}

BaseMatInstance* ShadowMaterialHook::getShadowMat( ShadowType type ) const
{ 
   AssertFatal( type < ShadowType_Count, "ShadowMaterialHook::getShadowMat() - Bad light type!" );

   // The cubemap and pssm shadows use the same
   // spotlight material for shadows.
   if (  type == ShadowType_Spot ||         
         type == ShadowType_PSSM )
      return mShadowMat[ShadowType_Spot];   

   // Get the specialized shadow material.
   return mShadowMat[type]; 
}

void ShadowMaterialHook::_overrideFeatures(  ProcessedMaterial *mat,
                                             U32 stageNum,
                                             MaterialFeatureData &fd, 
                                             const FeatureSet &features )
{
   if ( stageNum != 0 )
   {
      fd.features.clear();
      return;
   }

   // Disable the base texture if we don't 
   // have alpha test enabled.
   if ( !fd.features[ MFT_AlphaTest ] )
   {
      fd.features.removeFeature( MFT_TexAnim );
      fd.features.removeFeature( MFT_DiffuseMap );
   }

   // HACK: Need to figure out how to enable these 
   // suckers without this override call!

   fd.features.setFeature( MFT_ParaboloidVertTransform, 
      features.hasFeature( MFT_ParaboloidVertTransform ) );
   fd.features.setFeature( MFT_IsSinglePassParaboloid, 
      features.hasFeature( MFT_IsSinglePassParaboloid ) );
      
   // The paraboloid transform outputs linear depth, so
   // it needs to use the plain depth out feature.
   if ( fd.features.hasFeature( MFT_ParaboloidVertTransform ) ) 
      fd.features.addFeature( MFT_DepthOut );      
   else
      fd.features.addFeature( MFT_EyeSpaceDepthOut );
}

ShadowMatInstance::ShadowMatInstance( Material *mat ) 
 : MatInstance( *mat )
{
   mLightmappedMaterial = mMaterial->isLightmapped();
}

bool ShadowMatInstance::setupPass( SceneState *state, const SceneGraphData &sgData )
{
   // Respect SceneState render flags
   if( (mLightmappedMaterial && !state->mRenderLightmappedMeshes) ||
       (!mLightmappedMaterial && !state->mRenderNonLightmappedMeshes) )
      return false;

   return Parent::setupPass(state, sgData);
}
