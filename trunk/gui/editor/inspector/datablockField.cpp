//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/simBase.h"
#include "console/simDatablock.h"
#include "gui/editor/guiInspector.h"
#include "gui/editor/inspector/datablockField.h"
#include "gui/editor/inspector/group.h"
#include "gui/buttons/guiIconButtonCtrl.h"
#include "gui/editor/inspector/datablockField.h"

//-----------------------------------------------------------------------------
// GuiInspectorDatablockField 
// Field construction for datablock types
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorDatablockField);

static S32 QSORT_CALLBACK stringCompare(const void *a,const void *b)
{
   StringTableEntry sa = *(StringTableEntry*)a;
   StringTableEntry sb = *(StringTableEntry*)b;
   return(dStrnatcasecmp(sa, sb));
}

GuiInspectorDatablockField::GuiInspectorDatablockField( StringTableEntry className )
{
   setClassName(className);
};

void GuiInspectorDatablockField::setClassName( StringTableEntry className )
{
   // Walk the ACR list and find a matching class if any.
   AbstractClassRep *walk = AbstractClassRep::getClassList();
   while(walk)
   {
      if(!dStricmp(walk->getClassName(), className))
      {
         // Match!
         mDesiredClass = walk;
         return;
      }

      walk = walk->getNextClass();
   }

   // No dice.
   Con::warnf("GuiInspectorDatablockField::setClassName - no class '%s' found!", className);
   return;
}

GuiControl* GuiInspectorDatablockField::constructEditControl()
{
   GuiControl* retCtrl = new GuiPopUpMenuCtrl();

   // If we couldn't construct the control, bail!
   if( retCtrl == NULL )
      return retCtrl;

   GuiPopUpMenuCtrl *menu = dynamic_cast<GuiPopUpMenuCtrl*>(retCtrl);

   // Let's make it look pretty.
   retCtrl->setDataField( StringTable->insert("profile"), NULL, "InspectorTypeEnumProfile" );

   menu->setField("text", getData());

   _registerEditControl( retCtrl );

   // Configure it to update our value when the popup is closed
   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply(%d.getText());%d.inspect(%d);",getId(), retCtrl->getId(),mParent->mParent->getId(), mTarget->getId() );
   //dSprintf( szBuffer, 512, "%d.%s = %d.getText();%d.inspect(%d);",mTarget->getId(), getFieldName(), menu->getId(), mParent->mParent->getId(), mTarget->getId() );
   menu->setField("Command", szBuffer );

   Vector<StringTableEntry> entries;

   SimDataBlockGroup * grp = Sim::getDataBlockGroup();
   for(SimDataBlockGroup::iterator i = grp->begin(); i != grp->end(); i++)
   {
      SimDataBlock * datablock = dynamic_cast<SimDataBlock*>(*i);

      // Skip non-datablocks if we somehow encounter them.
      if(!datablock)
         continue;

      // Ok, now we have to figure inheritance info.
      if( datablock && datablock->getClassRep()->isClass(mDesiredClass) )
         entries.push_back(datablock->getName());
   }

   // sort the entries
   dQsort(entries.address(), entries.size(), sizeof(StringTableEntry), stringCompare);

   // add them to our enum
   for(U32 j = 0; j < entries.size(); j++)
      menu->addEntry(entries[j], 0);

   return retCtrl;
}
