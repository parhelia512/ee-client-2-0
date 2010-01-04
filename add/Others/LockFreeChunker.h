#ifndef _LOCKFREEDATACHUNKER_H_
#define _LOCKFREEDATACHUNKER_H_

#include "CSLock.h"

namespace WL
{

	typedef signed char        S8;      ///< Compiler independent Signed Char
	typedef unsigned char      U8;      ///< Compiler independent Unsigned Char

	typedef signed short       S16;     ///< Compiler independent Signed 16-bit short
	typedef unsigned short     U16;     ///< Compiler independent Unsigned 16-bit short

	typedef signed int         S32;     ///< Compiler independent Signed 32-bit integer
	typedef unsigned int       U32;     ///< Compiler independent Unsigned 32-bit integer

	typedef float              F32;     ///< Compiler independent 32-bit float
	typedef double             F64;     ///< Compiler independent 64-bit float
	
class DataChunker
{
  public:
   enum {
      ChunkSize = 16376 ///< Default size for chunks.
   };

  private:

   struct DataBlock
   {
      DataBlock *next;
      U8 *data;
      S32 curIndex;
      DataBlock(S32 size);
      ~DataBlock();
   };
   DataBlock *curBlock;
   S32 chunkSize;

  public:

   void *alloc(S32 size);

   void freeBlocks();

   DataChunker(S32 size=ChunkSize);
	virtual ~DataChunker();
};


//----------------------------------------------------------------------------

template<class T>
class Chunker: private DataChunker
{
public:
   Chunker(S32 size = DataChunker::ChunkSize) : DataChunker(size) {};
   T* alloc()  { return reinterpret_cast<T*>(DataChunker::alloc(S32(sizeof(T)))); }
   void clear()  { freeBlocks(); };
};

template<class T>
class FreeListChunker: private DataChunker
{
   S32 numAllocated;
   S32 elementSize;
   T *freeListHead;
public:
   FreeListChunker(S32 size = DataChunker::ChunkSize) : DataChunker(size)
   {
      numAllocated = 0;
      elementSize = getMax(U32(sizeof(T)), U32(sizeof(T *)));
      freeListHead = NULL;
   }

   T *alloc()
   {
      numAllocated++;
      if(freeListHead == NULL)
         return reinterpret_cast<T*>(DataChunker::alloc(elementSize));
      T* ret = freeListHead;
      freeListHead = *(reinterpret_cast<T**>(freeListHead));
      return ret;
   }

   void free(T* elem)
   {
      numAllocated--;
      *(reinterpret_cast<T**>(elem)) = freeListHead;
      freeListHead = elem;

      // If nothing's allocated, clean up!
      if(!numAllocated)
      {
         freeBlocks();
         freeListHead = NULL;
      }
   }

   // Allow people to free all their memory if they want.
   void freeBlocks()
   {
      DataChunker::freeBlocks();

      // We have to terminate the freelist as well or else we'll run
      // into crazy unused memory.
      freeListHead = NULL;
   }
};


template<class T>
class LockFreeChunker: private DataChunker
{
	S32 numAllocated;
	S32 elementSize;
	T *freeListHead;
	CRITICAL_SECTION _mCS;
public:
	LockFreeChunker(S32 size = DataChunker::ChunkSize) : DataChunker(size)
	{
		numAllocated = 0;
		elementSize = getMax(U32(sizeof(T)), U32(sizeof(T *)));
		freeListHead = NULL;
		::InitializeCriticalSection(&_mCS);
	}
	~LockFreeChunker()
	{
		::DeleteCriticalSection(&_mCS);
	}

	T *alloc()
	{
		CSLock lock(&_mCS);
		numAllocated++;
		if(freeListHead == NULL)
			return reinterpret_cast<T*>(DataChunker::alloc(elementSize));
		T* ret = freeListHead;
		freeListHead = *(reinterpret_cast<T**>(freeListHead));
		return ret;
	}

	void free(T* elem)
	{
		CSLock lock(&_mCS);
		numAllocated--;
		*(reinterpret_cast<T**>(elem)) = freeListHead;
		freeListHead = elem;

		// If nothing's allocated, clean up!
		if(!numAllocated)
		{
			freeBlocks();
			freeListHead = NULL;
		}
	}

	// Allow people to free all their memory if they want.
	void freeBlocks()
	{
		CSLock lock(&_mCS);
		DataChunker::freeBlocks();
		// We have to terminate the freelist as well or else we'll run
		// into crazy unused memory.
		freeListHead = NULL;
	}
};

}
#endif
