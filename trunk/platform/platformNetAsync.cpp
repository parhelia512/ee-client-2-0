//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platformNetAsync.h"
#include "core/strings/stringFunctions.h"
#include "platform/threads/threadPool.h"
#include "console/console.h"

#if defined(TORQUE_OS_WIN32)
#  include <winsock.h>
#elif defined(TORQUE_OS_XENON)
#  include <Xtl.h>
#else
#  include <netdb.h>
#  include <unistd.h>
#endif

#include <errno.h>

NetAsync gNetAsync;

//--------------------------------------------------------------------------
//    NetAsync::NameLookupRequest.
//--------------------------------------------------------------------------

// internal structure for storing information about a name lookup request
struct NetAsync::NameLookupRequest
{
      NetSocket sock;
      char remoteAddr[4096];
      char out_h_addr[4096];
      int out_h_length;
      bool complete;

      NameLookupRequest()
      {
         sock = InvalidSocket;
         remoteAddr[0] = 0;
         out_h_addr[0] = 0;
         out_h_length = -1;
         complete = false;
      }
};

//--------------------------------------------------------------------------
//    NetAsync::NameLookupWorkItem.
//--------------------------------------------------------------------------

/// Work item issued to the thread pool for each lookup request.

struct NetAsync::NameLookupWorkItem : public ThreadPool::WorkItem
{
   typedef ThreadPool::WorkItem Parent;

   NameLookupWorkItem( NameLookupRequest& request, ThreadPool::Context* context = 0 )
      : Parent( context ),
        mRequest( request )
   {
   }

protected:
   virtual void execute()
   {
#ifndef TORQUE_OS_XENON
      // do it
      struct hostent* hostent = gethostbyname(mRequest.remoteAddr);
      if (hostent == NULL)
      {
         // oh well!  leave the lookup data unmodified (h_length) should
         // still be -1 from initialization
         mRequest.complete = true;
      }
      else
      {
         // copy the stuff we need from the hostent 
         dMemset(mRequest.out_h_addr, 0, 
            sizeof(mRequest.out_h_addr));
         dMemcpy(mRequest.out_h_addr, hostent->h_addr, hostent->h_length);

         mRequest.out_h_length = hostent->h_length;
         mRequest.complete = true;
      }
#else
      AssertFatal( false, "NetAsync not supported on Xenon" );
#endif
   }

private:
   NameLookupRequest&   mRequest;
};

//--------------------------------------------------------------------------
//    NetAsync.
//--------------------------------------------------------------------------

NetAsync::NetAsync()
{
   VECTOR_SET_ASSOCIATION( mLookupRequests );
}

void NetAsync::queueLookup(const char* remoteAddr, NetSocket socket)
{
   // do we have it already?
   
   unsigned int i = 0;
   for (i = 0; i < mLookupRequests.size(); ++i)
   {
      if (mLookupRequests[i].sock == socket)
         // found it.  ignore more than one lookup at a time for a socket.
         return;
   }

   // not found, so add it

   mLookupRequests.increment();
   NameLookupRequest& lookupRequest = mLookupRequests.last();
   lookupRequest.sock = socket;
   dStrncpy(lookupRequest.remoteAddr, remoteAddr, sizeof(lookupRequest.remoteAddr));

   ThreadSafeRef< NameLookupWorkItem > workItem( new NameLookupWorkItem( lookupRequest ) );
   ThreadPool::GLOBAL().queueWorkItem( workItem );
}

bool NetAsync::checkLookup(NetSocket socket, char* out_h_addr, 
                           int* out_h_length, int out_h_addr_size)
{
   bool found = false;

   // search for the socket
   RequestIterator iter;
   for (iter = mLookupRequests.begin(); 
        iter != mLookupRequests.end(); 
        ++iter)
      // if we found it and it is complete...
      if (socket == iter->sock && iter->complete)
      {
         // copy the lookup data to the callers parameters
         dMemcpy(out_h_addr, iter->out_h_addr, out_h_addr_size);
         *out_h_length = iter->out_h_length;
         found = true;
         break;
      }

   // we found the socket, so we are done with it.  erase.
   if (found)
      mLookupRequests.erase(iter);

   return found;
}
