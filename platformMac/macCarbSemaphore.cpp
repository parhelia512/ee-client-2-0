//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include <CoreServices/CoreServices.h>
#include "platform/platform.h"
#include "platform/threads/semaphore.h"

class PlatformSemaphore
{
public:
   MPSemaphoreID mSemaphore;

   PlatformSemaphore(S32 initialCount)
   {
      OSStatus err = MPCreateSemaphore(S32_MAX - 1, initialCount, &mSemaphore);
      AssertFatal(err == noErr, "Failed to allocate semaphore!");
   }

   ~PlatformSemaphore()
   {
      OSStatus err = MPDeleteSemaphore(mSemaphore);
      AssertFatal(err == noErr, "Failed to destroy semaphore!");
   }
};

Semaphore::Semaphore(S32 initialCount)
{
   mData = new PlatformSemaphore(initialCount);
}

Semaphore::~Semaphore()
{
   AssertFatal(mData && mData->mSemaphore, "Semaphore::destroySemaphore: invalid semaphore");
   delete mData;
}

bool Semaphore::acquire(bool block)
{
   AssertFatal(mData && mData->mSemaphore, "Semaphore::acquireSemaphore: invalid semaphore");
   OSStatus err = MPWaitOnSemaphore(mData->mSemaphore, block ? kDurationForever : kDurationImmediate);
   return(err == noErr);
}

void Semaphore::release()
{
   AssertFatal(mData && mData->mSemaphore, "Semaphore::releaseSemaphore: invalid semaphore");
   OSStatus err = MPSignalSemaphore(mData->mSemaphore);
   AssertFatal(err == noErr, "Failed to release semaphore!");
}
