//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "util/messaging/messageForwarder.h"

#include "console/consoleTypes.h"

//-----------------------------------------------------------------------------
// Constructor/Destructor
//-----------------------------------------------------------------------------

MessageForwarder::MessageForwarder()
{
   mToQueue = "";
}

MessageForwarder::~MessageForwarder()
{
}

IMPLEMENT_CONOBJECT(MessageForwarder);

//-----------------------------------------------------------------------------

void MessageForwarder::initPersistFields()
{
   addField("toQueue", TypeCaseString, Offset(mToQueue, MessageForwarder), "Queue to forward to");

   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------
// Public Methods
//-----------------------------------------------------------------------------

bool MessageForwarder::onMessageReceived(StringTableEntry queue, const char *event, const char *data)
{
   if(*mToQueue)
      Dispatcher::dispatchMessage(queue, event, data);
   return Parent::onMessageReceived(queue, event, data);
}

bool MessageForwarder::onMessageObjectReceived(StringTableEntry queue, Message *msg)
{
   if(*mToQueue)
      Dispatcher::dispatchMessageObject(mToQueue, msg);
   return Parent::onMessageObjectReceived(queue, msg);
}
