//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/editor/inspector/field.h"

#include "gui/buttons/guiIconButtonCtrl.h"
#include "gui/editor/guiInspector.h"
#include "core/util/safeDelete.h"
#include "gfx/gfxDrawUtil.h"

//-----------------------------------------------------------------------------
// GuiInspectorField
//-----------------------------------------------------------------------------
// The GuiInspectorField control is a representation of a single abstract
// field for a given ConsoleObject derived object.  It handles creation
// getting and setting of it's fields data and editing control.  
//
// Creation of custom edit controls is done through this class and is
// dependent upon the dynamic console type, which may be defined to be
// custom for different types.
//
// Note : GuiInspectorField controls must have a GuiInspectorGroup as their
//        parent.  
IMPLEMENT_CONOBJECT(GuiInspectorField);


GuiInspectorField::GuiInspectorField( GuiInspector* inspector,
                                      GuiInspectorGroup* parent, 
                                      SimObjectPtr<SimObject> target, 
                                      AbstractClassRep::Field* field ) 
 : mInspector( inspector ),
   mParent( parent ), 
   mTarget( target ), 
   mField( field ), 
   mFieldArrayIndex( NULL ), 
   mEdit( NULL )
{
   if( field != NULL )
      mCaption    = StringTable->insert( field->pFieldname );
   else
      mCaption    = StringTable->insert( "" );

   mCanSave = false;
   setBounds(0,0,100,18);
}

GuiInspectorField::GuiInspectorField() 
 : mInspector( NULL ),
   mParent( NULL ), 
   mTarget( NULL ), 
   mEdit( NULL ),
   mField( NULL ), 
   mFieldArrayIndex( NULL ), 
   mCaption( StringTable->insert( "" ) ),
   mHighlighted( false )
{
   mCanSave = false;
}

GuiInspectorField::~GuiInspectorField()
{
}

void GuiInspectorField::init( GuiInspector *inspector, GuiInspectorGroup *group, SimObjectPtr<SimObject> target )
{   
   mInspector = inspector;
   mParent = group;
   mTarget = target;
}

bool GuiInspectorField::onAdd()
{   
   setInspectorProfile();   

   if ( !Parent::onAdd() )
      return false;

   if ( !mTarget || !mInspector )
      return false;   

   mEdit = constructEditControl();
   if ( mEdit == NULL )
      return false;

   setBounds(0,0,100,18);

   // Add our edit as a child
   addObject( mEdit );

   // Calculate Caption and EditCtrl Rects
   updateRects();   

   // Force our editField to set it's value
   updateValue();

   return true;
}

bool GuiInspectorField::resize( const Point2I &newPosition, const Point2I &newExtent )
{
   if ( !Parent::resize( newPosition, newExtent ) )
      return false;

   return updateRects();
}

void GuiInspectorField::onRender( Point2I offset, const RectI &updateRect )
{
   RectI ctrlRect(offset, getExtent());

   // Render fillcolor...
   if ( mProfile->mOpaque )
      GFX->getDrawUtil()->drawRectFill(ctrlRect, mProfile->mFillColor);   

   // Render caption...
   if ( mCaption && mCaption[0] )
   {      
      // Backup current ClipRect
      RectI clipBackup = GFX->getClipRect();

      RectI clipRect = updateRect;

      // The rect within this control in which our caption must fit.
      RectI rect( offset + mCaptionRect.point + mProfile->mTextOffset, mCaptionRect.extent + Point2I(1,1) - Point2I(5,0) );

      // Now clipRect is the amount of our caption rect that is actually visible.
      bool hit = clipRect.intersect( rect );

      if ( hit )
      {
         GFX->setClipRect( clipRect );
         GFXDrawUtil *drawer = GFX->getDrawUtil();

         // Backup modulation color
         ColorI currColor;
         drawer->getBitmapModulation( &currColor );

         // Draw caption background...
         if ( mHighlighted )         
            GFX->getDrawUtil()->drawRectFill( clipRect, mProfile->mFillColorHL );             

         // Draw caption text...

         drawer->setBitmapModulation( mHighlighted ? mProfile->mFontColorHL : mProfile->mFontColor );
         
         // Clip text with '...' if too long to fit
         String clippedText( mCaption );
         clipText( clippedText, clipRect.extent.x );

         renderJustifiedText( offset + mProfile->mTextOffset, getExtent(), clippedText );

         // Restore modulation color
         drawer->setBitmapModulation( currColor );

         // Restore previous ClipRect
         GFX->setClipRect( clipBackup );
      }
   }

   // Render Children...
   renderChildControls(offset, updateRect);

   // Render border...
   if ( mProfile->mBorder )
      renderBorder(ctrlRect, mProfile);   

   // Render divider...
   Point2I worldPnt = mEditCtrlRect.point + offset;
   GFX->getDrawUtil()->drawLine( worldPnt.x - 5,
      worldPnt.y, 
      worldPnt.x - 5,
      worldPnt.y + getHeight(),
      mProfile->mBorderColor );
}

