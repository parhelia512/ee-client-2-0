//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/editor/guiInspectorTypes.h"

#include "gui/editor/inspector/group.h"
#include "gui/controls/guiTextEditSliderCtrl.h"
#include "gui/controls/guiTextEditSliderBitmapCtrl.h"
#include "gui/buttons/guiSwatchButtonCtrl.h"
#include "gui/containers/guiDynamicCtrlArrayCtrl.h"
#include "core/strings/stringUnit.h"
#include "materials/materialDefinition.h"
#include "materials/materialManager.h"
#include "materials/customMaterialDefinition.h"
#include "gfx/gfxDrawUtil.h"


//-----------------------------------------------------------------------------
// GuiInspectorTypeMenuBase
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeMenuBase);

GuiControl* GuiInspectorTypeMenuBase::constructEditControl()
{
   GuiControl* retCtrl = new GuiPopUpMenuCtrl();

   // If we couldn't construct the control, bail!
   if( retCtrl == NULL )
      return retCtrl;

   GuiPopUpMenuCtrl *menu = dynamic_cast<GuiPopUpMenuCtrl*>(retCtrl);

   // Let's make it look pretty.
   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiPopUpMenuProfile" );

   menu->setField("text", getData());

   _registerEditControl( retCtrl );

   // Configure it to update our value when the popup is closed
   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply( %d.getText() );", getId(), menu->getId() );
   menu->setField("Command", szBuffer );

   //now add the entries, allow derived classes to override this
   _populateMenu( menu );

   return retCtrl;
}

void GuiInspectorTypeMenuBase::setValue( StringTableEntry newValue )
{
   GuiPopUpMenuCtrl *ctrl = dynamic_cast<GuiPopUpMenuCtrl*>( mEdit );
   if ( ctrl != NULL )
      ctrl->setText( newValue );
}

void GuiInspectorTypeMenuBase::_populateMenu( GuiPopUpMenuCtrl *menu )
{
   // do nothing, child classes override this.
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeEnum 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeEnum);

void GuiInspectorTypeEnum::_populateMenu( GuiPopUpMenuCtrl *menu )
{
   //now add the entries
   for(S32 i = 0; i < mField->table->size; i++)
      menu->addEntry(mField->table->table[i].label, mField->table->table[i].index);
}

void GuiInspectorTypeEnum::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeEnum)->setInspectorFieldType("GuiInspectorTypeEnum");
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeCubemapName 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeCubemapName);

void GuiInspectorTypeCubemapName::_populateMenu( GuiPopUpMenuCtrl *menu )
{
   PROFILE_SCOPE( GuiInspectorTypeCubemapName_populateMenu );

   // This could be expensive looping through the whole RootGroup
   // and performing string comparisons... Put a profile here
   // to keep an eye on it.

   SimGroup *root = Sim::getRootGroup();
   
   SimGroupIterator iter( root );
   for ( ; *iter; ++iter )
   {
      if ( dStricmp( (*iter)->getClassName(), "CubemapData" ) == 0 )
         menu->addEntry( (*iter)->getName(), 0 );
   }

   menu->sort();
}

void GuiInspectorTypeCubemapName::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeCubemapName)->setInspectorFieldType("GuiInspectorTypeCubemapName");
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeMaterialName 
//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiInspectorTypeMaterialName);
GuiInspectorTypeMaterialName::GuiInspectorTypeMaterialName()
 : mBrowseButton( NULL )
{
}

void GuiInspectorTypeMaterialName::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeMaterialName)->setInspectorFieldType("GuiInspectorTypeMaterialName");
}

GuiControl* GuiInspectorTypeMaterialName::constructEditControl()
{	
   GuiControl* retCtrl = new GuiTextEditCtrl();

   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorTextEditProfile" );

   _registerEditControl( retCtrl );

   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply(%d.getText());",getId(), retCtrl->getId() );
   retCtrl->setField("AltCommand", szBuffer );
   retCtrl->setField("Validate", szBuffer );

   //return retCtrl;
   mBrowseButton = new GuiBitmapButtonCtrl();

   if ( mBrowseButton != NULL )
   {
      RectI browseRect( Point2I( ( getLeft() + getWidth()) - 26, getTop() + 2), Point2I(20, getHeight() - 4) );

      char szBuffer[512];
      dSprintf( szBuffer, 512, "materialSelector.showDialog(\"%d.apply\", \"name\");", getId());
		mBrowseButton->setField( "Command", szBuffer );

		//temporary static button name
		char bitmapName[512] = "tools/materialEditor/gui/change-material-btn";
		mBrowseButton->setBitmap( bitmapName );

      mBrowseButton->setDataField( StringTable->insert("Profile"), NULL, "GuiButtonProfile" );
      mBrowseButton->registerObject();
      addObject( mBrowseButton );

      // Position
      mBrowseButton->resize( browseRect.point, browseRect.extent );
   }

   return retCtrl;
}

