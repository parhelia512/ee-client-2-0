//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "terrain/terrFeatureTypes.h"

#include "materials/materialFeatureTypes.h"


ImplementFeatureType( MFT_TerrainEmpty, MFG_PostTransform, 1.0f, true );
ImplementFeatureType( MFT_TerrainBaseMap, MFG_Texture, 100.0f, true );
ImplementFeatureType( MFT_TerrainParallaxMap, MFG_Texture, 101.0f, true );
ImplementFeatureType( MFT_TerrainDetailMap, MFG_Texture, 102.0f, true );
ImplementFeatureType( MFT_TerrainNormalMap, MFG_Texture, 103.0f, true );
ImplementFeatureType( MFT_TerrainLightMap, MFG_Texture, 104.0f, true );
ImplementFeatureType( MFT_TerrainSideProject, MFG_Texture, 105.0f, true );
ImplementFeatureType( MFT_TerrainAdditive, MFG_PostProcess, 999.0f, true );

