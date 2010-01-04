//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/gl/gfxGLCardProfiler.h"
#include "gfx/gl/gfxGLDevice.h"
#include "gfx/gl/gfxGLEnumTranslate.h"

void GFXGLCardProfiler::init()
{
   mChipSet = reinterpret_cast<const char*>(glGetString(GL_VENDOR));

   // get the major and minor parts of the GL version. These are defined to be
   // in the order "[major].[minor] [other]|[major].[minor].[release] [other] in the spec
   const char *versionStart = reinterpret_cast<const char*>(glGetString(GL_VERSION));
   const char *versionEnd = versionStart;
   // get the text for the version "x.x.xxxx "
   for( S32 tok = 0; tok < 2; ++tok )
   {
      char *text = dStrdup( versionEnd );
      dStrtok(text, ". ");
      versionEnd += dStrlen( text ) + 1;
      dFree( text );
   }

   mRendererString = "GL";
   mRendererString += String::SpanToString(versionStart, versionEnd - 1);

   mCardDescription = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
   mVersionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
   
   mVideoMemory = static_cast<GFXGLDevice*>(GFX)->getTotalVideoMemory();

   Parent::init();
   
   // Set new enums here so if our profile script forces this to be false we keep the GL_ZEROs.
   if(queryProfile("GL::suppFloatTexture"))
   {
      GFXGLTextureInternalFormat[GFXFormatR16G16F] = GL_RGBA_FLOAT16_ATI;
      GFXGLTextureFormat[GFXFormatR16G16F] = GL_RGBA;
      GFXGLTextureInternalFormat[GFXFormatR16G16B16A16F] = GL_RGBA_FLOAT16_ATI;
      GFXGLTextureInternalFormat[GFXFormatR32G32B32A32F] = GL_RGBA_FLOAT32_ATI;
      GFXGLTextureInternalFormat[GFXFormatR32F] = GL_RGBA_FLOAT32_ATI;
   }

   if(queryProfile("GL::suppMipLodBias"))
   {
      GFXGLSamplerState[GFXSAMPMipMapLODBias] = GL_TEXTURE_LOD_BIAS_EXT;
   }
}

void GFXGLCardProfiler::setupCardCapabilities()
{
   GLint maxTexSize;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

   const char* versionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
   F32 glVersion = dAtof(versionString);
   
   // OpenGL doesn't have separate maximum width/height.
   setCapability("maxTextureWidth", maxTexSize);
   setCapability("maxTextureHeight", maxTexSize);
   setCapability("maxTextureSize", maxTexSize);

   // If extensions haven't been inited, we're in trouble here.
   bool suppVBO = (gglHasExtension(GL_ARB_vertex_buffer_object) || glVersion >= 1.499f);
   setCapability("GL::suppVertexBufferObject", suppVBO);

   // check if render to texture supported is available
   bool suppRTT = gglHasExtension(GL_EXT_framebuffer_object);
   setCapability("GL::suppRenderTexture", suppRTT);
   
   bool suppBlit = gglHasExtension(GL_EXT_framebuffer_blit);
   setCapability("GL::suppRTBlit", suppBlit);
   
   bool suppFloatTex = gglHasExtension(GL_ATI_texture_float);
   setCapability("GL::suppFloatTexture", suppFloatTex);

   // Check for anisotropic filtering support.
   bool suppAnisotropic = gglHasExtension( GL_EXT_texture_filter_anisotropic );
   setCapability( "GL::suppAnisotropic", suppAnisotropic );
   
   // Check to see if mipmap lod bias is supported
   bool suppMipLodBias = gglHasExtension(GL_EXT_texture_lod_bias);
   setCapability("GL::suppMipLodBias", suppMipLodBias);

   // check to see if we have the fragment shader extension or the gl version is high enough for glsl to be core
   // also check to see if the language version is high enough
   F32 glslVersion = dAtof(reinterpret_cast<const char*>(glGetString( GL_SHADING_LANGUAGE_VERSION)));
   bool suppSPU = (gglHasExtension(GL_ARB_fragment_shader) || glVersion >= 1.999f) && glslVersion >= 1.0999;
   setCapability("GL::suppFragmentShader", suppSPU);
   
   bool suppAppleFence = gglHasExtension(GL_APPLE_fence);
   setCapability("GL::APPLE::suppFence", suppAppleFence);
   
   // When enabled, call glGenerateMipmapEXT() to generate mipmaps instead of relying on GL_GENERATE_MIPMAP
   setCapability("GL::Workaround::needsExplicitGenerateMipmap", false);
   // When enabled, binds and unbinds a texture target before doing the depth buffer copy.  Failure to do
   // so will cause a hard freeze on Mac OS 10.4 with a Radeon X1600
   setCapability("GL::Workaround::X1600DepthBufferCopy", false);
   // When enabled, does not copy the last column and row of the depth buffer in a depth buffer copy.  Failure
   // to do so will cause a kernel panic on Mac OS 10.5(.1) with a Radeon HD 2600 (fixed in 10.5.2)
   setCapability("GL::Workaround::HD2600DepthBufferCopy", false);
   
   // Certain Intel drivers have a divide by 0 crash if mipmaps are specified with
   // glTexSubImage2D.
   setCapability("GL::Workaround::noManualMips", false);
}

bool GFXGLCardProfiler::_queryCardCap(const String& query, U32& foundResult)
{
   // Just doing what the D3D9 layer does
   return 0;
}

bool GFXGLCardProfiler::_queryFormat(const GFXFormat fmt, const GFXTextureProfile *profile, bool &inOutAutogenMips)
{
   // Alex I am sure this isn't proper, but it seems like you set up the enums
   // like this. I am not sure how to query for using them as render targets, so 
   // I just set it to Apple Rules(tm) -Pat

   return GFXGLTextureInternalFormat[fmt] != GL_ZERO;
}
