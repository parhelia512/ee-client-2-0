//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _BLURTEXTURE_H_
#define _BLURTEXTURE_H_

#ifndef _GFXTARGET_H_
#include "gfx/gfxTarget.h"
#endif
#ifndef _GFXSHADER_H_
#include "gfx/gfxShader.h"
#endif 
#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif
#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif


/// Simple two pass texture blurring.  This may end up 
/// in a more generic spot soon.
class BlurOp
{
public:

   BlurOp();
   virtual ~BlurOp();

   bool init(const String& shaderName, U32 texWidth, U32 texHeight);
   virtual void blur(GFXTextureObject* input, GFXTextureObject* scratch);
   GFXShaderConstBufferRef getBlurConsts() { return mBlurConsts; }
   const Point2I& getTexDimensions() const { return mTexDimensions; }

private:

   GFXShaderRef mBlurShader;
   GFXShaderConstBufferRef mBlurConsts;
   GFXShaderConstHandle* mModelViewProjSC;
   GFXShaderConstHandle* mTexSizeSC;
   GFXShaderConstHandle* mBlurDimensionSC;

   GFXStateBlockRef mBlurSB;
   GFXTextureTargetRef mTarget;   
   Point2I mTexDimensions;   
};

#endif
