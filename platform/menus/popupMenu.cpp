//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/menus/popupMenu.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "core/util/safeDelete.h"

//-----------------------------------------------------------------------------
// Constructor/Destructor
//-----------------------------------------------------------------------------

PopupMenu::PopupMenu() : mCanvas(NULL)
{
   createPlatformPopupMenuData();

   mSubmenus = new SimSet;
   mSubmenus->registerObject();

   mBarTitle = StringTable->insert("");
   mIsPopup = false;

   mNSLinkMask = LinkSuperClassName | LinkClassName;
}

PopupMenu::~PopupMenu()
{
   // This searches the menu bar so is safe to call for menus
   // that aren't on it, since nothing will happen.
   removeFromMenuBar();

   SimSet::iterator i;
   while((i = mSubmenus->begin()) != mSubmenus->end())
   {
      (*i)->deleteObject();
   }

   mSubmenus->deleteObject();
   deletePlatformPopupMenuData();
}

IMPLEMENT_CONOBJECT(PopupMenu);

//-----------------------------------------------------------------------------

void PopupMenu::initPersistFields()
{
   addField("isPopup",     TypeBool,         Offset(mIsPopup, PopupMenu),  "true if this is a pop-up/context menu. defaults to false.");
   addField("barTitle",    TypeCaseString,   Offset(mBarTitle, PopupMenu), "the title of this menu when attached to a menu bar");

   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------

bool PopupMenu::onAdd()
{
   if(! Parent::onAdd())
      return false;

   createPlatformMenu();

   Con::executef(this, "onAdd");
   return true;
}

void PopupMenu::onRemove()
{
   Con::executef(this, "onRemove");

   Parent::onRemove();
}

//-----------------------------------------------------------------------------

void PopupMenu::onMenuSelect()
{
   Con::executef(this, "onMenuSelect");
}

//-----------------------------------------------------------------------------

void PopupMenu::onAttachToMenuBar(GuiCanvas *canvas, S32 pos, const char *title)
{
   mCanvas = canvas;
   
   // Pass on to sub menus
   for(SimSet::iterator i = mSubmenus->begin();i != mSubmenus->end();++i)
   {
      PopupMenu *mnu = dynamic_cast<PopupMenu *>(*i);
      if(mnu == NULL)
         continue;

      mnu->onAttachToMenuBar(canvas, pos, title);
   }

   // Call script
   if(isProperlyAdded())
      Con::executef(this, "onAttachToMenuBar", Con::getIntArg(canvas ? canvas->getId() : 0), Con::getIntArg(pos), title);
}

void PopupMenu::onRemoveFromMenuBar(GuiCanvas *canvas)
{
   mCanvas = NULL;
   
   // Pass on to sub menus
   for(SimSet::iterator i = mSubmenus->begin();i != mSubmenus->end();++i)
   {
      PopupMenu *mnu = dynamic_cast<PopupMenu *>(*i);
      if(mnu == NULL)
         continue;

      mnu->onRemoveFromMenuBar(canvas);
   }

   // Call script
   if(isProperlyAdded())
      Con::executef(this, "onRemoveFromMenuBar", Con::getIntArg(canvas ? canvas->getId() : 0));
}

//-----------------------------------------------------------------------------

bool PopupMenu::onMessageReceived(StringTableEntry queue, const char* event, const char* data)
{
   return Con::executef(this, "onMessageReceived", queue, event, data);
}


bool PopupMenu::onMessageObjectReceived(StringTableEntry queue, Message *msg )
{
   return Con::executef(this, "onMessageReceived", queue, Con::getIntArg(msg->getId()));
}

//-----------------------------------------------------------------------------
// Console Methods
//-----------------------------------------------------------------------------

ConsoleMethod(PopupMenu, insertItem, S32, 3, 5, "(pos[, title][, accelerator])")
{
   return object->insertItem(dAtoi(argv[2]), argc < 4 ? NULL : argv[3], argc < 5 ? "" : argv[4]);
}

ConsoleMethod(PopupMenu, removeItem, void, 3, 3, "(pos)")
{
   object->removeItem(dAtoi(argv[2]));
}

ConsoleMethod(PopupMenu, insertSubMenu, S32, 5, 5, "(pos, title, subMenu)")
{
   PopupMenu *mnu = dynamic_cast<PopupMenu *>(Sim::findObject(argv[4]));
   if(mnu == NULL)
   {
      Con::errorf("PopupMenu::insertSubMenu - Invalid PopupMenu object specified for submenu");
      return -1;
   }
   return object->insertSubMenu(dAtoi(argv[2]), argv[3], mnu);
}

ConsoleMethod(PopupMenu, setItem, bool, 4, 5, "(pos, title[, accelerator])")
{
   return object->setItem(dAtoi(argv[2]), argv[3], argc < 5 ? "" : argv[4]);
}

//-----------------------------------------------------------------------------

ConsoleMethod(PopupMenu, enableItem, void, 4, 4, "(pos, enabled)")
{
   object->enableItem(dAtoi(argv[2]), dAtob(argv[3]));
}

ConsoleMethod(PopupMenu, checkItem, void, 4, 4, "(pos, checked)")
{
   object->checkItem(dAtoi(argv[2]), dAtob(argv[3]));
}

ConsoleMethod(PopupMenu, checkRadioItem, void, 5, 5, "(firstPos, lastPos, checkPos)")
{
   object->checkRadioItem(dAtoi(argv[2]), dAtoi(argv[3]), dAtoi(argv[4]));
}

ConsoleMethod(PopupMenu, isItemChecked, bool, 3, 3, "(pos)")
{
   return object->isItemChecked(dAtoi(argv[2]));
}

ConsoleMethod(PopupMenu, getItemCount, S32, 2, 2, "()")
{
   return object->getItemCount();
}

//-----------------------------------------------------------------------------

ConsoleMethod(PopupMenu, attachToMenuBar, void, 5, 5, "(GuiCanvas, pos, title)")
{
   object->attachToMenuBar(dynamic_cast<GuiCanvas*>(Sim::findObject(argv[2])),dAtoi(argv[3]), argv[4]);
}

ConsoleMethod(PopupMenu, removeFromMenuBar, void, 2, 2, "()")
{
   object->removeFromMenuBar();
}

//-----------------------------------------------------------------------------

ConsoleMethod(PopupMenu, showPopup, void, 3, 5, "(Canvas,[x, y])")
{
   GuiCanvas *pCanvas = dynamic_cast<GuiCanvas*>(Sim::findObject(argv[2]));
   S32 x = argc >= 4 ? dAtoi(argv[3]) : -1;
   S32 y = argc >= 5 ? dAtoi(argv[4]) : -1;
   object->showPopup(pCanvas, x, y);
}
