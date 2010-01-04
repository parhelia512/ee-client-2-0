//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXGLPRIMITIVEBUFFER_H_
#define _GFXGLPRIMITIVEBUFFER_H_

#include "gfx/gfxPrimitiveBuffer.h"

/// This is a primitive buffer (index buffer to GL users) which uses VBOs.
class GFXGLPrimitiveBuffer : public GFXPrimitiveBuffer
{
public:
	GFXGLPrimitiveBuffer(GFXDevice *device, U32 indexCount, U32 primitiveCount, GFXBufferType bufferType);
	~GFXGLPrimitiveBuffer();

	virtual void lock(U16 indexStart, U16 indexEnd, U16 **indexPtr); ///< calls glMapBuffer, offets pointer by indexStart
	virtual void unlock(); ///< calls glUnmapBuffer, unbinds the buffer
	virtual void prepare();  ///< binds the buffer
   virtual void finish(); ///< We're done with this buffer

	virtual void* getBuffer(); ///< returns NULL

   // GFXResource interface
   virtual void zombify();
   virtual void resurrect();
   
private:
	/// Handle to our GL buffer object
	GLuint mBuffer;
   
   U8* mZombieCache;
};

#endif