bool GuiInspectorTypeMaterialName::updateRects()
{
   Point2I fieldPos = getPosition();
   Point2I fieldExtent = getExtent();
   S32 dividerPos, dividerMargin;
   mInspector->getDivider( dividerPos, dividerMargin );

   mCaptionRect.set( 0, 0, fieldExtent.x - dividerPos - dividerMargin, fieldExtent.y );
	// Icon extent 17 x 17
   mBrowseRect.set( fieldExtent.x - 20, 2, 17, fieldExtent.y - 1 );
   mEditCtrlRect.set( fieldExtent.x - dividerPos + dividerMargin, 1, dividerPos - dividerMargin - 29, fieldExtent.y );

   bool editResize = mEdit->resize( mEditCtrlRect.point, mEditCtrlRect.extent );
   bool browseResize = false;

   if ( mBrowseButton != NULL )
   {         
      browseResize = mBrowseButton->resize( mBrowseRect.point, mBrowseRect.extent );
   }

   return ( editResize || browseResize );
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeGuiProfile 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeGuiProfile);

void GuiInspectorTypeGuiProfile::_populateMenu( GuiPopUpMenuCtrl *menu )
{
   SimGroup *grp = Sim::getGuiDataGroup();
   SimSetIterator iter( grp );
   for ( ; *iter; ++iter )
   {
      GuiControlProfile *profile = dynamic_cast<GuiControlProfile*>(*iter);
      if ( profile )
         menu->addEntry( profile->getName(), 0 );
   }
   
   menu->sort();
}

void GuiInspectorTypeGuiProfile::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeGuiProfile)->setInspectorFieldType("GuiInspectorTypeGuiProfile");
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeCheckBox 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeCheckBox);

GuiControl* GuiInspectorTypeCheckBox::constructEditControl()
{
   GuiControl* retCtrl = new GuiCheckBoxCtrl();

   // If we couldn't construct the control, bail!
   if( retCtrl == NULL )
      return retCtrl;

   GuiCheckBoxCtrl *check = dynamic_cast<GuiCheckBoxCtrl*>(retCtrl);

   // Let's make it look pretty.
   retCtrl->setDataField( StringTable->insert("profile"), NULL, "InspectorTypeCheckboxProfile" );
   retCtrl->setField( "text", "" );

   check->mIndent = 4;

   retCtrl->setScriptValue( getData() );

   _registerEditControl( retCtrl );

   // Configure it to update our value when the popup is closed
   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply(%d.getValue());",getId(),check->getId() );
   check->setField("Command", szBuffer );

   return retCtrl;
}


void GuiInspectorTypeCheckBox::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeBool)->setInspectorFieldType("GuiInspectorTypeCheckBox");
}

void GuiInspectorTypeCheckBox::setValue( StringTableEntry newValue )
{
   GuiButtonBaseCtrl *ctrl = dynamic_cast<GuiButtonBaseCtrl*>( mEdit );
   if ( ctrl != NULL )
      ctrl->setStateOn( dAtob(newValue) );
}

const char* GuiInspectorTypeCheckBox::getValue()
{
   GuiButtonBaseCtrl *ctrl = dynamic_cast<GuiButtonBaseCtrl*>( mEdit );
   if ( ctrl != NULL )
      return ctrl->getScriptValue();

   return NULL;
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeFileName 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeFileName);

void GuiInspectorTypeFileName::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeFilename)->setInspectorFieldType("GuiInspectorTypeFileName");
   ConsoleBaseType::getType(TypeStringFilename)->setInspectorFieldType("GuiInspectorTypeFileName");
}

