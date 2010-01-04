//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SHADOWMANAGER_H_
#define _SHADOWMANAGER_H_

#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif
#ifndef _TORQUE_STRING_H_
#include "core/util/str.h"
#endif

class SceneGraph;

class ShadowManager
{
public:
   ShadowManager() : mSceneManager(NULL) {}
   virtual ~ShadowManager() { }

   // Called when the shadow manager should become active
   virtual void activate();

   // Called when we don't want the shadow manager active (should clean up)
   virtual void deactivate() { }

   // Return an "id" that other systems can use to load different versions of assets (custom shaders, etc.)
   // Should be short and contain no spaces and safe for filename use.
   //virtual const char* getId() const = 0;

   // Scene manager   
   virtual SceneGraph* getSceneManager();

   // Called to find out if it is valid to activate this shadow system.  If not, we should print out
   // a console warning explaining why.
   virtual bool canActivate();

   // SimWorldManager
   static const String ManagerTypeName;
   const String & getManagerTypeName() const { return ManagerTypeName; }

private:
   SceneGraph* mSceneManager;
};

#endif
