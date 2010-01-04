//-----------------------------------------------------------------------------
// Torque Game Engine Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platformMac/platformMacCarb.h"
#include "platform/menus/menuBar.h"
#include "platform/menus/popupMenu.h"
#include "gui/core/guiCanvas.h"
#include "windowManager/platformWindowMgr.h"
#include "windowManager/platformWindow.h"


class PlatformMenuBarData
{
public:
   PlatformMenuBarData() :
      mMenuEventHandlerRef(NULL),
      mCommandEventHandlerRef(NULL),
      mMenuOpenCount( 0 )
   {}

   EventHandlerRef mMenuEventHandlerRef;
   EventHandlerRef mCommandEventHandlerRef;
   
   /// More evil hacking for OSX.  There seems to be no way to disable menu shortcuts and
   /// they are automatically routed within that Cocoa thing outside of our control.  Also,
   /// there's no way of telling what triggered a command event and thus no way of knowing
   /// whether it was a keyboard shortcut.  Sigh.
   ///
   /// So what we do is monitor the sequence of events leading to a command event:
   ///
   /// If we get one or more menu open events (without the respective number of close events) and
   /// then a command event, we know must have been either triggered by clicking a menu or by
   /// pressing the shortcut with the menu open.  Both is acceptable for running the menu command
   /// even when hotkeys are disabled.
   ///
   /// If, however, we simply receive a command event without a prior opening of menus we know
   /// it (very likely) must be a shortcut so when hotkeys are disabled, we reject handling those
   /// events so they get passed on to the usual input handling code.
   U32 mMenuOpenCount;
};

//-----------------------------------------------------------------------------

#pragma mark -
#pragma mark ---- menu event handler ----
static OSStatus _OnMenuEvent(EventHandlerCallRef nextHandler, EventRef theEvent, void *userData)
{
   PlatformMenuBarData* mbData = ( PlatformMenuBarData* ) userData;
   MenuRef menu;

   GetEventParameter(theEvent, kEventParamDirectObject, typeMenuRef, NULL, sizeof(MenuRef), NULL, &menu);
   
   // Count open/close for the sake of hotkey disabling.
   
   UInt32 kind = GetEventKind( theEvent );
   if( kind == kEventMenuOpening )
      mbData->mMenuOpenCount ++;
   else
   {
      AssertWarn( mbData->mMenuOpenCount > 0, "Unbalanced menu open/close events in _OnMenuEvent" );
      if( mbData->mMenuOpenCount )
         mbData->mMenuOpenCount --;
   }

   OSStatus err = eventNotHandledErr;
   PopupMenu *torqueMenu;
   if( CountMenuItems( menu ) > 0 )
   {
      // I don't know of a way to get the Torque PopupMenu object from a Carbon MenuRef
      //   other than going through its first menu item
      err = GetMenuItemProperty(menu, 1, 'GG2d', 'ownr', sizeof(PopupMenu*), NULL, &torqueMenu);
      if( err == noErr && torqueMenu != NULL )
      {
         torqueMenu->onMenuSelect();
      }
   }

   return err;
}

//-----------------------------------------------------------------------------

#pragma mark -
#pragma mark ---- menu command event handler ----
static bool MacCarbHandleMenuCommand( void* hiCommand, PlatformMenuBarData* mbData )
{
   HICommand *cmd = (HICommand*)hiCommand;
   
   if(cmd->commandID != kHICommandTorque)
      return false;
      
   MenuRef menu = cmd->menu.menuRef;
   MenuItemIndex item = cmd->menu.menuItemIndex;

   // If this command event came about without a menu open, then it was (probably)
   // triggered by a hotkey.  As we don't want hotkeys to trigger when they are disabled,
   // don't handle the event.
      
   if( !mbData->mMenuOpenCount )
   {
      PlatformWindow* window = PlatformWindowManager::get()->getFocusedWindow();
      if( !window || !window->getAcceleratorsEnabled() )
         return false;
   }
   
   // Run the command handler.
   
   PopupMenu* torqueMenu;
   OSStatus err = GetMenuItemProperty(menu, item, 'GG2d', 'ownr', sizeof(PopupMenu*), NULL, &torqueMenu);
   AssertFatal(err == noErr, "Could not resolve the PopupMenu stored on a native menu item");
   
   UInt32 command;
   err = GetMenuItemRefCon(menu, item, &command);
   AssertFatal(err == noErr, "Could not find the tag of a native menu item");
   
   if(!torqueMenu->canHandleID(command))
      Con::errorf("menu claims it cannot handle that id. how odd.");

   // un-highlight currently selected menu
   HiliteMenu( 0 );

   return torqueMenu->handleSelect(command,NULL);
}

//-----------------------------------------------------------------------------

#pragma mark -
#pragma mark ---- Command Events ----

