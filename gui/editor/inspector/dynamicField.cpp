//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/editor/inspector/dynamicField.h"
#include "gui/editor/inspector/dynamicGroup.h"
#include "gui/editor/guiInspector.h"
#include "gui/buttons/guiIconButtonCtrl.h"

//-----------------------------------------------------------------------------
// GuiInspectorDynamicField - Child class of GuiInspectorField 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT( GuiInspectorDynamicField );

GuiInspectorDynamicField::GuiInspectorDynamicField( GuiInspector *inspector,
                                                    GuiInspectorGroup* parent, 
                                                    SimObjectPtr<SimObject> target, 
                                                    SimFieldDictionary::Entry* field )
 : mRenameCtrl( NULL ),
   mDeleteButton( NULL )
{
   mInspector = inspector;
   mParent = parent;
   mTarget = target;
   mDynField = field;
   setBounds(0,0,100,20);   
}

void GuiInspectorDynamicField::setData( StringTableEntry data )
{
   // SICKHEAD: add script callback to create an undo action here
   // similar to GuiInspectorField to support UndoActions for
   // dynamicFields

   if ( mTarget == NULL || mDynField == NULL )
      return;

   data = StringTable->insert( data, true );

   mTarget->inspectPreApply();

   // Callback on the inspector when the field is modified
   // to allow creation of undo/redo actions.
   const char *oldData = mTarget->getDataField( mDynField->slotName, NULL );
   if ( !oldData )
      oldData = "";
   if ( dStrcmp( oldData, data ) != 0 )
      Con::executef( mInspector, "onInspectorFieldModified", Con::getIntArg(mTarget->getId()), mDynField->slotName, oldData, data );

   mTarget->setDataField( mDynField->slotName, NULL, data );

   // give the target a chance to validate
   mTarget->inspectPostApply();

   // Force our edit to update
   updateValue();
}

StringTableEntry GuiInspectorDynamicField::getData()
{
   if( mDynField == NULL || mTarget == NULL )
      return StringTable->insert( "" );

   return mTarget->getDataField( mDynField->slotName, NULL );
}

void GuiInspectorDynamicField::updateValue()
{
   if ( mTarget && mDynField )
      setValue( StringTable->insert( mTarget->getDataField( mDynField->slotName, NULL ) ) );
}

void GuiInspectorDynamicField::renameField( StringTableEntry newFieldName )
{
   // SICKHEAD: add script callback to create an undo action here
   // similar to GuiInspectorField to support UndoActions for
   // dynamicFields

   if ( mTarget == NULL || mDynField == NULL || mParent == NULL || mEdit == NULL )
   {
      Con::warnf("GuiInspectorDynamicField::renameField - No target object or dynamic field data found!" );
      return;
   }

   if ( !newFieldName )
   {
      Con::warnf("GuiInspectorDynamicField::renameField - Invalid field name specified!" );
      return;
   }

   // Only proceed if the name has changed
   if ( dStricmp( newFieldName, getFieldName() ) == 0 )
      return;

   // Grab a pointer to our parent and cast it to GuiInspectorDynamicGroup
   GuiInspectorDynamicGroup *group = dynamic_cast<GuiInspectorDynamicGroup*>(mParent);

   if ( group == NULL )
   {
      Con::warnf("GuiInspectorDynamicField::renameField - Unable to locate GuiInspectorDynamicGroup parent!" );
      return;
   }

   char currentValue[512] = {0};
   
   // Grab our current dynamic field value (we use a temporary buffer as this gets corrupted upon Con::eval)
   dSprintf( currentValue, 512, "%s", getData() );

   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.%s = \"\";", mTarget->getId(), getFieldName() );
   Con::evaluate(szBuffer);


   dSprintf( szBuffer, 512, "%d.%s = \"%s\";", mTarget->getId(), newFieldName, currentValue );
   Con::evaluate(szBuffer);

   // Configure our field to grab data from the new dynamic field
   SimFieldDictionary::Entry *newEntry = group->findDynamicFieldInDictionary( newFieldName );

   if( newEntry == NULL )
   {
      Con::warnf("GuiInspectorDynamicField::renameField - Unable to find new field!" );
      return;
   }

   // Assign our dynamic field pointer (where we retrieve field information from) to our new field pointer
   mDynField = newEntry;

   // Lastly we need to reassign our Command and AltCommand fields for our value edit control
   dSprintf( szBuffer, 512, "%d.%s = %d.getText();", mTarget->getId(), newFieldName, mEdit->getId() );
   mEdit->setField("AltCommand", szBuffer );
   mEdit->setField("Validate", szBuffer );

   if ( mDeleteButton )
   {
      dSprintf(szBuffer, 512, "%d.%s = \"\";%d.inspectGroup();", mTarget->getId(), newFieldName, group->getId());
      Con::printf(szBuffer);
      mDeleteButton->setField("Command", szBuffer);
   }
}