GuiControl* GuiInspectorTypeFileName::constructEditControl()
{
   GuiControl* retCtrl = new GuiTextEditCtrl();

   // If we couldn't construct the control, bail!
   if( retCtrl == NULL )
      return retCtrl;

   // Let's make it look pretty.
   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorTextEditRightProfile" );
   retCtrl->setDataField( StringTable->insert("tooltipprofile"), NULL, "GuiToolTipProfile" );
   retCtrl->setDataField( StringTable->insert("hovertime"), NULL, "1000" );

   // Don't forget to register ourselves
   _registerEditControl( retCtrl );

   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply(%d.getText());",getId(),retCtrl->getId() );
   retCtrl->setField("AltCommand", szBuffer );
   retCtrl->setField("Validate", szBuffer );

   mBrowseButton = new GuiButtonCtrl();

   if( mBrowseButton != NULL )
   {
      RectI browseRect( Point2I( ( getLeft() + getWidth()) - 26, getTop() + 2), Point2I(20, getHeight() - 4) );
      char szBuffer[512];
      dSprintf( szBuffer, 512, "getLoadFilename(\"*.*|*.*\", \"%d.apply\", %d.getData());", getId(), getId() );
      mBrowseButton->setField( "Command", szBuffer );
      mBrowseButton->setField( "text", "..." );
      mBrowseButton->setDataField( StringTable->insert("Profile"), NULL, "GuiInspectorButtonProfile" );
      mBrowseButton->registerObject();
      addObject( mBrowseButton );

      // Position
      mBrowseButton->resize( browseRect.point, browseRect.extent );
   }

   return retCtrl;
}

bool GuiInspectorTypeFileName::resize( const Point2I &newPosition, const Point2I &newExtent )
{
   if ( !Parent::resize( newPosition, newExtent ) )
      return false;

   if ( mEdit != NULL )
   {
      return updateRects();
   }

   return false;
}

bool GuiInspectorTypeFileName::updateRects()
{   
   S32 dividerPos, dividerMargin;
   mInspector->getDivider( dividerPos, dividerMargin );
   Point2I fieldExtent = getExtent();
   Point2I fieldPos = getPosition();

   mCaptionRect.set( 0, 0, fieldExtent.x - dividerPos - dividerMargin, fieldExtent.y );
   mEditCtrlRect.set( fieldExtent.x - dividerPos + dividerMargin, 1, dividerPos - dividerMargin - 32, fieldExtent.y );

   bool editResize = mEdit->resize( mEditCtrlRect.point, mEditCtrlRect.extent );
   bool browseResize = false;

   if ( mBrowseButton != NULL )
   {
      mBrowseRect.set( fieldExtent.x - 20, 2, 14, fieldExtent.y - 4 );
      browseResize = mBrowseButton->resize( mBrowseRect.point, mBrowseRect.extent );
   }

   return ( editResize || browseResize );
}

void GuiInspectorTypeFileName::updateValue()
{
   if ( mTarget && mField )
   {
      StringTableEntry data = StringTable->insert( mTarget->getDataField( mField->pFieldname, mFieldArrayIndex ), mField->type == TypeCaseString || mField->type == TypeRealString ? true : false );
      setValue( data );
      mEdit->setDataField( StringTable->insert("tooltip"), NULL, data );
   }
}

ConsoleMethod( GuiInspectorTypeFileName, apply, void, 3,3, "apply(newValue);" )
{
   String path( argv[2] );
   if ( path.isNotEmpty() )
      path = Platform::makeRelativePathName( path, Platform::getMainDotCsDir() );
      
   object->setData( path.c_str() );
}


//-----------------------------------------------------------------------------
// GuiInspectorTypeImageFileName 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeImageFileName);

void GuiInspectorTypeImageFileName::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeImageFilename)->setInspectorFieldType("GuiInspectorTypeImageFileName");
}

