//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "util/messaging/scriptMsgListener.h"

#include "console/consoleTypes.h"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ScriptMsgListener::ScriptMsgListener()
{
   mNSLinkMask = LinkSuperClassName | LinkClassName;
}

IMPLEMENT_CONOBJECT(ScriptMsgListener);

//-----------------------------------------------------------------------------

bool ScriptMsgListener::onAdd()
{
   if(! Parent::onAdd())
      return false;

   linkNamespaces();
   Con::executef(this, "onAdd");
   return true;
}

void ScriptMsgListener::onRemove()
{
   Con::executef(this, "onRemove");
   unlinkNamespaces();
   
   Parent::onRemove();
}

//-----------------------------------------------------------------------------
// Public Methods
//-----------------------------------------------------------------------------

bool ScriptMsgListener::onMessageReceived(StringTableEntry queue, const char* event, const char* data)
{
   return dAtob(Con::executef(this, "onMessageReceived", queue, event, data));
}

bool ScriptMsgListener::onMessageObjectReceived(StringTableEntry queue, Message *msg)
{
   return dAtob(Con::executef(this, "onMessageObjectReceived", queue, Con::getIntArg(msg->getId())));
}

//-----------------------------------------------------------------------------

void ScriptMsgListener::onAddToQueue(StringTableEntry queue)
{
   Con::executef(this, "onAddToQueue", queue);
   IMLParent::onAddToQueue(queue);
}

void ScriptMsgListener::onRemoveFromQueue(StringTableEntry queue)
{
   Con::executef(this, "onRemoveFromQueue", queue);
   IMLParent::onRemoveFromQueue(queue);
}