bool GuiInspectorDynamicField::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   //pushObjectToBack(mEdit);

   // Create our renaming field
   mRenameCtrl = new GuiTextEditCtrl();
   mRenameCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorDynamicFieldProfile" );

   char szName[512];
   dSprintf( szName, 512, "IE_%s_%d_%s_Rename", mRenameCtrl->getClassName(), mTarget->getId(), getFieldName() );
   mRenameCtrl->registerObject( szName );

   // Our command will evaluate to :
   //
   //    if( (editCtrl).getText() !$= "" )
   //       (field).renameField((editCtrl).getText());
   //
   char szBuffer[512];
   dSprintf( szBuffer, 512, "if( %d.getText() !$= \"\" ) %d.renameField(%d.getText());", mRenameCtrl->getId(), getId(), mRenameCtrl->getId() );
   mRenameCtrl->setText( getFieldName() );
   mRenameCtrl->setField("AltCommand", szBuffer );
   mRenameCtrl->setField("Validate", szBuffer );
   addObject( mRenameCtrl );

   // Resize the name control to fit in our caption rect
   mRenameCtrl->resize( mCaptionRect.point, mCaptionRect.extent );

   // Resize the value control to leave space for the delete button
   mEdit->resize( mValueRect.point, mValueRect.extent);

   // Clear out any caption set from Parent::onAdd
   // since we are rendering the fieldname with our 'rename' control.
   mCaption = StringTable->insert( "" );

   // Create delete button control
   mDeleteButton = new GuiBitmapButtonCtrl();

	SimObject* profilePtr = Sim::findObject("InspectorDynamicFieldButton");
   if( profilePtr != NULL )
		mDeleteButton->setControlProfile( dynamic_cast<GuiControlProfile*>(profilePtr) );

   dSprintf( szBuffer, 512, "%d.%s = \"\";%d.schedule(1,\"inspectGroup\");", mTarget->getId(), getFieldName(), mParent->getId() );

   // FIXME Hardcoded image
   mDeleteButton->setField( "Bitmap", "tools/gui/images/iconDelete" );
   mDeleteButton->setField( "Text", "X" );
   mDeleteButton->setField( "Command", szBuffer );
   mDeleteButton->setSizing( horizResizeLeft, vertResizeCenter );
	mDeleteButton->resize(Point2I(getWidth() - 20,2), Point2I(16, 16));
   mDeleteButton->registerObject();

   addObject(mDeleteButton);

   return true;
}

bool GuiInspectorDynamicField::updateRects()
{   
   Point2I fieldExtent = getExtent();   
   S32 dividerPos, dividerMargin;
   mInspector->getDivider( dividerPos, dividerMargin );

   S32 editWidth = dividerPos - dividerMargin;

   mEditCtrlRect.set( fieldExtent.x - dividerPos + dividerMargin, 1, editWidth, fieldExtent.y - 1 );
   mCaptionRect.set( 0, 0, fieldExtent.x - dividerPos - dividerMargin, fieldExtent.y );
   mValueRect.set( mEditCtrlRect.point, mEditCtrlRect.extent - Point2I( 20, 0 ) );
   mDeleteRect.set( fieldExtent.x - 20, 2, 16, fieldExtent.y - 4 );

   // This is probably being called during Parent::onAdd
   // so our special controls haven't been created yet but are just about to
   // so we just need to calculate the extents.
   if ( mRenameCtrl == NULL )
      return false;

   bool sized0 = mRenameCtrl->resize( mCaptionRect.point, mCaptionRect.extent );
   bool sized1 = mEdit->resize( mValueRect.point, mValueRect.extent );
   bool sized2 = mDeleteButton->resize(Point2I(getWidth() - 20,2), Point2I(16, 16));

   return ( sized0 || sized1 || sized2 );
}

void GuiInspectorDynamicField::setInspectorField( AbstractClassRep::Field *field, 
                                                  StringTableEntry caption, 
                                                  const char*arrayIndex ) 
{
   // Override the base just to be sure it doesn't get called.
   // We don't use an AbstractClassRep::Field...

//    mField = field; 
//    mCaption = StringTable->EmptyString();
//    mRenameCtrl->setText( getFieldName() );
}

void GuiInspectorDynamicField::_executeSelectedCallback()
{
   ConsoleBaseType* type = mDynField->type;
   if ( type )
      Con::executef( mInspector, "onFieldSelected", mDynField->slotName, type->getTypeName() );
   else
      Con::executef( mInspector, "onFieldSelected", mDynField->slotName, "TypeDynamicField" );
}

ConsoleMethod( GuiInspectorDynamicField, renameField, void, 3,3, "field.renameField(newDynamicFieldName);" )
{
   object->renameField( argv[2] );
}
