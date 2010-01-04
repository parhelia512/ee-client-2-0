//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/editor/guiInspector.h"
#include "gui/editor/inspector/field.h"
#include "gui/editor/inspector/group.h"
#include "gui/buttons/guiIconButtonCtrl.h"
#include "gui/editor/inspector/dynamicGroup.h"
#include "gui/containers/guiScrollCtrl.h"
#include "gui/editor/inspector/customField.h"


GuiInspector::GuiInspector()
 : mTarget( NULL ),   
   mDividerPos( 0.35f ),
   mDividerMargin( 5 ),
   mOverDivider( false ),
   mMovingDivider( false ),
   mHLField( NULL )
{
   mPadding = 1;
}

GuiInspector::~GuiInspector()
{
   clearGroups();
}

IMPLEMENT_CONOBJECT(GuiInspector);


// ConsoleObject

bool GuiInspector::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   return true;
}

void GuiInspector::initPersistFields()
{
   addField( "dividerMargin", TypeS32, Offset( mDividerMargin, GuiInspector ) );
   addField( "groupFilters", TypeRealString, Offset( mGroupFilters, GuiInspector ), "Specify groups that should be shown or not. Specifying 'shown' implicitly does 'not show' all other groups. Example string: +name -otherName" );

   Parent::initPersistFields();
}

// SimObject

void GuiInspector::onDeleteNotify( SimObject *object )
{
   if ( object == mTarget )
      clearGroups();
}

// GuiControl

void GuiInspector::parentResized(const RectI &oldParentRect, const RectI &newParentRect)
{
   GuiControl *parent = getParent();
   if ( parent && dynamic_cast<GuiScrollCtrl*>(parent) != NULL )
   {
      GuiScrollCtrl *scroll = dynamic_cast<GuiScrollCtrl*>(parent);
      setWidth( ( newParentRect.extent.x - ( scroll->scrollBarThickness() + 4  ) ) );
   }
   else
      Parent::parentResized(oldParentRect,newParentRect);
}

bool GuiInspector::resize( const Point2I &newPosition, const Point2I &newExtent )
{
   //F32 dividerPerc = (F32)getWidth() / (F32)mDividerPos;

   bool result = Parent::resize( newPosition, newExtent );

   //mDividerPos = (F32)getWidth() * dividerPerc;

   updateDivider();

   return result;
}

GuiControl* GuiInspector::findHitControl( const Point2I &pt, S32 initialLayer )
{
   if ( mOverDivider || mMovingDivider )
      return this;

   return Parent::findHitControl( pt, initialLayer );
}

void GuiInspector::getCursor( GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent )
{
   GuiCanvas *pRoot = getRoot();
   if( !pRoot )
      return;

   S32 desiredCursor = mOverDivider ? PlatformCursorController::curResizeVert : PlatformCursorController::curArrow;

   // Bail if we're already at the desired cursor
   if ( pRoot->mCursorChanged == desiredCursor )
      return;

   PlatformWindow *pWindow = static_cast<GuiCanvas*>(getRoot())->getPlatformWindow();
   AssertFatal(pWindow != NULL,"GuiControl without owning platform window!  This should not be possible.");
   PlatformCursorController *pController = pWindow->getCursorController();
   AssertFatal(pController != NULL,"PlatformWindow without an owned CursorController!");

   // Now change the cursor shape
   pController->popCursor();
   pController->pushCursor(desiredCursor);
   pRoot->mCursorChanged = desiredCursor;
}

void GuiInspector::onMouseMove(const GuiEvent &event)
{
   if ( collideDivider( globalToLocalCoord( event.mousePoint ) ) )
      mOverDivider = true;
   else
      mOverDivider = false;
}

void GuiInspector::onMouseDown(const GuiEvent &event)
{
   if ( mOverDivider )
   {
      mMovingDivider = true;
   }
}

void GuiInspector::onMouseUp(const GuiEvent &event)
{
   mMovingDivider = false;   
}

void GuiInspector::onMouseDragged(const GuiEvent &event)
{
   if ( !mMovingDivider )   
      return;

   Point2I localPnt = globalToLocalCoord( event.mousePoint );

   S32 inspectorWidth = getWidth();

   // Distance from mouse/divider position in local space
   // to the right edge of the inspector
   mDividerPos = inspectorWidth - localPnt.x;
   mDividerPos = mClamp( mDividerPos, 0, inspectorWidth );

   // Divide that by the inspectorWidth to get a percentage
   mDividerPos /= inspectorWidth;

   updateDivider();
}

bool GuiInspector::findExistentGroup( StringTableEntry groupName )
{
   // If we have no groups, it couldn't possibly exist
   if( mGroups.empty() )
      return false;

   // Attempt to find it in the group list
   Vector<GuiInspectorGroup*>::iterator i = mGroups.begin();

   for( ; i != mGroups.end(); i++ )
   {
      if( dStricmp( (*i)->getGroupName(), groupName ) == 0 )
         return true;
   }

   return false;
}

