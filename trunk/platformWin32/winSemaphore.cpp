//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platformWin32/platformWin32.h"
#include "platform/threads/semaphore.h"

class PlatformSemaphore
{
public:
   HANDLE   *semaphore;

   PlatformSemaphore(S32 initialCount)
   {
      semaphore = new HANDLE;
      *semaphore = CreateSemaphore(0, initialCount, S32_MAX, 0);
   }

   ~PlatformSemaphore()
   {
      CloseHandle(*(HANDLE*)(semaphore));
      delete semaphore;
      semaphore = NULL;
   }
};

Semaphore::Semaphore(S32 initialCount)
{
   mData = new PlatformSemaphore(initialCount);
}

Semaphore::~Semaphore()
{
   AssertFatal(mData && mData->semaphore, "Semaphore::destroySemaphore: invalid semaphore");
   delete mData;
}

bool Semaphore::acquire(bool block)
{
   AssertFatal(mData && mData->semaphore, "Semaphore::acquireSemaphore: invalid semaphore");
   if(block)
   {
      WaitForSingleObject(*(HANDLE*)(mData->semaphore), INFINITE);
      return(true);
   }
   else
   {
      DWORD result = WaitForSingleObject(*(HANDLE*)(mData->semaphore), 0);
      return(result == WAIT_OBJECT_0);
   }
}

void Semaphore::release()
{
   AssertFatal(mData && mData->semaphore, "Semaphore::releaseSemaphore: invalid semaphore");
   ReleaseSemaphore(*(HANDLE*)(mData->semaphore), 1, 0);
}
