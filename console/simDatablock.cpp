//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/simDatablock.h"

#include "console/console.h"
#include "console/consoleInternal.h"


IMPLEMENT_CO_DATABLOCK_V1(SimDataBlock);
SimObjectId SimDataBlock::sNextObjectId = DataBlockObjectIdFirst;
S32 SimDataBlock::sNextModifiedKey = 0;

//---------------------------------------------------------------------------

SimDataBlock::SimDataBlock()
{
   setModDynamicFields(true);
   setModStaticFields(true);
}

bool SimDataBlock::onAdd()
{
   Parent::onAdd();

   // This initialization is done here, and not in the constructor,
   // because some jokers like to construct and destruct objects
   // (without adding them to the manager) to check what class
   // they are.
   modifiedKey = ++sNextModifiedKey;
   AssertFatal(sNextObjectId <= DataBlockObjectIdLast,
      "Exceeded maximum number of data blocks");

   // add DataBlock to the DataBlockGroup unless it is client side ONLY DataBlock
   if ( !isClientOnly() )
      if (SimGroup* grp = Sim::getDataBlockGroup())
         grp->addObject(this);

   return true;
}

void SimDataBlock::assignId()
{
   // We don't want the id assigned by the manager, but it may have
   // already been assigned a correct data block id.
   if ( isClientOnly() )
      setId(sNextObjectId++);
}

void SimDataBlock::onStaticModified(const char* slotName, const char* newValue)
{
   modifiedKey = sNextModifiedKey++;
}

void SimDataBlock::packData(BitStream*)
{
}

void SimDataBlock::unpackData(BitStream*)
{
}

bool SimDataBlock::preload(bool, String&)
{
   return true;
}

void SimDataBlock::write(Stream &stream, U32 tabStop, U32 flags)
{
   // Only output selected objects if they want that.
   if((flags & SelectedOnly) && !isSelected())
      return;

   stream.writeTabs(tabStop);
   char buffer[1024];

   // Client side datablocks are created with 'new' while
   // regular server datablocks use the 'datablock' keyword.
   if ( isClientOnly() )
      dSprintf(buffer, sizeof(buffer), "new %s(%s) {\r\n", getClassName(), getName() ? getName() : "");
   else
      dSprintf(buffer, sizeof(buffer), "datablock %s(%s) {\r\n", getClassName(), getName() ? getName() : "");

   stream.write(dStrlen(buffer), buffer);
   writeFields(stream, tabStop + 1);

   stream.writeTabs(tabStop);
   stream.write(4, "};\r\n");
}

ConsoleFunction(preloadClientDataBlocks, void, 1, 1, "Preload all datablocks in client mode.  "
                "(Server parameter is set to false).  This will take some time to complete.")
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   // we go from last to first because we cut 'n pasted the loop from deleteDataBlocks
   SimGroup *grp = Sim::getDataBlockGroup();
   String errorStr;
   for(S32 i = grp->size() - 1; i >= 0; i--)
   {
      AssertFatal(dynamic_cast<SimDataBlock*>((*grp)[i]), "Doh! non-datablock in datablock group!");
      SimDataBlock *obj = (SimDataBlock*)(*grp)[i];
      if (!obj->preload(false, errorStr))
         Con::errorf("Failed to preload client datablock, %s: %s", obj->getName(), errorStr.c_str());
   }
}

ConsoleFunction(deleteDataBlocks, void, 1, 1, "Delete all the datablocks we've downloaded. "
                "This is usually done in preparation of downloading a new set of datablocks, "
                " such as occurs on a mission change, but it's also good post-mission cleanup.")
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   // delete from last to first:
   SimGroup *grp = Sim::getDataBlockGroup();
   grp->deleteAllObjects();
   SimDataBlock::sNextObjectId = DataBlockObjectIdFirst;
   SimDataBlock::sNextModifiedKey = 0;
}
