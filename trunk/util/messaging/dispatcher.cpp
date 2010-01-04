//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "util/messaging/dispatcher.h"

#include "platform/threads/mutex.h"
#include "core/tSimpleHashTable.h"
#include "core/util/safeDelete.h"

namespace Dispatcher
{

//-----------------------------------------------------------------------------
// IMessageListener Methods
//-----------------------------------------------------------------------------

IMessageListener::~IMessageListener()
{
   for(S32 i = 0;i < mQueues.size();i++)
   {
      unregisterMessageListener(mQueues[i], this);
   }
}

void IMessageListener::onAddToQueue(StringTableEntry queue)
{
   // [tom, 8/20/2006] The dispatcher won't let us get added twice, so no need
   // to worry about it here.

   mQueues.push_back(queue);
}

void IMessageListener::onRemoveFromQueue(StringTableEntry queue)
{
   for(S32 i = 0;i < mQueues.size();i++)
   {
      if(mQueues[i] == queue)
      {
         mQueues.erase(i);
         return;
      }
   }
}

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// @brief Internal class used by the dispatcher
//-----------------------------------------------------------------------------
typedef struct _DispatchData
{
   void *mMutex;
   SimpleHashTable<MessageQueue> mQueues;
   U32 mLastAnonQueueID;

   _DispatchData()
   {
      mMutex = Mutex::createMutex();
      mLastAnonQueueID = 0;
   }

   ~_DispatchData()
   {
      if(Mutex::lockMutex( mMutex ) )
      {
         mQueues.clearTables();

         Mutex::unlockMutex( mMutex );
      }

      Mutex::destroyMutex( mMutex );
      //SAFE_DELETE(mMutex);
      mMutex = NULL;
   }

   const char *makeAnonQueueName()
   {
      char buf[512];
      dSprintf(buf, sizeof(buf), "AnonQueue.%lu", mLastAnonQueueID++);
      return StringTable->insert(buf);
   }
} _DispatchData;

static _DispatchData& _dispatcherGetGDispatchData()
{
   static _DispatchData dispatchData;
   return dispatchData;
}

#define gDispatchData _dispatcherGetGDispatchData()


//-----------------------------------------------------------------------------
// Queue Registration
//-----------------------------------------------------------------------------

bool isQueueRegistered(const char *name)
{
   MutexHandle mh;
   if(mh.lock(gDispatchData.mMutex, true))
   {
      return gDispatchData.mQueues.retreive(name) != NULL;
   }

   return false;
}

void registerMessageQueue(const char *name)
{
   if(isQueueRegistered(name))
      return;

   if(Mutex::lockMutex( gDispatchData.mMutex, true ))
   {
      MessageQueue *queue = new MessageQueue;
      queue->mQueueName = StringTable->insert(name);
      gDispatchData.mQueues.insert(queue, name);

      Mutex::unlockMutex( gDispatchData.mMutex );
   }
}

extern const char * registerAnonMessageQueue()
{
   const char *name = NULL;
   if(Mutex::lockMutex( gDispatchData.mMutex, true ))
   {
      name = gDispatchData.makeAnonQueueName();
      Mutex::unlockMutex( gDispatchData.mMutex );
   }

   if(name)
      registerMessageQueue(name);

   return name;
}

void unregisterMessageQueue(const char *name)
{
   MutexHandle mh;
   if(mh.lock(gDispatchData.mMutex, true))
   {
      MessageQueue *queue = gDispatchData.mQueues.remove(name);
      if(queue == NULL)
         return;

      // Tell the listeners about it
      for(S32 i = 0;i < queue->mListeners.size();i++)
      {
         queue->mListeners[i]->onRemoveFromQueue(name);
      }

      delete queue;
   }
}

//-----------------------------------------------------------------------------
// Message Listener Registration
//-----------------------------------------------------------------------------

bool registerMessageListener(const char *queue, IMessageListener *listener)
{
   if(! isQueueRegistered(queue))
      registerMessageQueue(queue);

   MutexHandle mh;

   if(! mh.lock(gDispatchData.mMutex, true))
      return false;

   MessageQueue *q = gDispatchData.mQueues.retreive(queue);
   if(q == NULL)
   {
      Con::errorf("Dispatcher::registerMessageListener - Queue '%s' not found?! It should have been added automatically!", queue);
      return false;
   }

   for(VectorPtr<IMessageListener *>::iterator i = q->mListeners.begin();i != q->mListeners.end();i++)
   {
      if(*i == listener)
         return false;
   }

   q->mListeners.push_front(listener);
   listener->onAddToQueue(StringTable->insert(queue));
   return true;
}

void unregisterMessageListener(const char *queue, IMessageListener *listener)
{
   if(! isQueueRegistered(queue))
      return;

   MutexHandle mh;

   if(! mh.lock(gDispatchData.mMutex, true))
      return;

   MessageQueue *q = gDispatchData.mQueues.retreive(queue);
   if(q == NULL)
      return;

   for(VectorPtr<IMessageListener *>::iterator i = q->mListeners.begin();i != q->mListeners.end();i++)
   {
      if(*i == listener)
      {
         listener->onRemoveFromQueue(StringTable->insert(queue));
         q->mListeners.erase(i);
         return;
      }
   }
}

//-----------------------------------------------------------------------------
// Dispatcher
//-----------------------------------------------------------------------------

bool dispatchMessage(const char *queue, const char *msg, const char *data)
{
   MutexHandle mh;

   if(! mh.lock(gDispatchData.mMutex, true))
      return true;

   MessageQueue *q = gDispatchData.mQueues.retreive(queue);
   if(q == NULL)
   {
      Con::errorf("Dispatcher::dispatchMessage - Attempting to dispatch to unknown queue '%s'", queue);
      return true;
   }

   return q->dispatchMessage(msg, data);
}


bool dispatchMessageObject(const char *queue, Message *msg)
{
   MutexHandle mh;

   if(msg == NULL)
      return true;

   msg->addReference();

   if(! mh.lock(gDispatchData.mMutex, true))
   {
      msg->freeReference();
      return true;
   }

   MessageQueue *q = gDispatchData.mQueues.retreive(queue);
   if(q == NULL)
   {
      Con::errorf("Dispatcher::dispatchMessage - Attempting to dispatch to unknown queue '%s'", queue);
      msg->freeReference();
      return true;
   }

   // [tom, 8/19/2006] Make sure that the message is registered with the sim, since
   // when it's ref count is zero it'll be deleted with deleteObject()
   if(! msg->isProperlyAdded())
   {
      SimObjectId id = Message::getNextMessageID();
      if(id != 0xffffffff)
         msg->registerObject(id);
      else
      {
         Con::errorf("dispatchMessageObject: Message was not registered and no more object IDs are available for messages");

         msg->freeReference();
         return false;
      }
   }

   bool bResult = q->dispatchMessageObject(msg);
   msg->freeReference();

   return bResult;
}

//-----------------------------------------------------------------------------
// Internal Functions
//-----------------------------------------------------------------------------

MessageQueue * getMessageQueue(const char *name)
{
   return gDispatchData.mQueues.retreive(name);
}

extern bool lockDispatcherMutex()
{
   return Mutex::lockMutex(gDispatchData.mMutex);
}

extern void unlockDispatcherMutex()
{
   Mutex::unlockMutex(gDispatchData.mMutex);
}

} // end namespace Dispatcher

