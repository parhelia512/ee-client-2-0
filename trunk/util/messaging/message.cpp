//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "util/messaging/message.h"

#include "console/consoleTypes.h"
#include "core/util/safeDelete.h"
#include "core/stream/bitStream.h"

//-----------------------------------------------------------------------------

namespace Sim
{
extern SimIdDictionary *gIdDictionary;
}

//-----------------------------------------------------------------------------
// Constructor/Destructor
//-----------------------------------------------------------------------------

Message::Message()
{
   mRefCount = 0;

   mNSLinkMask = LinkSuperClassName | LinkClassName;
}


IMPLEMENT_CONOBJECT(Message);

//-----------------------------------------------------------------------------

bool Message::onAdd()
{
   if(! Parent::onAdd())
      return false;

   linkNamespaces();
   Con::executef(this, "onAdd");
   return true;
}

void Message::onRemove()
{
   Con::executef(this, "onRemove");
   unlinkNamespaces();
   
   Parent::onRemove();
}

//-----------------------------------------------------------------------------
// Public Methods
//-----------------------------------------------------------------------------

SimObjectId Message::getNextMessageID()
{
   for(S32 i = MessageObjectIdFirst;i < MessageObjectIdLast;i++)
   {
      if(Sim::gIdDictionary->find(i) == NULL)
         return i;
   }

   // Oh shit ...
   return 0xffffffff;
}

//-----------------------------------------------------------------------------

const char *Message::getType()
{
   if(mClassName && mClassName[0] != 0)
      return mClassName;

   return getClassName();
}

//-----------------------------------------------------------------------------

void Message::addReference()
{
   mRefCount++;
}

void Message::freeReference()
{
   AssertFatal(mRefCount >= 0, "Negative refcount ? Someone's not cleaning up properly!");

   mRefCount--;
   if(mRefCount <= 0)
   {
      // [tom, 8/26/2006] When messages are dispatched, the calling code assumes
      // that dispatchMessage() will free the message unless a reference has been
      // added.
      //
      // It is possible if dispatchMessage() fails for the message to not get added
      // to the sim, and thus we need to delete this; in that case.
      if( isProperlyAdded() )
         deleteObject();
      else
         delete this;
      return;
   }
}

//-----------------------------------------------------------------------------
// Console Methods
//-----------------------------------------------------------------------------

ConsoleMethod(Message, getType, const char *, 2, 2, "() Get message type (script class name or C++ class name if no script defined class)")
{
   return object->getType();
}

//-----------------------------------------------------------------------------

ConsoleMethod(Message, addReference, void, 2, 2, "() Increment the reference count for this message")
{
   object->addReference();
}

ConsoleMethod(Message, freeReference, void, 2, 2, "() Decrement the reference count for this message")
{
   object->freeReference();
}
