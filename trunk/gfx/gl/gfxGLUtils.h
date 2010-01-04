//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef TORQUE_GFX_GL_GFXGLUTILS_H_
#define TORQUE_GFX_GL_GFXGLUTILS_H_

#include "core/util/preprocessorHelpers.h"
#include "gfx/gl/gfxGLEnumTranslate.h"

static inline GLenum minificationFilter(U32 minFilter, U32 mipFilter, U32 mipLevels)
{
   if(mipLevels == 1)
      return GFXGLTextureFilter[minFilter];

   // the compiler should interpret this as array lookups
   switch( minFilter ) 
   {
      case GFXTextureFilterLinear:
         switch( mipFilter ) 
         {
         case GFXTextureFilterLinear:
            return GL_LINEAR_MIPMAP_LINEAR;
         case GFXTextureFilterPoint:
            return GL_LINEAR_MIPMAP_NEAREST;
         default: 
            return GL_LINEAR;
         }
      default:
         switch( mipFilter ) {
      case GFXTextureFilterLinear:
         return GL_NEAREST_MIPMAP_LINEAR;
      case GFXTextureFilterPoint:
         return GL_NEAREST_MIPMAP_NEAREST;
      default:
         return GL_NEAREST;
         }
   }
}

/// Simple class which preserves a given GL integer.
/// This class determines the integer to preserve on construction and restores 
/// it on destruction.
class GFXGLPreserveInteger
{
public:
   typedef void(*BindFn)(GLenum, GLuint);

   /// Preserve the integer.
   /// @param binding The binding which should be set on destruction.
   /// @param getBinding The parameter to be passed to glGetIntegerv to determine
   /// the integer to be preserved.
   /// @param binder The gl function to call to restore the integer.
   GFXGLPreserveInteger(GLenum binding, GLint getBinding, BindFn binder) :
      mBinding(binding), mPreserved(0), mBinder(binder)
   {
      AssertFatal(mBinder, "GFXGLPreserveInteger - Need a valid binder function");
      glGetIntegerv(getBinding, &mPreserved);
   }
   
   /// Restores the integer.
   ~GFXGLPreserveInteger()
   {
      mBinder(mBinding, mPreserved);
   }

private:
   GLenum mBinding;
   GLint mPreserved;
   BindFn mBinder;
};

/// Helper macro to preserve the current VBO binding.
#define PRESERVE_VERTEX_BUFFER() \
GFXGLPreserveInteger TORQUE_CONCAT(preserve_, __LINE__) (GL_ARRAY_BUFFER, GL_ARRAY_BUFFER_BINDING, glBindBuffer)

/// Helper macro to preserve the current element array binding.
#define PRESERVE_INDEX_BUFFER() \
GFXGLPreserveInteger TORQUE_CONCAT(preserve_, __LINE__) (GL_ELEMENT_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER_BINDING, glBindBuffer)

/// Helper macro to preserve the current 2D texture binding.
#define PRESERVE_2D_TEXTURE() \
GFXGLPreserveInteger TORQUE_CONCAT(preserve_, __LINE__) (GL_TEXTURE_2D, GL_TEXTURE_BINDING_2D, glBindTexture)

/// Helper macro to preserve the current 3D texture binding.
#define PRESERVE_3D_TEXTURE() \
GFXGLPreserveInteger TORQUE_CONCAT(preserve_, __LINE__) (GL_TEXTURE_3D, GL_TEXTURE_BINDING_3D, glBindTexture)

#define PRESERVE_FRAMEBUFFER() \
GFXGLPreserveInteger TORQUE_CONCAT(preserve_, __LINE__) (GL_READ_FRAMEBUFFER_EXT, GL_READ_FRAMEBUFFER_BINDING_EXT, glBindFramebufferEXT);\
GFXGLPreserveInteger TORQUE_CONCAT(preserve2_, __LINE__) (GL_DRAW_FRAMEBUFFER_EXT, GL_DRAW_FRAMEBUFFER_BINDING_EXT, glBindFramebufferEXT)

#endif
