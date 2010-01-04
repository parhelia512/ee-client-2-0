//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platformX86UNIX/platformX86UNIX.h"
#include <stdlib.h>
#include <mm_malloc.h>

void* dMemcpy(void *dst, const void *src, unsigned size)
{
   return memcpy(dst,src,size);
}

//--------------------------------------
void* dMemmove(void *dst, const void *src, unsigned size)
{
   return memmove(dst,src,size);
}

//--------------------------------------
void* dMemset(void *dst, S32 c, unsigned size)
{
   return memset(dst,c,size);
}

//--------------------------------------
S32 dMemcmp(const void *ptr1, const void *ptr2, unsigned len)
{
   return memcmp(ptr1, ptr2, len);
}

void* dRealMalloc(dsize_t s)
{
   return malloc(s);
}

void dRealFree(void* p)
{
   free(p);
}

void *dAligned_malloc(dsize_t in_size, dsize_t alignment)
{
   return _mm_malloc(in_size, alignment);
}

void dAligned_free(void* p)
{
   return _mm_free(p);
}
