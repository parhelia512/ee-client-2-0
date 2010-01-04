//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef TORQUE_GFX_UTIL_GFXFRUSTUMSAVER_H_
#define TORQUE_GFX_UTIL_GFXFRUSTUMSAVER_H_

#include "platform/types.h"

class GFXFrustumSaver
{
public:
   /// Saves the current frustum state.
   GFXFrustumSaver();
   
   /// Restores the saved frustum state.
   ~GFXFrustumSaver();
   
private:
   F32 mLeft;
   F32 mRight;
   F32 mBottom;
   F32 mTop;
   F32 mNearPlane;
   F32 mFarPlane;
   bool mIsOrtho;
};

#endif
