//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/containers/guiAutoScrollCtrl.h"
#include "console/consoleTypes.h"

//-----------------------------------------------------------------------------
// GuiAutoScrollCtrl
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiAutoScrollCtrl);

GuiAutoScrollCtrl::GuiAutoScrollCtrl()
{
   mScrolling = false;
   mCurrentTime = 0.0f;
   mStartDelay = 3.0f;
   mResetDelay = 5.0f;
   mChildBorder = 10;
   mScrollSpeed = 1.0f;
   mTickCallback = false;
   mIsContainer = true;

   // Make sure we receive our ticks.
   setProcessTicks();
}

GuiAutoScrollCtrl::~GuiAutoScrollCtrl()
{
}

//-----------------------------------------------------------------------------
// Persistence 
//-----------------------------------------------------------------------------
void GuiAutoScrollCtrl::initPersistFields()
{
   addField("startDelay", TypeF32, Offset(mStartDelay, GuiAutoScrollCtrl));
   addField("resetDelay", TypeF32, Offset(mResetDelay, GuiAutoScrollCtrl));
   addField("childBorder", TypeS32, Offset(mChildBorder, GuiAutoScrollCtrl));
   addField("scrollSpeed", TypeF32, Offset(mScrollSpeed, GuiAutoScrollCtrl));
   addField("tickCallback", TypeBool, Offset(mTickCallback, GuiAutoScrollCtrl));

   Parent::initPersistFields();
}

void GuiAutoScrollCtrl::onChildAdded(GuiControl* control)
{
   resetChild(control);
}

void GuiAutoScrollCtrl::onChildRemoved(GuiControl* control)
{
   mScrolling = false;
}

void GuiAutoScrollCtrl::resetChild(GuiControl* control)
{
   Point2I extent = control->getExtent();

   control->setPosition(Point2I(mChildBorder, mChildBorder));
   control->setExtent(Point2I(getExtent().x - (mChildBorder * 2), extent.y));

   mControlPositionY = F32(control->getPosition().y);

   if ((mControlPositionY + extent.y) > getExtent().y)
      mScrolling = true;
   else
      mScrolling = false;

   mCurrentTime = 0.0f;
}

bool GuiAutoScrollCtrl::resize( const Point2I &newPosition, const Point2I &newExtent )
{
   if( !Parent::resize( newPosition, newExtent ) )
      return false;

   for (iterator i = begin(); i != end(); i++)
   {
      GuiControl* control = static_cast<GuiControl*>(*i);
      if (control)
         resetChild(control);
   }

   return true;
}

void GuiAutoScrollCtrl::childResized(GuiControl *child)
{
   Parent::childResized( child );

   resetChild(child);
}

void GuiAutoScrollCtrl::processTick()
{
   if (mTickCallback && isMethod("onTick") )
      Con::executef(this, "onTick");
}

void GuiAutoScrollCtrl::advanceTime(F32 timeDelta)
{
   if (!mScrolling)
      return;

   if ((mCurrentTime + timeDelta) < mStartDelay)
   {
      mCurrentTime += timeDelta;
      return;
   }

   GuiControl* control = static_cast<GuiControl*>(at(0));
   if (!control)
      return;

   Point2I oldPosition = control->getPosition();
   if ((oldPosition.y + control->getExtent().y) < (getExtent().y - mChildBorder))
   {
      mCurrentTime += timeDelta;
      if (mCurrentTime > (mStartDelay + mResetDelay))
      {
         resetChild(control);
         return;
      }
   }

   else
   {
      mControlPositionY -= mScrollSpeed * timeDelta;
      control->setPosition(Point2I(oldPosition.x, (S32)mControlPositionY));
   }
}
