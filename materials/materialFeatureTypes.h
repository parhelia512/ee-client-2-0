//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MATERIALFEATURETYPES_H_
#define _MATERIALFEATURETYPES_H_

#ifndef _FEATURETYPE_H_
#include "shaderGen/featureType.h"
#endif


///
enum MaterialFeatureGroup
{
   /// One or more pre-transform features are 
   /// allowed at any one time and are executed
   /// in order to each other.
   MFG_PreTransform,

   /// Only one transform feature is allowed at
   /// any one time.
   MFG_Transform,

   /// 
   MFG_PostTransform,

   /// The features that need to occur before texturing
   /// takes place.  Usually these are features that will
   /// manipulate or generate texture coords.
   MFG_PreTexture,

   /// The different diffuse color features including
   /// textures and colors.
   MFG_Texture,

   /// 
   MFG_PreLighting,

   /// 
   MFG_Lighting,

   /// 
   MFG_PostLighting,

   /// Final features like fogging.
   MFG_PostProcess,

   /// Miscellaneous features that require no specialized 
   /// ShaderFeature object and are just queried as flags.
   MFG_Misc = -1,
};


/// The standard vertex transform.
DeclareFeatureType( MFT_VertTransform );

/// A special transform with paraboloid warp used 
/// in shadow and reflection rendering.
DeclareFeatureType( MFT_ParaboloidVertTransform );

/// This feature is queried from the MFT_ParaboloidVertTransform 
/// feature to detect if it needs to generate a single pass.
DeclareFeatureType( MFT_IsSinglePassParaboloid );

/// This feature does normal map decompression for DXT1/5.
DeclareFeatureType( MFT_IsDXTnm );

DeclareFeatureType( MFT_TexAnim );
DeclareFeatureType( MFT_Parallax );

DeclareFeatureType( MFT_DiffuseMap );
DeclareFeatureType( MFT_OverlayMap );
DeclareFeatureType( MFT_DetailMap );
DeclareFeatureType( MFT_DiffuseColor );
DeclareFeatureType( MFT_ColorMultiply );

/// This feature is used to do alpha test clipping in
/// the shader which can be faster on SM3 and is needed
/// when the render state alpha test is not available.
DeclareFeatureType( MFT_AlphaTest );

/// Stock Basic Lighting features.
//DeclareFeatureType( MFT_DynamicLight );
//DeclareFeatureType( MFT_DynamicLightDual );
//DeclareFeatureType( MFT_DynamicLightAttenuateBackFace );
/*
DeclareFeatureType( MFT_DynamicLightMask );
DeclareFeatureType( MFT_CustomShadowCoord );
DeclareFeatureType( MFT_CustomShadowCoord2 );
DeclareFeatureType( MFT_CustomShadowCompare );
DeclareFeatureType( MFT_CustomShadowCompare2 );
DeclareFeatureType( MFT_CustomShadow );
DeclareFeatureType( MFT_CustomShadow2 );
*/

DeclareFeatureType( MFT_NormalMap );
DeclareFeatureType( MFT_RTLighting );
/*
DeclareFeatureType( MFT_CustomLighting );
DeclareFeatureType( MFT_CustomLighting2 );
*/

DeclareFeatureType( MFT_IsEmissive );
DeclareFeatureType( MFT_SubSurface );
DeclareFeatureType( MFT_LightMap );
DeclareFeatureType( MFT_ToneMap );
DeclareFeatureType( MFT_VertLit );
DeclareFeatureType( MFT_VertLitTone );

DeclareFeatureType( MFT_IsExposureX2 );
DeclareFeatureType( MFT_IsExposureX4 );
DeclareFeatureType( MFT_EnvMap );
DeclareFeatureType( MFT_CubeMap );
DeclareFeatureType( MFT_PixSpecular );
DeclareFeatureType( MFT_SpecularMap );
DeclareFeatureType( MFT_GlossMap );

/// This feature is only used to detect alpha transparency
/// and does not have any code associtated with it. 
DeclareFeatureType( MFT_IsTranslucent );

///
DeclareFeatureType( MFT_IsTranslucentZWrite );

/// This feature causes MFT_NormalMap to set the world
/// space normal vector to the output color rgb.
DeclareFeatureType( MFT_NormalsOut );

DeclareFeatureType( MFT_MinnaertShading );
DeclareFeatureType( MFT_GlowMask );
DeclareFeatureType( MFT_Visibility );
DeclareFeatureType( MFT_EyeSpaceDepthOut );
DeclareFeatureType( MFT_DepthOut );
DeclareFeatureType( MFT_Fog );

/// This should be the last feature of any material that
/// renders to a HDR render target.  It converts the high
/// dynamic range color into the correct HDR encoded format.
DeclareFeatureType( MFT_HDROut );

///
DeclareFeatureType( MFT_PrePassConditioner );

/// This feature causes MFT_ToneMap and MFT_LightMap to output their light color
/// to the second render-target
DeclareFeatureType( MFT_LightbufferMRT );

/// This feature outputs black to RenderTarget1
DeclareFeatureType( MFT_RenderTarget1_Zero );

DeclareFeatureType( MFT_Foliage );


#endif // _MATERIALFEATURETYPES_H_
