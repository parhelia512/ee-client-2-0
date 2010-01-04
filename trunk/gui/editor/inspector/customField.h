//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUI_INSPECTOR_CUSTOMFIELD_H_
#define _GUI_INSPECTOR_CUSTOMFIELD_H_

#include "console/simFieldDictionary.h"
#include "gui/editor/inspector/field.h"

class GuiInspectorCustomField : public GuiInspectorField
{
   typedef GuiInspectorField Parent;   

public:

   GuiInspectorCustomField( GuiInspector *inspector, GuiInspectorGroup* parent, SimObjectPtr<SimObject> target, SimFieldDictionary::Entry* field );
   GuiInspectorCustomField();
   ~GuiInspectorCustomField() {};

   DECLARE_CONOBJECT( GuiInspectorCustomField );

   virtual void             setData( StringTableEntry data );
   virtual StringTableEntry getData();
   virtual void             updateValue();
   virtual StringTableEntry getFieldName() { return StringTable->insert( "" ); }

   virtual void setDoc( StringTableEntry data );
   virtual void setToolTip( StringTableEntry data );

   virtual bool onAdd();

   virtual void setInspectorField( AbstractClassRep::Field *field, 
                                   StringTableEntry caption = NULL,
                                   const char *arrayIndex = NULL );
   
   virtual GuiControl* constructEditControl();

   virtual void setValue( StringTableEntry newValue );

protected:

   virtual void _executeSelectedCallback();

protected:

   StringTableEntry  mCustomValue;
   StringTableEntry  mDoc;
};

#endif // _GUI_INSPECTOR_DYNAMICFIELD_H_
