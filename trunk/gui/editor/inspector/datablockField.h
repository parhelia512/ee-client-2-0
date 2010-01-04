//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUI_INSPECTOR_DATABLOCKFIELD_H_
#define _GUI_INSPECTOR_DATABLOCKFIELD_H_

#include "gui/editor/inspector/field.h"


//-----------------------------------------------------------------------------
// GuiInspectorDatablockField - custom field type for datablock enumeration
//-----------------------------------------------------------------------------
class GuiInspectorDatablockField : public GuiInspectorField
{
private:
   typedef GuiInspectorField Parent;

   AbstractClassRep *mDesiredClass;
public:
   DECLARE_CONOBJECT(GuiInspectorDatablockField);
   GuiInspectorDatablockField( StringTableEntry className );
   GuiInspectorDatablockField() { mDesiredClass = NULL; };

   void setClassName( StringTableEntry className );

   //-----------------------------------------------------------------------------
   // Override able methods for custom edit fields (Both are REQUIRED)
   //-----------------------------------------------------------------------------
   virtual GuiControl* constructEditControl();

};

#endif