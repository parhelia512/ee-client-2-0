//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SCRIPTOBJECTS_H_
#define _SCRIPTOBJECTS_H_

#ifndef _CONSOLEINTERNAL_H_
#include "console/consoleInternal.h"
#endif

//-----------------------------------------------------------------------------
// Script object placeholder
//-----------------------------------------------------------------------------

class ScriptObject : public SimObject
{
   typedef SimObject Parent;

public:
   ScriptObject();
   bool onAdd();
   void onRemove();

   DECLARE_CONOBJECT(ScriptObject);
};

#endif