GuiControl* GuiInspectorTypeImageFileName::constructEditControl()
{
   GuiControl *retCtrl = Parent::constructEditControl();
   
   if ( retCtrl == NULL )
      return retCtrl;
   
   retCtrl->mRenderTooltipDelegate.bind( this, &GuiInspectorTypeImageFileName::renderTooltip );
   char szBuffer[512];

   String extList = GBitmap::sGetExtensionList();
   U32 extCount = StringUnit::getUnitCount( extList, " " );

   String fileSpec;

   // building the fileSpec string 

   fileSpec += "All Image Files|";

   for ( U32 i = 0; i < extCount; i++ )
   {
      fileSpec += "*.";
      fileSpec += StringUnit::getUnit( extList, i, " " );

      if ( i < extCount - 1 )
         fileSpec += ";";
   }

   fileSpec += "|";

   for ( U32 i = 0; i < extCount; i++ )
   {
      String ext = StringUnit::getUnit( extList, i, " " );
      fileSpec += ext;
      fileSpec += "|*.";
      fileSpec += ext;

      if ( i != extCount - 1 )
         fileSpec += "|";
   }

   dSprintf( szBuffer, 512, "getLoadFilename(\"%s\", \"%d.apply\", %d.getData());", fileSpec.c_str(), getId(), getId() );
   mBrowseButton->setField( "Command", szBuffer );

   return retCtrl;
}

bool GuiInspectorTypeImageFileName::renderTooltip( const Point2I &hoverPos, const Point2I &cursorPos, const char *tipText )
{
   if ( !mAwake ) 
      return false;

   GuiCanvas *root = getRoot();
   if ( !root )
      return false;

   StringTableEntry filename = getData();
   if ( !filename || !filename[0] )
      return false;

   GFXTexHandle texture( filename, &GFXDefaultStaticDiffuseProfile, avar("%s() - tooltip texture (line %d)", __FUNCTION__, __LINE__) );
   if ( texture.isNull() )
      return false;

   // Render image at a reasonable screen size while 
   // keeping its aspect ratio...
   Point2I screensize = getRoot()->getWindowSize();
   Point2I offset = hoverPos; 
   Point2I tipBounds;

   U32 texWidth = texture.getWidth();
   U32 texHeight = texture.getHeight();
   F32 aspect = (F32)texHeight / (F32)texWidth;

   const F32 newWidth = 150.0f;
   F32 newHeight = aspect * newWidth;

   // Offset below cursor image
   offset.y += 20; // TODO: Attempt to fix?: root->getCursorExtent().y;
   tipBounds.x = newWidth;
   tipBounds.y = newHeight;

   // Make sure all of the tooltip will be rendered width the app window,
   // 5 is given as a buffer against the edge
   if ( screensize.x < offset.x + tipBounds.x + 5 )
      offset.x = screensize.x - tipBounds.x - 5;
   if ( screensize.y < offset.y + tipBounds.y + 5 )
      offset.y = hoverPos.y - tipBounds.y - 5;

   RectI oldClip = GFX->getClipRect();
   RectI rect( offset, tipBounds );
   GFX->setClipRect( rect );

   GFXDrawUtil *drawer = GFX->getDrawUtil();
   drawer->clearBitmapModulation();
   GFX->getDrawUtil()->drawBitmapStretch( texture, rect );

   GFX->setClipRect( oldClip );

   return true;
}



//-----------------------------------------------------------------------------
// GuiInspectorTypeCommand
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeCommand);

void GuiInspectorTypeCommand::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeCommand)->setInspectorFieldType("GuiInspectorTypeCommand");
}

GuiInspectorTypeCommand::GuiInspectorTypeCommand()
{
   mTextEditorCommand = StringTable->insert("TextPad");
}

GuiControl* GuiInspectorTypeCommand::constructEditControl()
{
   GuiButtonCtrl* retCtrl = new GuiButtonCtrl();

   // If we couldn't construct the control, bail!
   if( retCtrl == NULL )
      return retCtrl;

   // Let's make it look pretty.
   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorTextEditProfile" );

   // Don't forget to register ourselves
   _registerEditControl( retCtrl );

   _setCommand( retCtrl, getData() );

   return retCtrl;
}

void GuiInspectorTypeCommand::setValue( StringTableEntry newValue )
{
   GuiButtonCtrl *ctrl = dynamic_cast<GuiButtonCtrl*>( mEdit );
   _setCommand( ctrl, newValue );
}