void GuiInspectorField::setFirstResponder( GuiControl *firstResponder )
{
   Parent::setFirstResponder( firstResponder );

   if ( firstResponder == this || firstResponder == mEdit )
   {
      mInspector->setHighlightField( this );      
   }   
}

void GuiInspectorField::onMouseDown( const GuiEvent &event )
{
   if ( mCaptionRect.pointInRect( globalToLocalCoord( event.mousePoint ) ) )  
   {
      if ( mEdit )
         //mEdit->onMouseDown( event );
         mInspector->setHighlightField( this );
   }
   else
      Parent::onMouseDown( event );
}

void GuiInspectorField::setData( StringTableEntry data )
{
   if ( mField == NULL || mTarget == NULL )
      return;

   data = StringTable->insert( data, true );

   if ( verifyData( data ) )
   {
      mTarget->inspectPreApply();

      // Callback on the inspector when the field is modified
      // to allow creation of undo/redo actions.
      String oldData = mTarget->getDataField( mField->pFieldname, mFieldArrayIndex);
      if ( dStrcmp( oldData.c_str(), data ) != 0 )
      {
         Con::executef( mInspector, "onInspectorFieldModified", 
                                       Con::getIntArg(mTarget->getId()), 
                                       mField->pFieldname, 
                                       mFieldArrayIndex ? mFieldArrayIndex : "(null)", 
                                       oldData.c_str(), 
                                       data );
      }

      mTarget->setDataField( mField->pFieldname, mFieldArrayIndex, data );
      
      // give the target a chance to validate
      mTarget->inspectPostApply();
   }

   // Force our edit to update
   updateValue();
}

StringTableEntry GuiInspectorField::getData()
{
   if( mField == NULL || mTarget == NULL )
      return StringTable->insert( "" );

   return StringTable->insert( mTarget->getDataField( mField->pFieldname, mFieldArrayIndex ) );
}

void GuiInspectorField::setInspectorField( AbstractClassRep::Field *field, StringTableEntry caption, const char*arrayIndex ) 
{
   mField = field; 

   if ( arrayIndex != NULL )   
      mFieldArrayIndex = StringTable->insert( arrayIndex );

   if ( !caption || !caption[0] )
      mCaption = getFieldName(); 
   else
      mCaption = caption;
}


StringTableEntry GuiInspectorField::getFieldName() 
{ 
   // Sanity
   if ( mField == NULL )
      return StringTable->insert( "" );

   // Array element?
   if( mFieldArrayIndex != NULL )
   {
      S32 frameTempSize = dStrlen( mField->pFieldname ) + 32;
      FrameTemp<char> valCopy( frameTempSize );
      dSprintf( (char *)valCopy, frameTempSize, "%s[%s]", mField->pFieldname, mFieldArrayIndex );

      // Return formatted element
      return StringTable->insert( valCopy );
   }

   // Plain field name.
   return StringTable->insert( mField->pFieldname ); 
};

