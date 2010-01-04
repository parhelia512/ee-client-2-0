//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SHADOW_COMMON_H_
#define _SHADOW_COMMON_H_

///
enum ShadowType
{   
   ShadowType_None = -1,

   ShadowType_Spot,
   ShadowType_PSSM,

   ShadowType_Paraboloid,
   ShadowType_DualParaboloidSinglePass,
   ShadowType_DualParaboloid,
   ShadowType_CubeMap,

   ShadowType_Count,
};


/// The different shadow filter modes used when rendering 
/// shadowed lights.
/// @see setShadowFilterMode
enum ShadowFilterMode
{
   ShadowFilterMode_None,
   ShadowFilterMode_SoftShadow,
   ShadowFilterMode_SoftShadowHighQuality
};

#endif // _SHADOW_COMMON_H_