void GuiInspectorTypeCommand::_setCommand( GuiButtonCtrl *ctrl, StringTableEntry command )
{
   if( ctrl != NULL )
   {
      ctrl->setField( "text", command );

      // expandEscape isn't length-limited, so while this _should_ work
      // in most circumstances, it may still fail if getData() has lots of
      // non-printable characters
      S32 len = 2 * dStrlen(command) + 512;

      FrameTemp<char> szBuffer(len);

	   S32 written = dSprintf( szBuffer, len, "%s(\"", mTextEditorCommand );
      expandEscape(szBuffer.address() + written, command);
      written = strlen(szBuffer);
      dSprintf( szBuffer.address() + written, len - written, "\", \"%d.apply\", %d.getRoot());", getId(), getId() );

	   ctrl->setField( "Command", szBuffer );
   }
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeColor (Base for ColorI/ColorF) 
//-----------------------------------------------------------------------------
GuiInspectorTypeColor::GuiInspectorTypeColor()
 : mBrowseButton( NULL )
{
}

IMPLEMENT_CONOBJECT(GuiInspectorTypeColor);

GuiControl* GuiInspectorTypeColor::constructEditControl()
{
   GuiControl* retCtrl = new GuiTextEditCtrl();

   // If we couldn't construct the control, bail!
   if( retCtrl == NULL )
      return retCtrl;

   // Let's make it look pretty.
   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorTextEditProfile" );

   // Don't forget to register ourselves
   _registerEditControl( retCtrl );

   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply(%d.getText());",getId(), retCtrl->getId() );
   retCtrl->setField("AltCommand", szBuffer );
   retCtrl->setField("Validate", szBuffer );

   mBrowseButton = new GuiSwatchButtonCtrl();

   if ( mBrowseButton != NULL )
   {
      RectI browseRect( Point2I( ( getLeft() + getWidth()) - 26, getTop() + 2), Point2I(20, getHeight() - 4) );
      mBrowseButton->setDataField( StringTable->insert("Profile"), NULL, "GuiInspectorSwatchButtonProfile" );
      mBrowseButton->registerObject();
      addObject( mBrowseButton );
		
		char szColor[512];
      if( _getColorConversionFunction() )
         dSprintf( szColor, 512, "%s( %d.color )", _getColorConversionFunction(), mBrowseButton->getId() );
      else
         dSprintf( szColor, 512, "%d.color", mBrowseButton->getId() );

		char szBuffer[512];
		dSprintf( szBuffer, 512, "%s(%s, \"%d.apply\", %d.getRoot());", mColorFunction, szColor, getId(), getId() );
		
		mBrowseButton->setField( "Command", szBuffer );

      // Position
      mBrowseButton->resize( browseRect.point, browseRect.extent );
   }

   return retCtrl;
}

bool GuiInspectorTypeColor::resize( const Point2I &newPosition, const Point2I &newExtent )
{
   if( !Parent::resize( newPosition, newExtent ) )
      return false;

   return false;
}

bool GuiInspectorTypeColor::updateRects()
{
   Point2I fieldPos = getPosition();
   Point2I fieldExtent = getExtent();
   S32 dividerPos, dividerMargin;
   mInspector->getDivider( dividerPos, dividerMargin );

   mCaptionRect.set( 0, 0, fieldExtent.x - dividerPos - dividerMargin, fieldExtent.y );
   mBrowseRect.set( fieldExtent.x - 20, 2, 14, fieldExtent.y - 4 );
   mEditCtrlRect.set( fieldExtent.x - dividerPos + dividerMargin, 1, dividerPos - dividerMargin - 29, fieldExtent.y );

   bool editResize = mEdit->resize( mEditCtrlRect.point, mEditCtrlRect.extent );
   bool browseResize = false;

   if ( mBrowseButton != NULL )
   {         
      browseResize = mBrowseButton->resize( mBrowseRect.point, mBrowseRect.extent );
   }

   return ( editResize || browseResize );
}


//-----------------------------------------------------------------------------
// GuiInspectorTypeColorI
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeColorI);

void GuiInspectorTypeColorI::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeColorI)->setInspectorFieldType("GuiInspectorTypeColorI");
}

GuiInspectorTypeColorI::GuiInspectorTypeColorI()
{
   mColorFunction = StringTable->insert("getColorI");
}

void GuiInspectorTypeColorI::setValue( StringTableEntry newValue )
{
   // Allow parent to set the edit-ctrl text to the new value.
   Parent::setValue( newValue );

   // Now we also set our color swatch button to the new color value.
   if ( mBrowseButton )
   {      
      ColorI color(255,0,255,255);
      S32 r,g,b,a;
      dSscanf( newValue, "%d %d %d %d", &r, &g, &b, &a );
      color.red = r;
      color.green = g;
      color.blue = b;
      color.alpha = a;
      mBrowseButton->setColor( color );
   }
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeColorF
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeColorF);

