//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/containers/guiPanel.h"

#include "console/consoleTypes.h"
#include "gfx/primBuilder.h"


//-----------------------------------------------------------------------------
// GuiPanel
//-----------------------------------------------------------------------------

GuiPanel::GuiPanel()
{
   setMinExtent( Point2I( 16,16 ) );
   setDocking( Docking::dockNone );
}

GuiPanel::~GuiPanel()
{
}

IMPLEMENT_CONOBJECT(GuiPanel);


void GuiPanel::onRender(Point2I offset, const RectI &updateRect)
{
   if ( !mProfile->mOpaque )
   {
      RectI ctrlRect = getClientRect();
      ctrlRect.point += offset;

      // Draw a gradient left to right
      PrimBuild::begin( GFXTriangleStrip, 4 );
         PrimBuild::color( mProfile->mFillColorHL );
         PrimBuild::vertex2i( ctrlRect.point.x, ctrlRect.point.y );
         PrimBuild::vertex2i( ctrlRect.point.x, ctrlRect.point.y + ctrlRect.extent.y );

         PrimBuild::color( mProfile->mFillColor );
         PrimBuild::vertex2i( ctrlRect.point.x + ctrlRect.extent.x, ctrlRect.point.y);
         PrimBuild::vertex2i( ctrlRect.point.x + ctrlRect.extent.x, ctrlRect.point.y + ctrlRect.extent.y );
      PrimBuild::end();
   }

   Parent::onRender( offset, updateRect );
}