//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/simBase.h"
#include "console/consoleTypes.h"
#include "console/scriptObjects.h"
#include "console/simBase.h"

IMPLEMENT_CONOBJECT(ScriptObject);

ScriptObject::ScriptObject()
{
   mNSLinkMask = LinkSuperClassName | LinkClassName;
}

bool ScriptObject::onAdd()
{
   if (!Parent::onAdd())
      return false;

   // Call onAdd in script!
   Con::executef(this, "onAdd", Con::getIntArg(getId()));
   return true;
}

void ScriptObject::onRemove()
{
   // We call this on this objects namespace so we unlink them after. - jdd
   //
   // Call onRemove in script!
   Con::executef(this, "onRemove", Con::getIntArg(getId()));

   Parent::onRemove();
}

//-----------------------------------------------------------------------------
// Script group placeholder
//-----------------------------------------------------------------------------

class ScriptGroup : public SimGroup
{
   typedef SimGroup Parent;
   
public:
   ScriptGroup();
   bool onAdd();
   void onRemove();

   DECLARE_CONOBJECT(ScriptGroup);
};

IMPLEMENT_CONOBJECT(ScriptGroup);

ScriptGroup::ScriptGroup()
{
   mNSLinkMask = LinkSuperClassName | LinkClassName;
}

bool ScriptGroup::onAdd()
{
   if (!Parent::onAdd())
      return false;

   // Call onAdd in script!
   Con::executef(this, "onAdd", Con::getIntArg(getId()));
   return true;
}

void ScriptGroup::onRemove()
{
   // Call onRemove in script!
   Con::executef(this, "onRemove", Con::getIntArg(getId()));

   Parent::onRemove();
}