void GuiInspectorTypeColorF::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeColorF)->setInspectorFieldType("GuiInspectorTypeColorF");
}

GuiInspectorTypeColorF::GuiInspectorTypeColorF()
{
   mColorFunction = StringTable->insert("getColorF");
}

void GuiInspectorTypeColorF::setValue( StringTableEntry newValue )
{
   // Allow parent to set the edit-ctrl text to the new value.
   Parent::setValue( newValue );

   // Now we also set our color swatch button to the new color value.
   if ( mBrowseButton )
   {      
      ColorF color(1,0,1,1);
      dSscanf( newValue, "%f %f %f %f", &color.red, &color.green, &color.blue, &color.alpha );
      mBrowseButton->setColor( color );
   }
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeS32
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeS32);

void GuiInspectorTypeS32::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeS32)->setInspectorFieldType("GuiInspectorTypeS32");
}

GuiControl* GuiInspectorTypeS32::constructEditControl()
{
   GuiControl* retCtrl = new GuiTextEditSliderCtrl();

   // If we couldn't construct the control, bail!
   if( retCtrl == NULL )
      return retCtrl;

   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorTextEditProfile" );

   // Don't forget to register ourselves
   _registerEditControl( retCtrl );

   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply(%d.getText());",getId(), retCtrl->getId() );
   retCtrl->setField("AltCommand", szBuffer );
   retCtrl->setField("Validate", szBuffer );
   retCtrl->setField("increment","1");
   retCtrl->setField("format","%d");
   retCtrl->setField("range","-2147483648 2147483647");

   return retCtrl;
}

void GuiInspectorTypeS32::setValue( StringTableEntry newValue )
{
   GuiTextEditSliderCtrl *ctrl = dynamic_cast<GuiTextEditSliderCtrl*>( mEdit );
   if( ctrl != NULL )
      ctrl->setText( newValue );
}

//-----------------------------------------------------------------------------
// GuiInspectorTypeS32Mask
//-----------------------------------------------------------------------------

GuiInspectorTypeBitMask32::GuiInspectorTypeBitMask32()
 : mRollout( NULL ),
   mArrayCtrl( NULL ),
   mHelper( NULL )
{
}

IMPLEMENT_CONOBJECT( GuiInspectorTypeBitMask32 );

bool GuiInspectorTypeBitMask32::onAdd()
{
   // Skip our parent because we aren't using mEditCtrl
   // and according to our parent that would be cause to fail onAdd.
   if ( !Parent::Parent::onAdd() )
      return false;

   if ( !mTarget || !mInspector )
      return false;

   if ( !mField->table )
      return false;

   setDataField( StringTable->insert("profile"), NULL, "GuiInspectorFieldProfile" );   
   setBounds(0,0,100,18);

   // Allocate our children controls...

   mRollout = new GuiRolloutCtrl();
   mRollout->mMargin.set( 14, 0, 0, 0 );
   mRollout->mCanCollapse = false;
   mRollout->registerObject();
   addObject( mRollout );

   mArrayCtrl = new GuiDynamicCtrlArrayControl();
   mArrayCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorBitMaskArrayProfile" );
   mArrayCtrl->setField( "autoCellSize", "true" );
   mArrayCtrl->setField( "fillRowFirst", "true" );
   mArrayCtrl->setField( "dynamicSize", "true" );
   mArrayCtrl->setField( "rowSpacing", "4" );
   mArrayCtrl->setField( "colSpacing", "1" );
   mArrayCtrl->setField( "frozen", "true" );
   mArrayCtrl->registerObject();
   
   mRollout->addObject( mArrayCtrl );

   GuiCheckBoxCtrl *pCheckBox = NULL;

   for ( S32 i = 0; i < mField->table->size; i++ )
   {   
      pCheckBox = new GuiCheckBoxCtrl();
      pCheckBox->setText( mField->table->table[i].label );
      pCheckBox->registerObject();      
      mArrayCtrl->addObject( pCheckBox );

      pCheckBox->autoSize();

      // Override the normal script callbacks for GuiInspectorTypeCheckBox
      char szBuffer[512];
      dSprintf( szBuffer, 512, "%d.applyBit();", getId() );
      pCheckBox->setField( "Command", szBuffer );   
   }      

   mArrayCtrl->setField( "frozen", "false" );
   mArrayCtrl->refresh(); 

   mHelper = new GuiInspectorTypeBitMask32Helper();
   mHelper->init( mInspector, mParent, mTarget );
   mHelper->mParentRollout = mRollout;
   mHelper->mParentField = this;
   mHelper->setInspectorField( mField, mCaption, mFieldArrayIndex );
   mHelper->registerObject();
   mHelper->setExtent( pCheckBox->getExtent() );
   mHelper->setPosition( 0, 0 );
   mRollout->addObject( mHelper );

   mRollout->sizeToContents();
   mRollout->instantCollapse();  

   updateValue();

   return true;
}

