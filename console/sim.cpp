//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"

#include "console/sim.h"
#include "console/simEvents.h"
#include "console/simObject.h"
#include "console/simSet.h"

namespace Sim
{
   // Don't forget to InstantiateNamed* in simManager.cc - DMM
   ImplementNamedSet(ActiveActionMapSet)
   ImplementNamedSet(GhostAlwaysSet)
   ImplementNamedSet(WayPointSet)
   ImplementNamedSet(fxReplicatorSet)
   ImplementNamedSet(fxFoliageSet)
   ImplementNamedSet(BehaviorSet)
   ImplementNamedSet(MaterialSet)
   ImplementNamedSet(SFXSourceSet)
   ImplementNamedSet(TerrainMaterialSet)
   ImplementNamedGroup(ActionMapGroup)
   ImplementNamedGroup(ClientGroup)
   ImplementNamedGroup(GuiGroup)
   ImplementNamedGroup(GuiDataGroup)
   ImplementNamedGroup(TCPGroup)

   //groups created on the client
   ImplementNamedGroup(ClientConnectionGroup)
   ImplementNamedGroup(ChunkFileGroup)
   ImplementNamedSet(sgMissionLightingFilterSet)
}

//-----------------------------------------------------------------------------
// Console Functions
//-----------------------------------------------------------------------------

ConsoleFunctionGroupBegin ( SimFunctions, "Functions relating to Sim.");

ConsoleFunction(nameToID, S32, 2, 2, "nameToID(object)")
{
   TORQUE_UNUSED(argc);
   SimObject *obj = Sim::findObject(argv[1]);
   if(obj)
      return obj->getId();
   else
      return -1;
}

ConsoleFunction(isObject, bool, 2, 2, "isObject(object)")
{
   TORQUE_UNUSED(argc);
   if (!dStrcmp(argv[1], "0") || !dStrcmp(argv[1], ""))
      return false;
   else
      return (Sim::findObject(argv[1]) != NULL);
}

ConsoleFunction(spawnObject, S32, 3, 6, "spawnObject(class [, dataBlock, name, properties, script])")
{
   String spawnClass(argv[1]);
   String spawnDataBlock;
   String spawnName;
   String spawnProperties;
   String spawnScript;

   if (argc >= 3)
      spawnDataBlock = argv[2];
   if (argc >= 4)
      spawnName = argv[3];
   if (argc >= 5)
      spawnProperties = argv[4];
   if (argc >= 6)
      spawnScript = argv[5];

   SimObject* spawnObject = Sim::spawnObject(spawnClass, spawnDataBlock, spawnName, spawnProperties, spawnScript);

   if (spawnObject)
      return spawnObject->getId();
   else
      return -1;
}

ConsoleFunction(cancel,void,2,2,"cancel(eventId)")
{
   TORQUE_UNUSED(argc);
   Sim::cancelEvent(dAtoi(argv[1]));
}

ConsoleFunction(isEventPending, bool, 2, 2, "isEventPending(%scheduleId);")
{
   TORQUE_UNUSED(argc);
   return Sim::isEventPending(dAtoi(argv[1]));
}

ConsoleFunction(getEventTimeLeft, S32, 2, 2, "getEventTimeLeft(scheduleId) Get the time left in ms until this event will trigger.")
{
   return Sim::getEventTimeLeft(dAtoi(argv[1]));
}

ConsoleFunction(getScheduleDuration, S32, 2, 2, "getScheduleDuration(%scheduleId);")
{
   TORQUE_UNUSED(argc);   S32 ret = Sim::getScheduleDuration(dAtoi(argv[1]));
   return ret;
}

ConsoleFunction(getTimeSinceStart, S32, 2, 2, "getTimeSinceStart(%scheduleId);")
{
   TORQUE_UNUSED(argc);   S32 ret = Sim::getTimeSinceStart(dAtoi(argv[1]));
   return ret;
}

ConsoleFunction(schedule, S32, 4, 0, "schedule(time, refobject|0, command, <arg1...argN>)")
{
   U32 timeDelta = U32(dAtof(argv[1]));
   SimObject *refObject = Sim::findObject(argv[2]);
   if(!refObject)
   {
      if(argv[2][0] != '0')
         return 0;

      refObject = Sim::getRootGroup();
   }
   SimConsoleEvent *evt = new SimConsoleEvent(argc - 3, argv + 3, false);

   S32 ret = Sim::postEvent(refObject, evt, Sim::getCurrentTime() + timeDelta);
   // #ifdef DEBUG
   //    Con::printf("ref %s schedule(%s) = %d", argv[2], argv[3], ret);
   //    Con::executef( "backtrace");
   // #endif
   return ret;
}

ConsoleFunction(getUniqueName, const char*, 2, 2, 
	"( String baseName )\n"
	"Returns a unique unused SimObject name based on a given base name.")
{
   String outName = Sim::getUniqueName( argv[1] );
   
   if ( outName.isEmpty() )
      return NULL;

   char *buffer = Con::getReturnBuffer( outName.size() );
   dStrcpy( buffer, outName );

   return buffer;
}

ConsoleFunctionGroupEnd( SimFunctions );
