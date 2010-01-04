//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef TORQUE_OS_XENON
#include "platformWin32/platformWin32.h"
#endif
#include "platform/threads/thread.h"
#include "platform/threads/semaphore.h"
#include "platform/platformIntrinsics.h"
#include "core/util/safeDelete.h"

#include <process.h> // [tom, 4/20/2006] for _beginthread()

ThreadManager::MainThreadId ThreadManager::smMainThreadId;

//-----------------------------------------------------------------------------
// Thread data
//-----------------------------------------------------------------------------

class PlatformThreadData
{
public:
   ThreadRunFunction       mRunFunc;
   void*                   mRunArg;
   Thread*                 mThread;
   HANDLE                  mThreadHnd;
   Semaphore               mGateway;
   U32                     mThreadID;
   U32                     mDead;

   PlatformThreadData()
   {
      mRunFunc    = NULL;
      mRunArg     = 0;
      mThread     = 0;
      mThreadHnd  = 0;
      mDead       = false;
   };
};

//-----------------------------------------------------------------------------
// Static Functions/Methods
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Function:    ThreadRunHandler
// Summary:     Calls Thread::run() with the thread's specified run argument.
//               Neccesary because Thread::run() is provided as a non-threaded
//               way to execute the thread's run function. So we have to keep
//               track of the thread's lock here.
static unsigned int __stdcall ThreadRunHandler(void * arg)
{
   PlatformThreadData* mData = reinterpret_cast<PlatformThreadData*>(arg);
   mData->mThreadID = GetCurrentThreadId();

   ThreadManager::addThread(mData->mThread);
   mData->mThread->run(mData->mRunArg);
   ThreadManager::removeThread(mData->mThread);

   bool autoDelete = mData->mThread->autoDelete;

   mData->mThreadHnd = NULL; // mark as dead
   dCompareAndSwap( mData->mDead, false, true );
   mData->mGateway.release(); // don't access data after this.

   if( autoDelete )
      delete mData->mThread; // Safe as we own the data.

   _endthreadex( 0 );
   return 0;
}

//-----------------------------------------------------------------------------
// Constructor/Destructor
//-----------------------------------------------------------------------------

Thread::Thread(ThreadRunFunction func /* = 0 */, void *arg /* = 0 */, bool start_thread /* = true */, bool autodelete /*= false*/)
   : autoDelete( autodelete )
{
   AssertFatal( !start_thread, "Thread::Thread() - auto-starting threads from ctor has been disallowed since the run() method is virtual" );
   
   mData = new PlatformThreadData;
   mData->mRunFunc = func;
   mData->mRunArg = arg;
   mData->mThread = this;
}

Thread::~Thread()
{
   stop();
   if( isAlive() )
      join();

   SAFE_DELETE(mData);
}

//-----------------------------------------------------------------------------
// Public Methods
//-----------------------------------------------------------------------------

void Thread::start( void* arg )
{
   AssertFatal( !mData->mThreadHnd,
      "Thread::start() - thread already started" );

   // cause start to block out other pthreads from using this Thread, 
   // at least until ThreadRunHandler exits.
   mData->mGateway.acquire();
   
   // reset the shouldStop flag, so we'll know when someone asks us to stop.
   shouldStop = false;
   
   mData->mDead = false;
   
   if( !mData->mRunArg )
      mData->mRunArg = arg;

   mData->mThreadHnd = (HANDLE)_beginthreadex(0, 0, ThreadRunHandler, mData, 0, 0);
}

bool Thread::join()
{
   mData->mGateway.acquire();
   AssertFatal( !isAlive(), "Thread::join() - thread still alive after join" );
   mData->mGateway.release(); // release for further joins
   return true;
}

void Thread::run(void *arg /* = 0 */)
{
   if(mData->mRunFunc)
      mData->mRunFunc(arg);
}

bool Thread::isAlive()
{
   return ( !mData->mDead );
}

U32 Thread::getId()
{
   return mData->mThreadID;
}

void Thread::_setName( const char* name )
{
#if defined( TORQUE_DEBUG ) && defined( TORQUE_COMPILER_VISUALC ) && defined( TORQUE_OS_WIN32 )

   // See http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx

   #define MS_VC_EXCEPTION 0x406D1388

   #pragma pack(push,8)
   typedef struct tagTHREADNAME_INFO
   {
      DWORD dwType; // Must be 0x1000.
      LPCSTR szName; // Pointer to name (in user addr space).
      DWORD dwThreadID; // Thread ID (-1=caller thread).
      DWORD dwFlags; // Reserved for future use, must be zero.
   } THREADNAME_INFO;
   #pragma pack(pop)

   Sleep(10);
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = name;
   info.dwThreadID = getId();
   info.dwFlags = 0;

   __try
   {
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
#endif
}

U32 ThreadManager::getCurrentThreadId()
{
   return GetCurrentThreadId();
}

bool ThreadManager::compare(U32 threadId_1, U32 threadId_2)
{
   return (threadId_1 == threadId_2);
}
