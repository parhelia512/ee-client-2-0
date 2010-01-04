//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/containers/guiDragAndDropCtrl.h"
#include "gui/core/guiCanvas.h"

IMPLEMENT_CONOBJECT(GuiDragAndDropControl);

void GuiDragAndDropControl::initPersistFields()
{
   addField("deleteOnMouseUp", TypeBool, Offset(mDeleteOnMouseUp, GuiDragAndDropControl));
   Parent::initPersistFields();
}

ConsoleMethod(GuiDragAndDropControl, startDragging, void, 2, 4, "startDragging(%offset)")
{
   Point2I offset = Point2I(0, 0);
   if (argc > 3)
   {
      offset.x = dAtoi(argv[2]);
      offset.y = dAtoi(argv[3]);
   }
   object->startDragging(offset);
}

void GuiDragAndDropControl::startDragging(Point2I offset)
{
   GuiCanvas* canvas = getRoot();
   AssertFatal(canvas, "DragAndDropControl wasn't added to the gui before the drag started.");
   if (canvas->getMouseLockedControl())
   {
      GuiEvent event;
      canvas->getMouseLockedControl()->onMouseLeave(event);
      canvas->mouseUnlock(canvas->getMouseLockedControl());
   }
   canvas->mouseLock(this);
   canvas->setFirstResponder(this);
   mOffset = offset;
   mLastTarget=NULL;
}

void GuiDragAndDropControl::onMouseDown(const GuiEvent& event)
{
   startDragging(event.mousePoint - getPosition());
}

void GuiDragAndDropControl::onMouseDragged(const GuiEvent& event)
{
   setPosition(event.mousePoint - mOffset);
   
   // Allow the control under the drag to react to a potential drop
   GuiControl* enterTarget = findDragTarget(event.mousePoint, "onControlDragEnter");
   if(mLastTarget != enterTarget)
   {
      sendDragEvent(mLastTarget, "onControlDragExit");
      sendDragEvent(enterTarget, "onControlDragEnter");
      mLastTarget = enterTarget;
   }

   GuiControl* dragTarget = findDragTarget(event.mousePoint, "onControlDragged");
   sendDragEvent(dragTarget, "onControlDragged");
}

void GuiDragAndDropControl::onMouseUp(const GuiEvent& event)
{
   mouseUnlock();

   GuiControl* target = findDragTarget(event.mousePoint, "onControlDropped");
   sendDragEvent(target, "onControlDropped");

   if (mDeleteOnMouseUp)
      deleteObject();
}

void GuiDragAndDropControl::sendDragEvent(GuiControl* target, const char* event)
{
   if(!target || !target->isMethod(event))
      return;

   char position[32];
   Point2I dropPoint = getPosition() + (getExtent() / 2);
   dSprintf(position, 32, "%d %d", dropPoint.x, dropPoint.y);
   Con::executef(target, event, Con::getIntArg(at(0)->getId()), position);
}

GuiControl* GuiDragAndDropControl::findDragTarget(Point2I mousePoint, const char* method)
{
   // If there are any children and we have a parent.
   GuiControl* parent = getParent();
   if (size() && parent)
   {
      mVisible = false;
      GuiControl* dropControl = parent->findHitControl(mousePoint);
      mVisible = true;
      while( dropControl )
      {      
         if (dropControl->isMethod(method))
            return dropControl;
         else
            dropControl = dropControl->getParent();
      }
   }
   return NULL;
}