void GuiInspectorTypeBitMask32::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeBitMask32)->setInspectorFieldType("GuiInspectorTypeBitMask32");
}

void GuiInspectorTypeBitMask32::childResized( GuiControl *child )
{
   setExtent( mRollout->getExtent() );
}

bool GuiInspectorTypeBitMask32::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   if ( !Parent::resize( newPosition, newExtent ) )
      return false;
   
   // Hack... height of 18 is hardcoded
   return mHelper->resize( Point2I(0,0), Point2I( newExtent.x, 18 ) );
}

bool GuiInspectorTypeBitMask32::updateRects()
{
   if ( !mRollout )
      return false;

   bool result = mRollout->setExtent( getExtent() );   
   
   for ( U32 i = 0; i < mArrayCtrl->size(); i++ )
   {
      GuiInspectorField *pField = dynamic_cast<GuiInspectorField*>( mArrayCtrl->at(i) );
      if ( pField )
         if ( pField->updateRects() )
            result = true;
   }

   if ( mHelper && mHelper->updateRects() )
      result = true;

   return result;   
}

void GuiInspectorTypeBitMask32::setData( StringTableEntry data )
{
   if ( mField == NULL || mTarget == NULL )
      return;

   mTarget->inspectPreApply();

   // Callback on the inspector when the field is modified
   // to allow creation of undo/redo actions.
   const char *oldData = mTarget->getDataField( mField->pFieldname, mFieldArrayIndex);
   if ( !oldData )
      oldData = "";
   if ( dStrcmp( oldData, data ) != 0 )
      Con::executef( mInspector, "onInspectorFieldModified", Con::getIntArg(mTarget->getId()), mField->pFieldname, oldData, data );

   mTarget->setDataField( mField->pFieldname, mFieldArrayIndex, data );

   // give the target a chance to validate
   mTarget->inspectPostApply();

   // Force our edit to update
   updateValue();
}

StringTableEntry GuiInspectorTypeBitMask32::getValue()
{
   if ( !mRollout )
      return StringTable->insert( "" );

   S32 mask = 0;

   for ( U32 i = 0; i < mArrayCtrl->size(); i++ )
   {
      GuiCheckBoxCtrl *pCheckBox = dynamic_cast<GuiCheckBoxCtrl*>( mArrayCtrl->at(i) );

      bool bit = pCheckBox->getStateOn();
      mask |= bit << i;
   }

   return StringTable->insert( String::ToString(mask).c_str() );
}

void GuiInspectorTypeBitMask32::setValue( StringTableEntry value )
{
   U32 mask = dAtoui( value, 0 );

   for ( U32 i = 0; i < mArrayCtrl->size(); i++ )
   {
      GuiCheckBoxCtrl *pCheckBox = dynamic_cast<GuiCheckBoxCtrl*>( mArrayCtrl->at(i) );

      bool bit = mask & ( 1 << i );
      pCheckBox->setStateOn( bit );
   }

   mHelper->setValue( value );
}

void GuiInspectorTypeBitMask32::updateData()
{
   StringTableEntry data = getValue();
   setData( data );   
}

ConsoleMethod( GuiInspectorTypeBitMask32, applyBit, void, 2,2, "apply();" )
{
   object->updateData();
}

//------------------------------------------------------------------------------
// GuiInspectorTypeS32MaskHelper
//------------------------------------------------------------------------------

