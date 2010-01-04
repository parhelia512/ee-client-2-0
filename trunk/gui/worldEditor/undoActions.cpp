//-----------------------------------------------------------------------------
// Copyright (C) Sickhead Games, LLC
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/worldEditor/undoActions.h"

#include "gui/editor/inspector/field.h"
#include "gui/editor/guiInspector.h"
#include "console/consoleTypes.h"


IMPLEMENT_CONOBJECT( MECreateUndoAction );

MECreateUndoAction::MECreateUndoAction( const UTF8* actionName )
   :  UndoAction( actionName )
{
}

MECreateUndoAction::~MECreateUndoAction()
{
}

void MECreateUndoAction::initPersistFields()
{
   Parent::initPersistFields();
}

void MECreateUndoAction::addObject( SimObject *object )
{
   AssertFatal( object, "MECreateUndoAction::addObject() - Got null object!" );

   mObjects.increment();
   mObjects.last().id = object->getId();
}

ConsoleMethod( MECreateUndoAction, addObject, void, 3, 3, "( SimObject obj )")
{
   SimObject *obj = NULL;
   if ( Sim::findObject( argv[2], obj ) && obj )
   	object->addObject( obj );
}

void MECreateUndoAction::undo()
{
   for ( S32 i= mObjects.size()-1; i >= 0; i-- )
   {
      ObjectState &state = mObjects[i];

      SimObject *object = Sim::findObject( state.id );
      if ( !object )
         continue;

      // Save the state.
      if ( !state.memento.hasState() )
         state.memento.save( object );

      // Store the group.
      SimGroup *group = object->getGroup();
      if ( group )
         state.groupId = group->getId();

      // We got what we need... delete it.
      object->deleteObject();
   }
}

void MECreateUndoAction::redo()
{
   for ( S32 i=0; i < mObjects.size(); i++ )
   {
      const ObjectState &state = mObjects[i];

      // Create the object.
      SimObject::setForcedId(state.id); // Restore the object's Id
      SimObject *object = state.memento.restore();
      if ( !object )
         continue;

      // Now restore its group.
      SimGroup *group;
      if ( Sim::findObject( state.groupId, group ) )
         group->addObject( object );
   }
}


IMPLEMENT_CONOBJECT( MEDeleteUndoAction );

MEDeleteUndoAction::MEDeleteUndoAction( const UTF8 *actionName )
   :  UndoAction( actionName )
{
}

MEDeleteUndoAction::~MEDeleteUndoAction()
{
}

void MEDeleteUndoAction::initPersistFields()
{
   Parent::initPersistFields();
}

void MEDeleteUndoAction::deleteObject( SimObject *object )
{
   AssertFatal( object, "MEDeleteUndoAction::deleteObject() - Got null object!" );
   AssertFatal( object->isProperlyAdded(), 
      "MEDeleteUndoAction::deleteObject() - Object should be registered!" );

   mObjects.increment();
   ObjectState& state = mObjects.last();

   // Capture the object id.
   state.id = object->getId();

   // Save the state.
   state.memento.save( object );

   // Store the group.
   SimGroup *group = object->getGroup();
   if ( group )
      state.groupId = group->getId();

   // Now delete the object.
   object->deleteObject();
}

ConsoleMethod( MEDeleteUndoAction, deleteObject, void, 3, 3, "( SimObject obj )")
{
   SimObject *obj = NULL;
   if ( Sim::findObject( argv[2], obj ) && obj )
   	object->deleteObject( obj );
}

void MEDeleteUndoAction::undo()
{
   for ( S32 i= mObjects.size()-1; i >= 0; i-- )
   {
      const ObjectState &state = mObjects[i];

      // Create the object.
      SimObject::setForcedId(state.id); // Restore the object's Id
      SimObject *object = state.memento.restore();
      if ( !object )
         continue;

      // Now restore its group.
      SimGroup *group;
      if ( Sim::findObject( state.groupId, group ) )
         group->addObject( object );
   }
}

void MEDeleteUndoAction::redo()
{
   for ( S32 i=0; i < mObjects.size(); i++ )
   {
      const ObjectState& state = mObjects[i];
      SimObject *object = Sim::findObject( state.id );
      if ( object )
         object->deleteObject();
   }
}

IMPLEMENT_CONOBJECT( InspectorFieldUndoAction );

InspectorFieldUndoAction::InspectorFieldUndoAction()
{
   mObjId = 0;
   mField = NULL; 
   mSlotName = StringTable->insert("");
   mArrayIdx = StringTable->insert("");
}

InspectorFieldUndoAction::InspectorFieldUndoAction( const UTF8 *actionName )
:  UndoAction( actionName )
{
   mInspector = NULL;
   mObjId = 0;
   mField = NULL; 
   mSlotName = StringTable->insert("");
   mArrayIdx = StringTable->insert("");
}

void InspectorFieldUndoAction::initPersistFields()
{
   addField( "inspectorGui", TypeSimObjectPtr, Offset( mInspector, InspectorFieldUndoAction ) );
   addField( "objectId", TypeS32, Offset( mObjId, InspectorFieldUndoAction ) );
   addField( "fieldName", TypeString, Offset( mSlotName, InspectorFieldUndoAction ) );
   addField( "fieldValue", TypeRealString, Offset( mData, InspectorFieldUndoAction ) );
   addField( "arrayIndex", TypeString, Offset( mArrayIdx, InspectorFieldUndoAction ) );

   Parent::initPersistFields();
}

void InspectorFieldUndoAction::undo()
{
   SimObject *obj = NULL;
   if ( !Sim::findObject( mObjId, obj ) )
      return;

   if ( mArrayIdx && dStricmp( mArrayIdx, "(null)" ) == 0 )
      mArrayIdx = NULL;

   // Grab the current data.
   String data = obj->getDataField( mSlotName, mArrayIdx );

   // Call this to mirror the way field changes are done through the inspector.
   obj->inspectPreApply();

   // Restore the data from the UndoAction
   obj->setDataField( mSlotName, mArrayIdx, mData.c_str() );   

   // Call this to mirror the way field changes are done through the inspector.
   obj->inspectPostApply();

   // If the affected object is still being inspected,
   // update the InspectorField to reflect the changed value.
   if ( mInspector && mInspector->getInspectObject() == obj )
      mInspector->updateFieldValue( mSlotName, mArrayIdx );

   // Now save the previous data in this UndoAction
   // since an undo action must become a redo action and vice-versa
   mData = data;
}
