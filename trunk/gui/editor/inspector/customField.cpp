//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/editor/inspector/customField.h"
#include "gui/editor/guiInspector.h"

//-----------------------------------------------------------------------------
// GuiInspectorCustomField - Child class of GuiInspectorField 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT( GuiInspectorCustomField );

GuiInspectorCustomField::GuiInspectorCustomField( GuiInspector *inspector,
                                                    GuiInspectorGroup* parent, 
                                                    SimObjectPtr<SimObject> target, 
                                                    SimFieldDictionary::Entry* field )
{
   mInspector = inspector;
   mParent = parent;
   mTarget = target;
   setBounds(0,0,100,20);   

   mCustomValue = StringTable->insert("");
   mDoc = StringTable->insert("");
}

GuiInspectorCustomField::GuiInspectorCustomField()
{
   mInspector = NULL;
   mParent = NULL;
   mTarget = NULL;
   mCustomValue = StringTable->insert("");
   mDoc = StringTable->insert("");
}

void GuiInspectorCustomField::setData( StringTableEntry data )
{
   mCustomValue = data;

   // Force our edit to update
   updateValue();
}

StringTableEntry GuiInspectorCustomField::getData()
{
   return mCustomValue;
}

void GuiInspectorCustomField::updateValue()
{
   setValue( mCustomValue );
}

void GuiInspectorCustomField::setDoc( StringTableEntry data )
{
   mDoc = data;
}

void GuiInspectorCustomField::setToolTip( StringTableEntry data )
{
   mEdit->setDataField( StringTable->insert("tooltipprofile"), NULL, "GuiToolTipProfile" );
   mEdit->setDataField( StringTable->insert("hovertime"), NULL, "1000" );
   mEdit->setDataField( StringTable->insert("tooltip"), NULL, data );
}

bool GuiInspectorCustomField::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   return true;
}

void GuiInspectorCustomField::setInspectorField( AbstractClassRep::Field *field, 
                                                  StringTableEntry caption, 
                                                  const char*arrayIndex ) 
{
   // Override the base just to be sure it doesn't get called.
   // We don't use an AbstractClassRep::Field...

//    mField = field; 
//    mCaption = StringTable->EmptyString();
//    mRenameCtrl->setText( getFieldName() );
}

GuiControl* GuiInspectorCustomField::constructEditControl()
{
   GuiControl* retCtrl = new GuiTextCtrl();

   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorTextEditProfile" );

   // Register the object
   retCtrl->registerObject();

   return retCtrl;
}

void GuiInspectorCustomField::setValue( StringTableEntry newValue )
{
   GuiTextCtrl *ctrl = dynamic_cast<GuiTextCtrl*>( mEdit );
   if( ctrl != NULL )
      ctrl->setText( newValue );
}

void GuiInspectorCustomField::_executeSelectedCallback()
{
   Con::executef( mInspector, "onFieldSelected", mCaption, ConsoleBaseType::getType(TypeCaseString)->getTypeName(), mDoc );
}
