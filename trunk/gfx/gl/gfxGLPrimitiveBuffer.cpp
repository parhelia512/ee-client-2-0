//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/gl/gfxGLDevice.h"
#include "gfx/gl/gfxGLPrimitiveBuffer.h"
#include "gfx/gl/gfxGLEnumTranslate.h"

#include "gfx/gl/ggl/ggl.h"
#include "gfx/gl/gfxGLUtils.h"

GFXGLPrimitiveBuffer::GFXGLPrimitiveBuffer(GFXDevice *device, U32 indexCount, U32 primitiveCount, GFXBufferType bufferType) :
GFXPrimitiveBuffer(device, indexCount, primitiveCount, bufferType), mZombieCache(NULL) 
{
   PRESERVE_INDEX_BUFFER();
	// Generate a buffer and allocate the needed memory
	glGenBuffers(1, &mBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(U16), NULL, GFXGLBufferType[bufferType]);
}

GFXGLPrimitiveBuffer::~GFXGLPrimitiveBuffer()
{
	// This is heavy handed, but it frees the buffer memory
	glDeleteBuffersARB(1, &mBuffer);
   
   if( mZombieCache )
      delete [] mZombieCache;
}

void GFXGLPrimitiveBuffer::lock(U16 indexStart, U16 indexEnd, U16 **indexPtr)
{
	// Preserve previous binding
   PRESERVE_INDEX_BUFFER();
   
   // Bind ourselves and map
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBuffer);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndexCount * sizeof(U16), NULL, GFXGLBufferType[mBufferType]);
   
   // Offset the buffer to indexStart
	*indexPtr = (U16*)((U8*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY) + (indexStart * sizeof(U16)));
}

void GFXGLPrimitiveBuffer::unlock()
{
	// Preserve previous binding
   PRESERVE_INDEX_BUFFER();
   
   // Bind ourselves and unmap
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBuffer);
	bool res = glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
   AssertFatal(res, "GFXGLPrimitiveBuffer::unlock - shouldn't fail!");
}

void GFXGLPrimitiveBuffer::prepare()
{
	// Bind
	static_cast<GFXGLDevice*>(mDevice)->setPB(this);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBuffer);
}

void GFXGLPrimitiveBuffer::finish()
{
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

GLvoid* GFXGLPrimitiveBuffer::getBuffer()
{
	// NULL specifies no offset into the hardware buffer
	return (GLvoid*)NULL;
}

void GFXGLPrimitiveBuffer::zombify()
{
   if(mZombieCache)
      return;
      
   mZombieCache = new U8[mIndexCount * sizeof(U16)];
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBuffer);
   glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mIndexCount * sizeof(U16), mZombieCache);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   glDeleteBuffers(1, &mBuffer);
   mBuffer = 0;
}

void GFXGLPrimitiveBuffer::resurrect()
{
   if(!mZombieCache)
      return;
   
   glGenBuffers(1, &mBuffer);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBuffer);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndexCount * sizeof(U16), mZombieCache, GFXGLBufferType[mBufferType]);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   
   delete[] mZombieCache;
   mZombieCache = NULL;
}
