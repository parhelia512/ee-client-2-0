//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXVERTEXBUFFER_H_
#define _GFXVERTEXBUFFER_H_

#ifndef _GFXSTRUCTS_H_
#include "gfx/gfxStructs.h"
#endif


//*****************************************************************************
// GFXVertexBuffer - base vertex buffer class
//*****************************************************************************
class GFXVertexBuffer : public StrongRefBase, public GFXResource
{
   friend class GFXVertexBufferHandleBase;
   friend class GFXDevice;

public:

   /// Number of vertices in this buffer.
   U32 mNumVerts;

   /// The vertex format for this buffer.
   const GFXVertexFormat *mVertexFormat;

   /// Vertex size in bytes.
   U32 mVertexSize;

   /// GFX buffer type (static, dynamic or volatile).
   GFXBufferType mBufferType;

   /// Device this vertex buffer was allocated on.
   GFXDevice *mDevice;

   bool  isLocked;
   U32   lockedVertexStart;
   U32   lockedVertexEnd;
   void* lockedVertexPtr;
   U32   mVolatileStart;

   GFXVertexBuffer(  GFXDevice *device, 
                     U32 numVerts, 
                     const GFXVertexFormat *vertexFormat, 
                     U32 vertexSize, 
                     GFXBufferType bufferType )
      :  mDevice( device ),
         mVolatileStart( 0 ),         
         mNumVerts( numVerts ),
         mVertexFormat( vertexFormat ),
         mVertexSize( vertexSize ),
         mBufferType( bufferType )      
   {
   }
   
   virtual void lock(U32 vertexStart, U32 vertexEnd, void **vertexPtr) = 0;
   virtual void unlock() = 0;
   virtual void prepare() = 0;

   // GFXResource
   virtual const String describeSelf() const;
};


//*****************************************************************************
// GFXVertexBufferHandleBase
//*****************************************************************************
class GFXVertexBufferHandleBase : public StrongRefPtr<GFXVertexBuffer>
{
   friend class GFXDevice;

protected:

   void set(   GFXDevice *theDevice,
               U32 numVerts, 
               const GFXVertexFormat *vertexFormat, 
               U32 vertexSize,
               GFXBufferType type );

   void* lock(U32 vertexStart, U32 vertexEnd)
   {
      if(vertexEnd == 0)
         vertexEnd = getPointer()->mNumVerts;
      AssertFatal(vertexEnd > vertexStart, "Can't get a lock with the end before the start.");
      AssertFatal(vertexEnd <= getPointer()->mNumVerts || getPointer()->mBufferType == GFXBufferTypeVolatile, "Tried to get vertices beyond the end of the buffer!");
      getPointer()->lock(vertexStart, vertexEnd, &getPointer()->lockedVertexPtr);
      return getPointer()->lockedVertexPtr;
   }
   void unlock() ///< unlocks the vertex data, making changes illegal.
   {
      getPointer()->unlock();
   }
};

