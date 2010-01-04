//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/containers/guiStackCtrl.h"

IMPLEMENT_CONOBJECT(GuiStackControl);

static const EnumTable::Enums stackTypeEnum[] =
{
   { GuiStackControl::stackingTypeVert, "Vertical"  },
   { GuiStackControl::stackingTypeHoriz,"Horizontal" },
   { GuiStackControl::stackingTypeDyn,"Dynamic" }
};
static const EnumTable gStackTypeTable(3, &stackTypeEnum[0]);

static const EnumTable::Enums stackHorizEnum[] =
{
   { GuiStackControl::horizStackLeft, "Left to Right"  },
   { GuiStackControl::horizStackRight,"Right to Left" }
};
static const EnumTable gStackHorizSizingTable(2, &stackHorizEnum[0]);

static const EnumTable::Enums stackVertEnum[] =
{
   { GuiStackControl::vertStackTop, "Top to Bottom"  },
   { GuiStackControl::vertStackBottom,"Bottom to Top" }
};
static const EnumTable gStackVertSizingTable(2, &stackVertEnum[0]);



GuiStackControl::GuiStackControl()
{
   setMinExtent(Point2I(16,16));
   mResizing = false;
   mStackingType = stackingTypeVert;
   mStackVertSizing = vertStackTop;
   mStackHorizSizing = horizStackLeft;
   mPadding = 0;
   mIsContainer = true;
   mDynamicSize = true;
   mChangeChildSizeToFit = true;
   mChangeChildPosition = true;
}

void GuiStackControl::initPersistFields()
{

   addGroup( "Stacking" );
   addField( "StackingType",           TypeEnum,   Offset(mStackingType, GuiStackControl), 1, &gStackTypeTable);
   addField( "HorizStacking",          TypeEnum,   Offset(mStackHorizSizing, GuiStackControl), 1, &gStackHorizSizingTable);
   addField( "VertStacking",           TypeEnum,   Offset(mStackVertSizing, GuiStackControl), 1, &gStackVertSizingTable);
   addField( "Padding",                TypeS32,    Offset(mPadding, GuiStackControl));
   addField( "DynamicSize",            TypeBool,   Offset(mDynamicSize, GuiStackControl));
   addField( "ChangeChildSizeToFit",   TypeBool,   Offset(mChangeChildSizeToFit, GuiStackControl));
   addField( "ChangeChildPosition",    TypeBool,   Offset(mChangeChildPosition, GuiStackControl));
   endGroup( "Stacking" );

   Parent::initPersistFields();
}

ConsoleMethod( GuiStackControl, freeze, void, 3, 3, "%stackCtrl.freeze(bool) - Prevents control from restacking")
{
   object->freeze(dAtob(argv[2]));
}

ConsoleMethod( GuiStackControl, updateStack, void, 2, 2, "%stackCtrl.updateStack() - Restacks controls it owns")
{
   object->updatePanes();
}

bool GuiStackControl::onWake()
{
   if ( !Parent::onWake() )
      return false;

   updatePanes();

   return true;
}

void GuiStackControl::onSleep()
{
   Parent::onSleep();
}

void GuiStackControl::updatePanes()
{
   // Prevent recursion
   if(mResizing) 
      return;

   // Set Resizing.
   mResizing = true;

   Point2I extent = getExtent();

   // Do we need to stack horizontally?
   if( ( extent.x > extent.y && mStackingType == stackingTypeDyn ) || mStackingType == stackingTypeHoriz )
   {
      switch( mStackHorizSizing )
      {
      case horizStackLeft:
         stackFromLeft();
         break;
      case horizStackRight:
         stackFromRight();
         break;
      }
   }
   // Or, vertically?
   else if( ( extent.y > extent.x && mStackingType == stackingTypeDyn ) || mStackingType == stackingTypeVert)
   {
      switch( mStackVertSizing )
      {
      case vertStackTop:
         stackFromTop();
         break;
      case vertStackBottom:
         stackFromBottom();

         break;
      }
   }

   // Clear Sizing Flag.
   mResizing = false;
}

void GuiStackControl::freeze(bool _shouldfreeze)
{
   mResizing = _shouldfreeze;
}

