//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/materialFeatureTypes.h"


ImplementFeatureType( MFT_VertTransform, MFG_Transform, 0, true );

ImplementFeatureType( MFT_TexAnim, MFG_PreTexture, 1.0f, true );
ImplementFeatureType( MFT_Parallax, MFG_PreTexture, 2.0f, true );

ImplementFeatureType( MFT_DiffuseMap, MFG_Texture, 2.0f, true );
ImplementFeatureType( MFT_OverlayMap, MFG_Texture, 3.0f, true );
ImplementFeatureType( MFT_DetailMap, MFG_Texture, 4.0f, true );
ImplementFeatureType( MFT_DiffuseColor, MFG_Texture, 5.0f, true );
ImplementFeatureType( MFT_ColorMultiply, MFG_Texture, 6.0f, true );
ImplementFeatureType( MFT_AlphaTest, MFG_Texture, 7.0f, true );
ImplementFeatureType( MFT_SpecularMap, MFG_Texture, 8.0f, true );

ImplementFeatureType( MFT_NormalMap, MFG_Lighting, 1.0f, true );
ImplementFeatureType( MFT_RTLighting, MFG_Lighting, 2.0f, true );
ImplementFeatureType( MFT_SubSurface, MFG_Lighting, 3.0f, true );
ImplementFeatureType( MFT_LightMap, MFG_Lighting, 4.0f, true );
ImplementFeatureType( MFT_ToneMap, MFG_Lighting, 5.0f, true );
ImplementFeatureType( MFT_VertLitTone, MFG_Lighting, 6.0f, false );
ImplementFeatureType( MFT_VertLit, MFG_Lighting, 7.0f, true );
ImplementFeatureType( MFT_EnvMap, MFG_Lighting, 8.0f, true );
ImplementFeatureType( MFT_CubeMap, MFG_Lighting, 9.0f, true );
ImplementFeatureType( MFT_PixSpecular, MFG_Lighting, 10.0f, true );
ImplementFeatureType( MFT_MinnaertShading, MFG_Lighting, 12.0f, true );

ImplementFeatureType( MFT_GlowMask, MFG_PostLighting, 1.0f, true );
ImplementFeatureType( MFT_Visibility, MFG_PostLighting, 2.0f, true );
ImplementFeatureType( MFT_Fog, MFG_PostProcess, 3.0f, true );

ImplementFeatureType( MFT_HDROut, MFG_PostProcess, 999.0f, true );

ImplementFeatureType( MFT_IsDXTnm, U32(-1), -1, true );
ImplementFeatureType( MFT_IsExposureX2, U32(-1), -1, true );
ImplementFeatureType( MFT_IsExposureX4, U32(-1), -1, true );
ImplementFeatureType( MFT_IsTranslucent, U32(-1), -1, true );
ImplementFeatureType( MFT_IsTranslucentZWrite, U32(-1), -1, true );
ImplementFeatureType( MFT_IsEmissive, U32(-1), -1, true );
ImplementFeatureType( MFT_GlossMap, U32(-1), -1, true );

ImplementFeatureType( MFT_ParaboloidVertTransform, MFG_Transform, -1, false );
ImplementFeatureType( MFT_IsSinglePassParaboloid, U32(-1), -1, false );
ImplementFeatureType( MFT_EyeSpaceDepthOut, MFG_PostLighting, 2.0f, false );
ImplementFeatureType( MFT_DepthOut, MFG_PostLighting, 3.0f, false );
ImplementFeatureType( MFT_PrePassConditioner, MFG_PostProcess, 1.0f, false );
ImplementFeatureType( MFT_NormalsOut, MFG_PreLighting, 1.0f, false );

ImplementFeatureType( MFT_LightbufferMRT, MFG_PreLighting, 1.0f, false );
ImplementFeatureType( MFT_RenderTarget1_Zero, MFG_PreTexture, 1.0f, false );

ImplementFeatureType( MFT_Foliage, MFG_PreTransform, 1.0f, false );