//*****************************************************************************
// GFXVertexBufferHandle
//*****************************************************************************
template<class T> class GFXVertexBufferHandle : public GFXVertexBufferHandleBase
{
   void prepare() ///< sets this vertex buffer as the current vertex buffer for the device it was allocated on
   {
      getPointer()->prepare();
   }
public:
   GFXVertexBufferHandle() { }
   GFXVertexBufferHandle(GFXDevice *theDevice, U32 numVerts = 0, GFXBufferType bufferType = GFXBufferTypeVolatile)
   {
      set(theDevice, numVerts, bufferType);
   }
   void set(GFXDevice *theDevice, U32 numVerts = 0, GFXBufferType t = GFXBufferTypeVolatile)
   {
      GFXVertexBufferHandleBase::set(theDevice, numVerts, getGFXVertexFormat<T>(), sizeof(T), t);
   }
   T *lock(U32 vertexStart = 0, U32 vertexEnd = 0) ///< locks the vertex buffer range, and returns a pointer to the beginning of the vertex array
                                                   ///< also allows the array operators to work on this vertex buffer.
   {
      return (T*)GFXVertexBufferHandleBase::lock(vertexStart, vertexEnd);
   }
   void unlock()
   {
      GFXVertexBufferHandleBase::unlock();
   }

   T& operator[](U32 index) ///< Array operator allows indexing into a locked vertex buffer.  The debug version of the code
                            ///< will range check the array access as well as validate the locked vertex buffer pointer.
   {
      return ((T*)getPointer()->lockedVertexPtr)[index];
   }
   const T& operator[](U32 index) const ///< Array operator allows indexing into a locked vertex buffer.  The debug version of the code
                                        ///< will range check the array access as well as validate the locked vertex buffer pointer.
   {
      index += getPointer()->mVolatileStart;
      AssertFatal(getPointer()->lockedVertexPtr != NULL, "Cannot access verts from an unlocked vertex buffer!!!");
      AssertFatal(index >= getPointer()->lockedVertexStart && index < getPointer()->lockedVertexEnd, "Out of range vertex access!");
      index -= getPointer()->mVolatileStart;
      return ((T*)getPointer()->lockedVertexPtr)[index];
   }
   T& operator[](S32 index) ///< Array operator allows indexing into a locked vertex buffer.  The debug version of the code
                            ///< will range check the array access as well as validate the locked vertex buffer pointer.
   {
      index += getPointer()->mVolatileStart;
      AssertFatal(getPointer()->lockedVertexPtr != NULL, "Cannot access verts from an unlocked vertex buffer!!!");
      AssertFatal(index >= getPointer()->lockedVertexStart && index < getPointer()->lockedVertexEnd, "Out of range vertex access!");
      index -= getPointer()->mVolatileStart;
      return ((T*)getPointer()->lockedVertexPtr)[index];
   }
   const T& operator[](S32 index) const ///< Array operator allows indexing into a locked vertex buffer.  The debug version of the code
                                        ///< will range check the array access as well as validate the locked vertex buffer pointer.
   {
      index += getPointer()->mVolatileStart;
      AssertFatal(getPointer()->lockedVertexPtr != NULL, "Cannot access verts from an unlocked vertex buffer!!!");
      AssertFatal(index >= getPointer()->lockedVertexStart && index < getPointer()->lockedVertexEnd, "Out of range vertex access!");
      index -= getPointer()->mVolatileStart;
      return ((T*)getPointer()->lockedVertexPtr)[index];
   }
   GFXVertexBufferHandle<T>& operator=(GFXVertexBuffer *ptr)
   {
      StrongObjectRef::set(ptr);
      return *this;
   }

};

/// This is a non-typed vertex buffer handle which can be
///  used when your vertex type is undefined until runtime.
class GFXVertexBufferDataHandle : public GFXVertexBufferHandleBase         
{
   typedef GFXVertexBufferHandleBase Parent;

protected:

   void prepare() { getPointer()->prepare(); }

   U32 mVertSize;

   const GFXVertexFormat *mVertexFormat;

public:

   GFXVertexBufferDataHandle()
      :  mVertSize( 0 ),
         mVertexFormat( NULL )
   {
   }

   void set(   GFXDevice *theDevice, 
               U32 vertSize, 
               const GFXVertexFormat *vertexFormat, 
               U32 numVerts, 
               GFXBufferType t )
   {
      mVertSize = vertSize;
      mVertexFormat = vertexFormat;
      Parent::set( theDevice, numVerts, mVertexFormat, mVertSize, t);
   }

   U8* lock( U32 vertexStart = 0, U32 vertexEnd = 0 )
   {
      return (U8*)Parent::lock( vertexStart, vertexEnd );
   }

   void unlock() { Parent::unlock(); }

   GFXVertexBufferDataHandle& operator=( GFXVertexBuffer *ptr )
   {
      StrongObjectRef::set(ptr);
      return *this;
   }
};


#endif // _GFXVERTEXBUFFER_H_


