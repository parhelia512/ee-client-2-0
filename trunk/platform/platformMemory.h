//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TORQUE_PLATFORM_PLATFORMMEMORY_H_
#define _TORQUE_PLATFORM_PLATFORMMEMORY_H_

#include "platform/platform.h"

namespace Memory
{
   enum EFlag
   {
      FLAG_Debug,
      FLAG_Global,
      FLAG_Static
   };

   void        checkPtr( void* ptr );
   void        flagCurrentAllocs( EFlag flag = FLAG_Debug );
   void        ensureAllFreed();
   void        dumpUnflaggedAllocs(const char *file, EFlag flag = FLAG_Debug );
   S32         countUnflaggedAllocs(const char *file, S32 *outUnflaggedRealloc = NULL, EFlag flag = FLAG_Debug );
   dsize_t     getMemoryUsed();
   dsize_t     getMemoryAllocated();
   void        validate();
}

#endif // _TORQUE_PLATFORM_PLATFORMMEMORY_H_
