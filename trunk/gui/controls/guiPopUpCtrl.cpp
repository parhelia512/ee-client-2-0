// Revision History:
// December 31, 2003	David Wyand		Changed a bunch of stuff.  Search for DAW below
//										and make better notes here and in the changes doc.
// May 19, 2004			David Wyand		Made changes to allow for a coloured rectangle to be
//										displayed to the left of the text in the popup.
// May 27, 2004			David Wyand		Added a check for mReverseTextList to see if we should
//										reverse the text list if we must render it above
//										the control.  NOTE: there are some issues with setting
//										the selected ID using the 'setSelected' command externally
//										and the list is rendered in reverse.  It seems that the
//										entry ID's also get reversed.
// November 16, 2005	David Wyand		Added the method setNoneSelected() to set none of the
//										items as selected.  Use this over setSelected(-1); when
//										there are negative IDs in the item list.  This also
//										includes the setNoneSelected console method.
//
//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/core/guiCanvas.h"
#include "gui/controls/guiPopUpCtrl.h"
#include "console/consoleTypes.h"
#include "gui/core/guiDefaultControlRender.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxDrawUtil.h"

static ColorI colorWhite(255,255,255); //  Added

// Function to return the number of columns in 'string' given delimeters in 'set'
static U32 getColumnCount(const char *string, const char *set)
{
   U32 count = 0;
   U8 last = 0;
   while(*string)
   {
      last = *string++;

      for(U32 i =0; set[i]; i++)
      {
         if(last == set[i])
         {
            count++;
            last = 0;
            break;
         }   
      }
   }
   if(last)
      count++;
   return count;
}   

// Function to return the 'index' column from 'string' given delimeters in 'set'
static const char *getColumn(const char *string, char* returnbuff, U32 index, const char *set)
{
   U32 sz;
   while(index--)
   {
      if(!*string)
         return "";
      sz = dStrcspn(string, set);
      if (string[sz] == 0)
         return "";
      string += (sz + 1);    
   }
   sz = dStrcspn(string, set);
   if (sz == 0)
      return "";
   char *ret = returnbuff;
   dStrncpy(ret, string, sz);
   ret[sz] = '\0';
   return ret;
}   

GuiPopUpBackgroundCtrl::GuiPopUpBackgroundCtrl(GuiPopUpMenuCtrl *ctrl, GuiPopupTextListCtrl *textList)
{
   mPopUpCtrl = ctrl;
   mTextList = textList;
}

void GuiPopUpBackgroundCtrl::onMouseDown(const GuiEvent &event)
{
   //   mTextList->setSelectedCell(Point2I(-1,-1)); //  Removed
   mPopUpCtrl->mBackgroundCancel = true; //  Set that the user didn't click within the text list.  Replaces the line above.
   mPopUpCtrl->closePopUp();
}

//------------------------------------------------------------------------------
GuiPopupTextListCtrl::GuiPopupTextListCtrl()
{
   mPopUpCtrl = NULL;
}


