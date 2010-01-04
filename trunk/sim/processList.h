//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PROCESSLIST_H_
#define _PROCESSLIST_H_

#ifndef _SIM_H_
#include "console/sim.h"
#endif
#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif

//----------------------------------------------------------------------------

#define TickMs      32
#define TickSec     (F32(TickMs) / 1000.0f)

//----------------------------------------------------------------------------

class ProcessObject
{
   friend class ProcessList;
   friend class ClientProcessList;
   friend class ServerProcessList;

public:

   ProcessObject() { mProcessTag = 0; mProcessLink.next=mProcessLink.prev=this; mOrderGUID=0; }
   virtual ProcessObject * getAfterObject() { return NULL; }

protected:

   struct Link
   {
      ProcessObject *next;
      ProcessObject *prev;
   };

   // Processing interface
   void plUnlink();
   void plLinkAfter(ProcessObject*);
   void plLinkBefore(ProcessObject*);
   void plJoin(ProcessObject*);

   U32 mProcessTag;                       // Tag used during sort
   U32 mOrderGUID;                        // UID for keeping order synced (e.g., across network or runs of sim)
   Link mProcessLink;                     // Ordered process queue
};

//----------------------------------------------------------------------------

typedef Signal<void()> PreTickSignal;
typedef Signal<void(SimTime)> PostTickSignal;

/// List of ProcessObjects.
class ProcessList
{

public:
   ProcessList();

   void markDirty()  { mDirty = true; }
   bool isDirty()  { return mDirty; }

   virtual void addObject(ProcessObject * obj);

   SimTime getLastTime() { return mLastTime; }
   F32 getLastDelta() { return mLastDelta; }
   F32 getLastInterpDelta() { return mLastDelta / F32(TickMs); }
   U32 getTotalTicks() { return mTotalTicks; }
   void dumpToConsole();

   PreTickSignal& preTickSignal() { return mPreTick; }

   PostTickSignal& postTickSignal() { return mPostTick; }

   /// @name Advancing Time
   /// The advance time functions return true if a tick was processed.
   /// @{

   bool advanceTime(SimTime timeDelta);

protected:

   ProcessObject mHead;

   U32 mCurrentTag;
   bool mDirty;

   U32 mTotalTicks;
   SimTime mLastTick;
   SimTime mLastTime;
   F32 mLastDelta;

   PreTickSignal mPreTick;
   PostTickSignal mPostTick;

   void orderList();
   virtual void advanceObjects();
   virtual void onAdvanceObjects() { advanceObjects(); }
   virtual void onTickObject(ProcessObject *) {}
};

#endif // _PROCESSLIST_H_