void GuiStackControl::stackFromBottom()
{
   // Store the sum of the heights of our controls.
   S32 totalHeight=0;

   Point2I curPos;
   // If we go from the bottom, things are basically the same...
   // except we have to assign positions in an arse backwards way
   // (literally :P)

   // Figure out how high everything is going to be...
   // Place each child...
   for( S32 i = 0; i < size(); i++ )
   {
      // Place control
      GuiControl * gc = dynamic_cast<GuiControl*>(operator [](i));

      if(gc && gc->isVisible() )
      {
         Point2I childExtent = gc->getExtent();
         totalHeight += childExtent.y;
      }
   }

   // Figure out where we're starting...
   curPos = getPosition();
   curPos.y += totalHeight;

   // Offset so the bottom control goes in the right place...
   GuiControl * gc = dynamic_cast<GuiControl*>(operator [](size()-1));
   if(gc)
      curPos.y -= gc->getExtent().y;


   // Now work up from there!
   for(S32 i=size()-1; i>=0; i--)
   {
      // Place control
      GuiControl * gc = dynamic_cast<GuiControl*>(operator [](i));

      if(gc)
      {
         // We must place the child...

         // Make it have our width but keep its height
         Point2I childExtent = gc->getExtent();

         // Update our state...
         curPos.y -= childExtent.y - ((i > 0) ? mPadding : 0);

         // And resize...
         gc->resize(curPos - getPosition(), Point2I(getExtent().x, childExtent.y));
      }
   }
}

void GuiStackControl::stackFromTop()
{
   // Position and resize everything...
   Point2I curPos = Point2I(0, 0);

   // Get the number of visible children
   S32 visibleChildrenCount = 0;
   for (S32 i = 0; i < size(); i++)
   {
      GuiControl * gc = dynamic_cast<GuiControl*>( at(i) );
	  if ( gc && gc->isVisible() )
	     visibleChildrenCount++;
   }

   // Place each child...
   S32 visibleChildrenIndex = 0;
   for (S32 i = 0; i < size(); i++)
   {
      // Place control
      GuiControl * gc = dynamic_cast<GuiControl*>( at(i) );
      if ( gc && gc->isVisible() )
      {
         Point2I childPos = curPos;
         if ( !mChangeChildPosition )
            childPos.x = gc->getLeft();

         // Make it have our width but keep its height, if appropriate
         if ( mChangeChildSizeToFit )
         {
            gc->resize(childPos, Point2I(getWidth(), gc->getHeight()));
         }
         else
         {
            gc->resize(childPos, Point2I(gc->getWidth(), gc->getHeight()));
         }

         // Update our state...
		 curPos.y += gc->getHeight() + ((++visibleChildrenIndex < visibleChildrenCount) ? mPadding : 0);
      }
   }

   if ( mDynamicSize )
   {
       // Conform our size to the sum of the child sizes.
       curPos.x = getWidth();
       curPos.y = getMax( curPos.y , getMinExtent().y );
       resize( getPosition(), curPos );
   }
}

void GuiStackControl::stackFromLeft()
{
   // Position and resize everything...
   Point2I curPos = Point2I(0, 0);

   // Place each child...
   for (S32 i = 0; i < size(); i++)
   {
      // Place control
      GuiControl * gc = dynamic_cast<GuiControl*>(operator [](i));
      if ( gc && gc->isVisible() )
      {
         Point2I childPos = curPos;
         if ( !mChangeChildPosition )
            childPos.y = gc->getTop();

         // Make it have our height but keep its width, if appropriate
         if ( mChangeChildSizeToFit )
         {
            gc->resize(childPos, Point2I( gc->getWidth(), getHeight() ));
         }
         else
         {
            gc->resize(childPos, Point2I( gc->getWidth(), gc->getHeight() ));
         }

         // Update our state...
         curPos.x += gc->getWidth() + ((i < size() - 1) ? mPadding : 0);
      }
   }

   if ( mDynamicSize )
   {
       // Conform our size to the sum of the child sizes.
       curPos.x = getMax( curPos.x, getMinExtent().x );
       curPos.y = getHeight();
       resize( getPosition(), curPos );
   }
}

void GuiStackControl::stackFromRight()
{
	// ToDo
}

bool GuiStackControl::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   if( !Parent::resize( newPosition, newExtent ) )
      return false;

   updatePanes();

   // CodeReview This logic should be updated to correctly return true/false
   //  based on whether it sized it's children. [7/1/2007 justind]
   return true;
}

void GuiStackControl::addObject(SimObject *obj)
{
   Parent::addObject(obj);

   updatePanes();
}

void GuiStackControl::removeObject(SimObject *obj)
{
   Parent::removeObject(obj);

   updatePanes();
}

bool GuiStackControl::reOrder(SimObject* obj, SimObject* target)
{
   bool ret = Parent::reOrder(obj, target);
   if (ret)
      updatePanes();

   return ret;
}

void GuiStackControl::childResized(GuiControl *child)
{
   updatePanes();
}