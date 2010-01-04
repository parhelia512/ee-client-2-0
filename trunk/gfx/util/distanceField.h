//-----------------------------------------------------------------------------
// Torque Shader Engine 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _DISTANCE_FIELD_H_
#define _DISTANCE_FIELD_H_

namespace GFXUtil
{
namespace DistanceField
{
   //GBitmap * makeDistanceField(const GBitmap * sourceBmp, S32 targetSizeX, S32 targetSizeY, F32 rangePct);
   void makeDistanceField(const U8 * sourceData, S32 sourceSizeX, S32 sourceSizeY, U8 * targetData, S32 targetSizeX, S32 targetSizeY, F32 radius);
}
}

#endif  // _DISTANCE_FIELD_H_
