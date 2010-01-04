//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "gui/editor/guiInspector.h"
#include "gui/editor/inspector/group.h"
#include "gui/editor/inspector/dynamicField.h"
#include "gui/editor/inspector/datablockField.h"
#include "gui/buttons/guiIconButtonCtrl.h"

//-----------------------------------------------------------------------------
// GuiInspectorGroup
//-----------------------------------------------------------------------------
//
// The GuiInspectorGroup control is a helper control that the inspector
// makes use of which houses a collapsible pane type control for separating
// inspected objects fields into groups.  The content of the inspector is 
// made up of zero or more GuiInspectorGroup controls inside of a GuiStackControl
//
//
//
IMPLEMENT_CONOBJECT(GuiInspectorGroup);

GuiInspectorGroup::GuiInspectorGroup() 
 : mTarget( NULL ), 
   mParent( NULL ), 
   mStack(NULL)
{
   setBounds(0,0,200,20);

   mChildren.clear();

   mCanSave = false;

   // Make sure we receive our ticks.
   setProcessTicks();
   mMargin.set(0,0,5,0);
}

GuiInspectorGroup::GuiInspectorGroup( SimObjectPtr<SimObject> target, 
                                      StringTableEntry groupName, 
                                      SimObjectPtr<GuiInspector> parent ) 
 : mTarget( target ), 
   mParent( parent ), 
   mStack(NULL)
{

   setBounds(0,0,200,20);

   mCaption = StringTable->insert( groupName );
   mCanSave = false;

   mChildren.clear();
   mMargin.set(0,0,4,0);
}

GuiInspectorGroup::~GuiInspectorGroup()
{  
}

