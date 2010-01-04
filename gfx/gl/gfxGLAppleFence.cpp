//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/gl/gfxGLAppleFence.h"

GFXGLAppleFence::GFXGLAppleFence(GFXDevice* device) : GFXFence(device), mIssued(false)
{
   glGenFencesAPPLE(1, &mHandle);
}

GFXGLAppleFence::~GFXGLAppleFence()
{
   glDeleteFencesAPPLE(1, &mHandle);
}

void GFXGLAppleFence::issue()
{
   glSetFenceAPPLE(mHandle);
   mIssued = true;
}

GFXFence::FenceStatus GFXGLAppleFence::getStatus() const
{
   if(!mIssued)
      return GFXFence::Unset;
      
   GLboolean res = glTestFenceAPPLE(mHandle);
   return res ? GFXFence::Processed : GFXFence::Pending;
}

void GFXGLAppleFence::block()
{
   if(!mIssued)
      return;
      
   glFinishFenceAPPLE(mHandle);
}

void GFXGLAppleFence::zombify()
{
   glDeleteFencesAPPLE(1, &mHandle);
}

void GFXGLAppleFence::resurrect()
{
   glGenFencesAPPLE(1, &mHandle);
}

const String GFXGLAppleFence::describeSelf() const
{
   return String::ToString("   GL Handle: %i", mHandle);
}