GuiInspectorTypeBitMask32Helper::GuiInspectorTypeBitMask32Helper()
: mButton( NULL ),
  mParentRollout( NULL ),
  mParentField( NULL )
{
}

IMPLEMENT_CONOBJECT( GuiInspectorTypeBitMask32Helper );

GuiControl* GuiInspectorTypeBitMask32Helper::constructEditControl()
{
   GuiControl *retCtrl = new GuiTextEditCtrl();
   retCtrl->setDataField( StringTable->insert("profile"), NULL, "GuiInspectorTextEditProfile" );
   retCtrl->setField( "hexDisplay", "true" );

   _registerEditControl( retCtrl );

   char szBuffer[512];
   dSprintf( szBuffer, 512, "%d.apply(%d.getText());", mParentField->getId(), retCtrl->getId() );
   retCtrl->setField( "AltCommand", szBuffer );
   retCtrl->setField( "Validate", szBuffer );

   mButton = new GuiBitmapButtonCtrl();

   RectI browseRect( Point2I( ( getLeft() + getWidth()) - 26, getTop() + 2), Point2I(20, getHeight() - 4) );
   dSprintf( szBuffer, 512, "%d.toggleExpanded(false);", mParentRollout->getId() );
   mButton->setField( "Command", szBuffer );
   mButton->setField( "buttonType", "ToggleButton" );
   mButton->setDataField( StringTable->insert("Profile"), NULL, "GuiInspectorButtonProfile" );
   mButton->setBitmap( "core/gui/images/arrowBtn" );
   mButton->setStateOn( true );
   mButton->setExtent( 16, 16 );
   mButton->registerObject();
   addObject( mButton );

   mButton->resize( browseRect.point, browseRect.extent );

   return retCtrl;
}

bool GuiInspectorTypeBitMask32Helper::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   if ( !Parent::resize( newPosition, newExtent ) )
      return false;

   if ( mEdit != NULL )
   {
      return updateRects();
   }

   return false;
}

bool GuiInspectorTypeBitMask32Helper::updateRects()
{
   S32 dividerPos, dividerMargin;
   mInspector->getDivider( dividerPos, dividerMargin );
   Point2I fieldExtent = getExtent();
   Point2I fieldPos = getPosition();

   mCaptionRect.set( 0, 0, fieldExtent.x - dividerPos - dividerMargin, fieldExtent.y );
   mEditCtrlRect.set( fieldExtent.x - dividerPos + dividerMargin, 1, dividerPos - dividerMargin - 32, fieldExtent.y );

   bool editResize = mEdit->resize( mEditCtrlRect.point, mEditCtrlRect.extent );
   bool buttonResize = false;

   if ( mButton != NULL )
   {
      mButtonRect.set( fieldExtent.x - 26, 2, 16, 16 );
      buttonResize = mButton->resize( mButtonRect.point, mButtonRect.extent );
   }

   return ( editResize || buttonResize );
}

void GuiInspectorTypeBitMask32Helper::setValue( StringTableEntry newValue )
{
   GuiTextEditCtrl *edit = dynamic_cast<GuiTextEditCtrl*>(mEdit);
   edit->setText( newValue );
}


//-----------------------------------------------------------------------------
// GuiInspectorTypeName 
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiInspectorTypeName);

void GuiInspectorTypeName::consoleInit()
{
   Parent::consoleInit();

   ConsoleBaseType::getType(TypeName)->setInspectorFieldType("GuiInspectorTypeName");
}

bool GuiInspectorTypeName::verifyData( StringTableEntry data )
{   
   if( !data || !data[ 0 ] )
      return true;

   bool isValidId = true;
   if( !dIsalpha( data[ 0 ] ) && data[ 0 ] != '_' )
      isValidId = false;
   else
   {
      for( U32 i = 1; data[ i ] != '\0'; ++ i )
      {
         if( !dIsalnum( data[ i ] ) && data[ i ] != '_' )
         {
            isValidId = false;
            break;
         }
      }
   }
   
   if( !isValidId )
   {
      Platform::AlertOK( "Error", "Object name must be a valid TorqueScript identifier" );
      return false;
   }

   SimObject *pTemp = NULL;
   if ( Sim::findObject( data, pTemp ) && pTemp != mTarget )
   {
      Platform::AlertOK( "Error", "Cannot assign name, object with that name already exists." );
      return false;
   }

   return true;
}

