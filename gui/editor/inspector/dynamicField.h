//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUI_INSPECTOR_DYNAMICFIELD_H_
#define _GUI_INSPECTOR_DYNAMICFIELD_H_

#include "console/simFieldDictionary.h"
#include "gui/editor/inspector/field.h"


class GuiInspectorDynamicField : public GuiInspectorField
{
   typedef GuiInspectorField Parent;   

public:

   GuiInspectorDynamicField( GuiInspector *inspector, GuiInspectorGroup* parent, SimObjectPtr<SimObject> target, SimFieldDictionary::Entry* field );
   GuiInspectorDynamicField() {};
   ~GuiInspectorDynamicField() {};

   DECLARE_CONOBJECT( GuiInspectorDynamicField );

   virtual void             setData( StringTableEntry data );
   virtual StringTableEntry getData();
   virtual void             updateValue();
   virtual StringTableEntry getFieldName() { return ( mDynField != NULL ) ? mDynField->slotName : StringTable->insert( "" ); }

   virtual bool onAdd();

   void renameField( StringTableEntry newFieldName );
   GuiControl* constructRenameControl();

   virtual bool updateRects();
   virtual void setInspectorField( AbstractClassRep::Field *field, 
                                   StringTableEntry caption = NULL,
                                   const char *arrayIndex = NULL );

protected:

   virtual void _executeSelectedCallback();

protected:

   SimFieldDictionary::Entry*    mDynField;
   SimObjectPtr<GuiTextEditCtrl> mRenameCtrl;
   GuiBitmapButtonCtrl*          mDeleteButton;
   RectI                         mDeleteRect;
   RectI                         mValueRect;
};

#endif // _GUI_INSPECTOR_DYNAMICFIELD_H_