static OSStatus _OnCommandEvent(EventHandlerCallRef nextHandler, EventRef theEvent, void* userData)
{
   PlatformMenuBarData* mbData = ( PlatformMenuBarData* ) userData;
   
   HICommand commandStruct;

   OSStatus result = eventNotHandledErr;
   
   GetEventParameter(theEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &commandStruct);
   
   // pass menu command events to a more specific handler.
   if(commandStruct.attributes & kHICommandFromMenu)
      if(MacCarbHandleMenuCommand(&commandStruct, mbData))
         result = noErr;
   
   return result;
}



//-----------------------------------------------------------------------------
// MenuBar Methods
//-----------------------------------------------------------------------------

void MenuBar::createPlatformPopupMenuData()
{
   
   mData = new PlatformMenuBarData;

}

void MenuBar::deletePlatformPopupMenuData()
{
   
    SAFE_DELETE(mData);
}

//-----------------------------------------------------------------------------

void MenuBar::attachToCanvas(GuiCanvas *owner, S32 pos)
{
   if(owner == NULL || isAttachedToCanvas())
      return;
      
   mCanvas = owner;
   
   
  // Add the items
   for(S32 i = 0;i < size();++i)
   {
      PopupMenu *mnu = dynamic_cast<PopupMenu *>(at(i));
      if(mnu == NULL)
      {
         Con::warnf("MenuBar::attachToMenuBar - Non-PopupMenu object in set");
         continue;
      }

      if(mnu->isAttachedToMenuBar())
         mnu->removeFromMenuBar();

      mnu->attachToMenuBar(owner, pos + i, mnu->getBarTitle());
   }
   
   // register as listener for menu opening events
   static EventTypeSpec menuEventTypes[ 2 ];
   
   menuEventTypes[ 0 ].eventClass = kEventClassMenu;
   menuEventTypes[ 0 ].eventKind = kEventMenuOpening;
   menuEventTypes[ 1 ].eventClass = kEventClassMenu;
   menuEventTypes[ 1 ].eventKind = kEventMenuClosed;

   EventHandlerUPP menuEventHandlerUPP;
   menuEventHandlerUPP = NewEventHandlerUPP(_OnMenuEvent);   
   InstallEventHandler(GetApplicationEventTarget(), menuEventHandlerUPP, 2, menuEventTypes, mData, &mData->mMenuEventHandlerRef);
   
   // register as listener for process command events
   static EventTypeSpec comEventTypes[1];
   comEventTypes[0].eventClass = kEventClassCommand;
   comEventTypes[0].eventKind = kEventCommandProcess;
   
   EventHandlerUPP commandEventHandlerUPP;
   commandEventHandlerUPP = NewEventHandlerUPP(_OnCommandEvent);
   InstallEventHandler(GetApplicationEventTarget(), commandEventHandlerUPP, 1, comEventTypes, mData, &mData->mCommandEventHandlerRef);
}

//-----------------------------------------------------------------------------

void MenuBar::removeFromCanvas()
{
   if(mCanvas == NULL || ! isAttachedToCanvas())
      return;
   
   if(mData->mCommandEventHandlerRef != NULL)
      RemoveEventHandler( mData->mCommandEventHandlerRef );
   mData->mCommandEventHandlerRef = NULL;
   
   if(mData->mMenuEventHandlerRef != NULL)
      RemoveEventHandler( mData->mMenuEventHandlerRef );
   mData->mMenuEventHandlerRef = NULL;

   // Add the items
   for(S32 i = 0;i < size();++i)
   {
      PopupMenu *mnu = dynamic_cast<PopupMenu *>(at(i));
      if(mnu == NULL)
      {
         Con::warnf("MenuBar::removeFromMenuBar - Non-PopupMenu object in set");
         continue;
      }

      mnu->removeFromMenuBar();
   }

   mCanvas = NULL;
}

//-----------------------------------------------------------------------------

void MenuBar::updateMenuBar(PopupMenu* menu)
{
   if(! isAttachedToCanvas())
      return;
   
   menu->removeFromMenuBar();
   SimSet::iterator itr = find(begin(), end(), menu);
   if(itr == end())
      return;
   
   // Get the item currently at the position we want to add to
   S32 pos = itr - begin();
   S16 posID = 0;
   
   PopupMenu *nextMenu = NULL;
   for(S32 i = pos + 1; i < size(); i++)
   {
      PopupMenu *testMenu = dynamic_cast<PopupMenu *>(at(i));
      if (testMenu && testMenu->isAttachedToMenuBar())
      {
         nextMenu = testMenu;
         break;
      }
   }

   if(nextMenu)
      posID = GetMenuID(nextMenu->mData->mMenu);
   
   menu->attachToMenuBar(mCanvas, posID, menu->mBarTitle);
}