//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/strings/findMatch.h"
#include "gui/controls/guiDirectoryFileListCtrl.h"


IMPLEMENT_CONOBJECT( GuiDirectoryFileListCtrl );

GuiDirectoryFileListCtrl::GuiDirectoryFileListCtrl()
{
   mFilePath = StringTable->insert( "" );
   mFilter = StringTable->insert( "*.*" );
}

void GuiDirectoryFileListCtrl::initPersistFields()
{
   addProtectedField( "filePath", TypeString, Offset( mFilePath, GuiDirectoryFileListCtrl ),
                      &_setFilePath, &defaultProtectedGetFn, 1, NULL, "Path in game directory from which to list files." );
   addProtectedField( "fileFilter", TypeString, Offset( mFilter, GuiDirectoryFileListCtrl ),
                      &_setFilter, &defaultProtectedGetFn, 1, NULL, "Tab-delimited list of file name patterns. Only matched files will be displayed." );
                      
   Parent::initPersistFields();
}

bool GuiDirectoryFileListCtrl::onWake()
{
   if( !Parent::onWake() )
      return false;
      
   update();
   
   return true;
}

void GuiDirectoryFileListCtrl::onMouseDown(const GuiEvent &event)
{
   Parent::onMouseDown( event );

   if( event.mouseClickCount == 2 && isMethod("onDoubleClick") )
      Con::executef(this, "onDoubleClick");
}


void GuiDirectoryFileListCtrl::openDirectory()
{
   String path;
   if( mFilePath && mFilePath[ 0 ] )
      path = String::ToString( "%s/%s", Platform::getMainDotCsDir(), mFilePath );
   else
      path = Platform::getMainDotCsDir();
   
   Vector<Platform::FileInfo> fileVector;
   Platform::dumpPath( path, fileVector, 0 );

   // Clear the current file listing
   clearItems();

   // Does this dir have any files?
   if( fileVector.empty() )
      return;

   // If so, iterate through and list them
   Vector<Platform::FileInfo>::iterator i = fileVector.begin();
   for( S32 j=0 ; i != fileVector.end(); i++, j++ )
   {
      if( !mFilter[ 0 ] || FindMatch::isMatchMultipleExprs( mFilter, (*i).pFileName,false ) )
         addItem( (*i).pFileName );
   }
}


void GuiDirectoryFileListCtrl::setCurrentFilter( const char* filter )
{
   if( !filter )
      filter = "";

   mFilter = StringTable->insert( filter );

   // Update our view
   openDirectory();
}

ConsoleMethod( GuiDirectoryFileListCtrl, setFilter, void, 3, 3, "%obj.setFilter([mask space delimited])")
{
   object->setCurrentFilter( argv[2] );
}

bool GuiDirectoryFileListCtrl::setCurrentPath( const char* path, const char* filter )
{
   if( !path )
      return false;

   const U32 pathLen = dStrlen( path );
   if( pathLen > 0 && path[ pathLen - 1 ] == '/' )
      mFilePath = StringTable->insertn( path, pathLen - 1 );
   else
      mFilePath = StringTable->insert( path );

   if( filter )
      mFilter  = StringTable->insert( filter );

   // Update our view
   openDirectory();

   return true;
}

ConsoleMethod( GuiDirectoryFileListCtrl, reload, void, 2, 2, "() - Update the file list." )
{
   object->update();
}

ConsoleMethod( GuiDirectoryFileListCtrl, setPath, bool, 3, 4, "setPath(path,filter) - directory to enumerate files from (without trailing slash)" )
{
   return object->setCurrentPath( argv[2], argv[3] );
}

ConsoleMethod( GuiDirectoryFileListCtrl, getSelectedFiles, const char*, 2, 2, "getSelectedFiles () - returns a word separated list of selected file(s)" )
{
   Vector<S32> ItemVector;
   object->getSelectedItems( ItemVector );

   if( ItemVector.empty() )
      return StringTable->insert( "" );

   // Get an adequate buffer
   char itemBuffer[256];
   dMemset( itemBuffer, 0, 256 );

   char* returnBuffer = Con::getReturnBuffer( ItemVector.size() * 64 );
   dMemset( returnBuffer, 0, ItemVector.size() * 64 );

   // Fetch the first entry
   StringTableEntry itemText = object->getItemText( ItemVector[0] );
   if( !itemText )
      return StringTable->lookup("");
   dSprintf( returnBuffer, ItemVector.size() * 64, "%s", itemText );

   // If only one entry, return it.
   if( ItemVector.size() == 1 )
      return returnBuffer;

   // Fetch the remaining entries
   for( S32 i = 1; i < ItemVector.size(); i++ )
   {
      StringTableEntry itemText = object->getItemText( ItemVector[i] );
      if( !itemText )
         continue;

      dMemset( itemBuffer, 0, 256 );
      dSprintf( itemBuffer, 256, " %s", itemText );
      dStrcat( returnBuffer, itemBuffer );
   }

   return returnBuffer;

}

StringTableEntry GuiDirectoryFileListCtrl::getSelectedFileName()
{
   S32 item = getSelectedItem();
   if( item == -1 )
      return StringTable->lookup("");

   StringTableEntry itemText = getItemText( item );
   if( !itemText )
      return StringTable->lookup("");

   return itemText;
}

ConsoleMethod( GuiDirectoryFileListCtrl, getSelectedFile, const char*, 2, 2, "getSelectedFile () - returns the currently selected file name" )
{
   return object->getSelectedFileName();
}