GuiControl* GuiInspectorField::constructEditControl()
{
   GuiControl* retCtrl = new GuiTextEditCtrl();

   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorTextEditProfile" );

   _registerEditControl( retCtrl );

   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply(%d.getText());",getId(), retCtrl->getId() );
   retCtrl->setField("AltCommand", szBuffer );
   retCtrl->setField("Validate", szBuffer );

   return retCtrl;
}

void GuiInspectorField::setInspectorProfile()
{
   GuiControlProfile *profile = NULL;   
   if ( Sim::findObject( "GuiInspectorFieldProfile", profile ) )
      setControlProfile( profile );

   // Use the same trick as GuiControl::onAdd
   // to try finding a GuiControlProfile with the name scheme
   // 'InspectorTypeClassName' + 'Profile'
   
//    String name = getClassName();   
//    name += "Profile";
//    
//    GuiControlProfile *profile = NULL;
//    Sim::findObject( name, profile );
// 
//    if ( !profile )
//       Sim::findObject( "GuiInspectorFieldProfile", profile );
// 
//    if ( !profile )
//       Con::errorf( "GuiInspectorField::setInspectorProfile - no profile found! Tried (%s) and (GuiInspectorFieldProfile) !", name.c_str() );
//    else
//       setControlProfile( profile );   
}

void GuiInspectorField::setValue( StringTableEntry newValue )
{
   GuiTextEditCtrl *ctrl = dynamic_cast<GuiTextEditCtrl*>( mEdit );
   if( ctrl != NULL )
      ctrl->setText( newValue );
}

bool GuiInspectorField::updateRects()
{
   S32 dividerPos, dividerMargin;
   mInspector->getDivider( dividerPos, dividerMargin );   

   Point2I fieldExtent = getExtent();
   Point2I fieldPos = getPosition();

   S32 editWidth = dividerPos - dividerMargin;

   mEditCtrlRect.set( fieldExtent.x - dividerPos + dividerMargin, 1, editWidth, fieldExtent.y - 1 );
   mCaptionRect.set( 0, 0, fieldExtent.x - dividerPos - dividerMargin, fieldExtent.y );
   
   if ( !mEdit )
      return false;

   return mEdit->resize( mEditCtrlRect.point, mEditCtrlRect.extent );
}

void GuiInspectorField::updateValue()
{
   if ( mTarget && mField )
      setValue( StringTable->insert( mTarget->getDataField( mField->pFieldname, mFieldArrayIndex ),
                                     mField->type == TypeCaseString || mField->type == TypeRealString ? true : false ) );
}

void GuiInspectorField::setHLEnabled( bool enabled )
{
   mHighlighted = enabled;
   if ( mHighlighted )
   {
      if ( mEdit && !mEdit->isFirstResponder() )
      {
         mEdit->setFirstResponder();
         GuiTextEditCtrl *edit = dynamic_cast<GuiTextEditCtrl*>( mEdit );
         if ( edit )
         {
            mouseUnlock();
            edit->mouseLock();
            edit->setCursorPos(0);
         }
      }
      _executeSelectedCallback();
   }
}

void GuiInspectorField::_executeSelectedCallback()
{
   if( mField )
   {
      if ( mField->pFieldDocs && mField->pFieldDocs[0] )
         Con::executef( mInspector, "onFieldSelected", mField->pFieldname, ConsoleBaseType::getType(mField->type)->getTypeName(), mField->pFieldDocs );
      else
         Con::executef( mInspector, "onFieldSelected", mField->pFieldname, ConsoleBaseType::getType(mField->type)->getTypeName() );
   }
}


void GuiInspectorField::_registerEditControl( GuiControl *ctrl )
{
   if ( !mTarget )
      return;

   char szName[512];
   dSprintf( szName, 512, "IE_%s_%d_%s_Field", ctrl->getClassName(), mTarget->getId(),mCaption);

   // Register the object
   ctrl->registerObject( szName );
}

ConsoleMethod( GuiInspectorField, apply, void, 3,3, "apply(newValue);" )
{
   object->setData( argv[2] );
}

ConsoleMethod( GuiInspectorField, getData, const char*, 2, 2, "getData();" )
{
   return object->getData();
}