//-----------------------------------------------------------------------------
// Persistence 
//-----------------------------------------------------------------------------
void GuiInspectorGroup::initPersistFields()
{
   addField("Caption", TypeString, Offset(mCaption, GuiInspectorGroup));

   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------
// Scene Events
//-----------------------------------------------------------------------------
bool GuiInspectorGroup::onAdd()
{
   setDataField( StringTable->insert("profile"), NULL, "GuiInspectorGroupProfile" );

   if( !Parent::onAdd() )
      return false;

   // Create our inner controls. Allow subclasses to provide other content.
   if(!createContent())
      return false;

   inspectGroup();

   return true;
}

bool GuiInspectorGroup::createContent()
{
   // Create our field stack control
   mStack = new GuiStackControl();

   // Prefer GuiTransperantProfile for the stack.
   mStack->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorStackProfile" );
   if( !mStack->registerObject() )
   {
      SAFE_DELETE( mStack );
      return false;
   }

   addObject( mStack );
   mStack->setField( "padding", "0" );
   return true;
}

//-----------------------------------------------------------------------------
// Control Sizing Animation Functions
//-----------------------------------------------------------------------------
void GuiInspectorGroup::animateToContents()
{
   calculateHeights();
   if(size() > 0)
      animateTo( mExpanded.extent.y );
   else
      animateTo( mHeader.extent.y );
}

GuiInspectorField* GuiInspectorGroup::constructField( S32 fieldType )
{
   // See if we can construct a field of this type
   ConsoleBaseType *cbt = ConsoleBaseType::getType(fieldType);
   if( !cbt )
      return NULL;

   // Alright, is it a datablock?
   if(cbt->isDatablock())
   {
      // Default to GameBaseData
      StringTableEntry typeClassName = cbt->getTypeClassName();

      if (mTarget && !dStricmp(typeClassName, "GameBaseData"))
      {
         // Try and setup the classname based on the object type
         char className[256];
         dSprintf(className,256,"%sData",mTarget->getClassName());
         // Walk the ACR list and find a matching class if any.
         AbstractClassRep *walk = AbstractClassRep::getClassList();
         while(walk)
         {
            if(!dStricmp(walk->getClassName(), className))
               break;

            walk = walk->getNextClass();
         }

         // We found a valid class
         if (walk)
            typeClassName = walk->getClassName();

      }


      GuiInspectorDatablockField *dbFieldClass = new GuiInspectorDatablockField( typeClassName );
      if( dbFieldClass != NULL )
      {
         // return our new datablock field with correct datablock type enumeration info
         return dbFieldClass;
      }
   }

   // Nope, not a datablock. So maybe it has a valid inspector field override we can use?
   if(!cbt->getInspectorFieldType())
      // Nothing, so bail.
      return NULL;

   // Otherwise try to make it!
   ConsoleObject *co = create(cbt->getInspectorFieldType());
   GuiInspectorField *gif = dynamic_cast<GuiInspectorField*>(co);

   if(!gif)
   {
      // Wasn't appropriate type, bail.
      delete co;
      return NULL;
   }

   return gif;
}

GuiInspectorField *GuiInspectorGroup::findField( const char *fieldName )
{
   // If we don't have any field children we can't very well find one then can we?
   if( mChildren.empty() )
      return NULL;

   Vector<GuiInspectorField*>::iterator i = mChildren.begin();

   for( ; i != mChildren.end(); i++ )
   {
      if( (*i)->getFieldName() != NULL && dStricmp( (*i)->getFieldName(), fieldName ) == 0 )
         return (*i);
   }

   return NULL;
}

void GuiInspectorGroup::clearFields()
{
   // Deallocates all field related controls.
   mStack->clear();

   // Then just cleanup our vectors which also point to children
   // that we keep for our own convenience.
   mArrayCtrls.clear();
   mChildren.clear();
}

bool GuiInspectorGroup::inspectGroup()
{
   // We can't inspect a group without a target!
   if( !mTarget )
      return false;

   // to prevent crazy resizing, we'll just freeze our stack for a sec..
   mStack->freeze(true);

   bool bNoGroup = false;

   // Un-grouped fields are all sorted into the 'general' group
   if ( dStricmp( mCaption, "General" ) == 0 )
      bNoGroup = true;

   AbstractClassRep::FieldList &fieldList = mTarget->getModifiableFieldList();
   AbstractClassRep::FieldList::iterator itr;

   bool bGrabItems = false;
   bool bNewItems = false;
   bool bMakingArray = false;
   GuiStackControl *pArrayStack = NULL;
   GuiRolloutCtrl *pArrayRollout = NULL;

   // Just delete all fields and recreate them (like the dynamicGroup)
   // because that makes creating controls for array fields a lot easier
   clearFields();

   for ( itr = fieldList.begin(); itr != fieldList.end(); itr++ )
   {
      if( itr->type == AbstractClassRep::StartGroupFieldType )
      {
         // If we're dealing with general fields, always set grabItems to true (to skip them)
         if( bNoGroup == true )
            bGrabItems = true;
         else if( itr->pGroupname != NULL && dStricmp( itr->pGroupname, mCaption ) == 0 )
            bGrabItems = true;
         continue;
      }
      else if ( itr->type == AbstractClassRep::EndGroupFieldType )
      {
         // If we're dealing with general fields, always set grabItems to false (to grab them)
         if( bNoGroup == true )
            bGrabItems = false;
         else if( itr->pGroupname != NULL && dStricmp( itr->pGroupname, mCaption ) == 0 )
            bGrabItems = false;
         continue;
      }

      if( ( bGrabItems == true || ( bNoGroup == true && bGrabItems == false ) ) && itr->type != AbstractClassRep::DeprecatedFieldType )
      {
         if( bNoGroup == true && bGrabItems == true )
            continue; 

          if ( itr->type == AbstractClassRep::StartArrayFieldType )
          {
             // Starting an array...
             // Create a rollout for the Array, give it the array's name.
             GuiRolloutCtrl *arrayRollout = new GuiRolloutCtrl();            
             GuiControlProfile *arrayRolloutProfile = dynamic_cast<GuiControlProfile*>( Sim::findObject( "GuiInspectorRolloutProfile0" ) );
 
             arrayRollout->setControlProfile(arrayRolloutProfile);
             //arrayRollout->mCaption = StringTable->insert( String::ToString( "%s (%i)", itr->pGroupname, itr->elementCount ) );
             arrayRollout->mCaption = StringTable->insert( itr->pGroupname );
             arrayRollout->mMargin.set( 14, 0, 0, 0 );
             arrayRollout->registerObject();
 
             GuiStackControl *arrayStack = new GuiStackControl();
             arrayStack->registerObject();
             arrayStack->freeze(true);
             arrayRollout->addObject(arrayStack);
 
             // Allocate a rollout for each element-count in the array
             // Give it the element count name.
             for ( U32 i = 0; i < itr->elementCount; i++ )
             {
                GuiRolloutCtrl *elementRollout = new GuiRolloutCtrl();            
                GuiControlProfile *elementRolloutProfile = dynamic_cast<GuiControlProfile*>( Sim::findObject( "GuiInspectorRolloutProfile0" ) );
 
                char buf[256];
                dSprintf( buf, 256, "  [%i]", i ); 
 
                elementRollout->setControlProfile(elementRolloutProfile);
                elementRollout->mCaption = StringTable->insert(buf);
                elementRollout->mMargin.set( 14, 0, 0, 0 );
                elementRollout->registerObject();
 
                GuiStackControl *elementStack = new GuiStackControl();
                elementStack->registerObject();            
                elementRollout->addObject(elementStack);
                elementRollout->instantCollapse();
 
                arrayStack->addObject( elementRollout );
             }
 
             pArrayRollout = arrayRollout;
             pArrayStack = arrayStack;
             arrayStack->freeze(false);
             pArrayRollout->instantCollapse();
             mStack->addObject(arrayRollout);
 
             bMakingArray = true;
             continue;
          }      
          else if ( itr->type == AbstractClassRep::EndArrayFieldType )
          {
             bMakingArray = false;
             continue;
          }
 
          if ( bMakingArray )
          {
             // Add a GuiInspectorField for this field, 
             // for every element in the array...
             for ( U32 i = 0; i < pArrayStack->size(); i++ )
             {
                FrameTemp<char> intToStr( 64 );
                dSprintf( intToStr, 64, "%d", i );
 
                // The array stack should have a rollout for each element
                // as children...
                GuiRolloutCtrl *pRollout = dynamic_cast<GuiRolloutCtrl*>(pArrayStack->at(i));
                // And the each of those rollouts should have a stack for 
                // fields...
                GuiStackControl *pStack = dynamic_cast<GuiStackControl*>(pRollout->at(0));
 
                // And we add a new GuiInspectorField to each of those stacks...            
                GuiInspectorField *field = constructField( itr->type );
                if ( field == NULL )                
                   field = new GuiInspectorField();
                                
                field->init( mParent, this, mTarget );
                StringTableEntry caption = StringTable->insert( itr->pFieldname );
                field->setInspectorField( itr, caption, intToStr );
 
                if( field->registerObject() )
                {
                   mChildren.push_back( field );
                   pStack->addObject( field );
                }
                else
                   delete field;
             }
 
             continue;
          }

         // This is weird, but it should work for now. - JDD
         // We are going to check to see if this item is an array
         // if so, we're going to construct a field for each array element
         if( itr->elementCount > 1 )
         {
            // Make a rollout control for this array
            //
            GuiRolloutCtrl *rollout = new GuiRolloutCtrl();  
            rollout->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorRolloutProfile0" );            
            rollout->mCaption = StringTable->insert(String::ToString( "%s (%i)", itr->pFieldname, itr->elementCount));
            rollout->mMargin.set( 14, 0, 0, 0 );
            rollout->registerObject();
            mArrayCtrls.push_back(rollout);
            
            // Put a stack control within the rollout
            //
            GuiStackControl *stack = new GuiStackControl();
            stack->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorStackProfile" );
            stack->registerObject();
            stack->freeze(true);
            rollout->addObject(stack);

            mStack->addObject(rollout);

            // Create each field and add it to the stack.
            //
            for (S32 nI = 0; nI < itr->elementCount; nI++)
            {
               FrameTemp<char> intToStr( 64 );
               dSprintf( intToStr, 64, "%d", nI );

               // Construct proper ValueName[nI] format which is "ValueName0" for index 0, etc.
               
               String fieldName = String::ToString( "%s%d", itr->pFieldname, nI );

               // If the field already exists, just update it
               GuiInspectorField *field = findField( fieldName );
               if( field != NULL )
               {
                  field->updateValue();
                  continue;
               }

               bNewItems = true;

               field = constructField( itr->type );
               if ( field == NULL )               
                  field = new GuiInspectorField();
               
               field->init( mParent, this, mTarget );               
               StringTableEntry caption = StringTable->insert( String::ToString("   [%i]",nI) );
               field->setInspectorField( itr, caption, intToStr );

               if ( field->registerObject() )
               {
                  mChildren.push_back( field );
                  stack->addObject( field );
               }
               else
                  delete field;
            }

            stack->freeze(false);
            stack->updatePanes();
            rollout->instantCollapse();
         }
         else
         {
            // If the field already exists, just update it
            GuiInspectorField *field = findField( itr->pFieldname );
            if ( field != NULL )
            {
               field->updateValue();
               continue;
            }

            bNewItems = true;

            field = constructField( itr->type );
            if ( field == NULL )
               field = new GuiInspectorField();
            
            field->init( mParent, this, mTarget );            
            field->setInspectorField( itr );            

            if ( field->registerObject() )
            {
               mChildren.push_back( field );
               mStack->addObject( field );
            }
            else
               delete field;
         }       
      }
   }
   mStack->freeze(false);
   mStack->updatePanes();

   // If we've no new items, there's no need to resize anything!
   if( bNewItems == false && !mChildren.empty() )
      return true;

   sizeToContents();

   setUpdate();

   return true;
}

bool GuiInspectorGroup::updateFieldValue( StringTableEntry fieldName, StringTableEntry arrayIdx )
{
   // Check if we contain a field of this name,
   // if so update its value and return true.
   Vector<GuiInspectorField*>::iterator iter = mChildren.begin();

   for( ; iter != mChildren.end(); iter++ )
   {   
      GuiInspectorField *field = (*iter);
      if ( field->mField &&
           field->mField->pFieldname == fieldName &&
           field->mFieldArrayIndex == arrayIdx )
      {
         field->updateValue();
         return true;
      }
   }

   return false;
}

void GuiInspectorGroup::updateAllFields()
{   
   Vector<GuiInspectorField*>::iterator iter = mChildren.begin();
   for( ; iter != mChildren.end(); iter++ )
      (*iter)->updateValue();
}
