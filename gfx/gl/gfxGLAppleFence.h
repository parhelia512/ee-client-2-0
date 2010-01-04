//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXGLAPPLEFENCE_H_
#define _GFXGLAPPLEFENCE_H_

#include "gfx/gfxFence.h"
#include "gfx/gl/ggl/ggl.h"

class GFXGLAppleFence : public GFXFence
{
public:
   GFXGLAppleFence(GFXDevice* device);
   virtual ~GFXGLAppleFence();
   
   // GFXFence interface
   virtual void issue();
   virtual FenceStatus getStatus() const;
   virtual void block();
   
   // GFXResource interface
   virtual void zombify();
   virtual void resurrect();
   virtual const String describeSelf() const;
   
private:
   GLuint mHandle;
   bool mIssued;
};

#endif