void GuiInspector::updateFieldValue( StringTableEntry fieldName, StringTableEntry arrayIdx )
{
   // We don't know which group contains the field of this name,
   // so ask each group in turn, and break when a group returns true
   // signifying it contained and updated that field.

   Vector<GuiInspectorGroup*>::iterator groupIter = mGroups.begin();

   for( ; groupIter != mGroups.end(); groupIter++ )
   {   
      if ( (*groupIter)->updateFieldValue( fieldName, arrayIdx ) )
         break;
   }
}

void GuiInspector::clearGroups()
{
   // If we're clearing the groups, we want to clear our target too.
   mTarget = NULL;

   // And the HL field
   mHLField = NULL;

   // If we have no groups, there's nothing to clear!
   if( mGroups.empty() )
      return;

   // Attempt to find it in the group list
   Vector<GuiInspectorGroup*>::iterator i = mGroups.begin();

   freeze(true);

   // Delete Groups
   for( ; i != mGroups.end(); i++ )
   {
      if((*i) && (*i)->isProperlyAdded())
         (*i)->deleteObject();
   }   

   mGroups.clear();

   freeze(false);
   updatePanes();
}

void GuiInspector::inspectObject( SimObject *object )
{  
   //GuiCanvas  *guiCanvas = getRoot();
   //if( !guiCanvas )
   //   return;

   //SimObjectPtr<GuiControl> currResponder = guiCanvas->getFirstResponder();

   // If our target is the same as our current target, just update the groups.
   if( mTarget == object )
   {
      Vector<GuiInspectorGroup*>::iterator i = mGroups.begin();
      for ( ; i != mGroups.end(); i++ )
         (*i)->updateAllFields();

      // Don't steal first responder
      //if( !currResponder.isNull() )
      //   guiCanvas->setFirstResponder( currResponder );

      return;
   }

   // Give users a chance to customize fields on this object
   if( object->isMethod("onDefineFieldTypes") )
      Con::executef( object, "onDefineFieldTypes" );

   // Clear our current groups
   clearGroups();

   // Set Target
   if ( mTarget )
      clearNotify( mTarget );
   mTarget = object;
   deleteNotify( mTarget );

   // Special group for fields which should appear at the top of the
   // list outside of a rollout control.
   GuiInspectorGroup *ungroup = new GuiInspectorGroup( mTarget, "Ungrouped", this );
   ungroup->mHideHeader = true;
   ungroup->mCanCollapse = false;
   if( ungroup != NULL )
   {
      ungroup->registerObject();
      mGroups.push_back( ungroup );
      addObject( ungroup );
   }   

   // Put the 'transform' group first
   GuiInspectorGroup *transform = new GuiInspectorGroup( mTarget, "Transform", this );
   if( transform != NULL )
   {
      transform->registerObject();
      mGroups.push_back( transform );
      addObject( transform );
   }

   // Always create the 'general' group (for un-grouped fields)      
   GuiInspectorGroup *general = new GuiInspectorGroup( mTarget, "General", this );
   if( general != NULL )
   {
      general->registerObject();
      mGroups.push_back( general );
      addObject( general );
   }

   // Grab this objects field list
   AbstractClassRep::FieldList &fieldList = mTarget->getModifiableFieldList();
   AbstractClassRep::FieldList::iterator itr;

   // Iterate through, identifying the groups and create necessary GuiInspectorGroups
   for(itr = fieldList.begin(); itr != fieldList.end(); itr++)
   {
      if ( itr->type == AbstractClassRep::StartGroupFieldType )
      {
         if ( !findExistentGroup( itr->pGroupname ) && !isGroupFiltered( itr->pGroupname ) )
         {
            GuiInspectorGroup *group = new GuiInspectorGroup( mTarget, itr->pGroupname, this );
            if( group != NULL )
            {
               group->registerObject();
               mGroups.push_back( group );
               addObject( group );
            }            
         }
      }
   }

   // Deal with dynamic fields
   if ( !isGroupFiltered( "Dynamic Fields" ) )
   {
      GuiInspectorGroup *dynGroup = new GuiInspectorDynamicGroup( mTarget, "Dynamic Fields", this);
      if( dynGroup != NULL )
      {
         dynGroup->registerObject();
         mGroups.push_back( dynGroup );
         addObject( dynGroup );
      }
   }

   // Add the SimObjectID field to the ungrouped group
   GuiInspectorCustomField *field = new GuiInspectorCustomField();
   field->init(this, ungroup, object);

   if( field->registerObject() )
   {
      ungroup->mChildren.push_back( field );
      ungroup->mStack->addObject( field );

      StringTableEntry caption = StringTable->insert( String("Id"), true );
      field->setCaption( caption );

      field->setData( StringTable->insert( object->getIdString() ) );

      field->setDoc( StringTable->insert("SimObjectId of this object. [Read Only]"));
   }
   else
      delete field;

   // Add the Source Class field to the ungrouped group
   field = new GuiInspectorCustomField();
   field->init(this, ungroup, object);

   if( field->registerObject() )
   {
      ungroup->mChildren.push_back( field );
      ungroup->mStack->addObject( field );

      StringTableEntry caption = StringTable->insert( String("Source Class"), true );
      field->setCaption( caption );

      if(object->getClassRep())
      {
         field->setData( StringTable->insert( object->getClassRep()->getClassName(), true ));

         Namespace* ns = object->getClassRep()->getNameSpace();
         field->setToolTip( StringTable->insert( Con::getNamespaceList(ns), true ));
      }
      else
      {
         field->setData( StringTable->insert( String("") ));
      }

      field->setDoc( StringTable->insert( "Source code class of this object. [Read Only]") );
   }
   else
      delete field;


   // If the general group is still empty at this point ( or filtered ), kill it.
   if ( isGroupFiltered( "General" ) || general->mStack->size() == 0 )
   {
      for(S32 i=0; i<mGroups.size(); i++)
      {
         if ( mGroups[i] == general )
         {
            mGroups.erase(i);
            general->deleteObject();
            updatePanes();

            break;
         }
      }
   }

   // If transform turns out to be empty or filtered, remove it
   if( isGroupFiltered( "Transform" ) || transform->mStack->size() == 0 )
   {
      for(S32 i=0; i<mGroups.size(); i++)
      {
         if ( mGroups[i] == transform )
         {
            mGroups.erase(i);
            transform->deleteObject();
            updatePanes();

            break;
         }
      }
   }

}

