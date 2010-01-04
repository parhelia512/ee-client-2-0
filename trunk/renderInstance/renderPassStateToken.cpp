//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "renderInstance/renderPassStateToken.h"
#include "console/consoleTypes.h"

IMPLEMENT_CONOBJECT(RenderPassStateToken);

void RenderPassStateToken::process(SceneState *state, RenderPassStateBin *callingBin)
{
   TORQUE_UNUSED(state);
   TORQUE_UNUSED(callingBin);
   AssertWarn(false, "RenderPassStateToken is an abstract class, you must re-implement process()");
}

void RenderPassStateToken::reset()
{
   AssertWarn(false, "RenderPassStateToken is an abstract class, you must re-implement reset()");
}

void RenderPassStateToken::enable( bool enabled /*= true*/ )
{
   TORQUE_UNUSED(enabled);
   AssertWarn(false, "RenderPassStateToken is an abstract class, you must re-implement enable()");
}

bool RenderPassStateToken::isEnabled() const
{
   AssertWarn(false, "RenderPassStateToken is an abstract class, you must re-implement isEnabled()");
   return false;
}

static bool _set_enable(void* obj, const char* data)
{
   reinterpret_cast<RenderPassStateToken *>(obj)->enable(dAtob(data));
   return false;
}

static const char *_get_enable(void* obj, const char* data)
{
   TORQUE_UNUSED(data);
   return reinterpret_cast<RenderPassStateToken *>(obj)->isEnabled() ? "true" : "false";
}

void RenderPassStateToken::initPersistFields()
{
   addProtectedField("enabled", TypeBool, NULL, &_set_enable, &_get_enable, "Enables or disables this token.");
   Parent::initPersistFields();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(RenderPassStateBin);

RenderPassStateBin::RenderPassStateBin() : Parent()
{
   // Disable adding instances
   setProcessAddOrder(RenderPassManager::PROCESSADD_NONE);
}

RenderPassStateBin::~RenderPassStateBin()
{

}

RenderBinManager::AddInstResult RenderPassStateBin::addElement( RenderInst *inst )
{
   return RenderBinManager::arSkipped;
}

void RenderPassStateBin::render(SceneState *state)
{
   if(mStateToken.isValid())
      mStateToken->process(state, this);
}

void RenderPassStateBin::clear()
{
   if(mStateToken.isValid())
      mStateToken->reset();
}

void RenderPassStateBin::sort()
{

}

void RenderPassStateBin::initPersistFields()
{
   addField("stateToken", TypeSimObjectPtr, Offset(mStateToken, RenderPassStateBin));
   Parent::initPersistFields();
}

//------------------------------------------------------------------------------

ConsoleMethod(RenderPassStateToken, enable, void, 2, 2, "")
{
   object->enable(true);
}

ConsoleMethod(RenderPassStateToken, disable, void, 2, 2, "")
{
   object->enable(false);
}

ConsoleMethod(RenderPassStateToken, toggle, void, 2, 2, "")
{
   object->enable(!object->isEnabled());
}