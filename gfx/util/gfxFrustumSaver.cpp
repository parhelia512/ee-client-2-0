//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/util/gfxFrustumSaver.h"
#include "gfx/gfxDevice.h"

GFXFrustumSaver::GFXFrustumSaver()
{
   GFX->getFrustum(&mLeft, &mRight, &mBottom, &mTop, &mNearPlane, &mFarPlane, &mIsOrtho);
}

GFXFrustumSaver::~GFXFrustumSaver()
{
   if(!mIsOrtho)
      GFX->setFrustum(mLeft, mRight, mBottom, mTop, mNearPlane, mFarPlane);
   else
      GFX->setOrtho(mLeft, mRight, mBottom, mTop, mNearPlane, mFarPlane);
}