void GuiInspector::setName( StringTableEntry newName )
{
   if( mTarget == NULL )
      return;

   StringTableEntry name = StringTable->insert(newName);

   // Only assign a new name if we provide one
   mTarget->assignName(name);
}

bool GuiInspector::collideDivider( const Point2I &localPnt )
{   
   RectI divisorRect( getWidth() - getWidth() * mDividerPos - mDividerMargin, 0, mDividerMargin * 2, getHeight() ); 

   if ( divisorRect.pointInRect( localPnt ) )
      return true;

   return false;
}

void GuiInspector::updateDivider()
{
   for ( U32 i = 0; i < mGroups.size(); i++ )
      for ( U32 j = 0; j < mGroups[i]->mChildren.size(); j++ )
         mGroups[i]->mChildren[j]->updateRects(); 

   //setUpdate();
}

void GuiInspector::getDivider( S32 &pos, S32 &margin )
{   
   pos = (F32)getWidth() * mDividerPos;
   margin = mDividerMargin;   
}

void GuiInspector::setHighlightField( GuiInspectorField *field )
{
   if ( mHLField == field )
      return;

   if ( mHLField.isValid() )
      mHLField->setHLEnabled( false );
   mHLField = field;

   // We could have been passed a null field, meaning, set no field highlighted.
   if ( mHLField.isNull() )
      return;

   mHLField->setHLEnabled( true );
}

bool GuiInspector::isGroupFiltered( const char *groupName ) const
{
   // Internal and Ungrouped always filtered, we never show them.   
   if ( dStricmp( groupName, "Internal" ) == 0 ||
        dStricmp( groupName, "Ungrouped" ) == 0 ||
		  dStricmp( groupName, "AdvCoordManipulation" ) == 0)
      return true;

   // Normal case, determine if filtered by looking at the mGroupFilters string.
   String searchStr;

   // Is this group explicitly show? Does it immediately follow a + char.
   searchStr = String::ToString( "+%s", groupName );
   if ( mGroupFilters.find( searchStr ) != String::NPos )
      return false;   

   // Were there any other + characters, if so, we are implicitly hidden.   
   if ( mGroupFilters.find( "+" ) != String::NPos )
      return true;

   // Is this group explicitly hidden? Does it immediately follow a - char.
   searchStr = String::ToString( "-%s", groupName );
   if ( mGroupFilters.find( searchStr ) != String::NPos )
      return true;   

   return false;
}

ConsoleMethod( GuiInspector, inspect, void, 3, 3, "Inspect(Object)")
{
   SimObject * target = Sim::findObject(argv[2]);
   if(!target)
   {
      if(dAtoi(argv[2]) > 0)
         Con::warnf("%s::inspect(): invalid object: %s", argv[0], argv[2]);

      object->clearGroups();
      return;
   }

   object->inspectObject(target);
}

ConsoleMethod( GuiInspector, refresh, void, 2, 2, "Reinspect the currently selected object." )
{
   SimObject *target = object->getInspectObject();
   if ( target )
      object->inspectObject( target );
}

ConsoleMethod( GuiInspector, getInspectObject, const char*, 2, 2, "getInspectObject() - Returns currently inspected object" )
{
   SimObject *pSimObject = object->getInspectObject();
   if( pSimObject != NULL )
      return pSimObject->getIdString();

   return "";
}

ConsoleMethod( GuiInspector, setName, void, 3, 3, "setName(NewObjectName)")
{
   object->setName(argv[2]);
}

ConsoleMethod( GuiInspector, apply, void, 2, 2, "apply() - Force application of inspected object's attributes" )
{
   SimObject *target = object->getInspectObject();
   if ( target )
      target->inspectPostApply();
}
