//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/blurTexture.h"

#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "materials/shaderData.h"
#include "shaderGen/shaderGenVars.h"


BlurOp::BlurOp()
{   
   mBlurShader = NULL;
   mBlurConsts = NULL;
   mBlurSB = NULL;
}

BlurOp::~BlurOp()
{
   mBlurConsts = NULL;
   mBlurSB = NULL;
}

bool BlurOp::init(const String& shaderName, U32 texWidth, U32 texHeight)
{
   // Setup shader
   ShaderData *blurShaderData;
   if ( !Sim::findObject( shaderName, blurShaderData ) )
   {
      Con::errorf( "Couldn't find blur shader data!");   
      return false;
   }

   mBlurShader = blurShaderData->getShader();
   if ( !mBlurShader )
      return false;

   mBlurConsts = mBlurShader->allocConstBuffer();
   mModelViewProjSC = mBlurShader->getShaderConstHandle(ShaderGenVars::modelview);
   mTexSizeSC = mBlurShader->getShaderConstHandle("$texSize");
   mBlurDimensionSC = mBlurShader->getShaderConstHandle("$blurDimension");

   // Setup state block
   GFXStateBlockDesc desc;
   desc.samplersDefined = true;
   desc.samplers[0] = GFXSamplerStateDesc::getClampLinear();   
   desc.zDefined = true;
   desc.zWriteEnable = false;
   desc.zEnable = false;
   mBlurSB = GFX->createStateBlock(desc);

   // Setup geometry
   // 0.5 pixel offset - the render is spread from -1 to 1 ( 2 width )
   mTexDimensions.x = texWidth;
   mTexDimensions.y = texHeight;

   mTarget = GFX->allocRenderToTextureTarget();

   return true;
}

void BlurOp::blur(GFXTextureObject* input, GFXTextureObject* scratch)
{
   if ( !mBlurShader )
      return;

   // Setup the VB.
   GFXVertexBufferHandle<GFXVertexPT> vb( GFX, 4, GFXBufferTypeVolatile );
   {
      F32 copyOffsetX = 1.0 / mTexDimensions.x;
      F32 copyOffsetY = 1.0 / mTexDimensions.y;
      GFXVertexPT *verts = vb.lock();

      verts[0].point      = Point3F( -1.0 - copyOffsetX, -1.0 + copyOffsetY, 0.0 );
      verts[0].texCoord   = Point2F(  0.0, 1.0 );

      verts[1].point      = Point3F( -1.0 - copyOffsetX,  1.0 + copyOffsetY, 0.0 );
      verts[1].texCoord   = Point2F(  0.0, 0.0 );

      verts[2].point      = Point3F(  1.0 - copyOffsetX,  1.0 + copyOffsetY, 0.0 );
      verts[2].texCoord   = Point2F(  1.0, 0.0 );

      verts[3].point      = Point3F(  1.0 - copyOffsetX, -1.0 + copyOffsetY, 0.0 );
      verts[3].texCoord   = Point2F(  1.0, 1.0 );

      vb.unlock();
   }

   GFX->pushActiveRenderTarget();   

   // Set our ortho projection
   GFXTransformSaver saver;
   GFX->setWorldMatrix(MatrixF::Identity);
   GFX->setProjectionMatrix(MatrixF::Identity);
   mBlurConsts->set(mModelViewProjSC, MatrixF::Identity);

   mBlurConsts->set(mTexSizeSC, (F32) mTexDimensions.x); // BTRTODO: FIX ME   

   // Set our shader stuff
   GFX->setShader( mBlurShader );
   GFX->setShaderConstBuffer( mBlurConsts );
   GFX->setStateBlock( mBlurSB );
   GFX->setVertexBuffer( vb );

   mTarget->attachTexture(GFXTextureTarget::Color0, scratch);
   mBlurConsts->set(mBlurDimensionSC, Point2F(1.0f, 0.0f));
   GFX->setActiveRenderTarget(mTarget);
   GFX->setTexture(0, input);
   GFX->drawPrimitive(GFXTriangleFan, 0, 2);

   mTarget->resolve();

   mTarget->attachTexture(GFXTextureTarget::Color0, input);
   GFX->setActiveRenderTarget(mTarget);
   mBlurConsts->set(mBlurDimensionSC, Point2F(0.0f, 1.0f));
   GFX->setTexture(0, scratch);
   GFX->drawPrimitive(GFXTriangleFan, 0, 2);

   mTarget->resolve();

   // Cleanup
   GFX->setTexture( 0, NULL );
   GFX->setShaderConstBuffer(NULL);
   GFX->popActiveRenderTarget();
}
