//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PLATFORM_THREAD_SEMAPHORE_H_
#define _PLATFORM_THREAD_SEMAPHORE_H_

#ifndef _TORQUE_TYPES_H_
#include "platform/types.h"
#endif

// Forward ref used by platform code
class PlatformSemaphore;

class Semaphore
{
protected:
   PlatformSemaphore *mData;

public:
   /// Create a semaphore. initialCount defaults to 1.
   Semaphore(S32 initialCount = 1);
   /// Delete a semaphore, ignoring it's count.
   ~Semaphore();

   /// Acquire the semaphore, decrementing its count.
   /// if the initial count is less than 1, block until it goes above 1, then acquire.
   /// Returns true if the semaphore was acquired, false if the semaphore could
   /// not be acquired and block was false.
   bool acquire(bool block = true);

   /// Release the semaphore, incrementing its count.
   /// Never blocks.
   void release();
};

#endif
