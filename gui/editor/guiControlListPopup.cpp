//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/controls/guiPopUpCtrl.h"
#include "gui/core/guiCanvas.h"
#include "gui/utility/guiInputCtrl.h"

class GuiControlListPopUp : public GuiPopUpMenuCtrl
{
   typedef GuiPopUpMenuCtrl Parent;
public:
   bool onAdd();

   DECLARE_CONOBJECT(GuiControlListPopUp);
   DECLARE_CATEGORY( "Gui Editor" );
};

IMPLEMENT_CONOBJECT(GuiControlListPopUp);

bool GuiControlListPopUp::onAdd()
{
   if(!Parent::onAdd())
      return false;
   clear();

   AbstractClassRep *guiCtrlRep = GuiControl::getStaticClassRep();
   AbstractClassRep *guiCanvasRep = GuiCanvas::getStaticClassRep();
   AbstractClassRep *guiInputRep = GuiInputCtrl::getStaticClassRep();

   for(AbstractClassRep *rep = AbstractClassRep::getClassList(); rep; rep = rep->getNextClass())
   {
      if( rep->isClass(guiCtrlRep))
      {
         if(!rep->isClass(guiCanvasRep) && !rep->isClass(guiInputRep))
            addEntry(rep->getClassName(), 0);
      }
   }

   // We want to be alphabetical!
   sort();

   return true;
}
