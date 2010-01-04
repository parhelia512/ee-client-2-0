//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowManager.h"

#include "sceneGraph/sceneGraph.h"
#include "materials/materialManager.h"

const String ShadowManager::ManagerTypeName("ShadowManager");

//------------------------------------------------------------------------------

bool ShadowManager::canActivate()
{
   return true;
}

//------------------------------------------------------------------------------

void ShadowManager::activate()
{
   mSceneManager = gClientSceneGraph; //;getWorld()->findWorldManager<SceneManager>();
}

//------------------------------------------------------------------------------

SceneGraph* ShadowManager::getSceneManager()
{
   return mSceneManager;   
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Runtime switching of shadow systems.  Requires correct world to be pushed at console.
ConsoleFunction( setShadowManager, bool, 1, 3, "string sShadowSystemName" )
{
   /*
   // Make sure this new one exists
   ShadowManager * newSM = dynamic_cast<ShadowManager*>(ConsoleObject::create(argv[1]));
   if (!newSM)
      return false;

   // Cleanup current
   ShadowManager * currentSM = world->findWorldManager<ShadowManager>();
   if (currentSM)
   {
      currentSM->deactivate();
      world->removeWorldManager(currentSM);
      delete currentSM;
   }

   // Add to world and init.
   world->addWorldManager(newSM);   
   newSM->activate();      
   MaterialManager::get()->reInitInstances();
   */
   return true;
}