//-----------------------------------------------------------------------------
// Console Methods
//-----------------------------------------------------------------------------

using namespace Dispatcher;

ConsoleFunction(isQueueRegistered, bool, 2, 2, "(queueName)")
{
   return isQueueRegistered(argv[1]);
}

ConsoleFunction(registerMessageQueue, void, 2, 2, "(queueName)")
{
   return registerMessageQueue(argv[1]);
}

ConsoleFunction(unregisterMessageQueue, void, 2, 2, "(queueName)")
{
   return unregisterMessageQueue(argv[1]);
}

//-----------------------------------------------------------------------------

ConsoleFunction(registerMessageListener, bool, 3, 3, "(queueName, listener)")
{
   IMessageListener *listener = dynamic_cast<IMessageListener *>(Sim::findObject(argv[2]));
   if(listener == NULL)
   {
      Con::errorf("registerMessageListener - Unable to find listener object, not an IMessageListener ?!");
      return false;
   }

   return registerMessageListener(argv[1], listener);
}

ConsoleFunction(unregisterMessageListener, void, 3, 3, "(queueName, listener)")
{
   IMessageListener *listener = dynamic_cast<IMessageListener *>(Sim::findObject(argv[2]));
   if(listener == NULL)
   {
      Con::errorf("unregisterMessageListener - Unable to find listener object, not an IMessageListener ?!");
      return;
   }

   unregisterMessageListener(argv[1], listener);
}

//-----------------------------------------------------------------------------

ConsoleFunction(dispatchMessage, bool, 3, 4, "(queueName, event, data)")
{
   return dispatchMessage(argv[1], argv[2], argc > 3 ? argv[3] : "" );
}

ConsoleFunction(dispatchMessageObject, bool, 3, 3, "(queueName, message)")
{
   Message *msg = dynamic_cast<Message *>(Sim::findObject(argv[2]));
   if(msg == NULL)
   {
      Con::errorf("dispatchMessageObject - Unable to find message object");
      return false;
   }

   return dispatchMessageObject(argv[1], msg);
}
