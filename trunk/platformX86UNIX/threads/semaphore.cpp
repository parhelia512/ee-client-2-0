//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platformX86UNIX/platformX86UNIX.h"
#include "platform/threads/semaphore.h"
// Instead of that mess that was here before, lets use the SDL lib to deal
// with the semaphores.

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

struct PlatformSemaphore
{
   SDL_sem *semaphore;

   PlatformSemaphore(S32 initialCount)
   {
       semaphore = SDL_CreateSemaphore(initialCount);
       AssertFatal(semaphore, "PlatformSemaphore constructor - Failed to create SDL Semaphore.");
   }

   ~PlatformSemaphore()
   {
       SDL_DestroySemaphore(semaphore);
   }
};

Semaphore::Semaphore(S32 initialCount)
{
  mData = new PlatformSemaphore(initialCount);
}

Semaphore::~Semaphore()
{
  AssertFatal(mData, "Semaphore destructor - Invalid semaphore.");
  delete mData;
}

bool Semaphore::acquire(bool block)
{
   AssertFatal(mData, "Semaphore::acquire - Invalid semaphore.");
   if (block)
   {
      if (SDL_SemWait(mData->semaphore) < 0)
         AssertFatal(false, "Semaphore::acquie - Wait failed.");
      return (true);
   }
   else
   {
      int res = SDL_SemTryWait(mData->semaphore);
      return (res == 0);
   }
}

void Semaphore::release()
{
   AssertFatal(mData, "Semaphore::releaseSemaphore - Invalid semaphore.");
   SDL_SemPost(mData->semaphore);
}