//------------------------------------------------------------------------------
GuiPopupTextListCtrl::GuiPopupTextListCtrl(GuiPopUpMenuCtrl *ctrl)
{
   mPopUpCtrl = ctrl;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//void GuiPopUpTextListCtrl::onCellSelected( Point2I /*cell*/ )
//{
//   // Do nothing, the parent control will take care of everything...
//}
void GuiPopupTextListCtrl::onCellSelected( Point2I cell )
{
   //  The old function is above.  This new one will only call the the select
   //      functions if we were not cancelled by a background click.

   //  Check if we were cancelled by the user clicking on the Background ie: anywhere
   //      other than within the text list.
   if(mPopUpCtrl->mBackgroundCancel)
      return;

   if( isMethod( "onSelect" ) )
      Con::executef(this, "onSelect", Con::getFloatArg(cell.x), Con::getFloatArg(cell.y));

   //call the console function
   execConsoleCallback();
   //if (mConsoleCommand[0])
   //   Con::evaluate(mConsoleCommand, false);

}

//------------------------------------------------------------------------------
bool GuiPopupTextListCtrl::onKeyDown(const GuiEvent &event)
{
   //if the control is a dead end, don't process the input:
   if ( !mVisible || !mActive || !mAwake )
      return false;

   //see if the key down is a <return> or not
   if ( event.modifier == 0 )
   {
      if ( event.keyCode == KEY_RETURN )
      {
         mPopUpCtrl->closePopUp();
         return true;
      }
      else if ( event.keyCode == KEY_ESCAPE )
      {
         mSelectedCell.set( -1, -1 );
         mPopUpCtrl->closePopUp();
         return true;
      }
   }

   //otherwise, pass the event to it's parent
   return Parent::onKeyDown(event);
}

void GuiPopupTextListCtrl::onMouseDown(const GuiEvent &event)
{
   // Moved to onMouseUp in order to capture the mouse for the entirety of the click. ADL.
   // This also has the side effect of allowing the mouse to be held down and a selection made.
   //Parent::onMouseDown(event);
   //mPopUpCtrl->closePopUp();
}

void GuiPopupTextListCtrl::onMouseUp(const GuiEvent &event)
{
   Parent::onMouseDown(event);
   mPopUpCtrl->closePopUp();
   Parent::onMouseUp(event);
}

//------------------------------------------------------------------------------
void GuiPopupTextListCtrl::onRenderCell(Point2I offset, Point2I cell, bool selected, bool mouseOver)
{
   Point2I size;
   getCellSize( size );

   // Render a background color for the cell
   if ( mouseOver )
   {      
      RectI cellR( offset.x, offset.y, size.x, size.y );
      GFX->getDrawUtil()->drawRectFill( cellR, mProfile->mFillColorHL );

   }
   else if ( selected )
   {
      RectI cellR( offset.x, offset.y, size.x, size.y );
      GFX->getDrawUtil()->drawRectFill( cellR, mProfile->mFillColorSEL );
   }

   //  Define the default x offset for the text
   U32 textXOffset = offset.x + mProfile->mTextOffset.x;

   //  Do we also draw a colored box beside the text?
   ColorI boxColor;
   bool drawbox = mPopUpCtrl->getColoredBox( boxColor, mList[cell.y].id);
   if(drawbox)
   {
      Point2I coloredboxsize(15,10);
      RectI r(offset.x + mProfile->mTextOffset.x, offset.y+2, coloredboxsize.x, coloredboxsize.y);
      GFX->getDrawUtil()->drawRectFill( r, boxColor);
      GFX->getDrawUtil()->drawRect( r, ColorI(0,0,0));

      textXOffset += coloredboxsize.x + mProfile->mTextOffset.x;
   }

   ColorI fontColor;
   mPopUpCtrl->getFontColor( fontColor, mList[cell.y].id, selected, mouseOver );

   GFX->getDrawUtil()->setBitmapModulation( fontColor );
   //GFX->drawText( mFont, Point2I( offset.x + 4, offset.y ), mList[cell.y].text );

   //  Get the number of columns in the cell
   S32 colcount = getColumnCount(mList[cell.y].text, "\t");

   //  Are there two or more columns?
   if(colcount >= 2)
   {
      char buff[256];

      // Draw the first column
      getColumn(mList[cell.y].text, buff, 0, "\t");
      GFX->getDrawUtil()->drawText( mFont, Point2I( textXOffset, offset.y ), buff ); //  Used mTextOffset as a margin for the text list rather than the hard coded value of '4'.

      // Draw the second column to the right
      getColumn(mList[cell.y].text, buff, 1, "\t");
      S32 txt_w = mFont->getStrWidth(buff);

      GFX->getDrawUtil()->drawText( mFont, Point2I( offset.x+size.x-mProfile->mTextOffset.x-txt_w, offset.y ), buff ); //  Used mTextOffset as a margin for the text list rather than the hard coded value of '4'.

   } else
   {
      GFX->getDrawUtil()->drawText( mFont, Point2I( textXOffset, offset.y ), mList[cell.y].text ); //  Used mTextOffset as a margin for the text list rather than the hard coded value of '4'.
   }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiPopUpMenuCtrl);

GuiPopUpMenuCtrl::GuiPopUpMenuCtrl(void)
{
   VECTOR_SET_ASSOCIATION(mEntries);
   VECTOR_SET_ASSOCIATION(mSchemes);

   mSelIndex = -1;
   mActive = true;
   mMaxPopupHeight = 200;
   mScrollDir = GuiScrollCtrl::None;
   mScrollCount = 0;
   mLastYvalue = 0;
   mIncValue = 0;
   mRevNum = 0;
   mInAction = false;
   mMouseOver = false; //  Added
   mRenderScrollInNA = false; //  Added
   mBackgroundCancel = false; //  Added
   mReverseTextList = false; //  Added - Don't reverse text list if displaying up
   mBitmapName = StringTable->insert(""); //  Added
   mBitmapBounds.set(16, 16); //  Added
	mIdMax = -1;
}

//------------------------------------------------------------------------------
GuiPopUpMenuCtrl::~GuiPopUpMenuCtrl()
{
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::initPersistFields(void)
{
   addField("maxPopupHeight",           TypeS32,          Offset(mMaxPopupHeight, GuiPopUpMenuCtrl));
   addField("sbUsesNAColor",            TypeBool,         Offset(mRenderScrollInNA, GuiPopUpMenuCtrl));
   addField("reverseTextList",          TypeBool,         Offset(mReverseTextList, GuiPopUpMenuCtrl));
   addField("bitmap",                   TypeFilename,     Offset(mBitmapName, GuiPopUpMenuCtrl));
   addField("bitmapBounds",             TypePoint2I,      Offset(mBitmapBounds, GuiPopUpMenuCtrl));

   Parent::initPersistFields();
}

//------------------------------------------------------------------------------
ConsoleMethod( GuiPopUpMenuCtrl, add, void, 3, 5, "(string name, int idNum, int scheme=0)")
{
	if ( argc == 4 )
		object->addEntry(argv[2],dAtoi(argv[3]));
   if ( argc == 5 )
      object->addEntry(argv[2],dAtoi(argv[3]),dAtoi(argv[4]));
   else
      object->addEntry(argv[2]);
}

ConsoleMethod( GuiPopUpMenuCtrl, addScheme, void, 6, 6, "(int id, ColorI fontColor, ColorI fontColorHL, ColorI fontColorSEL)")
{
   ColorI fontColor, fontColorHL, fontColorSEL;
   U32 r, g, b;
   char buf[64];

   dStrcpy( buf, argv[3] );
   char* temp = dStrtok( buf, " \0" );
   r = temp ? dAtoi( temp ) : 0;
   temp = dStrtok( NULL, " \0" );
   g = temp ? dAtoi( temp ) : 0;
   temp = dStrtok( NULL, " \0" );
   b = temp ? dAtoi( temp ) : 0;
   fontColor.set( r, g, b );

   dStrcpy( buf, argv[4] );
   temp = dStrtok( buf, " \0" );
   r = temp ? dAtoi( temp ) : 0;
   temp = dStrtok( NULL, " \0" );
   g = temp ? dAtoi( temp ) : 0;
   temp = dStrtok( NULL, " \0" );
   b = temp ? dAtoi( temp ) : 0;
   fontColorHL.set( r, g, b );

   dStrcpy( buf, argv[5] );
   temp = dStrtok( buf, " \0" );
   r = temp ? dAtoi( temp ) : 0;
   temp = dStrtok( NULL, " \0" );
   g = temp ? dAtoi( temp ) : 0;
   temp = dStrtok( NULL, " \0" );
   b = temp ? dAtoi( temp ) : 0;
   fontColorSEL.set( r, g, b );

   object->addScheme( dAtoi( argv[2] ), fontColor, fontColorHL, fontColorSEL );
}

ConsoleMethod( GuiPopUpMenuCtrl, setText, void, 3, 3, "(string text)")
{
   object->setText(argv[2]);
}

ConsoleMethod( GuiPopUpMenuCtrl, getText, const char*, 2, 2, "")
{
   return object->getText();
}

ConsoleMethod( GuiPopUpMenuCtrl, clear, void, 2, 2, "Clear the popup list.")
{
   object->clear();
}

ConsoleMethod(GuiPopUpMenuCtrl, sort, void, 2, 2, "Sort the list alphabetically.")
{
   object->sort();
}

//  Added to sort the entries by ID
ConsoleMethod(GuiPopUpMenuCtrl, sortID, void, 2, 2, "Sort the list by ID.")
{
   object->sortID();
}

ConsoleMethod( GuiPopUpMenuCtrl, forceOnAction, void, 2, 2, "")
{
   object->onAction();
}

ConsoleMethod( GuiPopUpMenuCtrl, forceClose, void, 2, 2, "")
{
   object->closePopUp();
}

ConsoleMethod( GuiPopUpMenuCtrl, getSelected, S32, 2, 2, "")
{
   return object->getSelected();
}

ConsoleMethod( GuiPopUpMenuCtrl, setSelected, void, 3, 4, "(int id, [scriptCallback=true])")
{
   if( argc > 3 )
      object->setSelected( dAtoi( argv[2] ), dAtob( argv[3] ) );
   else
      object->setSelected( dAtoi( argv[2] ) );
}

ConsoleMethod( GuiPopUpMenuCtrl, setFirstSelected, void, 2, 3, "([scriptCallback=true])")
{
	if( argc > 2 )
      object->setFirstSelected( dAtob( argv[2] ) );
   else
      object->setFirstSelected();
}

ConsoleMethod( GuiPopUpMenuCtrl, setNoneSelected, void, 2, 2, "")
{
   object->setNoneSelected();
}

ConsoleMethod( GuiPopUpMenuCtrl, getTextById, const char*, 3, 3,  "(int id)")
{
   return(object->getTextById(dAtoi(argv[2])));
}

ConsoleMethod( GuiPopUpMenuCtrl, setEnumContent, void, 4, 4, "(string class, string enum)"
              "This fills the popup with a classrep's field enumeration type info.\n\n"
              "More of a helper function than anything.   If console access to the field list is added, "
              "at least for the enumerated types, then this should go away..")
{
   AbstractClassRep * classRep = AbstractClassRep::getClassList();

   // walk the class list to get our class
   while(classRep)
   {
      if(!dStricmp(classRep->getClassName(), argv[2]))
         break;
      classRep = classRep->getNextClass();
   }

   // get it?
   if(!classRep)
   {
      Con::warnf(ConsoleLogEntry::General, "failed to locate class rep for '%s'", argv[2]);
      return;
   }

   // walk the fields to check for this one (findField checks StringTableEntry ptrs...)
   U32 i;
   for(i = 0; i < classRep->mFieldList.size(); i++)
      if(!dStricmp(classRep->mFieldList[i].pFieldname, argv[3]))
         break;

   // found it?   
   if(i == classRep->mFieldList.size())
   {   
      Con::warnf(ConsoleLogEntry::General, "failed to locate field '%s' for class '%s'", argv[3], argv[2]);
      return;
   }

   const AbstractClassRep::Field & field = classRep->mFieldList[i];

   // check the type
   if(field.type != TypeEnum)
   {
      Con::warnf(ConsoleLogEntry::General, "field '%s' is not an enumeration for class '%s'", argv[3], argv[2]);
      return;
   }

   AssertFatal(field.table, avar("enumeration '%s' for class '%s' with NULL ", argv[3], argv[2]));

   // fill it
   for(i = 0; i < field.table->size; i++)
      object->addEntry(field.table->table[i].label, field.table->table[i].index);
}

//------------------------------------------------------------------------------
ConsoleMethod( GuiPopUpMenuCtrl, findText, S32, 3, 3, "(string text)"
              "Returns the position of the first entry containing the specified text.")
{
   return( object->findText( argv[2] ) );   
}

//------------------------------------------------------------------------------
ConsoleMethod( GuiPopUpMenuCtrl, size, S32, 2, 2, "Get the size of the menu - the number of entries in it.")
{
   return( object->getNumEntries() ); 
}

//------------------------------------------------------------------------------
ConsoleMethod( GuiPopUpMenuCtrl, replaceText, void, 3, 3, "(bool doReplaceText)")
{
   object->replaceText(dAtoi(argv[2]));  
}

//------------------------------------------------------------------------------
//  Added
bool GuiPopUpMenuCtrl::onWake()
{
   if ( !Parent::onWake() )
      return false;

   // Set the bitmap for the popup.
   setBitmap( mBitmapName );

   // Now update the Form Control's bitmap array, and possibly the child's too
   mProfile->constructBitmapArray();

   if ( mProfile->getChildrenProfile() )
      mProfile->getChildrenProfile()->constructBitmapArray();

   return true;
}

//------------------------------------------------------------------------------
bool GuiPopUpMenuCtrl::onAdd()
{
   if ( !Parent::onAdd() )
      return false;
   mSelIndex = -1;
   mReplaceText = true;
   return true;
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::onSleep()
{
   mTextureNormal = NULL; //  Added
   mTextureDepressed = NULL; //  Added
   Parent::onSleep();
   closePopUp();  // Tests in function.
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::clear()
{
   mEntries.setSize(0);
   setText("");
   mSelIndex = -1;
   mRevNum = 0;
	mIdMax = -1;
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::clearEntry( S32 entry )
{	
	if( entry == -1 )
		return;

	U32 i = 0;
	for ( ; i < mEntries.size(); i++ )
   {
      if ( mEntries[i].id == entry )
         break;
   }

	mEntries.erase( i );

	if( mEntries.size() <= 0 )
	{
		mEntries.setSize(0);
		setText("");
		mSelIndex = -1;
		mRevNum = 0;
	}
	else
	{
		if( entry == mSelIndex )
		{
			setText("");
			mSelIndex = -1;
		}
		else
		{
			mSelIndex--;
		}
	}
}

//------------------------------------------------------------------------------
ConsoleMethod( GuiPopUpMenuCtrl, clearEntry, void, 3, 3, "(S32 entry)")
{
   object->clearEntry(dAtoi(argv[2]));  
}

//------------------------------------------------------------------------------
static S32 QSORT_CALLBACK textCompare(const void *a,const void *b)
{
   GuiPopUpMenuCtrl::Entry *ea = (GuiPopUpMenuCtrl::Entry *) (a);
   GuiPopUpMenuCtrl::Entry *eb = (GuiPopUpMenuCtrl::Entry *) (b);
   return (dStrnatcasecmp(ea->buf, eb->buf));
} 

//  Added to sort by entry ID
//------------------------------------------------------------------------------
static S32 QSORT_CALLBACK idCompare(const void *a,const void *b)
{
   GuiPopUpMenuCtrl::Entry *ea = (GuiPopUpMenuCtrl::Entry *) (a);
   GuiPopUpMenuCtrl::Entry *eb = (GuiPopUpMenuCtrl::Entry *) (b);
   return ( (ea->id < eb->id) ? -1 : ((ea->id > eb->id) ? 1 : 0) );
} 

//------------------------------------------------------------------------------
//  Added
void GuiPopUpMenuCtrl::setBitmap( const char *name )
{
   mBitmapName = StringTable->insert( name );
   if ( !isAwake() )
      return;

   if ( *mBitmapName )
   {
      char buffer[1024];
      char *p;
      dStrcpy(buffer, name);
      p = buffer + dStrlen(buffer);

      dStrcpy(p, "_n");
      mTextureNormal = GFXTexHandle( (StringTableEntry)buffer, &GFXDefaultGUIProfile, avar("%s() - mTextureNormal (line %d)", __FUNCTION__, __LINE__) );

      dStrcpy(p, "_d");
      mTextureDepressed = GFXTexHandle( (StringTableEntry)buffer, &GFXDefaultGUIProfile, avar("%s() - mTextureDepressed (line %d)", __FUNCTION__, __LINE__) );
      if ( !mTextureDepressed )
         mTextureDepressed = mTextureNormal;
   }
   else
   {
      mTextureNormal = NULL;
      mTextureDepressed = NULL;
   }
   setUpdate();
}   

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::sort()
{
   S32 size = mEntries.size();
   if( size > 0 )
      dQsort( mEntries.address(), size, sizeof(Entry), textCompare);
}

// Added to sort by entry ID
//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::sortID()
{
   S32 size = mEntries.size();
   if( size > 0 )
      dQsort( mEntries.address(), size, sizeof(Entry), idCompare);
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::addEntry( const char *buf, S32 id, U32 scheme )
{
   if( !buf )
   {
      //Con::printf( "GuiPopupMenuCtrlEx::addEntry - Invalid buffer!" );
      return;
   }
	
	// Ensure that there are no other entries with exactly the same name
	for ( U32 i = 0; i < mEntries.size(); i++ )
   {
      if ( dStrcmp( mEntries[i].buf, buf ) == 0 )
         return;
   }

	// If we don't give an id, create one from mIdMax
	if( id == -1 )
		id = mIdMax + 1;
	
	// Increase mIdMax when an id is greater than it
	if( id > mIdMax )
		mIdMax = id;

   Entry e;
   dStrcpy( e.buf, buf );
   e.id = id;
   e.scheme = scheme;

   // see if there is a shortcut key
   char * cp = dStrchr( e.buf, '~' );
   e.ascii = cp ? cp[1] : 0;

   //  See if there is a colour box defined with the text
   char *cb = dStrchr( e.buf, '|' );
   if ( cb )
   {
      e.usesColorBox = true;
      cb[0] = '\0';

      char* red = &cb[1];
      cb = dStrchr(red, '|');
      cb[0] = '\0';
      char* green = &cb[1];
      cb = dStrchr(green, '|');
      cb[0] = '\0';
      char* blue = &cb[1];

      U32 r = dAtoi(red);
      U32 g = dAtoi(green);
      U32 b = dAtoi(blue);

      e.colorbox = ColorI(r,g,b);

   } 
   else
   {
      e.usesColorBox = false;
   }

   mEntries.push_back(e);

   if ( mInAction && mTl )
   {
      // Add the new entry:
      mTl->addEntry( e.id, e.buf );
      repositionPopup();
   }
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::addScheme( U32 id, ColorI fontColor, ColorI fontColorHL, ColorI fontColorSEL )
{
   if ( !id )
      return;

   Scheme newScheme;
   newScheme.id = id;
   newScheme.fontColor = fontColor;
   newScheme.fontColorHL = fontColorHL;
   newScheme.fontColorSEL = fontColorSEL;

   mSchemes.push_back( newScheme );
}

//------------------------------------------------------------------------------
S32 GuiPopUpMenuCtrl::getSelected()
{
   if (mSelIndex == -1)
      return 0;
   return mEntries[mSelIndex].id;
}

//------------------------------------------------------------------------------
const char* GuiPopUpMenuCtrl::getTextById(S32 id)
{
   for ( U32 i = 0; i < mEntries.size(); i++ )
   {
      if ( mEntries[i].id == id )
         return( mEntries[i].buf );
   }

   return( "" );
}

//------------------------------------------------------------------------------
S32 GuiPopUpMenuCtrl::findText( const char* text )
{
   for ( U32 i = 0; i < mEntries.size(); i++ )
   {
      if ( dStrcmp( text, mEntries[i].buf ) == 0 )
         return( mEntries[i].id );        
   }
   return( -1 );
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::setSelected(S32 id, bool bNotifyScript )
{
   S32 i = 0;
   for ( ; i < mEntries.size(); i++ )
   {
      if ( id == mEntries[i].id )
      {
         i = ( mRevNum > i ) ? mRevNum - i : i;
         mSelIndex = i;
         if ( mReplaceText ) //  Only change the displayed text if appropriate.
         {
            setText(mEntries[i].buf);
         }

         // Now perform the popup action:
         char idval[24];
         dSprintf( idval, sizeof(idval), "%d", mEntries[mSelIndex].id );
         if ( isMethod( "onSelect" ) && bNotifyScript )
            Con::executef( this, "onSelect", idval, mEntries[mSelIndex].buf );
         return;
      }
   }

   if ( mReplaceText ) //  Only change the displayed text if appropriate.
   {
      setText("");
   }
   mSelIndex = -1;

   if ( isMethod( "onCancel" ) && bNotifyScript )
      Con::executef( this, "onCancel" );

   if ( id == -1 )
      return;

   // Execute the popup console command:
   if ( bNotifyScript )
      execConsoleCallback();
   //if ( mConsoleCommand[0]  && bNotifyScript )
   //   Con::evaluate( mConsoleCommand, false );
}

//------------------------------------------------------------------------------
//  Added to set the first item as selected.
void GuiPopUpMenuCtrl::setFirstSelected( bool bNotifyScript )
{
   if ( mEntries.size() > 0 )
   {
      mSelIndex = 0;
      if ( mReplaceText ) //  Only change the displayed text if appropriate.
      {
         setText( mEntries[0].buf );
      }

      // Now perform the popup action:
      char idval[24];
      dSprintf( idval, sizeof(idval), "%d", mEntries[mSelIndex].id );
      if ( isMethod( "onSelect" ) )
         Con::executef( this, "onSelect", idval, mEntries[mSelIndex].buf );
      
		// Execute the popup console command:
		if ( bNotifyScript )
			execConsoleCallback();
   }
	else
	{
		if ( mReplaceText ) //  Only change the displayed text if appropriate.
			setText("");
		
		mSelIndex = -1;

		if ( bNotifyScript )
			Con::executef( this, "onCancel" );
	}
}

//------------------------------------------------------------------------------
//  Added to set no items as selected.
void GuiPopUpMenuCtrl::setNoneSelected()
{
   if ( mReplaceText ) //  Only change the displayed text if appropriate.
   {
      setText("");
   }
   mSelIndex = -1;
}

//------------------------------------------------------------------------------
const char *GuiPopUpMenuCtrl::getScriptValue()
{
   return getText();
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::onRender( Point2I offset, const RectI &updateRect )
{
   TORQUE_UNUSED(updateRect);
   Point2I localStart;

   if ( mScrollDir != GuiScrollCtrl::None )
      autoScroll();

   RectI r( offset, getExtent() );
   if ( mInAction )
   {
      S32 l = r.point.x, r2 = r.point.x + r.extent.x - 1;
      S32 t = r.point.y, b = r.point.y + r.extent.y - 1;

      // Do we render a bitmap border or lines?
      if ( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() )
      {
         // Render the fixed, filled in border
         renderFixedBitmapBordersFilled(r, 3, mProfile );

      } 
      else
      {
         //renderSlightlyLoweredBox(r, mProfile);
         GFX->getDrawUtil()->drawRectFill( r, mProfile->mFillColor );
      }

      //  Draw a bitmap over the background?
      if ( mTextureDepressed )
      {
         RectI rect(offset, mBitmapBounds);
         GFX->getDrawUtil()->clearBitmapModulation();
         GFX->getDrawUtil()->drawBitmapStretch( mTextureDepressed, rect );
      } 
      else if ( mTextureNormal )
      {
         RectI rect(offset, mBitmapBounds);
         GFX->getDrawUtil()->clearBitmapModulation();
         GFX->getDrawUtil()->drawBitmapStretch( mTextureNormal, rect );
      }

      // Do we render a bitmap border or lines?
      if ( !( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() ) )
      {
         GFX->getDrawUtil()->drawLine( l, t, l, b, colorWhite );
         GFX->getDrawUtil()->drawLine( l, t, r2, t, colorWhite );
         GFX->getDrawUtil()->drawLine( l + 1, b, r2, b, mProfile->mBorderColor );
         GFX->getDrawUtil()->drawLine( r2, t + 1, r2, b - 1, mProfile->mBorderColor );
      }

   }
   else   
      // TODO: Implement
      // TODO: Add onMouseEnter() and onMouseLeave() and a definition of mMouseOver (see guiButtonBaseCtrl) for this to work.
      if ( mMouseOver ) 
      {
         S32 l = r.point.x, r2 = r.point.x + r.extent.x - 1;
         S32 t = r.point.y, b = r.point.y + r.extent.y - 1;

         // Do we render a bitmap border or lines?
         if ( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() )
         {
            // Render the fixed, filled in border
            renderFixedBitmapBordersFilled( r, 2, mProfile );

         } 
         else
         {
            GFX->getDrawUtil()->drawRectFill( r, mProfile->mFillColorHL );
         }

         //  Draw a bitmap over the background?
         if ( mTextureNormal )
         {
            RectI rect( offset, mBitmapBounds );
            GFX->getDrawUtil()->clearBitmapModulation();
            GFX->getDrawUtil()->drawBitmapStretch( mTextureNormal, rect );
         }

         // Do we render a bitmap border or lines?
         if ( !( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() ) )
         {
            GFX->getDrawUtil()->drawLine( l, t, l, b, colorWhite );
            GFX->getDrawUtil()->drawLine( l, t, r2, t, colorWhite );
            GFX->getDrawUtil()->drawLine( l + 1, b, r2, b, mProfile->mBorderColor );
            GFX->getDrawUtil()->drawLine( r2, t + 1, r2, b - 1, mProfile->mBorderColor );
         }
      }
      else
      {
         // Do we render a bitmap border or lines?
         if ( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() )
         {
            // Render the fixed, filled in border
            renderFixedBitmapBordersFilled( r, 1, mProfile );
         } 
         else
         {
            GFX->getDrawUtil()->drawRectFill( r, mProfile->mFillColorNA );
         }

         //  Draw a bitmap over the background?
         if ( mTextureNormal )
         {
            RectI rect(offset, mBitmapBounds);
            GFX->getDrawUtil()->clearBitmapModulation();
            GFX->getDrawUtil()->drawBitmapStretch( mTextureNormal, rect );
         }

         // Do we render a bitmap border or lines?
         if ( !( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() ) )
         {
            GFX->getDrawUtil()->drawRect( r, mProfile->mBorderColorNA );
         }
      }
      //      renderSlightlyRaisedBox(r, mProfile); //  Used to be the only 'else' condition to mInAction above.

      S32 txt_w = mFont->getStrWidth(mText);
      localStart.x = 0;
      localStart.y = (getHeight() - (mFont->getHeight())) / 2;

      // align the horizontal
      switch (mProfile->mAlignment)
      {
      case GuiControlProfile::RightJustify:
         if ( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() )
         {
            // We're making use of a bitmap border, so take into account the
            // right cap of the border.
            RectI* mBitmapBounds = mProfile->mBitmapArrayRects.address();
            localStart.x = getWidth() - mBitmapBounds[2].extent.x - txt_w;
         } 
         else
         {
            localStart.x = getWidth() - txt_w;  
         }
         break;
      case GuiControlProfile::CenterJustify:
         if ( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() )
         {
            // We're making use of a bitmap border, so take into account the
            // right cap of the border.
            RectI* mBitmapBounds = mProfile->mBitmapArrayRects.address();
            localStart.x = (getWidth() - mBitmapBounds[2].extent.x - txt_w) / 2;

         } else
         {
            localStart.x = (getWidth() - txt_w) / 2;
         }
         break;
      default:
         // GuiControlProfile::LeftJustify
         if ( txt_w > getWidth() )
         {
            //  The width of the text is greater than the width of the control.
            // In this case we will right justify the text and leave some space
            // for the down arrow.
            if ( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() )
            {
               // We're making use of a bitmap border, so take into account the
               // right cap of the border.
               RectI* mBitmapBounds = mProfile->mBitmapArrayRects.address();
               localStart.x = getWidth() - mBitmapBounds[2].extent.x - txt_w;
            } 
            else
            {
               localStart.x = getWidth() - txt_w - 12;
            }
         } 
         else
         {
            localStart.x = mProfile->mTextOffset.x; //  Use mProfile->mTextOffset as a controlable margin for the control's text.
         }
         break;
      }

      //  Do we first draw a coloured box beside the text?
      ColorI boxColor;
      bool drawbox = getColoredBox( boxColor, mSelIndex);
      if ( drawbox )
      {
         Point2I coloredboxsize( 15, 10 );
         RectI r( offset.x + mProfile->mTextOffset.x, offset.y + ( (getHeight() - coloredboxsize.y ) / 2 ), coloredboxsize.x, coloredboxsize.y );
         GFX->getDrawUtil()->drawRectFill( r, boxColor);
         GFX->getDrawUtil()->drawRect( r, ColorI(0,0,0));

         localStart.x += coloredboxsize.x + mProfile->mTextOffset.x;
      }

      // Draw the text
      Point2I globalStart = localToGlobalCoord( localStart );
      ColorI fontColor   = mActive ? ( mInAction ? mProfile->mFontColor : mProfile->mFontColorNA ) : mProfile->mFontColorNA;
      GFX->getDrawUtil()->setBitmapModulation( fontColor ); //  was: (mProfile->mFontColor);

      //  Get the number of columns in the text
      S32 colcount = getColumnCount( mText, "\t" );

      //  Are there two or more columns?
      if ( colcount >= 2 )
      {
         char buff[256];

         // Draw the first column
         getColumn( mText, buff, 0, "\t" );
         GFX->getDrawUtil()->drawText( mFont, globalStart, buff, mProfile->mFontColors );

         // Draw the second column to the right
         getColumn( mText, buff, 1, "\t" );
         S32 txt_w = mFont->getStrWidth( buff );
         if ( mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size() )
         {
            // We're making use of a bitmap border, so take into account the
            // right cap of the border.
            RectI* mBitmapBounds = mProfile->mBitmapArrayRects.address();
            Point2I textpos = localToGlobalCoord( Point2I( getWidth() - txt_w - mBitmapBounds[2].extent.x, localStart.y ) );
            GFX->getDrawUtil()->drawText( mFont, textpos, buff, mProfile->mFontColors );

         } else
         {
            Point2I textpos = localToGlobalCoord( Point2I( getWidth() - txt_w - 12, localStart.y ) );
            GFX->getDrawUtil()->drawText( mFont, textpos, buff, mProfile->mFontColors );
         }

      } else
      {
         GFX->getDrawUtil()->drawText( mFont, globalStart, mText, mProfile->mFontColors );
      }

      // If we're rendering a bitmap border, then it will take care of the arrow.
      if ( !(mProfile->getChildrenProfile() && mProfile->mBitmapArrayRects.size()) )
      {
         //  Draw a triangle (down arrow)
         S32 left = r.point.x + r.extent.x - 12;
         S32 right = left + 8;
         S32 middle = left + 4;
         S32 top = r.extent.y / 2 + r.point.y - 4;
         S32 bottom = top + 8;

         PrimBuild::color( mProfile->mFontColor );

         PrimBuild::begin( GFXTriangleList, 3 );
            PrimBuild::vertex2fv( Point3F( (F32)left, (F32)top, 0.0f ) );
            PrimBuild::vertex2fv( Point3F( (F32)right, (F32)top, 0.0f ) );
            PrimBuild::vertex2fv( Point3F( (F32)middle, (F32)bottom, 0.0f ) );
         PrimBuild::end();
      }
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::closePopUp()
{
   if ( !mInAction )
      return;

   // Get the selection from the text list:
   mSelIndex = mTl->getSelectedCell().y;
   mSelIndex = ( mRevNum >= mSelIndex && mSelIndex != -1 ) ? mRevNum - mSelIndex : mSelIndex;
   if ( mSelIndex != -1 )
   {
      if ( mReplaceText )
         setText( mEntries[mSelIndex].buf );
      setIntVariable( mEntries[mSelIndex].id );
   }

   // Release the mouse:
   mInAction = false;
   mTl->mouseUnlock();

   //  Commented out below and moved to the end of the function.  See the
   // note below for the reason why.
   /*
   // Pop the background:
   getRoot()->popDialogControl(mBackground);

   // Kill the popup:
   mBackground->removeObject( mSc );
   mTl->deleteObject();
   mSc->deleteObject();
   mBackground->deleteObject();

   // Set this as the first responder:
   setFocus();
   */

   // Now perform the popup action:
   if ( mSelIndex != -1 )
   {
      char idval[24];
      dSprintf( idval, sizeof(idval), "%d", mEntries[mSelIndex].id );
      if ( isMethod( "onSelect" ) )
         Con::executef( this, "onSelect", idval, mEntries[mSelIndex].buf );
   }
   else if ( isMethod( "onCancel" ) )
      Con::executef( this, "onCancel" );

   // Execute the popup console command:
   execConsoleCallback();
   //if ( mConsoleCommand[0] )
   //   Con::evaluate( mConsoleCommand, false );

   //  Reordered this pop dialog to be after the script select callback.  When the
   // background was popped it was causing all sorts of focus issues when
   // suddenly the first text edit control took the focus before it came back
   // to this popup.

   // Pop the background:
   GuiCanvas *root = getRoot();
   if ( root )
      root->popDialogControl(mBackground);

   // Kill the popup:
   mBackground->removeObject( mSc );
   mTl->deleteObject();
   mSc->deleteObject();
   mBackground->deleteObject();

   // Set this as the first responder:
   setFirstResponder();
}

//------------------------------------------------------------------------------
bool GuiPopUpMenuCtrl::onKeyDown(const GuiEvent &event)
{
   //if the control is a dead end, don't process the input:
   if ( !mVisible || !mActive || !mAwake )
      return false;

   //see if the key down is a <return> or not
   if ( event.keyCode == KEY_RETURN && event.modifier == 0 )
   {
      onAction();
      return true;
   }

   //otherwise, pass the event to its parent
   return Parent::onKeyDown( event );
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::onAction()
{
   GuiControl *canCtrl = getParent();

   addChildren();

   GuiCanvas *root = getRoot();
   Point2I windowExt = root->getExtent();

   mBackground->resize( Point2I(0,0), root->getExtent() );
   
   S32 textWidth = 0, width = getWidth();
   //const S32 menuSpace = 5; //  Removed as no longer used.
   const S32 textSpace = 2;
   bool setScroll = false;

   for ( U32 i = 0; i < mEntries.size(); ++i )
      if ( S32(mFont->getStrWidth( mEntries[i].buf )) > textWidth )
         textWidth = mFont->getStrWidth( mEntries[i].buf );

   //if(textWidth > getWidth())
   S32 sbWidth = mSc->getControlProfile()->mBorderThickness * 2 + mSc->scrollBarThickness(); //  Calculate the scroll bar width
   if ( textWidth > ( getWidth() - sbWidth-mProfile->mTextOffset.x - mSc->getChildMargin().x * 2 ) ) //  The text draw area to test against is the width of the drop-down minus the scroll bar width, the text margin and the scroll bar child margins.
   {
      //textWidth +=10;
      textWidth +=sbWidth + mProfile->mTextOffset.x + mSc->getChildMargin().x * 2; //  The new width is the width of the text plus the scroll bar width plus the text margin size plus the scroll bar child margins.
      width = textWidth;

      //  If a child margin is not defined for the scroll control, let's add
      //      some space between the text and scroll control for readability
      if(mSc->getChildMargin().x == 0)
         width += textSpace;
   }

   //mTl->setCellSize(Point2I(width, mFont->getHeight()+3));
   mTl->setCellSize(Point2I(width, mFont->getHeight() + textSpace)); //  Modified the above line to use textSpace rather than the '3' as this is what is used below.

   for ( U32 j = 0; j < mEntries.size(); ++j )
      mTl->addEntry( mEntries[j].id, mEntries[j].buf );

   Point2I pointInGC = canCtrl->localToGlobalCoord( getPosition() );
   Point2I scrollPoint( pointInGC.x, pointInGC.y + getHeight() ); 

   //Calc max Y distance, so Scroll Ctrl will fit on window 
   //S32 maxYdis = windowExt.y - pointInGC.y - getHeight() - menuSpace; 
   S32 sbBorder = mSc->getControlProfile()->mBorderThickness * 2 + mSc->getChildMargin().y * 2; //  Added to take into account the border thickness and the margin of the child controls of the scroll control when figuring out the size of the contained text list control
   S32 maxYdis = windowExt.y - pointInGC.y - getHeight() - sbBorder; // - menuSpace; //  Need to remove the border thickness from the contained control maximum extent and got rid of the 'menuspace' variable

   //If scroll bars need to be added
   mRevNum = 0; //  Added here rather than within the following 'if' statements.
   if ( maxYdis < mTl->getHeight() + sbBorder ) //  Instead of adding sbBorder, it was: 'textSpace'
   {
      //Should we pop menu list above the button
      if ( maxYdis < pointInGC.y ) //  removed: '- menuSpace)' from check to see if there is more space above the control than below.
      {
         if(mReverseTextList) //  Added this check if we should reverse the text list.
            reverseTextList();

         maxYdis = pointInGC.y; //  Was at the end: '- menuSpace;'
         //Does the menu need a scroll bar 
         if ( maxYdis < mTl->getHeight() + sbBorder ) //  Instead of adding sbBorder, it was: 'textSpace'
         {
            //  Removed width recalculation for scroll bar as the scroll bar is already being taken into account.
            //Calc for the width of the scroll bar 
            //            if(textWidth >=  width)
            //               width += 20;
            //            mTl->setCellSize(Point2I(width,mFont->getHeight() + textSpace));

            //Pop menu list above the button
            //            scrollPoint.set(pointInGC.x, menuSpace - 1); //  Removed as calculated outside the 'if', and was wrong to begin with
            setScroll = true;
         }
         //No scroll bar needed
         else
         {
            maxYdis = mTl->getHeight() + sbBorder; //  Instead of adding sbBorder, it was: 'textSpace'
            //            scrollPoint.set(pointInGC.x, pointInGC.y - maxYdis -1); //  Removed as calculated outside the 'if' and the '-1' at the end is wrong
         }

         //  Added the next two lines
         scrollPoint.set(pointInGC.x, pointInGC.y - maxYdis); //  Used to have the following on the end: '-1);'
      } 
      //Scroll bar needed but Don't pop above button
      else
      {
         //mRevNum = 0; //  Commented out as it has been added at the beginning of this function
         if ( mSelIndex >= 0 )
            mTl->setSelectedCell( Point2I( 0, mSelIndex ) ); //  Added as we were not setting the selected cell if the list is displayed down.

         //  Removed width recalculation for scroll bar as the scroll bar is already being taken into account.
         //Calc for the width of the scroll bar 
         //         if(textWidth >=  width)
         //            width += 20;
         //         mTl->setCellSize(Point2I(width,mFont->getHeight() + textSpace));
         setScroll = true;
      }
   }
   //No scroll bar needed
   else
   {
      if ( mSelIndex >= 0 )
         mTl->setSelectedCell( Point2I( 0, mSelIndex ) ); //  Added as we were not setting the selected cell if the list is displayed down.

      //maxYdis = mTl->getHeight() + textSpace;
      maxYdis = mTl->getHeight() + sbBorder; //  Added in the border thickness of the scroll control and removed the addition of textSpace
   }

   RectI newBounds = mSc->getBounds();

   //offset it from the background so it lines up properly
   newBounds.point = mBackground->globalToLocalCoord( scrollPoint );

   if ( newBounds.point.x + width > mBackground->getWidth() )
      if ( width - getWidth() > 0 )
         newBounds.point.x -= width - getWidth();

   //mSc->getExtent().set(width-1, maxYdis);
   newBounds.extent.set( width, maxYdis );
   mSc->setBounds( newBounds ); //  Not sure why the '-1' above.

   mSc->registerObject();
   mTl->registerObject();
   mBackground->registerObject();

   mSc->addObject( mTl );
   mBackground->addObject( mSc );

   mBackgroundCancel = false; //  Setup check if user clicked on the background instead of the text list (ie: didn't want to change their current selection).

   root->pushDialogControl( mBackground, 99 );

   if ( setScroll )
   {
      // Resize the text list
	  Point2I cellSize;
	  mTl->getCellSize( cellSize );
	  cellSize.x = width - mSc->scrollBarThickness() - sbBorder;
	  mTl->setCellSize( cellSize );
	  mTl->setWidth( cellSize.x );

      if ( mSelIndex )
         mTl->scrollCellVisible( Point2I( 0, mSelIndex ) );
      else
         mTl->scrollCellVisible( Point2I( 0, 0 ) );
   }

   mTl->setFirstResponder();

   mInAction = true;
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::addChildren()
{
   // Create Text List.
   mTl = new GuiPopupTextListCtrl( this );
   AssertFatal( mTl, "Failed to create the GuiPopUpTextListCtrl for the PopUpMenu" );
   // Use the children's profile rather than the parent's profile, if it exists.
   mTl->setControlProfile( mProfile->getChildrenProfile() ? mProfile->getChildrenProfile() : mProfile ); 
   mTl->setField("noDuplicates", "false");

   mSc = new GuiScrollCtrl;
   AssertFatal( mSc, "Failed to create the GuiScrollCtrl for the PopUpMenu" );
   GuiControlProfile *prof;
   if ( Sim::findObject( "GuiScrollProfile", prof ) )
   {
      mSc->setControlProfile( prof );
   }
   else
   {
      // Use the children's profile rather than the parent's profile, if it exists.
	  mSc->setControlProfile( mProfile->getChildrenProfile() ? mProfile->getChildrenProfile() : mProfile );
   }

   mSc->setField( "hScrollBar", "AlwaysOff" );
   mSc->setField( "vScrollBar", "dynamic" );
   //if(mRenderScrollInNA) //  Force the scroll control to render using fillColorNA rather than fillColor
   // mSc->mUseNABackground = true;

   mBackground = new GuiPopUpBackgroundCtrl( this, mTl );
   AssertFatal( mBackground, "Failed to create the GuiBackgroundCtrl for the PopUpMenu" );
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::repositionPopup()
{
   if ( !mInAction || !mSc || !mTl )
      return;

   // I'm not concerned with this right now...
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::reverseTextList()
{
   mTl->clear();
   for ( S32 i = mEntries.size()-1; i >= 0; --i )
      mTl->addEntry( mEntries[i].id, mEntries[i].buf );

   // Don't lose the selected cell:
   if ( mSelIndex >= 0 )
      mTl->setSelectedCell( Point2I( 0, mEntries.size() - mSelIndex - 1 ) ); 

   mRevNum = mEntries.size() - 1;
}

//------------------------------------------------------------------------------
bool GuiPopUpMenuCtrl::getFontColor( ColorI &fontColor, S32 id, bool selected, bool mouseOver )
{
   U32 i;
   Entry* entry = NULL;
   for ( i = 0; i < mEntries.size(); i++ )
   {
      if ( mEntries[i].id == id )
      {
         entry = &mEntries[i];
         break;
      }
   }

   if ( !entry )
      return( false );

   if ( entry->scheme != 0 )
   {
      // Find the entry's color scheme:
      for ( i = 0; i < mSchemes.size(); i++ )
      {
         if ( mSchemes[i].id == entry->scheme )
         {
            fontColor = selected ? mSchemes[i].fontColorSEL : mouseOver ? mSchemes[i].fontColorHL : mSchemes[i].fontColor;
            return( true );
         }
      }
   }

   // Default color scheme...
   fontColor = selected ? mProfile->mFontColorSEL : mouseOver ? mProfile->mFontColorHL : mProfile->mFontColorNA; //  Modified the final color choice from mProfile->mFontColor to mProfile->mFontColorNA

   return( true );
}

//------------------------------------------------------------------------------
//  Added
bool GuiPopUpMenuCtrl::getColoredBox( ColorI &fontColor, S32 id )
{
   U32 i;
   Entry* entry = NULL;
   for ( i = 0; i < mEntries.size(); i++ )
   {
      if ( mEntries[i].id == id )
      {
         entry = &mEntries[i];
         break;
      }
   }

   if ( !entry )
      return false;

   if ( entry->usesColorBox == false )
      return false;

   fontColor = entry->colorbox;

   return true;
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::onMouseDown( const GuiEvent &event )
{
   TORQUE_UNUSED(event);
   onAction();
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::onMouseUp( const GuiEvent &event )
{
   TORQUE_UNUSED(event);
}

//------------------------------------------------------------------------------
//  Added
void GuiPopUpMenuCtrl::onMouseEnter( const GuiEvent &event )
{
   mMouseOver = true;
}

//------------------------------------------------------------------------------
//  Added
void GuiPopUpMenuCtrl::onMouseLeave( const GuiEvent &event )
{
   mMouseOver = false;
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::setupAutoScroll( const GuiEvent &event )
{
   GuiControl *parent = getParent();
   if ( !parent ) 
      return;

   Point2I mousePt = mSc->globalToLocalCoord( event.mousePoint );

   mEventSave = event;      

   if ( mLastYvalue != mousePt.y )
   {
      mScrollDir = GuiScrollCtrl::None;
      if ( mousePt.y > mSc->getHeight() || mousePt.y < 0 )
      {
         S32 topOrBottom = ( mousePt.y > mSc->getHeight() ) ? 1 : 0;
         mSc->scrollTo( 0, topOrBottom );
         return;
      }   

      F32 percent = (F32)mousePt.y / (F32)mSc->getHeight();
      if ( percent > 0.7f && mousePt.y > mLastYvalue )
      {
         mIncValue = percent - 0.5f;
         mScrollDir = GuiScrollCtrl::DownArrow;
      }
      else if ( percent < 0.3f && mousePt.y < mLastYvalue )
      {
         mIncValue = 0.5f - percent;         
         mScrollDir = GuiScrollCtrl::UpArrow;
      }
      mLastYvalue = mousePt.y;
   }
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::autoScroll()
{
   mScrollCount += mIncValue;

   while ( mScrollCount > 1 )
   {
      mSc->autoScroll( mScrollDir );
      mScrollCount -= 1;
   }
   mTl->onMouseMove( mEventSave );
}

//------------------------------------------------------------------------------
void GuiPopUpMenuCtrl::replaceText(S32 boolVal)
{
   mReplaceText = boolVal;
}
