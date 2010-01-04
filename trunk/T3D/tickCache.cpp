//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/gameBase.h"
#include "console/consoleTypes.h"
#include "console/consoleInternal.h"
#include "core/stream/bitStream.h"
#include "sim/netConnection.h"
#include "T3D/gameConnection.h"
#include "math/mathIO.h"
#include "T3D/moveManager.h"
#include "T3D/gameProcess.h"

struct TickCacheHead
{
   TickCacheEntry * oldest;
   TickCacheEntry * newest;
   TickCacheEntry * next;
   U32 numEntry;
};

namespace
{
   FreeListChunker<TickCacheHead>  sgTickCacheHeadStore;
   FreeListChunker<TickCacheEntry> sgTickCacheEntryStore;
   FreeListChunker<Move>           sgMoveStore;

   static TickCacheHead * allocHead() { return sgTickCacheHeadStore.alloc(); }
   static void freeHead(TickCacheHead * head) { sgTickCacheHeadStore.free(head); }

   static TickCacheEntry * allocEntry() { return sgTickCacheEntryStore.alloc(); }
   static void freeEntry(TickCacheEntry * entry) { sgTickCacheEntryStore.free(entry); }

   static Move * allocMove() { return sgMoveStore.alloc(); }
   static void freeMove(Move * move) { sgMoveStore.free(move); }
}

//----------------------------------------------------------------------------

TickCache::~TickCache()
{
   if (mTickCacheHead)
   {
      setCacheSize(0);
      freeHead(mTickCacheHead);
      mTickCacheHead = NULL;
   }
}

Move * TickCacheEntry::allocateMove()
{
   return allocMove();
}

TickCacheEntry * TickCache::addCacheEntry()
{
   // Add a new entry, creating head if needed
   if (!mTickCacheHead)
   {
      mTickCacheHead = allocHead();
      mTickCacheHead->newest = mTickCacheHead->oldest = mTickCacheHead->next = NULL;
      mTickCacheHead->numEntry = 0;
   }
   if (!mTickCacheHead->newest)
   {
      mTickCacheHead->newest = mTickCacheHead->oldest = allocEntry();
   }
   else
   {
      mTickCacheHead->newest->next = allocEntry();
      mTickCacheHead->newest = mTickCacheHead->newest->next;
   }
   mTickCacheHead->newest->next = NULL;
   mTickCacheHead->newest->move = NULL;
   mTickCacheHead->numEntry++;
   return mTickCacheHead->newest;
}

void TickCache::setCacheSize(S32 len)
{
   // grow cache to len size, adding to newest side of the list
   while (!mTickCacheHead || mTickCacheHead->numEntry < len)
      addCacheEntry();
   // shrink tick cache down to given size, popping off oldest entries first
   while (mTickCacheHead && mTickCacheHead->numEntry > len)
      dropOldest();
}

void TickCache::dropOldest()
{
   AssertFatal(mTickCacheHead->oldest,"Popping off too many tick cache entries");
   TickCacheEntry * oldest = mTickCacheHead->oldest;
   mTickCacheHead->oldest = oldest->next;
   if (oldest->move)
      freeMove(oldest->move);
   freeEntry(oldest);
   mTickCacheHead->numEntry--;
   if (mTickCacheHead->numEntry < 2)
      mTickCacheHead->newest = mTickCacheHead->oldest;
}

void TickCache::dropNextOldest()
{
   AssertFatal(mTickCacheHead->oldest && mTickCacheHead->numEntry>1,"Popping off too many tick cache entries");
   TickCacheEntry * oldest = mTickCacheHead->oldest;
   TickCacheEntry * nextoldest = mTickCacheHead->oldest->next;
   oldest->next = nextoldest->next;
   if (nextoldest->move)
      freeMove(nextoldest->move);
   freeEntry(nextoldest);
   mTickCacheHead->numEntry--;
   if (mTickCacheHead->numEntry==1)
      mTickCacheHead->newest = mTickCacheHead->oldest;
}

void TickCache::ageCache(S32 numToAge, S32 len)
{
   AssertFatal(mTickCacheHead,"No tick cache head");
   AssertFatal(mTickCacheHead->numEntry>=numToAge,"Too few entries!");
   AssertFatal(mTickCacheHead->numEntry>numToAge,"Too few entries!");

   while (numToAge--)
      dropOldest();
   while (mTickCacheHead->numEntry>len)
      dropNextOldest();
   while (mTickCacheHead->numEntry<len)
      addCacheEntry();
}

void TickCache::beginCacheList()
{
   // get ready iterate from oldest to newest entry
   if (mTickCacheHead)
      mTickCacheHead->next = mTickCacheHead->oldest;
   // if no head, that's ok, we'll just add entries as we go
}

TickCacheEntry * TickCache::incCacheList(bool addIfNeeded)
{
   // continue iterating through cache, returning current entry
   // we'll add new entries if need be
   TickCacheEntry * ret = NULL;
   if (mTickCacheHead && mTickCacheHead->next)
   {
      ret = mTickCacheHead->next;
      mTickCacheHead->next = mTickCacheHead->next->next;
   }
   else if (addIfNeeded)
   {
      addCacheEntry();
      ret = mTickCacheHead->newest;
   }
   return ret;
}