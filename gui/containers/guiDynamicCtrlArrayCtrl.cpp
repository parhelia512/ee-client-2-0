//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/containers/guiDynamicCtrlArrayCtrl.h"


GuiDynamicCtrlArrayControl::GuiDynamicCtrlArrayControl()
{
   mCols = 0;
   mColSize = 64;
   mRows = 0;
   mRowSize = 64;
   mRowSpacing = 0;
   mColSpacing = 0;
   mIsContainer = true;

   mResizing = false;

   mSizeToChildren = false;
   mAutoCellSize = false;

   mFrozen = false;
   mDynamicSize = false;
   mFillRowFirst = true;

   mPadding.set( 0, 0, 0, 0 );
}

GuiDynamicCtrlArrayControl::~GuiDynamicCtrlArrayControl()
{
}

IMPLEMENT_CONOBJECT(GuiDynamicCtrlArrayControl);


// ConsoleObject...

void GuiDynamicCtrlArrayControl::initPersistFields()
{
  addField( "colCount",        TypeS32,          Offset( mCols,           GuiDynamicCtrlArrayControl ) );
  addField( "colSize",         TypeS32,          Offset( mColSize,        GuiDynamicCtrlArrayControl ) );
  addField( "rowCount",        TypeS32,          Offset( mRows,           GuiDynamicCtrlArrayControl ) );
  addField( "rowSize",         TypeS32,          Offset( mRowSize,        GuiDynamicCtrlArrayControl ) );
  addField( "rowSpacing",      TypeS32,          Offset( mRowSpacing,     GuiDynamicCtrlArrayControl ) );
  addField( "colSpacing",      TypeS32,          Offset( mColSpacing,     GuiDynamicCtrlArrayControl ) );
  addField( "frozen",          TypeBool,         Offset( mFrozen,         GuiDynamicCtrlArrayControl ), "When true array will not updateChildrenControls when new children are added or in response to children resize events." );
  addField( "autoCellSize",    TypeBool,         Offset( mAutoCellSize,   GuiDynamicCtrlArrayControl ), "When true cell size is set to the widest/tallest child control." );  
  addField( "fillRowFirst",    TypeBool,         Offset( mFillRowFirst,   GuiDynamicCtrlArrayControl ), "Fill rows or columns first" );
  addField( "dynamicSize",     TypeBool,         Offset( mDynamicSize,    GuiDynamicCtrlArrayControl ), "Calculate one component of the extent based on number of children or use user defined extent only" );
  addField( "padding",         TypeRectSpacingI, Offset( mPadding,        GuiDynamicCtrlArrayControl ) );

  Parent::initPersistFields();
}


// SimObject...

void GuiDynamicCtrlArrayControl::inspectPostApply()
{
   resize(getPosition(), getExtent());
   Parent::inspectPostApply();
}


// SimSet...

void GuiDynamicCtrlArrayControl::addObject(SimObject *obj)
{
   Parent::addObject(obj);

   if ( !mFrozen )
      refresh();
}


// GuiControl...

bool GuiDynamicCtrlArrayControl::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   if ( size() == 0 )
      return Parent::resize( newPosition, newExtent );

   if ( mResizing ) 
      return false;

   mResizing = true;
   
   // Calculate the cellSize based on our widest/tallest child control
   // if the flag to do so is set.
   if ( mAutoCellSize )
   {
      mColSize = 1;
      mRowSize = 1;

      for ( U32 i = 0; i < size(); i++ )
      {
         GuiControl *child = dynamic_cast<GuiControl*>(operator [](i));
         if ( child && child->isVisible() )
         {
            if ( mColSize < child->getWidth() )
               mColSize = child->getWidth();
            if ( mRowSize < child->getHeight() )
               mRowSize = child->getHeight();
         }
      }
   }

   // Count number of visible, children guiControls.
   S32 numChildren = 0;
   for ( U32 i = 0; i < size(); i++ )
   {
      GuiControl *child = dynamic_cast<GuiControl*>(operator [](i));
      if ( child && child->isVisible() )
         numChildren++;         
   }

   // Calculate number of rows and columns.
   if ( !mFillRowFirst )
   {
      mRows = 1;
      while ( ( ( mRows + 1 ) * mRowSize + mRows * mRowSpacing ) <= ( newExtent.y - ( mPadding.top + mPadding.bottom ) ) )
         mRows++;

      mCols = numChildren / mRows;
      if ( numChildren % mRows > 0 )
         mCols++;
   }
   else
   {
      mCols = 1;
      while ( ( ( mCols + 1 ) * mColSize + mCols * mColSpacing ) <= ( newExtent.x - ( mPadding.left + mPadding.right ) ) )
         mCols++;

      mRows = numChildren / mCols;
      if ( numChildren % mCols > 0 )
         mRows++;
   }       

   // Place each child...
   S32 childcount = 0;
   for ( S32 i = 0; i < size(); i++ )
   {
      // Place control
      GuiControl *gc = dynamic_cast<GuiControl*>(operator [](i));

      // Added check if child is visible.  Invisible children don't take part
      if ( gc && gc->isVisible() ) 
      {
         S32 curCol, curRow;

         // Get the current column and row...
         if ( mFillRowFirst )
         {
            curCol = childcount % mCols;
            curRow = childcount / mCols;
         }
         else
         {
            curCol = childcount / mRows;
            curRow = childcount % mRows;
         }

         // Reposition and resize
         Point2I newPos( mPadding.left + curCol * ( mColSize + mColSpacing ), mPadding.top + curRow * ( mRowSize + mRowSpacing ) );
         gc->resize( newPos, Point2I( mColSize, mRowSize ) );

         childcount++;
      }
   }

   Point2I realExtent( newExtent );

   if ( mDynamicSize )
   {
      if ( mFillRowFirst )      
         realExtent.y = mRows * mRowSize + ( mRows - 1 ) * mRowSpacing + ( mPadding.top + mPadding.bottom );
      else
         realExtent.x = mCols * mColSize + ( mCols - 1 ) * mColSpacing + ( mPadding.left + mPadding.right );
   }
   
   mResizing = false;

   return Parent::resize( newPosition, realExtent );     
}

void GuiDynamicCtrlArrayControl::childResized(GuiControl *child)
{
   Parent::childResized(child);

   if ( !mFrozen )
      refresh();
}

void GuiDynamicCtrlArrayControl::refresh()
{
   resize( getPosition(), getExtent() );
}

ConsoleMethod(GuiDynamicCtrlArrayControl, refresh, void, 2, 2, "Forces the child controls to recalculate")
{
   object->refresh();
}
