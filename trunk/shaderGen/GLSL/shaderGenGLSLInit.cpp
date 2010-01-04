//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "shaderGen/shaderGen.h"
#include "shaderGen/GLSL/shaderGenGLSL.h"
#include "shaderGen/GLSL/shaderFeatureGLSL.h"
#include "shaderGen/featureMgr.h"
#include "shaderGen/GLSL/bumpGLSL.h"
#include "shaderGen/GLSL/pixSpecularGLSL.h"
#include "shaderGen/GLSL/depthGLSL.h"
#include "shaderGen/GLSL/paraboloidGLSL.h"
#include "materials/materialFeatureTypes.h"


class ShaderGenGLSLInit
{
public:
   ShaderGenGLSLInit();
private:
   ShaderGen::ShaderGenInitDelegate mInitDelegate;

   void initShaderGen(ShaderGen* shadergen);
};

ShaderGenGLSLInit::ShaderGenGLSLInit()
{
   mInitDelegate.bind(this, &ShaderGenGLSLInit::initShaderGen);
   SHADERGEN->registerInitDelegate(OpenGL, mInitDelegate);   
}

void ShaderGenGLSLInit::initShaderGen( ShaderGen *shaderGen )
{
   shaderGen->setPrinter( new ShaderGenPrinterGLSL );
   shaderGen->setComponentFactory( new ShaderGenComponentFactoryGLSL );
   shaderGen->setFileEnding( "glsl" );

   FEATUREMGR->registerFeature( MFT_VertTransform, new VertPositionGLSL );
   FEATUREMGR->registerFeature( MFT_RTLighting, new RTLightingFeatGLSL );
   FEATUREMGR->registerFeature( MFT_IsDXTnm, new NamedFeatureGLSL( "DXTnm" ) );
   FEATUREMGR->registerFeature( MFT_TexAnim, new TexAnimGLSL );
   FEATUREMGR->registerFeature( MFT_DiffuseMap, new DiffuseMapFeatGLSL );
   FEATUREMGR->registerFeature( MFT_OverlayMap, new OverlayTexFeatGLSL );
   FEATUREMGR->registerFeature( MFT_DiffuseColor, new DiffuseFeatureGLSL );
   FEATUREMGR->registerFeature( MFT_ColorMultiply, new ColorMultiplyFeatGLSL );
   FEATUREMGR->registerFeature( MFT_AlphaTest, new AlphaTestGLSL );
   FEATUREMGR->registerFeature( MFT_GlowMask, new GlowMaskGLSL );
   FEATUREMGR->registerFeature( MFT_LightMap, new LightmapFeatGLSL );
   FEATUREMGR->registerFeature( MFT_ToneMap, new TonemapFeatGLSL );
   FEATUREMGR->registerFeature( MFT_VertLit, new VertLitGLSL );
   FEATUREMGR->registerFeature( MFT_NormalMap, new BumpFeatGLSL );
   FEATUREMGR->registerFeature( MFT_DetailMap, new DetailFeatGLSL );
   FEATUREMGR->registerFeature( MFT_CubeMap, new ReflectCubeFeatGLSL );
   FEATUREMGR->registerFeature( MFT_PixSpecular, new PixelSpecularGLSL );
   FEATUREMGR->registerFeature( MFT_SpecularMap, new SpecularMapGLSL );
   FEATUREMGR->registerFeature( MFT_GlossMap, new NamedFeatureGLSL( "Gloss Map" ) );
   FEATUREMGR->registerFeature( MFT_IsTranslucent, new NamedFeatureGLSL( "Translucent" ) );
   FEATUREMGR->registerFeature( MFT_Visibility, new VisibilityFeatGLSL );
   FEATUREMGR->registerFeature( MFT_Fog, new FogFeatGLSL );

   FEATUREMGR->registerFeature( MFT_DepthOut, new DepthOutGLSL );
   FEATUREMGR->registerFeature(MFT_EyeSpaceDepthOut, new EyeSpaceDepthOutGLSL());

   FEATUREMGR->registerFeature( MFT_ParaboloidVertTransform, new ParaboloidVertTransformGLSL );
   FEATUREMGR->registerFeature( MFT_IsSinglePassParaboloid, new NamedFeatureGLSL( "Single Pass Paraboloid" ) );
}

static ShaderGenGLSLInit p_GLSLInit;