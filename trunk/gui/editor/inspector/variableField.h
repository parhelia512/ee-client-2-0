//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUI_INSPECTOR_VARIABLEFIELD_H_
#define _GUI_INSPECTOR_VARIABLEFIELD_H_

#ifndef _GUI_INSPECTOR_FIELD_H_
#include "gui/editor/inspector/field.h"
#endif

class GuiInspectorGroup;
class GuiInspector;


class GuiInspectorVariableField : public GuiInspectorField
{
   friend class GuiInspectorField;

public:

   typedef GuiInspectorField Parent;

   GuiInspectorVariableField();
   virtual ~GuiInspectorVariableField();

   DECLARE_CONOBJECT( GuiInspectorVariableField );
   DECLARE_CATEGORY( "Gui Editor" );

   virtual bool onAdd();


   virtual void setValue( StringTableEntry newValue );
   virtual const char* getValue() { return NULL; }
   virtual void updateValue();
   virtual void setData( StringTableEntry data );
   virtual StringTableEntry getData();
   virtual void updateData() {};

protected:

};

#endif // _GUI_INSPECTOR_VARIABLEFIELD_H_
