//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/editor/guiEditCtrl.h"

#include "core/frameAllocator.h"
#include "core/stream/fileStream.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "gui/containers/guiScrollCtrl.h"
#include "core/strings/stringUnit.h"


IMPLEMENT_CONOBJECT( GuiEditCtrl );


StringTableEntry GuiEditCtrl::smGuidesPropertyName[ 2 ];


//-----------------------------------------------------------------------------

GuiEditCtrl::GuiEditCtrl()
   : mCurrentAddSet( NULL ),
     mContentControl( NULL ),
     mGridSnap( 0, 0),
     mDragBeginPoint( -1, -1 ),
     mSnapToControls( true ),
     mSnapToEdges( true ),
     mSnapToCenters( true ),
     mSnapToGuides( true ),
     mSnapSensitivity( 2 ),
     mFullBoxSelection( false ),
     mDrawBorderLines( true ),
     mDrawGuides( true )
{
   VECTOR_SET_ASSOCIATION( mSelectedControls );
   VECTOR_SET_ASSOCIATION( mDragBeginPoints );
   VECTOR_SET_ASSOCIATION( mSnapHits[ 0 ] );
   VECTOR_SET_ASSOCIATION( mSnapHits[ 1 ] );
      
   mActive = true;
   mDotSB = NULL;
   
   mSnapped[ SnapVertical ] = false;
   mSnapped[ SnapHorizontal ] = false;

   mDragGuide[ GuideVertical ] = false;
   mDragGuide[ GuideHorizontal ] = false;
   
   if( !smGuidesPropertyName[ GuideVertical ] )
      smGuidesPropertyName[ GuideVertical ] = StringTable->insert( "guidesVertical" );
   if( !smGuidesPropertyName[ GuideHorizontal ] )
      smGuidesPropertyName[ GuideHorizontal ] = StringTable->insert( "guidesHorizontal" );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::initPersistFields()
{
   addGroup( "Snapping" );
   addField( "snapToControls",      TypeBool,   Offset( mSnapToControls, GuiEditCtrl ),
      "If true, edge and center snapping will work against controls." );
   addField( "snapToGuides",        TypeBool,   Offset( mSnapToGuides, GuiEditCtrl ),
      "If true, edge and center snapping will work against guides." );
   addField( "snapToEdges",         TypeBool,   Offset( mSnapToEdges, GuiEditCtrl ),
      "If true, selection edges will snap into alignment when moved or resized." );
   addField( "snapToCenters",       TypeBool,   Offset( mSnapToCenters, GuiEditCtrl ),
      "If true, selection centers will snap into alignment when moved or resized." );
   addField( "snapSensitivity",     TypeS32,    Offset( mSnapSensitivity, GuiEditCtrl ),
      "Distance in pixels that edge and center snapping will work across." );
   endGroup( "Snapping" );
   
   addGroup( "Selection" );
   addField( "fullBoxSelection",    TypeBool,   Offset( mFullBoxSelection, GuiEditCtrl ),
      "If true, rectangle selection will only select controls fully inside the drag rectangle. (default)" );
   endGroup( "Selection" );
   
   addGroup( "Rendering" );
   addField( "drawBorderLines",  TypeBool,   Offset( mDrawBorderLines, GuiEditCtrl ),
      "If true, lines will be drawn extending along the edges of selected objects." );
   addField( "drawGuides", TypeBool, Offset( mDrawGuides, GuiEditCtrl ),
      "If true, guides will be included in rendering." );
   endGroup( "Rendering" );

   Parent::initPersistFields();
}

//=============================================================================
//    Events.
//=============================================================================
// MARK: ---- Events ----

//-----------------------------------------------------------------------------

bool GuiEditCtrl::onAdd()
{
   if(!Parent::onAdd())
      return false;
   
   if( !mTrash.registerObject() )
      return false;
   if( !mSelectedSet.registerObject() )
      return false;
   if( !mUndoManager.registerObject() )
      return false;

   return true;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::onRemove()
{
   Parent::onRemove();
   
   mDotSB = NULL;
   mTrash.unregisterObject();
   mSelectedSet.unregisterObject();
   mUndoManager.unregisterObject();
}

//-----------------------------------------------------------------------------

bool GuiEditCtrl::onWake()
{
   if (! Parent::onWake())
      return false;

   // Set GUI Controls to DesignTime mode
   GuiControl::smDesignTime = true;
   GuiControl::smEditorHandle = this;

   setEditMode(true);

   // TODO: paxorr: I'm not sure this is the best way to set these defaults.
   bool snap = Con::getBoolVariable("$pref::GuiEditor::snap2grid",false);
   U32 grid = Con::getIntVariable("$pref::GuiEditor::snap2gridsize",8);
   Con::setBoolVariable("$pref::GuiEditor::snap2grid", snap);
   Con::setIntVariable("$pref::GuiEditor::snap2gridsize",grid);

   setSnapToGrid( snap ? grid : 0);

   return true;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::onSleep()
{
   // Set GUI Controls to run time mode
   GuiControl::smDesignTime = false;
   GuiControl::smEditorHandle = NULL;

   Parent::onSleep();
}

//-----------------------------------------------------------------------------

bool GuiEditCtrl::onKeyDown(const GuiEvent &event)
{
   if (! mActive)
      return Parent::onKeyDown(event);

   if (!(event.modifier & SI_PRIMARY_CTRL))
   {
      switch(event.keyCode)
      {
         case KEY_BACKSPACE:
         case KEY_DELETE:
            deleteSelection();
            Con::executef(this, "onDelete");
            return true;
         default:
            break;
      }
   }
   return false;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::onMouseDown(const GuiEvent &event)
{
   if (! mActive)
   {
      Parent::onMouseDown(event);
      return;
   }
   if(!mContentControl)
      return;

   setFirstResponder();
   mouseLock();

   if( !mCurrentAddSet )
      setCurrentAddSet( mContentControl, false );

   mLastMousePos = globalToLocalCoord( event.mousePoint );

   // Check whether we've hit a guide.  If so, start a guide drag.
   // Don't do this if SHIFT is down.
   
   if( !( event.modifier & SI_SHIFT ) )
   {
      for( U32 axis = 0; axis < 2; ++ axis )
      {
         const S32 guide = findGuide( ( guideAxis ) axis, event.mousePoint, 1 );
         if( guide != -1 )
         {
            mMouseDownMode = DragGuide;
            
            mDragGuide[ axis ] = true;
            mDragGuideIndex[ axis ] = guide;
         }
      }
            
      if( mMouseDownMode == DragGuide )
         return;
   }
   
   // Check whether we have hit a sizing knob on any of the currently selected
   // controls.
   
   for( U32 i = 0, num = mSelectedControls.size(); i < num; ++ i )
   {
      GuiControl* ctrl = mSelectedControls[ i ];
      
      Point2I cext = ctrl->getExtent();
      Point2I ctOffset = globalToLocalCoord( ctrl->localToGlobalCoord( Point2I( 0, 0 ) ) );
      
      RectI box( ctOffset.x, ctOffset.y, cext.x, cext.y );

      if( ( mSizingMode = ( GuiEditCtrl::sizingModes ) getSizingHitKnobs( mLastMousePos, box ) ) != 0 )
      {
         mMouseDownMode = SizingSelection;
         mLastDragPos = event.mousePoint;
         
         // undo
         Con::executef( this, "onPreEdit", Con::getIntArg( getSelectedSet().getId() ) );
         return;
      }
   }

   //find the control we clicked
   GuiControl* ctrl = mContentControl->findHitControl(mLastMousePos, mCurrentAddSet->mLayer);

   bool handledEvent = ctrl->onMouseDownEditor( event, localToGlobalCoord( Point2I(0,0) ) );
   if( handledEvent == true )
   {
      // The Control handled the event and requested the edit ctrl
      // *NOT* act on it.  The dude abides.
      return;
   }
   else if( event.modifier & SI_SHIFT )
   {
      startDragRectangle( event.mousePoint );
      mDragAddSelection = true;
   }
   else if ( selectionContains(ctrl) )
   {
      // We hit a selected control.  If the multiselect key is pressed,
      // deselect the control.  Otherwise start a drag move.
      
      if( event.modifier & SI_MULTISELECT )
      {
         removeSelection( ctrl );

         //set the mode
         mMouseDownMode = Selecting;
      }
      else
      {
         startDragMove( event.mousePoint );
      }
   }
   else
   {
      // We clicked an unselected control.
      
      if( ctrl == getContentControl() )
      {
         // Clicked in toplevel control.  Start a rectangle selection.
         
         startDragRectangle( event.mousePoint );
         mDragAddSelection = false;
      }
      else if( event.modifier & SI_MULTISELECT )
         addSelection( ctrl );
      else
      {
         // Clicked on child control.  Start move.
         
         clearSelection();
         addSelection( ctrl );
         
         startDragMove( event.mousePoint );
      }
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::onMouseUp(const GuiEvent &event)
{
   if (! mActive || !mContentControl || !mCurrentAddSet )
   {
      Parent::onMouseUp(event);
      return;
   }

   //find the control we clicked
   GuiControl *ctrl = mContentControl->findHitControl(mLastMousePos, mCurrentAddSet->mLayer);

   bool handledEvent = ctrl->onMouseUpEditor( event, localToGlobalCoord( Point2I(0,0) ) );
   if( handledEvent == true )
   {
      // The Control handled the event and requested the edit ctrl
      // *NOT* act on it.  The dude abides.
      return;
   }

   //unlock the mouse
   mouseUnlock();

   // Reset Drag Axis Alignment Information
   mDragBeginPoint.set(-1,-1);
   mDragBeginPoints.clear();

   mLastMousePos = globalToLocalCoord(event.mousePoint);
   if( mMouseDownMode == DragGuide )
   {
      // Check to see if the mouse has moved off the canvas.  If so,
      // remove the guides being dragged.
      
      for( U32 axis = 0; axis < 2; ++ axis )
         if( mDragGuide[ axis ] && !getContentControl()->getGlobalBounds().pointInRect( event.mousePoint ) )
            mGuides[ axis ].erase( mDragGuideIndex[ axis ] );
   }
   else if( mMouseDownMode == DragSelecting )
   {
      // If not multiselecting, clear the current selection.
      
      if( !( event.modifier & SI_MULTISELECT ) && !mDragAddSelection )
         clearSelection();
         
      RectI rect;
      getDragRect( rect );
            
      // If the region is somewhere less than at least 2x2, count this as a
      // normal, non-rectangular selection. 
      
      if( rect.extent.x <= 2 && rect.extent.y <= 2 )
         addSelectControlAt( rect.point );
      else
      {
         // Use HIT_AddParentHits by default except if ALT is pressed.
         // Use HIT_ParentPreventsChildHit if ALT+CTRL is pressed.
         
         U32 hitFlags = 0;
         if( !( event.modifier & SI_PRIMARY_ALT ) )
            hitFlags |= HIT_AddParentHits;
         if( event.modifier & SI_PRIMARY_ALT && event.modifier & SI_CTRL )
            hitFlags |= HIT_ParentPreventsChildHit;
            
         addSelectControlsInRegion( rect, hitFlags );      
      }
   }
   else if( ctrl == getContentControl() && mMouseDownMode == Selecting )
      setCurrentAddSet( NULL, true );
   
   // deliver post edit event if we've been editing
   // note: paxorr: this may need to be moved earlier, if the selection has changed.
   // undo
   if(mMouseDownMode == SizingSelection || mMouseDownMode == MovingSelection)
      Con::executef(this, "onPostEdit", Con::getIntArg(getSelectedSet().getId()));

   //reset the mouse mode
   setFirstResponder();
   mMouseDownMode = Selecting;
   mSizingMode = sizingNone;
   
   // Clear snapping state.
   
   mSnapped[ SnapVertical ] = false;
   mSnapped[ SnapHorizontal ] = false;
   
   mSnapTargets[ SnapVertical ] = NULL;
   mSnapTargets[ SnapHorizontal ] = NULL;
   
   // Clear guide drag state.
   
   mDragGuide[ GuideVertical ] = false;
   mDragGuide[ GuideHorizontal ] = false;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::onMouseDragged( const GuiEvent &event )
{
   if( !mActive || !mContentControl || !mCurrentAddSet )
   {
      Parent::onMouseDragged(event);
      return;
   }

   if( !mCurrentAddSet )
      mCurrentAddSet = mContentControl;

   Point2I mousePoint = globalToLocalCoord( event.mousePoint );

   //find the control we clicked
   GuiControl *ctrl = mContentControl->findHitControl( mousePoint, mCurrentAddSet->mLayer );

   bool handledEvent = ctrl->onMouseDraggedEditor( event, localToGlobalCoord( Point2I(0,0) ) );
   if( handledEvent == true )
   {
      // The Control handled the event and requested the edit ctrl
      // *NOT* act on it.  The dude abides.
      return;
   }
   
   if( mMouseDownMode == DragGuide )
   {
      for( U32 axis = 0; axis < 2; ++ axis )
         if( mDragGuide[ axis ] )
         {
            // Set the guide to the coordinate of the mouse cursor
            // on the guide's axis.
            
            Point2I point = event.mousePoint;
            point -= localToGlobalCoord( Point2I( 0, 0 ) );
            point[ axis ] = mClamp( point[ axis ], 0, getExtent()[ axis ] - 1 );
            
            mGuides[ axis ][ mDragGuideIndex[ axis ] ] = point[ axis ];
         }
   }
   else if( mMouseDownMode == SizingSelection )
   {
      // Snap the mouse cursor to grid if active.  Do this on the mouse cursor so that we handle
      // incremental drags correctly.
      
      Point2I mousePoint = event.mousePoint;
      snapToGrid( mousePoint );
                  
      Point2I delta = mousePoint - mLastDragPos;
      
      // If CTRL is down, apply smart snapping.
      
      if( event.modifier & SI_CTRL )
      {
         RectI selectionBounds = getSelectionBounds();
         
         doSnapping( event, selectionBounds, delta );
      }
      else
      {
         mSnapped[ SnapVertical ] = false;
         mSnapped[ SnapHorizontal ] = false;
      }
      
      // If ALT is down, do a move instead of a resize on the control
      // knob's axis.  Otherwise resize.

      if( event.modifier & SI_PRIMARY_ALT )
      {
         if( !( mSizingMode & sizingLeft ) && !( mSizingMode & sizingRight ) )
         {
            mSnapped[ SnapVertical ] = false;
            delta.x = 0;
         }
         if( !( mSizingMode & sizingTop ) && !( mSizingMode & sizingBottom ) )
         {
            mSnapped[ SnapHorizontal ] = false;
            delta.y = 0;
         }
            
         moveSelection( delta );
      }
      else
         resizeControlsInSelectionBy( delta, mSizingMode );
         
      // Remember drag point.
      
      mLastDragPos = mousePoint;
   }
   else if (mMouseDownMode == MovingSelection && mSelectedControls.size())
   {
      Point2I delta = mousePoint - mLastMousePos;
      RectI selectionBounds = getSelectionBounds();
      
      // Apply snaps.
      
      doSnapping( event, selectionBounds, delta );
      
      //RDTODO: to me seems to be in need of revision
      // Do we want to align this drag to the X and Y axes within a certain threshold?
      if( event.modifier & SI_SHIFT && !( event.modifier & SI_PRIMARY_ALT ) )
      {
         Point2I dragTotalDelta = event.mousePoint - localToGlobalCoord( mDragBeginPoint );
         if( dragTotalDelta.y < 10 && dragTotalDelta.y > -10 )
         {
            for(S32 i = 0; i < mSelectedControls.size(); i++)
            {
               Point2I selCtrlPos = mSelectedControls[i]->getPosition();
               Point2I snapBackPoint( selCtrlPos.x, mDragBeginPoints[i].y);
               // This is kind of nasty but we need to snap back if we're not at origin point with selection - JDD
               if( selCtrlPos.y != mDragBeginPoints[i].y )
                  mSelectedControls[i]->setPosition( snapBackPoint );
            }
            delta.y = 0;
         }
         if( dragTotalDelta.x < 10 && dragTotalDelta.x > -10 )
         {
            for(S32 i = 0; i < mSelectedControls.size(); i++)
            {
               Point2I selCtrlPos = mSelectedControls[i]->getPosition();
               Point2I snapBackPoint( mDragBeginPoints[i].x, selCtrlPos.y);
               // This is kind of nasty but we need to snap back if we're not at origin point with selection - JDD
               if( selCtrlPos.x != mDragBeginPoints[i].x )
                  mSelectedControls[i]->setPosition( snapBackPoint );
            }
            delta.x = 0;
         }
      }

      if( delta.x || delta.y )
         moveSelection( delta );

      // find the current control under the mouse

      canHitSelectedControls( false );
      GuiControl *inCtrl = mContentControl->findHitControl(mousePoint, mCurrentAddSet->mLayer);
      canHitSelectedControls( true );

      // find the nearest control up the heirarchy from the control the mouse is in
      // that is flagged as a container.
      while(! inCtrl->mIsContainer)
         inCtrl = inCtrl->getParent();
         
      // if the control under the mouse is not our parent, move the selected controls
      // into the new parent.
      if(mSelectedControls[0]->getParent() != inCtrl && inCtrl->mIsContainer)
      {
         moveSelectionToCtrl(inCtrl);
         setCurrentAddSet(inCtrl,false);
      }

      mLastMousePos += delta;
   }
   else
      mLastMousePos = mousePoint;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::onRightMouseDown(const GuiEvent &event)
{
   if (! mActive || !mContentControl)
   {
      Parent::onRightMouseDown(event);
      return;
   }
   setFirstResponder();

   //search for the control hit in any layer below the edit layer
   GuiControl *hitCtrl = mContentControl->findHitControl(globalToLocalCoord(event.mousePoint), mLayer - 1);
   if (hitCtrl != mCurrentAddSet)
   {
      clearSelection();
      mCurrentAddSet = hitCtrl;
   }
   // select the parent if we right-click on the current add set 
   else if( mCurrentAddSet != mContentControl)
   {
      mCurrentAddSet = hitCtrl->getParent();
      select(hitCtrl);
   }

   //Design time mouse events
   GuiEvent designEvent = event;
   designEvent.mousePoint = mLastMousePos;
   hitCtrl->onRightMouseDownEditor( designEvent, localToGlobalCoord( Point2I(0,0) ) );

}

//=============================================================================
//    Rendering.
//=============================================================================
// MARK: ---- Rendering ----

//-----------------------------------------------------------------------------

void GuiEditCtrl::onPreRender()
{
   setUpdate();
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::onRender(Point2I offset, const RectI &updateRect)
{   
   Point2I ctOffset;
   Point2I cext;
   bool keyFocused = isFirstResponder();

   GFXDrawUtil *drawer = GFX->getDrawUtil();

   if (mActive)
   {
      if (mCurrentAddSet)
      {
         // draw a white frame inset around the current add set.
         cext = mCurrentAddSet->getExtent();
         ctOffset = mCurrentAddSet->localToGlobalCoord(Point2I(0,0));
         RectI box(ctOffset.x, ctOffset.y, cext.x, cext.y);

			box.inset( -5, -5 );
         drawer->drawRect( box, ColorI( 50, 101, 152, 128 ) );
			box.inset( 1, 1 );
         drawer->drawRect( box, ColorI( 50, 101, 152, 128 ) );
			box.inset( 1, 1 );
         drawer->drawRect( box, ColorI( 50, 101, 152, 128 ) );
			box.inset( 1, 1 );
         drawer->drawRect( box, ColorI( 50, 101, 152, 128 ) );
			box.inset( 1, 1 );
			drawer->drawRect( box, ColorI( 50, 101, 152, 128 ) );
      }
      Vector<GuiControl *>::iterator i;
      bool multisel = mSelectedControls.size() > 1;
      for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
      {
         GuiControl *ctrl = (*i);
         cext = ctrl->getExtent();
         ctOffset = ctrl->localToGlobalCoord(Point2I(0,0));
         RectI box(ctOffset.x,ctOffset.y, cext.x, cext.y);
         ColorI nutColor = multisel ? ColorI( 255, 255, 255, 100 ) : ColorI( 0, 0, 0, 100 );
         ColorI outlineColor = multisel ? ColorI( 0, 0, 0, 100 ) : ColorI( 255, 255, 255, 100 );
         if(!keyFocused)
            nutColor.set( 128, 128, 128, 100 );

         drawNuts(box, outlineColor, nutColor);
      }
   }

   renderChildControls(offset, updateRect);
   
   // Draw selection rectangle.
   
   if( mActive && mMouseDownMode == DragSelecting )
   {
      RectI b;
      getDragRect(b);
      b.point += offset;
      
      // Draw outline.
      
      drawer->drawRect( b, ColorI( 100, 100, 100, 128 ) );
      
      // Draw fill.
      
      b.inset( 1, 1 );
      drawer->drawRectFill( b, ColorI( 150, 150, 150, 128 ) );
   }

   // Draw grid.

   if( mActive && mGridSnap.x && mGridSnap.y )
   {
      Point2I cext = getContentControl()->getExtent();
      Point2I coff = getContentControl()->localToGlobalCoord(Point2I(0,0));
      
      // create point-dots
      const Point2I& snap = mGridSnap;
      U32 maxdot = (U32)(mCeil(cext.x / (F32)snap.x) - 1) * (U32)(mCeil(cext.y / (F32)snap.y) - 1);

      if( mDots.isNull() || maxdot != mDots->mNumVerts)
      {
         mDots.set(GFX, maxdot, GFXBufferTypeStatic);

         U32 ndot = 0;
         mDots.lock();
         for(U32 ix = snap.x; ix < cext.x; ix += snap.x)
         { 
            for(U32 iy = snap.y; ndot < maxdot && iy < cext.y; iy += snap.y)
            {
               mDots[ndot].color.set( 50, 50, 254, 100 );
               mDots[ndot].point.x = F32(ix + coff.x);
               mDots[ndot].point.y = F32(iy + coff.y);
               ndot++;
            }
         }
         mDots.unlock();
         AssertFatal(ndot <= maxdot, "dot overflow");
         AssertFatal(ndot == maxdot, "dot underflow");
      }

      if (!mDotSB)
      {
         GFXStateBlockDesc dotdesc;
         dotdesc.setBlend(true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
         mDotSB = GFX->createStateBlock( dotdesc );
      }

      GFX->setStateBlock(mDotSB);

      // draw the points.
      GFX->setVertexBuffer( mDots );
      GFX->drawPrimitive( GFXPointList, 0, mDots->mNumVerts );
   }

   // Draw snapping lines.
   
   if( mActive && getContentControl() )
   {      
      RectI bounds = getContentControl()->getGlobalBounds();
            
      // Draw guide lines.

      if( mDrawGuides )
      {
         for( U32 axis = 0; axis < 2; ++ axis )
         {
            for( U32 i = 0, num = mGuides[ axis ].size(); i < num; ++ i )
               drawCrossSection( axis, mGuides[ axis ][ i ] + bounds.point[ axis ],
                  bounds, ColorI( 0, 255, 0, 100 ), drawer );
         }
      }

      // Draw smart snap lines.
      
      for( U32 axis = 0; axis < 2; ++ axis )
      {
         if( mSnapped[ axis ] )
         {
            // Draw the snap line.
            
            drawCrossSection( axis, mSnapOffset[ axis ],
               bounds, ColorI( 0, 0, 255, 100 ), drawer );

            // Draw a border around the snap target control.
            
            if( mSnapTargets[ axis ] )
            {
               RectI bounds = mSnapTargets[ axis ]->getGlobalBounds();
               drawer->drawRect( bounds, ColorF( .5, .5, .5, .5 ) );
            }
         }
      }
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::drawNuts(RectI &box, ColorI &outlineColor, ColorI &nutColor)
{
   GFXDrawUtil *drawer = GFX->getDrawUtil();

   S32 lx = box.point.x, rx = box.point.x + box.extent.x - 1;
   S32 cx = (lx + rx) >> 1;
   S32 ty = box.point.y, by = box.point.y + box.extent.y - 1;
   S32 cy = (ty + by) >> 1;

   if( mDrawBorderLines )
   {
      ColorF lineColor( 0.7f, 0.7f, 0.7f, 0.25f );
      ColorF lightLineColor( 0.5f, 0.5f, 0.5f, 0.1f );
      
      if(lx > 0 && ty > 0)
      {
         drawer->drawLine(0, ty, lx, ty, lineColor);
         drawer->drawLine(lx, 0, lx, ty, lineColor);
      }

      if(lx > 0 && by > 0)
         drawer->drawLine(0, by, lx, by, lineColor);

      if(rx > 0 && ty > 0)
         drawer->drawLine(rx, 0, rx, ty, lineColor);

      Point2I extent = localToGlobalCoord(getExtent());

      if(lx < extent.x && by < extent.y)
         drawer->drawLine(lx, by, lx, extent.y, lightLineColor);
      if(rx < extent.x && by < extent.y)
      {
         drawer->drawLine(rx, by, rx, extent.y, lightLineColor);
         drawer->drawLine(rx, by, extent.x, by, lightLineColor);
      }
      if(rx < extent.x && ty < extent.y)
         drawer->drawLine(rx, ty, extent.x, ty, lightLineColor);
   }

   // adjust nuts, so they dont straddle the controlslx -= NUT_SIZE;
   lx -= NUT_SIZE;
   ty -= NUT_SIZE;
   rx += NUT_SIZE;
   by += NUT_SIZE;

   drawNut(Point2I(lx, ty), outlineColor, nutColor);
   drawNut(Point2I(lx, cy), outlineColor, nutColor);
   drawNut(Point2I(lx, by), outlineColor, nutColor);
   drawNut(Point2I(rx, ty), outlineColor, nutColor);
   drawNut(Point2I(rx, cy), outlineColor, nutColor);
   drawNut(Point2I(rx, by), outlineColor, nutColor);
   drawNut(Point2I(cx, ty), outlineColor, nutColor);
   drawNut(Point2I(cx, by), outlineColor, nutColor);
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::drawNut(const Point2I &nut, ColorI &outlineColor, ColorI &nutColor)
{
   RectI r(nut.x - NUT_SIZE, nut.y - NUT_SIZE, 2 * NUT_SIZE, 2 * NUT_SIZE);
   r.inset(1, 1);
   GFX->getDrawUtil()->drawRectFill(r, nutColor);
   r.inset(-1, -1);
   GFX->getDrawUtil()->drawRect(r, outlineColor);
}

//=============================================================================
//    Selections.
//=============================================================================
// MARK: ---- Selections ----

//-----------------------------------------------------------------------------

void GuiEditCtrl::clearSelection(void)
{
   mSelectedControls.clear();
   if( isMethod( "onClearSelected" ) )
      Con::executef( this, "onClearSelected" );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::setSelection(GuiControl *ctrl, bool inclusive)
{
   //sanity check
   if( !ctrl )
      return;
      
   if( mSelectedControls.size() == 1 && mSelectedControls[ 0 ] == ctrl )
      return;
      
   if( !inclusive )
      clearSelection();

   if( mContentControl == ctrl )
      mCurrentAddSet = ctrl;
   else
      addSelection( ctrl );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::addSelection(S32 id)
{
   GuiControl * ctrl;
   if( Sim::findObject( id, ctrl ) )
      addSelection( ctrl );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::addSelection( GuiControl* ctrl )
{
   // Only add if this isn't the content control and the
   // control isn't yet in the selection.
   
   if( ctrl != getContentControl() && !selectionContains( ctrl ) )
   {
      mSelectedControls.push_back( ctrl );
      
      if( mSelectedControls.size() == 1 )
      {
         // Update the add set.
         
         if( ctrl->mIsContainer )
            setCurrentAddSet( ctrl, false );
         else
            setCurrentAddSet( ctrl->getParent(), false );
            
         // Notify script.

         Con::executef( this, "onSelect", Con::getIntArg( ctrl->getId() ) );
      }
      else
      {
         // Notify script.
         
         Con::executef( this, "onAddSelected", Con::getIntArg( ctrl->getId() ) );
      }
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::removeSelection( S32 id )
{
   GuiControl * ctrl;
   if ( Sim::findObject( id, ctrl ) )
      removeSelection( ctrl );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::removeSelection( GuiControl* ctrl )
{
   if( selectionContains( ctrl ) )
   {
      Vector< GuiControl* >::iterator i = ::find( mSelectedControls.begin(), mSelectedControls.end(), ctrl );
      if ( i != mSelectedControls.end() )
         mSelectedControls.erase( i );

      Con::executef( this, "onRemoveSelected", Con::getIntArg( ctrl->getId() ) );
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::canHitSelectedControls( bool state )
{
   for( U32 i = 0, num = mSelectedControls.size(); i < num; ++ i )
      mSelectedControls[ i ]->setCanHit( state );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::moveSelectionToCtrl(GuiControl *newParent)
{
   for(int i = 0; i < mSelectedControls.size(); i++)
   {
      GuiControl* ctrl = mSelectedControls[i];
      if( ctrl->getParent() == newParent
          || ctrl->isLocked()
          || selectionContainsParentOf( ctrl ) )
         continue;
   
      Point2I globalpos = ctrl->localToGlobalCoord(Point2I(0,0));
      newParent->addObject(ctrl);
      Point2I newpos = ctrl->globalToLocalCoord(globalpos) + ctrl->getPosition();
      ctrl->setPosition(newpos);
   }
   
   onHierarchyChanged();
}

//-----------------------------------------------------------------------------

static Point2I snapPoint(Point2I point, Point2I delta, Point2I gridSnap)
{ 
   S32 snap;
   if(gridSnap.x && delta.x)
   {
      snap = point.x % gridSnap.x;
      point.x -= snap;
      if(delta.x > 0 && snap != 0)
         point.x += gridSnap.x;
   }
   if(gridSnap.y && delta.y)
   {
      snap = point.y % gridSnap.y;
      point.y -= snap;
      if(delta.y > 0 && snap != 0)
         point.y += gridSnap.y;
   }
   return point;
}

void GuiEditCtrl::moveAndSnapSelection(const Point2I &delta)
{
   // move / nudge gets a special callback so that multiple small moves can be
   // coalesced into one large undo action.
   // undo
   Con::executef(this, "onPreSelectionNudged", Con::getIntArg(getSelectedSet().getId()));

   Vector<GuiControl *>::iterator i;
   Point2I newPos;
   for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
   {
      GuiControl* ctrl = *i;
      if( !ctrl->isLocked() && !selectionContainsParentOf( ctrl ) )
      {
         newPos = ctrl->getPosition() + delta;
         newPos = snapPoint( newPos, delta, mGridSnap );
         ctrl->setPosition( newPos );
      }
   }

   // undo
   Con::executef(this, "onPostSelectionNudged", Con::getIntArg(getSelectedSet().getId()));

   // allow script to update the inspector
   if (mSelectedControls.size() == 1)
      Con::executef(this, "onSelectionMoved", Con::getIntArg(mSelectedControls[0]->getId()));
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::moveSelection(const Point2I &delta)
{
   Vector<GuiControl *>::iterator i;
   for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
   {
      GuiControl* ctrl = *i;
      if( !ctrl->isLocked() && !selectionContainsParentOf( ctrl ) )
         ctrl->setPosition( ctrl->getPosition() + delta );
   }

   // allow script to update the inspector
   if( mSelectedControls.size() == 1 )
      Con::executef(this, "onSelectionMoved", Con::getIntArg(mSelectedControls[0]->getId()));
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::justifySelection(Justification j)
{
   S32 minX, maxX;
   S32 minY, maxY;
   S32 extentX, extentY;

   if (mSelectedControls.size() < 2)
      return;

   Vector<GuiControl *>::iterator i = mSelectedControls.begin();
   minX = (*i)->getLeft();
   maxX = minX + (*i)->getWidth();
   minY = (*i)->getTop();
   maxY = minY + (*i)->getHeight();
   extentX = (*i)->getWidth();
   extentY = (*i)->getHeight();
   i++;
   for(;i != mSelectedControls.end(); i++)
   {
      minX = getMin(minX, (*i)->getLeft());
      maxX = getMax(maxX, (*i)->getLeft() + (*i)->getWidth());
      minY = getMin(minY, (*i)->getTop());
      maxY = getMax(maxY, (*i)->getTop() + (*i)->getHeight());
      extentX += (*i)->getWidth();
      extentY += (*i)->getHeight();
   }
   S32 deltaX = maxX - minX;
   S32 deltaY = maxY - minY;
   switch(j)
   {
      case JUSTIFY_LEFT:
         for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
            if( !( *i )->isLocked() )
               (*i)->setLeft( minX );
         break;
      case JUSTIFY_TOP:
         for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
            if( !( *i )->isLocked() )
               (*i)->setTop( minY );
         break;
      case JUSTIFY_RIGHT:
         for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
            if( !( *i )->isLocked() )
               (*i)->setLeft( maxX - (*i)->getWidth() + 1 );
         break;
      case JUSTIFY_BOTTOM:
         for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
            if( !( *i )->isLocked() )
               (*i)->setTop( maxY - (*i)->getHeight() + 1 );
         break;
      case JUSTIFY_CENTER_VERTICAL:
         for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
            if( !( *i )->isLocked() )
               (*i)->setLeft( minX + ((deltaX - (*i)->getWidth()) >> 1 ));
         break;
      case JUSTIFY_CENTER_HORIZONTAL:
         for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
            if( !( *i )->isLocked() )
               (*i)->setTop( minY + ((deltaY - (*i)->getHeight()) >> 1 ));
         break;
      case SPACING_VERTICAL:
         {
            Vector<GuiControl *> sortedList;
            Vector<GuiControl *>::iterator k;
            for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
            {
               for(k = sortedList.begin(); k != sortedList.end(); k++)
               {
                  if ((*i)->getTop() < (*k)->getTop())
                     break;
               }
               sortedList.insert(k, *i);
            }
            S32 space = (deltaY - extentY) / (mSelectedControls.size() - 1);
            S32 curY = minY;
            for(k = sortedList.begin(); k != sortedList.end(); k++)
            {
               if( !( *k )->isLocked() )
                  (*k)->setTop( curY );
               curY += (*k)->getHeight() + space;
            }
         }
         break;
      case SPACING_HORIZONTAL:
         {
            Vector<GuiControl *> sortedList;
            Vector<GuiControl *>::iterator k;
            for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
            {
               for(k = sortedList.begin(); k != sortedList.end(); k++)
               {
                  if ((*i)->getLeft() < (*k)->getLeft())
                     break;
               }
               sortedList.insert(k, *i);
            }
            S32 space = (deltaX - extentX) / (mSelectedControls.size() - 1);
            S32 curX = minX;
            for(k = sortedList.begin(); k != sortedList.end(); k++)
            {
               if( !( *k )->isLocked() )
                  (*k)->setLeft( curX );
               curX += (*k)->getWidth() + space;
            }
         }
         break;
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::deleteSelection(void)
{
   // Notify script for undo.
   
   Con::executef(this, "onTrashSelection", Con::getIntArg(getSelectedSet().getId()));
   
   // Move all objects in selection to trash.

   Vector< GuiControl* >::iterator i;
   for( i = mSelectedControls.begin(); i != mSelectedControls.end(); i ++ )
      mTrash.addObject( *i );
      
   clearSelection();
   
   // Notify script it needs to update its views.
   
   onHierarchyChanged();
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::loadSelection(const char* filename)
{
   if (! mCurrentAddSet)
      mCurrentAddSet = mContentControl;
      
   // Set redefine behavior to rename.
   
   const char* oldRedefineBehavior = Con::getVariable( "$Con::redefineBehavior" );
   Con::setVariable( "$Con::redefineBehavior", "renameNew" );
   
   // Exec the file with the saved selection set.

   Con::executef( "exec", filename );
   SimSet* set;
   if( !Sim::findObject( "guiClipboard", set ) )
   {
      Con::errorf( "GuiEditCtrl::loadSelection() - could not find 'guiClipboard' in '%s'", filename );
      return;
   }
   
   // Restore redefine behavior.
   
   Con::setVariable( "$Con::redefineBehavior", oldRedefineBehavior );
   
   // Add the objects in the set.

   if( set->size() )
   {
      clearSelection();
      for( U32 i = 0, num = set->size(); i < num; ++ i )
      {
         GuiControl *ctrl = dynamic_cast< GuiControl* >( ( *set )[ i ] );
         if( ctrl )
         {
            mCurrentAddSet->addObject( ctrl );
            addSelection( ctrl );
         }
      }
      
      // Undo 
      Con::executef(this, "onAddNewCtrlSet", Con::getIntArg(getSelectedSet().getId()));

      // Notify the script it needs to update its treeview.

      onHierarchyChanged();
   }
   set->deleteObject();
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::saveSelection(const char* filename)
{
   // If there are no selected objects, then don't save.
   
   if( mSelectedControls.size() == 0 )
      return;

   FileStream *stream;
   if((stream = FileStream::createAndOpen( filename, Torque::FS::File::Write )) == NULL)
      return;
      
   // Create a temporary SimSet.
      
   SimSet* clipboardSet = new SimSet;
   clipboardSet->registerObject();
   Sim::getRootGroup()->addObject( clipboardSet, "guiClipboard" );
   
   // Add the selected controls to the set.

   for( Vector< GuiControl* >::iterator i = mSelectedControls.begin();
        i != mSelectedControls.end(); ++ i )
   {
      GuiControl* ctrl = *i;
      if( !selectionContainsParentOf( ctrl ) )
         clipboardSet->addObject( ctrl );
   }
   
   // Write the SimSet.  Use the IgnoreCanSave to ensure the controls
   // get actually written out (also disables the default parent inheritance
   // behavior for the flag).

   clipboardSet->write( *stream, 0, IgnoreCanSave );
   clipboardSet->deleteObject();

   delete stream;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::selectAll()
{
   GuiControl::iterator i;
   if (!mCurrentAddSet)
      return;
   
   clearSelection();
   for(i = mCurrentAddSet->begin(); i != mCurrentAddSet->end(); i++)
   {
      GuiControl *ctrl = dynamic_cast<GuiControl *>(*i);
      addSelection( ctrl );
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::bringToFront()
{
   // undo
   if (mSelectedControls.size() != 1)
      return;

   GuiControl *ctrl = *(mSelectedControls.begin());
   mCurrentAddSet->pushObjectToBack(ctrl);
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::pushToBack()
{
   // undo
   if (mSelectedControls.size() != 1)
      return;

   GuiControl *ctrl = *(mSelectedControls.begin());
   mCurrentAddSet->bringObjectToFront(ctrl);
}

//-----------------------------------------------------------------------------

RectI GuiEditCtrl::getSelectionBounds() const
{
   Vector<GuiControl *>::const_iterator i = mSelectedControls.begin();
   
   Point2I minPos = (*i)->localToGlobalCoord( Point2I( 0, 0 ) );
   Point2I maxPos = minPos;
   
   for(; i != mSelectedControls.end(); i++)
   {
      Point2I iPos = (**i).localToGlobalCoord( Point2I( 0 , 0 ) );
      
      minPos.x = getMin( iPos.x, minPos.x );
      minPos.y = getMin( iPos.y, minPos.y );
         
      Point2I iExt = ( **i ).getExtent();
      
      iPos.x += iExt.x;
      iPos.y += iExt.y;

      maxPos.x = getMax( iPos.x, maxPos.x );
      maxPos.y = getMax( iPos.y, maxPos.y );
   }
   
   minPos = getContentControl()->globalToLocalCoord( minPos );
   maxPos = getContentControl()->globalToLocalCoord( maxPos );
   
   return RectI( minPos.x, minPos.y, ( maxPos.x - minPos.x ), ( maxPos.y - minPos.y ) );
}

//-----------------------------------------------------------------------------

RectI GuiEditCtrl::getSelectionGlobalBounds() const
{
   Point2I minb( S32_MAX, S32_MAX );
   Point2I maxb( S32_MIN, S32_MIN );
   
   for( U32 i = 0, num = mSelectedControls.size(); i < num; ++ i )
   {
      // Min.

      Point2I pos = mSelectedControls[ i ]->localToGlobalCoord( Point2I( 0, 0 ) );
      
      minb.x = getMin( minb.x, pos.x );
      minb.y = getMin( minb.y, pos.y );
      
      // Max.
      
      const Point2I extent = mSelectedControls[ i ]->getExtent();
      
      maxb.x = getMax( maxb.x, pos.x + extent.x );
      maxb.y = getMax( maxb.y, pos.y + extent.y );
   }
   
   RectI bounds( minb.x, minb.y, maxb.x - minb.x, maxb.y - minb.y );
   return bounds;
}

//-----------------------------------------------------------------------------

bool GuiEditCtrl::selectionContains( GuiControl *ctrl )
{
   Vector<GuiControl *>::iterator i;
   for (i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
      if (ctrl == *i) return true;
   return false;
}

//-----------------------------------------------------------------------------

bool GuiEditCtrl::selectionContainsParentOf( GuiControl* ctrl )
{
   GuiControl* parent = ctrl->getParent();
   while( parent && parent != getContentControl() )
   {
      if( selectionContains( parent ) )
         return true;
         
      parent = parent->getParent();
   }
   
   return false;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::select( GuiControl* ctrl )
{
   clearSelection();
   addSelection( ctrl );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::updateSelectedSet()
{
   mSelectedSet.clear();
   Vector<GuiControl*>::iterator i;
   for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
   {
      mSelectedSet.addObject(*i);
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::addSelectControlsInRegion( const RectI& rect, U32 flags )
{
   // Do a hit test on the content control.
   
   canHitSelectedControls( false );
   Vector< GuiControl* > hits;
   
   if( mFullBoxSelection )
      flags |= GuiControl::HIT_FullBoxOnly;
      
   getContentControl()->findHitControls( rect, hits, flags );
   canHitSelectedControls( true );
   
   // Add all controls that got hit.
   
   for( U32 i = 0, num = hits.size(); i < num; ++ i )
      addSelection( hits[ i ] );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::addSelectControlAt( const Point2I& pos )
{
   // Run a hit test.
   
   canHitSelectedControls( false );
   GuiControl* hit = getContentControl()->findHitControl( pos );
   canHitSelectedControls( true );
   
   // Add to selection.
   
   if( hit )
      addSelection( hit );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::resizeControlsInSelectionBy( const Point2I& delta, U32 mode )
{
   for( U32 i = 0, num = mSelectedControls.size(); i < num; ++ i )
   {
      GuiControl *ctrl = mSelectedControls[ i ];
      if( ctrl->isLocked() )
         continue;
         
      Point2I minExtent    = ctrl->getMinExtent();
      Point2I newPosition  = ctrl->getPosition();
      Point2I newExtent    = ctrl->getExtent();

      if( mSizingMode & sizingLeft )
      {
         newPosition.x     += delta.x;
         newExtent.x       -= delta.x;
         
         if( newExtent.x < minExtent.x )
         {
            newPosition.x  -= minExtent.x - newExtent.x;
            newExtent.x    = minExtent.x;
         }
      }
      else if( mSizingMode & sizingRight )
      {
         newExtent.x       += delta.x;

         if( newExtent.x < minExtent.x )
            newExtent.x    = minExtent.x;
      }

      if( mSizingMode & sizingTop )
      {
         newPosition.y     += delta.y;
         newExtent.y       -= delta.y;
         
         if( newExtent.y < minExtent.y )
         {
            newPosition.y  -= minExtent.y - newExtent.y;
            newExtent.y    = minExtent.y;
         }
      }
      else if( mSizingMode & sizingBottom )
      {
         newExtent.y       += delta.y;
         
         if( newExtent.y < minExtent.y )
            newExtent.y = minExtent.y;
      }
      
      ctrl->resize( newPosition, newExtent );
   }
}

//=============================================================================
//    Guides.
//=============================================================================
// MARK: ---- Guides ----

//-----------------------------------------------------------------------------

void GuiEditCtrl::readGuides( guideAxis axis, GuiControl* ctrl )
{
   // Read the guide indices from the vector stored on the respective dynamic
   // property of the control.
   
   const char* guideIndices = ctrl->getDataField( smGuidesPropertyName[ axis ], NULL );
   if( guideIndices && guideIndices[ 0 ] )
   {
      U32 index = 0;
      while( true )
      {
         const char* posStr = StringUnit::getUnit( guideIndices, index, " \t" );
         if( !posStr[ 0 ] )
            break;
            
         mGuides[ axis ].push_back( dAtoi( posStr ) );
         
         index ++;
      }
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::writeGuides( guideAxis axis, GuiControl* ctrl )
{
   // Store the guide indices of the given axis in a vector on the respective
   // dynamic property of the control.
   
   StringBuilder str;
   bool isFirst = true;
   for( U32 i = 0, num = mGuides[ axis ].size(); i < num; ++ i )
   {
      if( !isFirst )
         str.append( ' ' );
         
      char buffer[ 32 ];
      dSprintf( buffer, sizeof( buffer ), "%i", mGuides[ axis ][ i ] );
      
      str.append( buffer );
      
      isFirst = false;
   }
   
   String value = str.end();
   ctrl->setDataField( smGuidesPropertyName[ axis ], NULL, value );
}

//-----------------------------------------------------------------------------

S32 GuiEditCtrl::findGuide( guideAxis axis, const Point2I& point, U32 tolerance )
{
   const S32 p = ( point - localToGlobalCoord( Point2I( 0, 0 ) ) )[ axis ];
   
   for( U32 i = 0, num = mGuides[ axis ].size(); i < num; ++ i )
   {
      const S32 g = mGuides[ axis ][ i ];
      if( p >= ( g - tolerance ) &&
          p <= ( g + tolerance ) )
         return i;
   }
         
   return -1;
}

//=============================================================================
//    Snapping.
//=============================================================================
// MARK: ---- Snapping ----

//-----------------------------------------------------------------------------

RectI GuiEditCtrl::getSnapRegion( snappingAxis axis, const Point2I& center ) const
{
   RectI rootBounds = getContentControl()->getBounds();
   
   RectI rect;
   if( axis == SnapHorizontal )
      rect = RectI( rootBounds.point.x,
                    center.y - mSnapSensitivity,
                    rootBounds.extent.x,
                    mSnapSensitivity * 2 );
   else // SnapVertical
      rect = RectI( center.x - mSnapSensitivity,
                    rootBounds.point.y,
                    mSnapSensitivity * 2,
                    rootBounds.extent.y );
   
   // Clip against root bounds.
   
   rect.intersect( rootBounds );
      
   return rect;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::registerSnap( snappingAxis axis, const Point2I& mousePoint, const Point2I& point, snappingEdges edge, GuiControl* ctrl )
{
   bool takeNewSnap = false;
   const Point2I globalPoint = getContentControl()->localToGlobalCoord( point );

   // If we have no snap yet, just take this one.
   
   if( !mSnapped[ axis ] )
      takeNewSnap = true;
   
   // Otherwise see if this snap is the better one.
   
   else
   {
      // Compare deltas to pointer.
      
      S32 deltaCurrent = mAbs( mSnapOffset[ axis ] - mousePoint[ axis ] );
      S32 deltaNew = mAbs( globalPoint[ axis ] - mousePoint[ axis ] );
                              
      if( deltaCurrent > deltaNew )
         takeNewSnap = true;
   }

   if( takeNewSnap )
   {      
      mSnapped[ axis ]     = true;
      mSnapOffset[ axis ]  = globalPoint[ axis ];
      mSnapEdge[ axis ]    = edge;
      mSnapTargets[ axis ] = ctrl;
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::findSnaps( snappingAxis axis, const Point2I& mousePoint, const RectI& minRegion, const RectI& midRegion, const RectI& maxRegion )
{
   // Find controls with edge in either minRegion, midRegion, or maxRegion
   // (depending on snap settings).
   
   for( U32 i = 0, num = mSnapHits[ axis ].size(); i < num; ++ i )
   {
      GuiControl* ctrl = mSnapHits[ axis ][ i ];
      if( ctrl == getContentControl() )
         continue;
         
      RectI bounds = ctrl->getGlobalBounds();
      bounds.point = getContentControl()->globalToLocalCoord( bounds.point );
      
      // Compute points on min, mid, and max lines of control.
      
      Point2I min = bounds.point;
      Point2I max = min + bounds.extent;

      Point2I mid = bounds.point;
      mid.x += bounds.extent.x / 2;
      mid.y += bounds.extent.y / 2;
      
      // Test edge snap cases.
      
      if( mSnapToEdges )
      {
         // Min to min.
         
         if( minRegion.pointInRect( min ) )
            registerSnap( axis, mousePoint, min, SnapEdgeMin, ctrl );
      
         // Max to max.
         
         if( maxRegion.pointInRect( max ) )
            registerSnap( axis, mousePoint, max, SnapEdgeMax, ctrl );
         
         // Min to max.
         
         if( minRegion.pointInRect( max ) )
            registerSnap( axis, mousePoint, max, SnapEdgeMin, ctrl );
         
         // Max to min.
         
         if( maxRegion.pointInRect( min ) )
            registerSnap( axis, mousePoint, min, SnapEdgeMax, ctrl );
      }
      
      // Test center snap cases.
      
      if( mSnapToCenters )
      {
         // Mid to mid.
         
         if( midRegion.pointInRect( mid ) )
            registerSnap( axis, mousePoint, mid, SnapEdgeMid, ctrl );
      }
      
      // Test combined center+edge snap cases.
      
      if( mSnapToEdges && mSnapToCenters )
      {
         // Min to mid.
         
         if( minRegion.pointInRect( mid ) )
            registerSnap( axis, mousePoint, mid, SnapEdgeMin, ctrl );
         
         // Max to mid.
         
         if( maxRegion.pointInRect( mid ) )
            registerSnap( axis, mousePoint, mid, SnapEdgeMax, ctrl );
         
         // Mid to min.
         
         if( midRegion.pointInRect( min ) )
            registerSnap( axis, mousePoint, min, SnapEdgeMid, ctrl );
         
         // Mid to max.
         
         if( midRegion.pointInRect( max ) )
            registerSnap( axis, mousePoint, max, SnapEdgeMid, ctrl );
      }
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::doControlSnap( const GuiEvent& event, const RectI& selectionBounds, const RectI& selectionBoundsGlobal, Point2I& delta )
{
   if( !mSnapToControls || ( !mSnapToEdges && !mSnapToCenters ) )
      return;
      
   // Allow restricting to just vertical (ALT+SHIFT) or just horizontal (ALT+CTRL)
   // snaps.
   
   bool snapAxisEnabled[ 2 ];
            
   if( event.modifier & SI_PRIMARY_ALT && event.modifier & SI_SHIFT )
      snapAxisEnabled[ SnapHorizontal ] = false;
   else
      snapAxisEnabled[ SnapHorizontal ] = true;
      
   if( event.modifier & SI_PRIMARY_ALT && event.modifier & SI_CTRL )
      snapAxisEnabled[ SnapVertical ] = false;
   else
      snapAxisEnabled[ SnapVertical ] = true;
   
   // Compute snap regions.  There is one region centered on and aligned with
   // each of the selection bounds edges plus two regions aligned on the selection
   // bounds center.  For the selection bounds origin, we use the point that the
   // selection would be at, if we had already done the mouse drag.
   
   RectI snapRegions[ 2 ][ 3 ];
   Point2I projectedOrigin( selectionBounds.point + delta );
   dMemset( snapRegions, 0, sizeof( snapRegions ) );
   
   for( U32 axis = 0; axis < 2; ++ axis )
   {
      if( !snapAxisEnabled[ axis ] )
         continue;
         
      if( mSizingMode == sizingNone ||
          ( axis == 0 && mSizingMode & sizingLeft ) ||
          ( axis == 1 && mSizingMode & sizingTop ) )
         snapRegions[ axis ][ 0 ] = getSnapRegion( ( snappingAxis ) axis, projectedOrigin );
         
      if( mSizingMode == sizingNone )
         snapRegions[ axis ][ 1 ] = getSnapRegion( ( snappingAxis ) axis, projectedOrigin + Point2I( selectionBounds.extent.x / 2, selectionBounds.extent.y / 2 ) );
         
      if( mSizingMode == sizingNone ||
          ( axis == 0 && mSizingMode & sizingRight ) ||
          ( axis == 1 && mSizingMode & sizingBottom ) )
         snapRegions[ axis ][ 2 ] = getSnapRegion( ( snappingAxis ) axis, projectedOrigin + selectionBounds.extent );
   }
   
   // Find hit controls.
   
   canHitSelectedControls( false );
   
   if( mSnapToEdges )
   {
      for( U32 axis = 0; axis < 2; ++ axis )
         if( snapAxisEnabled[ axis ] )
         {
               getContentControl()->findHitControls( snapRegions[ axis ][ 0 ], mSnapHits[ axis ], HIT_NoCanHitNoRecurse );
               
               getContentControl()->findHitControls( snapRegions[ axis ][ 2 ], mSnapHits[ axis ], HIT_NoCanHitNoRecurse );
         }
   }
   if( mSnapToCenters && mSizingMode == sizingNone )
   {
      for( U32 axis = 0; axis < 2; ++ axis )
         if( snapAxisEnabled[ axis ] )
            getContentControl()->findHitControls( snapRegions[ axis ][ 1 ], mSnapHits[ axis ], HIT_NoCanHitNoRecurse );
   }
   
   canHitSelectedControls( true );
            
   // Find snaps.
   
   for( U32 i = 0; i < 2; ++ i )
      if( snapAxisEnabled[ i ] )
         findSnaps( ( snappingAxis ) i,
                    event.mousePoint,
                    snapRegions[ i ][ 0 ],
                    snapRegions[ i ][ 1 ],
                    snapRegions[ i ][ 2 ] );
                                       
   // Clean up.
   
   mSnapHits[ 0 ].clear();
   mSnapHits[ 1 ].clear();
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::doGridSnap( const GuiEvent& event, const RectI& selectionBounds, const RectI& selectionBoundsGlobal, Point2I& delta )
{
   delta += selectionBounds.point;
         
   if( mGridSnap.x )
      delta.x -= delta.x % mGridSnap.x;
   if( mGridSnap.y )
      delta.y -= delta.y % mGridSnap.y;

   delta -= selectionBounds.point;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::doGuideSnap( const GuiEvent& event, const RectI& selectionBounds, const RectI& selectionBoundsGlobal, Point2I& delta )
{
   if( !mSnapToGuides )
      return;
      
   Point2I min = getContentControl()->localToGlobalCoord( selectionBounds.point + delta );
   Point2I mid = min + selectionBounds.extent / 2;
   Point2I max = min + selectionBounds.extent;
   
   for( U32 axis = 0; axis < 2; ++ axis )
   {
      if( mSnapToEdges )
      {
         S32 guideMin = -1;
         S32 guideMax = -1;
         
         if( mSizingMode == sizingNone ||
             ( axis == 0 && mSizingMode & sizingLeft ) ||
             ( axis == 1 && mSizingMode & sizingTop ) )
            guideMin = findGuide( ( guideAxis ) axis, min, mSnapSensitivity );
         if( mSizingMode == sizingNone ||
             ( axis == 0 && mSizingMode & sizingRight ) ||
             ( axis == 1 && mSizingMode & sizingBottom ) )
            guideMax = findGuide( ( guideAxis ) axis, max, mSnapSensitivity );
         
         Point2I pos( 0, 0 );
         
         if( guideMin != -1 )
         {
            pos[ axis ] = mGuides[ axis ][ guideMin ];
            registerSnap( ( snappingAxis ) axis, event.mousePoint, pos, SnapEdgeMin );
         }
         
         if( guideMax != -1 )
         {
            pos[ axis ] = mGuides[ axis ][ guideMax ];
            registerSnap( ( snappingAxis ) axis, event.mousePoint, pos, SnapEdgeMax );
         }
      }
      
      if( mSnapToCenters && mSizingMode == sizingNone )
      {
         const S32 guideMid = findGuide( ( guideAxis ) axis, mid, mSnapSensitivity );
         if( guideMid != -1 )
         {
            Point2I pos( 0, 0 );
            pos[ axis ] = mGuides[ axis ][ guideMid ];
            registerSnap( ( snappingAxis ) axis, event.mousePoint, pos, SnapEdgeMid );
         }
      }
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::doSnapping( const GuiEvent& event, const RectI& selectionBounds, Point2I& delta )
{
   // Clear snapping.  If we have snapping on, we want to find a new best snap.
   
   mSnapped[ SnapVertical ] = false;
   mSnapped[ SnapHorizontal ] = false;
   
   // Compute global bounds.
   
   RectI selectionBoundsGlobal = selectionBounds;
   selectionBoundsGlobal.point = getContentControl()->localToGlobalCoord( selectionBoundsGlobal.point );

   // Apply the snaps.
   
   doGridSnap( event, selectionBounds, selectionBoundsGlobal, delta );
   doGuideSnap( event, selectionBounds, selectionBoundsGlobal, delta );
   doControlSnap( event, selectionBounds, selectionBoundsGlobal, delta );

  // If we have a horizontal snap, compute a delta.

   if( mSnapped[ SnapVertical ] )
      snapDelta( SnapVertical, mSnapEdge[ SnapVertical ], mSnapOffset[ SnapVertical ], selectionBoundsGlobal, delta );
   
   // If we have a vertical snap, compute a delta.
   
   if( mSnapped[ SnapHorizontal ] )
      snapDelta( SnapHorizontal, mSnapEdge[ SnapHorizontal ], mSnapOffset[ SnapHorizontal ], selectionBoundsGlobal, delta );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::snapToGrid( Point2I& point )
{
   if( mGridSnap.x )
      point.x -= point.x % mGridSnap.x;
   if( mGridSnap.y )
      point.y -= point.y % mGridSnap.y;
}

//=============================================================================
//    Misc.
//=============================================================================
// MARK: ---- Misc ----

//-----------------------------------------------------------------------------

void GuiEditCtrl::setContentControl(GuiControl *root)
{
   mContentControl = root;
   if( root != NULL )
      root->mIsContainer = true;
      
	mCurrentAddSet = mContentControl;
   clearSelection();
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::setEditMode(bool value)
{
   mActive = value;

   clearSelection();
   if (mActive && mAwake)
      mCurrentAddSet = NULL;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::setCurrentAddSet(GuiControl *ctrl, bool doclearSelection)
{
   if (ctrl != mCurrentAddSet)
   {
      if(doclearSelection)
         clearSelection();

      mCurrentAddSet = ctrl;
   }
}

//-----------------------------------------------------------------------------

const GuiControl* GuiEditCtrl::getCurrentAddSet() const
{
   return mCurrentAddSet ? mCurrentAddSet : mContentControl;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::addNewControl(GuiControl *ctrl)
{
   if (! mCurrentAddSet)
      mCurrentAddSet = mContentControl;

   mCurrentAddSet->addObject(ctrl);
   select( ctrl );

   // undo
   Con::executef(this, "onAddNewCtrl", Con::getIntArg(ctrl->getId()));
}

//-----------------------------------------------------------------------------

S32 GuiEditCtrl::getSizingHitKnobs(const Point2I &pt, const RectI &box)
{
   S32 lx = box.point.x, rx = box.point.x + box.extent.x - 1;
   S32 cx = (lx + rx) >> 1;
   S32 ty = box.point.y, by = box.point.y + box.extent.y - 1;
   S32 cy = (ty + by) >> 1;
   
   // adjust nuts, so they dont straddle the controls
   lx -= NUT_SIZE;
   ty -= NUT_SIZE;
   rx += NUT_SIZE;
   by += NUT_SIZE;

   if (inNut(pt, lx, ty))
      return sizingLeft | sizingTop;
   if (inNut(pt, cx, ty))
      return sizingTop;
   if (inNut(pt, rx, ty))
      return sizingRight | sizingTop;
   if (inNut(pt, lx, by))
      return sizingLeft | sizingBottom;
   if (inNut(pt, cx, by))
      return sizingBottom;
   if (inNut(pt, rx, by))
      return sizingRight | sizingBottom;
   if (inNut(pt, lx, cy))
      return sizingLeft;
   if (inNut(pt, rx, cy))
      return sizingRight;
   return sizingNone;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::getDragRect(RectI &box)
{
   box.point.x = getMin(mLastMousePos.x, mSelectionAnchor.x);
   box.extent.x = getMax(mLastMousePos.x, mSelectionAnchor.x) - box.point.x + 1;
   box.point.y = getMin(mLastMousePos.y, mSelectionAnchor.y);
   box.extent.y = getMax(mLastMousePos.y, mSelectionAnchor.y) - box.point.y + 1;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::getCursor(GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent)
{
   GuiCanvas *pRoot = getRoot();
   if( !pRoot )
      return;

   showCursor = false;
   cursor = NULL;
   
   Point2I ctOffset;
   Point2I cext;
   GuiControl *ctrl;

   Point2I mousePos  = globalToLocalCoord(lastGuiEvent.mousePoint);

   PlatformWindow *pWindow = static_cast<GuiCanvas*>(getRoot())->getPlatformWindow();
   AssertFatal(pWindow != NULL,"GuiControl without owning platform window!  This should not be possible.");
   PlatformCursorController *pController = pWindow->getCursorController();
   AssertFatal(pController != NULL,"PlatformWindow without an owned CursorController!");

   S32 desiredCursor = PlatformCursorController::curArrow;

   // first see if we hit a sizing knob on the currently selected control...
   if (mSelectedControls.size() == 1 )
   {
      ctrl = mSelectedControls.first();
      cext = ctrl->getExtent();
      ctOffset = globalToLocalCoord(ctrl->localToGlobalCoord(Point2I(0,0)));
      RectI box(ctOffset.x,ctOffset.y,cext.x, cext.y);

      GuiEditCtrl::sizingModes sizeMode = (GuiEditCtrl::sizingModes)getSizingHitKnobs(mousePos, box);

      if( mMouseDownMode == SizingSelection )
      {
         if ( ( mSizingMode == ( sizingBottom | sizingRight ) ) || ( mSizingMode == ( sizingTop | sizingLeft ) ) )
            desiredCursor = PlatformCursorController::curResizeNWSE;
         else if (  ( mSizingMode == ( sizingBottom | sizingLeft ) ) || ( mSizingMode == ( sizingTop | sizingRight ) ) )
            desiredCursor = PlatformCursorController::curResizeNESW;
         else if ( mSizingMode == sizingLeft || mSizingMode == sizingRight ) 
            desiredCursor = PlatformCursorController::curResizeVert;
         else if (mSizingMode == sizingTop || mSizingMode == sizingBottom )
            desiredCursor = PlatformCursorController::curResizeHorz;
      }
      else
      {
         // Check for current mouse position after checking for actual sizing mode
         if ( ( sizeMode == ( sizingBottom | sizingRight ) ) || ( sizeMode == ( sizingTop | sizingLeft ) ) )
            desiredCursor = PlatformCursorController::curResizeNWSE;
         else if ( ( sizeMode == ( sizingBottom | sizingLeft ) ) || ( sizeMode == ( sizingTop | sizingRight ) ) )
            desiredCursor = PlatformCursorController::curResizeNESW;
         else if (sizeMode == sizingLeft || sizeMode == sizingRight )
            desiredCursor = PlatformCursorController::curResizeVert;
         else if (sizeMode == sizingTop || sizeMode == sizingBottom )
            desiredCursor = PlatformCursorController::curResizeHorz;
      }
   }
   
   if( mMouseDownMode == MovingSelection && cursor == NULL )
      desiredCursor = PlatformCursorController::curResizeAll;

   if( pRoot->mCursorChanged != desiredCursor )
   {
      // We've already changed the cursor, 
      // so set it back before we change it again.
      if(pRoot->mCursorChanged != -1)
         pController->popCursor();

      // Now change the cursor shape
      pController->pushCursor(desiredCursor);
      pRoot->mCursorChanged = desiredCursor;
   }
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::setSnapToGrid(U32 gridsize)
{
   if( gridsize != 0 )
      gridsize = getMax( gridsize, ( U32 ) MIN_GRID_SIZE );
   mGridSnap.set( gridsize, gridsize );
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::controlInspectPreApply(GuiControl* object)
{
   // undo
   Con::executef(this, "onControlInspectPreApply", Con::getIntArg(object->getId()));
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::controlInspectPostApply(GuiControl* object)
{
   // undo
   Con::executef(this, "onControlInspectPostApply", Con::getIntArg(object->getId()));
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::startDragMove( const Point2I& startPoint )
{
   // For calculating mouse delta
   mDragBeginPoint = globalToLocalCoord( startPoint );

   // Allocate enough space for our selected controls
   mDragBeginPoints.reserve( mSelectedControls.size() );

   // For snapping to origin
   Vector<GuiControl *>::iterator i;
   for(i = mSelectedControls.begin(); i != mSelectedControls.end(); i++)
      mDragBeginPoints.push_back( (*i)->getPosition() );

   // Set Mouse Mode
   mMouseDownMode = MovingSelection;
   
   // undo
   Con::executef(this, "onPreEdit", Con::getIntArg(getSelectedSet().getId()));
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::startDragRectangle( const Point2I& startPoint )
{
   mSelectionAnchor = globalToLocalCoord( startPoint );
   mMouseDownMode = DragSelecting;
}

//-----------------------------------------------------------------------------

void GuiEditCtrl::startMouseGuideDrag( guideAxis axis, U32 guideIndex, bool lockMouse )
{
   mDragGuideIndex[ axis ] = guideIndex;
   mDragGuide[ axis ] = true;
   
   mMouseDownMode = DragGuide;
   
   // Grab the mouse.
   
   if( lockMouse )
      mouseLock();
}

//=============================================================================
//    Console Methods.
//=============================================================================
// MARK: ---- Console Methods ----

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, getContentControl, S32, 2, 2, "() - Return the toplevel control edited inside the GUI editor." )
{
   GuiControl* ctrl = object->getContentControl();
   if( ctrl )
      return ctrl->getId();
   else
      return 0;
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, setContentControl, void, 3, 3, "( GuiControl ctrl ) - Set the toplevel control to edit in the GUI editor." )
{
   GuiControl *ctrl;
   if(!Sim::findObject(argv[2], ctrl))
      return;
   object->setContentControl(ctrl);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, addNewCtrl, void, 3, 3, "(GuiControl ctrl)")
{
   GuiControl *ctrl;
   if(!Sim::findObject(argv[2], ctrl))
      return;
   object->addNewControl(ctrl);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, addSelection, void, 3, 3, "selects a control.")
{
   S32 id = dAtoi(argv[2]);
   object->addSelection(id);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, removeSelection, void, 3, 3, "deselects a control.")
{
   S32 id = dAtoi(argv[2]);
   object->removeSelection(id);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, clearSelection, void, 2, 2, "Clear selected controls list.")
{
   object->clearSelection();
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, select, void, 3, 3, "(GuiControl ctrl)")
{
   GuiControl *ctrl;

   if(!Sim::findObject(argv[2], ctrl))
      return;

   object->setSelection(ctrl, false);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, setCurrentAddSet, void, 3, 3, "(GuiControl ctrl)")
{
   GuiControl *addSet;

   if (!Sim::findObject(argv[2], addSet))
   {
      Con::printf("%s(): Invalid control: %s", argv[0], argv[2]);
      return;
   }
   object->setCurrentAddSet(addSet);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, getCurrentAddSet, S32, 2, 2, "Returns the set to which new controls will be added")
{
   const GuiControl* add = object->getCurrentAddSet();
   return add ? add->getId() : 0;
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, toggle, void, 2, 2, "Toggle activation.")
{
   object->setEditMode(! object->mActive);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, justify, void, 3, 3, "(int mode)" )
{
   object->justifySelection((GuiEditCtrl::Justification)dAtoi(argv[2]));
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, bringToFront, void, 2, 2, "")
{
   object->bringToFront();
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, pushToBack, void, 2, 2, "")
{
   object->pushToBack();
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, deleteSelection, void, 2, 2, "Delete the selected text.")
{
   object->deleteSelection();
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, moveSelection, void, 4, 4, "(int deltax, int deltay)")
{
   object->moveAndSnapSelection(Point2I(dAtoi(argv[2]), dAtoi(argv[3])));
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, saveSelection, void, 3, 3, "(string fileName)")
{
   object->saveSelection(argv[2]);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, loadSelection, void, 3, 3, "(string fileName)")
{
   object->loadSelection(argv[2]);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, selectAll, void, 2, 2, "()")
{
   object->selectAll();
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, getSelected, S32, 2, 2, "() - Gets the GUI control(s) the editor is currently selecting" )
{
   return object->getSelectedSet().getId();
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, getSelectionGlobalBounds, const char*, 2, 2, "() - Returns global bounds of current selection as vector 'x y width height'." )
{
   RectI bounds = object->getSelectionGlobalBounds();
   String str = String::ToString( "%i %i %i %i", bounds.point.x, bounds.point.y, bounds.extent.x, bounds.extent.y );
   
   char* buffer = Con::getReturnBuffer( str.length() );
   dStrcpy( buffer, str.c_str() );
   
   return buffer;
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, getTrash, S32, 2, 2, "() - Gets the GUI controls(s) that are currently in the trash.")
{
   return object->getTrash().getId();
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, getUndoManager, S32, 2, 2, "() - Gets the Gui Editor's UndoManager object")
{
   return object->getUndoManager().getId();
}

//-----------------------------------------------------------------------------

ConsoleMethod(GuiEditCtrl, setSnapToGrid, void, 3, 3, "GuiEditCtrl.setSnapToGrid(gridsize)")
{
   U32 gridsize = dAtoi(argv[2]);
   object->setSnapToGrid(gridsize);
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, readGuides, void, 3, 4, "( GuiControl ctrl [, int axis ] ) - Read the guides from the given control." )
{
   // Find the control.
   
   GuiControl* ctrl;
   if( !Sim::findObject( argv[ 2 ], ctrl ) )
   {
      Con::errorf( "GuiEditCtrl::readGuides - no control '%s'", argv[ 2 ] );
      return;
   }
   
   // Read the guides.
   
   if( argc > 3 )
   {
      S32 axis = dAtoi( argv[ 3 ] );
      if( axis < 0 || axis > 1 )
      {
         Con::errorf( "GuiEditCtrl::readGuides - invalid axis '%s'", argv[ 3 ] );
         return;
      }
      
      object->readGuides( ( GuiEditCtrl::guideAxis ) axis, ctrl );
   }
   else
   {
      object->readGuides( GuiEditCtrl::GuideHorizontal, ctrl );
      object->readGuides( GuiEditCtrl::GuideVertical, ctrl );
   }
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, writeGuides, void, 3, 4, "( GuiControl ctrl [, int axis ] ) - Write the guides to the given control." )
{
   // Find the control.
   
   GuiControl* ctrl;
   if( !Sim::findObject( argv[ 2 ], ctrl ) )
   {
      Con::errorf( "GuiEditCtrl::writeGuides - no control '%i'", argv[ 2 ] );
      return;
   }
   
   // Write the guides.
   
   if( argc > 3 )
   {
      S32 axis = dAtoi( argv[ 3 ] );
      if( axis < 0 || axis > 1 )
      {
         Con::errorf( "GuiEditCtrl::writeGuides - invalid axis '%s'", argv[ 3 ] );
         return;
      }
      
      object->writeGuides( ( GuiEditCtrl::guideAxis ) axis, ctrl );
   }
   else
   {
      object->writeGuides( GuiEditCtrl::GuideHorizontal, ctrl );
      object->writeGuides( GuiEditCtrl::GuideVertical, ctrl );
   }
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiEditCtrl, clearGuides, void, 2, 3, "( [ int axis ] ) - Clear all currently set guide lines." )
{
   if( argc > 2 )
   {
      S32 axis = dAtoi( argv[ 2 ] );
      if( axis < 0 || axis > 1 )
      {
         Con::errorf( "GuiEditCtrl::clearGuides - invalid axis '%i'", axis );
         return;
      }
      
      object->clearGuides( ( GuiEditCtrl::guideAxis ) axis );
   }
   else
   {
      object->clearGuides( GuiEditCtrl::GuideHorizontal );
      object->clearGuides( GuiEditCtrl::GuideVertical );
   }
}

//=============================================================================
//    GuiEditorRuler.
//=============================================================================

class GuiEditorRuler : public GuiControl
{
   public:
   
      typedef GuiControl Parent;
      
   protected:
   
      String mRefCtrlName;
      String mEditCtrlName;
      
      GuiScrollCtrl* mRefCtrl;
      GuiEditCtrl* mEditCtrl;

   public:

      enum EOrientation
      {
         ORIENTATION_Horizontal,
         ORIENTATION_Vertical
      };
      
      GuiEditorRuler()
         : mRefCtrl( 0 ),
           mEditCtrl( 0 )
      {
      }
      
      EOrientation getOrientation() const
      {
         if( getWidth() > getHeight() )
            return ORIENTATION_Horizontal;
         else
            return ORIENTATION_Vertical;
      }
      
      bool onWake()
      {
         if( !Parent::onWake() )
            return false;
            
         if( !mEditCtrlName.isEmpty() && !Sim::findObject( mEditCtrlName, mEditCtrl ) )
            Con::errorf( "GuiEditorRuler::onWake() - no GuiEditCtrl '%s'", mEditCtrlName.c_str() );
         
         if( !mRefCtrlName.isEmpty() && !Sim::findObject( mRefCtrlName, mRefCtrl ) )
            Con::errorf( "GuiEditorRuler::onWake() - no GuiScrollCtrl '%s'", mRefCtrlName.c_str() );
         
         return true;
      }

      void onPreRender()
      {
         setUpdate();
      }
      
      void onMouseDown( const GuiEvent& event )
      {         
         if( !mEditCtrl )
            return;
            
         // Determine the guide axis.
         
         GuiEditCtrl::guideAxis axis;
         if( getOrientation() == ORIENTATION_Horizontal )
            axis = GuiEditCtrl::GuideHorizontal;
         else
            axis = GuiEditCtrl::GuideVertical;
            
         // Start dragging a new guide out in the editor.
         
         U32 guideIndex = mEditCtrl->addGuide( axis, 0 );
         mEditCtrl->startMouseGuideDrag( axis, guideIndex );
      }
      
      void onRender(Point2I offset, const RectI &updateRect)
      {
         GFX->getDrawUtil()->drawRectFill(updateRect, ColorF(1,1,1,1));
         
         Point2I choffset(0,0);
         if( mRefCtrl != NULL )
            choffset = mRefCtrl->getChildPos();

         if( getOrientation() == ORIENTATION_Horizontal )
         {
            // it's horizontal.
            for(U32 i = 0; i < getWidth(); i++)
            {
               S32 x = offset.x + i;
               S32 pos = i - choffset.x;
               if(!(pos % 10))
               {
                  S32 start = 6;
                  if(!(pos %20))
                     start = 4;
                  if(!(pos % 100))
                     start = 1;
                  GFX->getDrawUtil()->drawLine(x, offset.y + start, x, offset.y + 10, ColorF(0,0,0,1));
               }
            }
         }
         else
         {
            // it's vertical.
            for(U32 i = 0; i < getHeight(); i++)
            {
               S32 y = offset.y + i;
               S32 pos = i - choffset.y;
               if(!(pos % 10))
               {
                  S32 start = 6;
                  if(!(pos %20))
                     start = 4;
                  if(!(pos % 100))
                     start = 1;
                  GFX->getDrawUtil()->drawLine(offset.x + start, y, offset.x + 10, y, ColorF(0,0,0,1));
               }
            }
         }
      }
      static void initPersistFields()
      {
         addField( "refCtrl", TypeRealString, Offset( mRefCtrlName, GuiEditorRuler ) );
         addField( "editCtrl", TypeRealString, Offset( mEditCtrlName, GuiEditorRuler ) );
         
         Parent::initPersistFields();
      }
      
      DECLARE_CONOBJECT(GuiEditorRuler);
      DECLARE_CATEGORY( "Gui Editor" );
};

IMPLEMENT_CONOBJECT(GuiEditorRuler);