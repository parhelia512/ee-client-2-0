//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/editor/inspector/variableInspector.h"
#include "gui/editor/inspector/variableGroup.h"

GuiVariableInspector::GuiVariableInspector()
{
}

GuiVariableInspector::~GuiVariableInspector()
{
}

IMPLEMENT_CONOBJECT(GuiVariableInspector);


void GuiVariableInspector::loadVars( String searchStr )
{     
   clearGroups();

   GuiInspectorVariableGroup *group = new GuiInspectorVariableGroup();

   group->mHideHeader = true;
   group->mCanCollapse = false;
   group->mParent = this;
   group->mCaption = StringTable->insert( "Global Variables" );
   group->mSearchString = searchStr;

   if( group != NULL )
   {
      group->registerObject();
      mGroups.push_back( group );
      addObject( group );
   }   

   //group->inspectGroup();
}

ConsoleMethod( GuiVariableInspector, loadVars, void, 3, 3, "loadVars( searchString )" )
{
   object->loadVars( argv[2] );
}