//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/containers/guiCtrlArrayCtrl.h"

IMPLEMENT_CONOBJECT(GuiControlArrayControl);

GuiControlArrayControl::GuiControlArrayControl()
{
   mResizing = false;

   mCols = 0;
   mRowSize = 30;
   mRowSpacing = 2;
   mColSpacing = 0;
   mIsContainer = true;
}

void GuiControlArrayControl::initPersistFields()
{
  addField("colCount",     TypeS32,       Offset(mCols,        GuiControlArrayControl));
  addField("colSizes",     TypeS32Vector, Offset(mColumnSizes, GuiControlArrayControl));
  addField("rowSize",      TypeS32,       Offset(mRowSize,     GuiControlArrayControl));
  addField("rowSpacing",   TypeS32,       Offset(mRowSpacing,  GuiControlArrayControl));
  addField("colSpacing",   TypeS32,       Offset(mColSpacing,  GuiControlArrayControl));

  Parent::initPersistFields();
}

bool GuiControlArrayControl::onWake()
{
   if ( !Parent::onWake() )
      return false;

   return true;
}

void GuiControlArrayControl::onSleep()
{
   Parent::onSleep();
}

void GuiControlArrayControl::inspectPostApply()
{
   resize(getPosition(), getExtent());
}

bool GuiControlArrayControl::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   if( !Parent::resize(newPosition, newExtent) )
      return false;

   return updateArray();
}

void GuiControlArrayControl::addObject(SimObject *obj)
{
   Parent::addObject(obj);

   updateArray();
}

void GuiControlArrayControl::removeObject(SimObject *obj)
{
   Parent::removeObject(obj);

   updateArray();
}

bool GuiControlArrayControl::reOrder(SimObject* obj, SimObject* target)
{
   bool ret = Parent::reOrder(obj, target);
   if (ret)
      updateArray();

   return ret;
}

bool GuiControlArrayControl::updateArray()
{
   // Prevent recursion
   if(mResizing) 
      return false;

   // Set Resizing.
   mResizing = true;

   if(mCols < 1 || size() < 1)
   {
      mResizing = false;
      return false;
   }

   S32 *sizes = new S32[mCols];
   S32 *offsets = new S32[mCols];
   S32 totalSize = 0;
   Point2I extent = getExtent();

   // Calculate the column sizes
   for(S32 i=0; i<mCols; i++)
   {
      sizes[i] = mColumnSizes[i];
      offsets[i] = totalSize;

      // If it's an auto-size one, then... auto-size...
      if(sizes[i] == -1)
      {
         sizes[i] = extent.x - totalSize;
         break;
      }

      totalSize += sizes[i] + mColSpacing;
   }

   // Now iterate through the children and resize them to fit the grid...
   for(S32 i=0; i<size(); i++)
   {
      GuiControl *gc = dynamic_cast<GuiControl*>(operator[](i));

      // Get the current column and row...
      S32 curCol = i % mCols;
      S32 curRow = i / mCols;

      if(gc)
      {
         Point2I newPos(offsets[curCol], curRow * (mRowSize + mRowSpacing));
         Point2I newExtents(sizes[curCol], mRowSize);

         gc->resize(newPos, newExtents);
      }
   }

   // Clear Sizing Flag.
   mResizing = false;

   delete [] sizes;
   delete [] offsets;

   return true;
}
