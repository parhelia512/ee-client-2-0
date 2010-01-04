//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _FOGSTRUCTS_H_
#define _FOGSTRUCTS_H_

/// The aerial fog settings.
struct FogData
{   
   F32 density;
   F32 densityOffset;
   F32 atmosphereHeight;
   ColorF color;
};


/// The water fog settings.
struct WaterFogData
{   
   F32 density;
   
   F32 densityOffset;   
   
   F32 wetDepth;
   
   F32 wetDarkening;
   
   ColorI color;
   
   PlaneF plane;   
};

#endif // _FOGSTRUCTS_H_