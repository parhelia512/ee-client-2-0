//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/containers/guiWindowCtrl.h"
#include "gui/containers/guiWindowCollapseCtrl.h"
#include "console/consoleTypes.h"
#include "console/console.h"
#include "gui/core/guiCanvas.h"
#include "gui/core/guiDefaultControlRender.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"
#include "gui/containers/guiRolloutCtrl.h"

IMPLEMENT_CONOBJECT(GuiWindowCollapseCtrl);

//-----------------------------------------------------------------------------


GuiWindowCollapseCtrl::GuiWindowCollapseCtrl() : 
								 mCollapseGroup(-1),
								 mCollapseGroupNum(-1),
								 mIsCollapsed(false),
								 mIsMouseResizing(false)
{
}

void GuiWindowCollapseCtrl::initPersistFields()
{
	addField("CollapseGroup",     TypeS32,          Offset(mCollapseGroup,GuiWindowCollapseCtrl));//debug only
   addField("CollapseGroupNum",  TypeS32,          Offset(mCollapseGroupNum,GuiWindowCollapseCtrl));//debug only
   
   Parent::initPersistFields();
}

void GuiWindowCollapseCtrl::getSnappableWindows( Vector<EdgeRectI> &outVector, Vector<GuiWindowCollapseCtrl*> &windowOutVector)
{
   GuiControl *parent = getParent();
   if( !parent )
      return;

   S32 parentSize = parent->size();
   for( S32 i = 0; i < parentSize; i++ )
   {
      GuiWindowCollapseCtrl *childWindow = dynamic_cast<GuiWindowCollapseCtrl*>(parent->at(i));
		if( !childWindow || !childWindow->isVisible() || childWindow == this || childWindow->mEdgeSnap == false)
			continue;

      outVector.push_back(EdgeRectI(childWindow->getGlobalBounds(), mResizeMargin));
		windowOutVector.push_back(childWindow);
   }

}

void GuiWindowCollapseCtrl::onMouseDown(const GuiEvent &event)
{
   setUpdate();

   mOrigBounds = getBounds();

   mMouseDownPosition = event.mousePoint;
   Point2I localPoint = globalToLocalCoord(event.mousePoint);

   //select this window - move it to the front, and set the first responder
   selectWindow();

   mMouseMovingWin = false;

   S32 hitEdges = findHitEdges( event.mousePoint );

   mResizeEdge = edgeNone;

   // Set flag by default so we only clear it
   // if we don't match either edge
   mMouseResizeHeight = true;

   // Check Bottom/Top edges (Mutually Exclusive)
   if( mResizeHeight && hitEdges & edgeBottom )
      mResizeEdge |= edgeBottom;
   else if( mResizeHeight && hitEdges & edgeTop )
      mResizeEdge |= edgeTop;
   else
      mMouseResizeHeight = false;

   // Set flag by default so we only clear it
   // if we don't match either edge
   mMouseResizeWidth = true;

   // Check Left/Right edges (Mutually Exclusive)
   if( mResizeWidth && hitEdges & edgeLeft )
      mResizeEdge |= edgeLeft;
   else if( mResizeWidth && hitEdges & edgeRight )
      mResizeEdge |= edgeRight;
   else
      mMouseResizeWidth = false;


   //if we clicked within the title bar
   if ( !(mResizeEdge & edgeTop) && localPoint.y < mTitleHeight)
   {
      //if we clicked on the close button
      if (mCanClose && mCloseButton.pointInRect(localPoint))
      {
         mPressClose = mCanClose;
      }
      else if (mCanMaximize && mMaximizeButton.pointInRect(localPoint))
      {
         mPressMaximize = mCanMaximize;
      }
      else if (mCanMinimize && mMinimizeButton.pointInRect(localPoint))
      {
         mPressMinimize = mCanMinimize;
      }

      //else we clicked within the title
      else 
      {
         S32 docking = getDocking();
         if( docking == Docking::dockInvalid || docking == Docking::dockNone )
            mMouseMovingWin = mCanMove;
		
         mMouseResizeWidth = false;
         mMouseResizeHeight = false;
      }
   }


   if (mMouseMovingWin || mResizeEdge != edgeNone ||
         mPressClose || mPressMaximize || mPressMinimize)
   {
      mouseLock();
   }
   else
   {

      GuiControl *ctrl = findHitControl(localPoint);
      if (ctrl && ctrl != this)
         ctrl->onMouseDown(event);

   }
}

void GuiWindowCollapseCtrl::onMouseDragged(const GuiEvent &event)
{
   GuiControl *parent = getParent();
   GuiCanvas *root = getRoot();
   if ( !root ) return;

   Point2I deltaMousePosition = event.mousePoint - mMouseDownPosition;

   Point2I newPosition = getPosition();
   Point2I newExtent = getExtent();
   bool resizeX = false;
   bool resizeY = false;

   mRepositionWindow = false;
   mResizeWindow = false;

   if (mMouseMovingWin && parent)
   {
      if( parent != root )
      {
         newPosition.x = mOrigBounds.point.x + deltaMousePosition.x;
         newPosition.y = getMax(0, mOrigBounds.point.y + deltaMousePosition.y );
		 mRepositionWindow = true;
      }
      else
      {
         newPosition.x = getMax(0, getMin(parent->getWidth() - getWidth(), mOrigBounds.point.x + deltaMousePosition.x));
         newPosition.y = getMax(0, getMin(parent->getHeight() - getHeight(), mOrigBounds.point.y + deltaMousePosition.y));
      }

      // Check snapping to other windows
      if( mEdgeSnap )
      {
         RectI bounds = getGlobalBounds();
         bounds.point = mOrigBounds.point + deltaMousePosition;
         EdgeRectI edges = EdgeRectI( bounds, mResizeMargin );

         Vector<EdgeRectI> snapList;
		 Vector<GuiWindowCollapseCtrl*> windowList;
		 getSnappableWindows( snapList, windowList );
         for( S32 i =0; i < snapList.size(); i++ )
         {
            // Compare Edges for hit
            EdgeRectI &snapRect = snapList[i];
				EdgeRectI orignalSnapRect = snapRect;
			          

				if(windowList[i]->mCollapseGroupNum == -1) 
				{
					// Add some buffer room for snap detection: BOTTOM HITS TOP: BECOMES "PARENT"
					snapRect = orignalSnapRect;
					snapRect.top.position.y = snapRect.top.position.y-12;
					if( edges.bottom.hit( snapRect.top ) )
					{
						// Replace buffer room with original snap params
						snapRect = orignalSnapRect;
						newPosition.y = snapRect.top.position.y - bounds.extent.y;
						newPosition.x = snapRect.left.position.x;
					}
				}
            
				if( (windowList[i]->mCollapseGroupNum == -1) || (windowList[i]->mCollapseGroupNum == mCollapseGroupNum - 1) ||
					(!parent->mCollapseGroupVec.empty() && parent->mCollapseGroupVec[windowList[i]->mCollapseGroup].last() ==  windowList[i]) )
				{
					// Add some buffer room for snap detection: TOP HITS BOTTOM: BECOMES "CHILD"
					snapRect = orignalSnapRect;
					snapRect.bottom.position.y = snapRect.bottom.position.y+12;
					if( edges.top.hit( snapRect.bottom ) )
					{
						// Replace buffer room with original snap params
						snapRect = orignalSnapRect;
						newPosition.y = snapRect.bottom.position.y;
						newPosition.x = snapRect.left.position.x;
					}
				}
         }
      }
   }
   else if(mPressClose || mPressMaximize || mPressMinimize)
   {
      setUpdate();
      return;
   }
   else
   {
      if( ( !mMouseResizeHeight && !mMouseResizeWidth ) || !parent )
         return;

	  mResizeWindow = true;
      if( mResizeEdge & edgeBottom)
	  {
         newExtent.y = getMin(parent->getHeight(), mOrigBounds.extent.y + deltaMousePosition.y);
		 resizeY = true;
	  }
      else if ( mResizeEdge & edgeTop )
      {
         newPosition.y = mOrigBounds.point.y + deltaMousePosition.y;
         newExtent.y = getMin(parent->getHeight(), mOrigBounds.extent.y - deltaMousePosition.y);
		 resizeY = true;
      }
      
      if( mResizeEdge & edgeRight )
	  {
         newExtent.x = getMin(parent->getWidth(), mOrigBounds.extent.x + deltaMousePosition.x);
		 resizeX = true;
	  }
      else if( mResizeEdge & edgeLeft )
      {
         newPosition.x = mOrigBounds.point.x + deltaMousePosition.x;
         newExtent.x = getMin(parent->getWidth(), mOrigBounds.extent.x - deltaMousePosition.x);
		 resizeX = true;
      }
   }

   // if changed positions, change children positions
   // if changed extent, children should conform to width and adjust to height
	
	if(mCollapseGroup >= 0 && mRepositionWindow == true)
		moveWithCollapseGroup(deltaMousePosition);

	// resize myself
	Point2I pos = parent->localToGlobalCoord(getPosition());
	root->addUpdateRegion(pos, getExtent());

	// this makes me think that collapseable windows need to be there own class, there are far too many checks needed in 
	// order to grab the proper procedure. 
	if(mCollapseGroup >= 0 && mResizeWindow == true )
	{
		if(newExtent.y >= getMinExtent().y && newExtent.x >= getMinExtent().x)
		{
		   mIsMouseResizing = true;
			bool canResize = resizeCollapseGroup( resizeX, resizeY, (getPosition() - newPosition), (getExtent() - newExtent) );

			if(canResize == true)
				resize(newPosition, newExtent);
		}
	}
	else
		resize(newPosition, newExtent);
}

void GuiWindowCollapseCtrl::onMouseUp(const GuiEvent &event)
{
   bool closing = mPressClose;
   bool maximizing = mPressMaximize;
   bool minimizing = mPressMinimize;
   mPressClose = false;
   mPressMaximize = false;
   mPressMinimize = false;

   TORQUE_UNUSED(event);
   mouseUnlock();

   mMouseMovingWin = false;
   mMouseResizeWidth = false;
   mMouseResizeHeight = false;

   bool snapSignal = false;

   GuiControl *parent = getParent();
   if (! parent)
      return;

	if( mIsMouseResizing )
   {
	   mIsMouseResizing = false;
	   return;
   }

   //see if we take an action
   Point2I localPoint = globalToLocalCoord(event.mousePoint);
   if (closing && mCloseButton.pointInRect(localPoint))
   {
	   // here is where were going to put our other if statement
	   //if the window closes, and there were only 2 windows in the array, then just delete the array and default there params
	   //if not, delete the window from the array, default its params, and renumber the windows

      Con::evaluate(mCloseCommand);

   }
   else if (maximizing && mMaximizeButton.pointInRect(localPoint))
   {
      if (mMaximized)
      {
         //resize to the previous position and extent, bounded by the parent
         resize(Point2I(getMax(0, getMin(parent->getWidth() - mStandardBounds.extent.x, mStandardBounds.point.x)),
                        getMax(0, getMin(parent->getHeight() - mStandardBounds.extent.y, mStandardBounds.point.y))),
                        mStandardBounds.extent);
         //set the flag
         mMaximized = false;
      }
      else
      {
         //only save the position if we're not minimized
         if (! mMinimized)
         {
            mStandardBounds = getBounds();
         }
         else
         {
            mMinimized = false;
         }

         //resize to fit the parent
         resize(Point2I(0, 0), parent->getExtent());

         //set the flag
         mMaximized = true;
      }
   }
   else if (minimizing && mMinimizeButton.pointInRect(localPoint))
   {
      if (mMinimized)
      {
         //resize to the previous position and extent, bounded by the parent
         resize(Point2I(getMax(0, getMin(parent->getWidth() - mStandardBounds.extent.x, mStandardBounds.point.x)),
                        getMax(0, getMin(parent->getHeight() - mStandardBounds.extent.y, mStandardBounds.point.y))),
                        mStandardBounds.extent);
         //set the flag
         mMinimized = false;
      }
      else
      {
         if (parent->getWidth() < 100 || parent->getHeight() < mTitleHeight + 3)
            return;

         //only save the position if we're not maximized
         if (! mMaximized)
         {
            mStandardBounds = getBounds();
         }
         else
         {
            mMaximized = false;
         }

         //first find the lowest unused minimized index up to 32 minimized windows
         U32 indexMask = 0;
         iterator i;
         S32 count = 0;
         for (i = parent->begin(); i != parent->end() && count < 32; i++)
         {
            count++;
            S32 index;
            GuiWindowCollapseCtrl *ctrl = dynamic_cast<GuiWindowCollapseCtrl *>(*i);
            if (ctrl && ctrl->isMinimized(index))
            {
               indexMask |= (1 << index);
            }
         }

         //now find the first unused bit
         for (count = 0; count < 32; count++)
         {
            if (! (indexMask & (1 << count))) break;
         }

         //if we have more than 32 minimized windows, use the first position
         count = getMax(0, count);

         //this algorithm assumes all window have the same title height, and will minimize to 98 pix
         Point2I newExtent(98, mTitleHeight);

         //first, how many can fit across
         S32 numAcross = getMax(1, (parent->getWidth() / newExtent.x + 2));

         //find the new "mini position"
         Point2I newPosition;
         newPosition.x = (count % numAcross) * (newExtent.x + 2) + 2;
         newPosition.y = parent->getHeight() - (((count / numAcross) + 1) * (newExtent.y + 2)) - 2;

         //find the minimized position and extent
         resize(newPosition, newExtent);

         //set the index so other windows will not try to minimize to the same location
         mMinimizeIndex = count;

         //set the flag
         mMinimized = true;
      }
   }
   else if ( !(mResizeEdge & edgeTop) && localPoint.y < mTitleHeight && event.mousePoint == mMouseDownPosition)
	{
		//if we clicked on the close button
		if (mCanClose && mCloseButton.pointInRect(localPoint))
			return;
		else if (mCanMaximize && mMaximizeButton.pointInRect(localPoint))
			return;
		else if (mCanMinimize && mMinimizeButton.pointInRect(localPoint))
			return;
		else
			toggleCollapseGroup();
   }
   else if( mEdgeSnap )
   {
		Point2I deltaMousePosition = event.mousePoint - mMouseDownPosition;

		Point2I newPosition = getPosition();
		Point2I newExtent = getExtent();
        RectI bounds = getGlobalBounds();
        bounds.point = mOrigBounds.point + deltaMousePosition;
        EdgeRectI edges = EdgeRectI( bounds, mResizeMargin );

        Vector<EdgeRectI> snapList;
		Vector<GuiWindowCollapseCtrl*> windowList;
        getSnappableWindows( snapList, windowList );
        for( S32 i =0; i < snapList.size(); i++ )
        {
			// Compare Edges for hit
			EdgeRectI &snapRect = snapList[i];
			EdgeRectI orignalSnapRect = snapRect;
			
			// Grab Window being hit
			GuiWindowCollapseCtrl *hitWindow = windowList[i];

			if(windowList[i]->mCollapseGroupNum == -1) 
			{
				// Add some buffer room for snap detection : BECOMES "PARENT"
				snapRect = orignalSnapRect;
				snapRect.top.position.y = snapRect.top.position.y-12;
				if( edges.bottom.hit( snapRect.top ) )
				{
				   // Replace buffer room with original snap params
				   snapRect = orignalSnapRect;
				   newExtent.x = snapRect.right.position.x - snapRect.left.position.x;
					
					moveToCollapseGroup(hitWindow, 0);

				   snapSignal = true;
				}
			}
            
			if( (windowList[i]->mCollapseGroupNum == -1) || (windowList[i]->mCollapseGroupNum == mCollapseGroupNum - 1) ||
				(!parent->mCollapseGroupVec.empty() && parent->mCollapseGroupVec[windowList[i]->mCollapseGroup].last() ==  windowList[i]) )
			{
				// Add some buffer room for snap detection : BECOMES "CHILD"
				snapRect = orignalSnapRect;
				snapRect.bottom.position.y = snapRect.bottom.position.y+12;
				if( edges.top.hit( snapRect.bottom ) )
				{
				   // Replace buffer room with original snap params
				   snapRect = orignalSnapRect;
				   newExtent.x = snapRect.right.position.x - snapRect.left.position.x;
					moveToCollapseGroup(hitWindow, 1);

				   snapSignal = true;
				}
			}
         }
		 resize(newPosition, newExtent);

		 if(mCollapseGroup >= 0 && mCollapseGroupNum != 0  && snapSignal == false)
			moveFromCollapseGroup();
	}
}

void GuiWindowCollapseCtrl::moveFromCollapseGroup()
{
   GuiControl *parent = getParent();
   if( !parent )
		return;

	S32 groupVec = mCollapseGroup;
	S32 vecPos = mCollapseGroupNum;
	S32 groupVecCount = parent->mCollapseGroupVec[groupVec].size() - 1;

	typedef Vector< GuiWindowCollapseCtrl *>	CollapseGroupNumVec;
	CollapseGroupNumVec					mCollapseGroupNumVec;

	if( groupVecCount > vecPos )
	{
		if (vecPos == 1)
		{
			

			CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[groupVec].begin();
			for(; iter != parent->mCollapseGroupVec[groupVec].end(); iter++ )
			{
				if((*iter)->mCollapseGroupNum >= vecPos)
					mCollapseGroupNumVec.push_back((*iter));
			}

			parent->mCollapseGroupVec[groupVec].first()->mCollapseGroup = -1;
			parent->mCollapseGroupVec[groupVec].first()->mCollapseGroupNum = -1;
			parent->mCollapseGroupVec[groupVec].erase(U32(0));
			parent->mCollapseGroupVec[groupVec].setSize(groupVecCount - 1);
			parent->mCollapseGroupVec.erase(groupVec);
			if(groupVec > 0)
				parent->mCollapseGroupVec.setSize(groupVec);

			//iterate through the newly created array; delete my references in the old group, create a new group and organize me accord.
			S32 assignWindowNumber = 0;
			CollapseGroupNumVec::iterator iter2 = mCollapseGroupNumVec.begin();
			for(; iter2 != mCollapseGroupNumVec.end(); iter2++ )
			{
				(*iter2)->mCollapseGroup = (parent->mCollapseGroupVec.size());
				(*iter2)->mCollapseGroupNum = assignWindowNumber;
				
				assignWindowNumber++;
				groupVecCount--;
			}

			parent->mCollapseGroupVec.push_back( mCollapseGroupNumVec );
		}
		else
		{
			//iterate through the group i was once in, gather myself and the controls below me and store them in an array
			CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[groupVec].begin();
			for(; iter != parent->mCollapseGroupVec[groupVec].end(); iter++ )
			{
				if((*iter)->mCollapseGroupNum >= vecPos)
					mCollapseGroupNumVec.push_back((*iter));
			}
			
			//iterate through the newly created array; delete my references in the old group, create a new group and organize me accord.
			S32 assignWindowNumber = 0;
			CollapseGroupNumVec::iterator iter2 = mCollapseGroupNumVec.begin();
			for(; iter2 != mCollapseGroupNumVec.end(); iter2++ )
			{
				parent->mCollapseGroupVec[groupVec].pop_back();
				parent->mCollapseGroupVec[groupVec].setSize(groupVecCount);
				(*iter2)->mCollapseGroup = (parent->mCollapseGroupVec.size());
				(*iter2)->mCollapseGroupNum = assignWindowNumber;
				
				assignWindowNumber++;
				groupVecCount--;
			}

			parent->mCollapseGroupVec.push_back( mCollapseGroupNumVec );
		}
	}
	else
	{
		parent->mCollapseGroupVec[groupVec].erase(mCollapseGroupNum);
		parent->mCollapseGroupVec[groupVec].setSize(groupVecCount);
		mCollapseGroup = -1;
		mCollapseGroupNum = -1;

		if(groupVecCount <= 1)
		{
			parent->mCollapseGroupVec[groupVec].first()->mCollapseGroup = -1;
			parent->mCollapseGroupVec[groupVec].first()->mCollapseGroupNum = -1;
			parent->mCollapseGroupVec[groupVec].erase(U32(0));
			parent->mCollapseGroupVec[groupVec].setSize(groupVecCount - 1);
			parent->mCollapseGroupVec.erase(groupVec);
			//if(groupVec > 0)
			//	parent->mCollapseGroupVec.setSize(groupVec);
	
		}
	}

	refreshCollapseGroups();
}
void GuiWindowCollapseCtrl::moveToCollapseGroup(GuiWindowCollapseCtrl* hitWindow, bool orientation )
{
	// orientation 0 - window in question is being connected to top of another window
	// orientation 1 - window in question is being connected to bottom of another window

	GuiControl *parent = getParent();
	if( !parent )
		return;
	
   	S32 groupVec = mCollapseGroup;
	S32 attatchedGroupVec = hitWindow->mCollapseGroup;
	S32 vecPos = mCollapseGroupNum;
	
	if(mCollapseGroup == attatchedGroupVec && vecPos != -1)
		return;

    typedef Vector< GuiWindowCollapseCtrl *>	CollapseGroupNumVec;
	CollapseGroupNumVec					mCollapseGroupNumVec;

	// window colliding with is not in a collapse group
	if(hitWindow->mCollapseGroup < 0) 
	{
		// we(the collider) are in a group of windows
		if(mCollapseGroup >= 0) 
		{
			S32 groupVecCount = parent->mCollapseGroupVec[groupVec].size() - 1;
				
			//copy pointer window data in my array, and store in a temp array
			CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[groupVec].begin();
			for(; iter != parent->mCollapseGroupVec[groupVec].end(); iter++ )
			{
				if((*iter)->mCollapseGroupNum >= vecPos)
				{
					mCollapseGroupNumVec.push_back((*iter));
					groupVecCount--;
				}
			}

			// kill my old array group and erase its footprints
			if( vecPos <= 1 || groupVecCount == 0 )
			{
				//sanity check, always reset the first window just to be sure
				parent->mCollapseGroupVec[groupVec].first()->mCollapseGroup = -1;
				parent->mCollapseGroupVec[groupVec].first()->mCollapseGroupNum = -1;

				parent->mCollapseGroupVec[groupVec].clear();
				parent->mCollapseGroupVec[groupVec].setSize(U32(0));
				parent->mCollapseGroupVec.erase(groupVec);

				if(groupVec > 0)
					parent->mCollapseGroupVec.setSize(groupVec);
			}

			// push the collided window
			if(orientation == 0)
				mCollapseGroupNumVec.push_back(hitWindow);
			else
				mCollapseGroupNumVec.push_front(hitWindow);

			// iterate and renumber the temp array
			S32 assignWindowNumber = 0;
			CollapseGroupNumVec::iterator iter2 = mCollapseGroupNumVec.begin();
			for(; iter2 != mCollapseGroupNumVec.end(); iter2++ )
			{
				(*iter2)->mCollapseGroup = (parent->mCollapseGroupVec.size());
				(*iter2)->mCollapseGroupNum = assignWindowNumber;
				
				assignWindowNumber++;
			}

			// push the temp array in the guiControl array holder
			parent->mCollapseGroupVec.push_back( mCollapseGroupNumVec );
		}
		else
		{
			if(orientation == 0)
			{
				mCollapseGroupNumVec.push_front(hitWindow);
				mCollapseGroupNumVec.push_front(this);
			}
			else
			{
				mCollapseGroupNumVec.push_front(this);
				mCollapseGroupNumVec.push_front(hitWindow);
			}
			
			S32 assignWindowNumber = 0;
			CollapseGroupNumVec::iterator iter = mCollapseGroupNumVec.begin();
			for(; iter != mCollapseGroupNumVec.end(); iter++ )
			{
				(*iter)->mCollapseGroup = (parent->mCollapseGroupVec.size());
				(*iter)->mCollapseGroupNum = assignWindowNumber;
				assignWindowNumber++;
			}

			parent->mCollapseGroupVec.push_back( mCollapseGroupNumVec );
		}
	}
	else // window colliding with *IS* in a collapse group
	{
		if(mCollapseGroup >= 0)
		{
			S32 groupVecCount = parent->mCollapseGroupVec[groupVec].size() - 1;
				
			CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[groupVec].begin();
			for(; iter != parent->mCollapseGroupVec[groupVec].end(); iter++ )
			{
				if((*iter)->mCollapseGroupNum >= vecPos)
				{
					// push back the pointer window controls to the collided array
					parent->mCollapseGroupVec[attatchedGroupVec].push_back((*iter));
					groupVecCount--;
				}
			}

			if( vecPos <= 1 || groupVecCount == 0 )
			{
				//sanity check, always reset the first window just to be sure
				parent->mCollapseGroupVec[groupVec].first()->mCollapseGroup = -1;
				parent->mCollapseGroupVec[groupVec].first()->mCollapseGroupNum = -1;

				parent->mCollapseGroupVec[groupVec].clear();
				parent->mCollapseGroupVec[groupVec].setSize(U32(0));
				parent->mCollapseGroupVec.erase(groupVec);

				if(groupVec > 0)
					parent->mCollapseGroupVec.setSize(groupVec);
			}
			
			// since we killed my old array group, run in case the guiControl array moved me down a notch
			if(attatchedGroupVec > groupVec )
				attatchedGroupVec--;

			// iterate through the collided array, renumbering the windows pointers
			S32 assignWindowNumber = 0;
			CollapseGroupNumVec::iterator iter2;
			for(iter2 = parent->mCollapseGroupVec[attatchedGroupVec].begin(); iter2 != parent->mCollapseGroupVec[attatchedGroupVec].end(); iter2++ )
			{
				(*iter2)->mCollapseGroup = attatchedGroupVec;
				(*iter2)->mCollapseGroupNum = assignWindowNumber;
				
				assignWindowNumber++;
			}

		}
		else
		{
			S32 groupVec = hitWindow->mCollapseGroup;
			
			if(orientation == 0)
			{
				parent->mCollapseGroupVec[groupVec].push_front(this);
			}
			else
			{
				parent->mCollapseGroupVec[groupVec].push_back(this);
			}
			S32 assignWindowNumber = 0;
			CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[groupVec].begin();
			for(; iter != parent->mCollapseGroupVec[groupVec].end(); iter++ )
			{
				(*iter)->mCollapseGroup = hitWindow->mCollapseGroup;
				(*iter)->mCollapseGroupNum = assignWindowNumber;
				assignWindowNumber++;
			}
		}
	}
	refreshCollapseGroups();
}


void GuiWindowCollapseCtrl::refreshCollapseGroups()
{
	GuiControl *parent = getParent();
	if( !parent )
		return;
	
	typedef Vector< GuiWindowCollapseCtrl *>	CollapseGroupNumVec;
	CollapseGroupNumVec					mCollapseGroupNumVec;

	// iterate through the collided array, renumbering the windows pointers
	S32 assignGroupNum = 0;
	CollapseGroupVec::iterator iter = parent->mCollapseGroupVec.begin();
	for(; iter != parent->mCollapseGroupVec.end(); iter++ )
	{
		S32 assignWindowNumber = 0;
		CollapseGroupNumVec::iterator iter2 = parent->mCollapseGroupVec[assignGroupNum].begin();
		for(; iter2 != parent->mCollapseGroupVec[assignGroupNum].end(); iter2++ )
		{
			(*iter2)->mCollapseGroup = assignGroupNum;
			(*iter2)->mCollapseGroupNum = assignWindowNumber;
			assignWindowNumber++;
		}
				
		assignGroupNum++;
	}
}

void GuiWindowCollapseCtrl::moveWithCollapseGroup(Point2I deltaMousePosition)
{
   GuiControl *parent = getParent();
   if( !parent )
      return;

	typedef Vector< GuiWindowCollapseCtrl *>	CollapseGroupNumVec;
	CollapseGroupNumVec					mCollapseGroupNumVec;

	S32 addedPosition = getExtent().y;

	CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[mCollapseGroup].begin();
	for(; iter != parent->mCollapseGroupVec[mCollapseGroup].end(); iter++ )
	{
		if((*iter)->mCollapseGroupNum > mCollapseGroupNum)
		{
			Point2I newChildPosition =  (*iter)->getPosition();
			newChildPosition.x = mOrigBounds.point.x + deltaMousePosition.x;
			newChildPosition.y = getMax(0, mOrigBounds.point.y + deltaMousePosition.y +  addedPosition);

			(*iter)->resize(newChildPosition, (*iter)->getExtent());
			addedPosition += (*iter)->getExtent().y;
		}
	}
}

void GuiWindowCollapseCtrl::setCollapseGroup(bool state)
{
	GuiControl *parent = getParent();
	if( !parent )
		return;

   if( mIsCollapsed != state )
   {
      mIsCollapsed = state;
      handleCollapseGroup();
   }
}

void GuiWindowCollapseCtrl::toggleCollapseGroup()
{
	GuiControl *parent = getParent();
	if( !parent )
		return;

   mIsCollapsed = !mIsCollapsed;
   handleCollapseGroup();
}

void GuiWindowCollapseCtrl::handleCollapseGroup()
{
	GuiControl *parent = getParent();
	if( !parent )
		return;

	typedef Vector< GuiWindowCollapseCtrl *>	CollapseGroupNumVec;
	CollapseGroupNumVec					mCollapseGroupNumVec;

	if( mIsCollapsed ) // minimize window up to its header bar
	{
		//save settings 
		mPreCollapsedYExtent = getExtent().y;
		mPreCollapsedYMinExtent = getMinExtent().y;

		//create settings for collapsed window to abide by
		mResizeHeight = false;
		setMinExtent( Point2I( getMinExtent().x, 24 ) );

		iterator i;
      for(i = begin(); i != end(); i++)
      {
         GuiControl *ctrl = static_cast<GuiControl *>(*i);
			ctrl->setVisible(false);
			ctrl->mCanResize = false;
      }

		resize( getPosition(), Point2I( getExtent().x, 24 ) );

		if(mCollapseGroup >= 0)
		{
			S32 moveChildYBy = (mPreCollapsedYExtent - 24);

			CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[mCollapseGroup].begin();
			for(; iter != parent->mCollapseGroupVec[mCollapseGroup].end(); iter++ )
			{
				if((*iter)->mCollapseGroupNum > mCollapseGroupNum)
				{
					Point2I newChildPosition =  (*iter)->getPosition();
					newChildPosition.y -= moveChildYBy;
					(*iter)->resize(newChildPosition, (*iter)->getExtent());
				}
			}
		}
	}
	else // maximize the window to its previous position
	{
		//create and load settings
		mResizeHeight = true;
		setMinExtent( Point2I( getMinExtent().x, mPreCollapsedYMinExtent ) );
		
		resize( getPosition(), Point2I( getExtent().x, mPreCollapsedYExtent ) );
		
		iterator i;
      for(i = begin(); i != end(); i++)
      {
         GuiControl *ctrl = static_cast<GuiControl *>(*i);
			ctrl->setVisible(true);
			ctrl->mCanResize = true;
      }

		if(mCollapseGroup >= 0)
		{
			S32 moveChildYBy = (mPreCollapsedYExtent - 24);

			CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[mCollapseGroup].begin();
			for(; iter != parent->mCollapseGroupVec[mCollapseGroup].end(); iter++ )
			{
				if((*iter)->mCollapseGroupNum > mCollapseGroupNum)
				{
					Point2I newChildPosition =  (*iter)->getPosition();
					newChildPosition.y += moveChildYBy;					
					(*iter)->resize(newChildPosition, (*iter)->getExtent());
				}
			}
		}
	}
}

bool GuiWindowCollapseCtrl::resizeCollapseGroup(bool resizeX, bool resizeY, Point2I resizePos, Point2I resizeExtent)
{

	GuiControl *parent = getParent();
	if( !parent )
		return false;

	typedef Vector< GuiWindowCollapseCtrl *>	CollapseGroupNumVec;
	CollapseGroupNumVec					mCollapseGroupNumVec;

	bool canResize = true;

	CollapseGroupNumVec::iterator iter = parent->mCollapseGroupVec[mCollapseGroup].begin();
	for(; iter != parent->mCollapseGroupVec[mCollapseGroup].end(); iter++ )
	{
		if((*iter) == this)
			continue; 
		
		Point2I newChildPosition = (*iter)->getPosition();
		Point2I newChildExtent = (*iter)->getExtent();

		if( resizeX == true )
		{
			newChildPosition.x -= resizePos.x;
			newChildExtent.x -= resizeExtent.x;
			
		}
		if( resizeY == true )
		{
			if((*iter)->mCollapseGroupNum > mCollapseGroupNum)
			{
				newChildPosition.y -= resizeExtent.y;
				newChildPosition.y -= resizePos.y;
			}
			else if((*iter)->mCollapseGroupNum == mCollapseGroupNum - 1)
				newChildExtent.y -= resizePos.y;
		}

		// check is done for normal extent of windows. if false, check again in case its just giving false
		// due to being a collapsed window. if your truly being forced passed your extent, return false
		if( !(*iter)->mIsCollapsed && newChildExtent.y >= (*iter)->getMinExtent().y )
			(*iter)->resize( newChildPosition, newChildExtent);
		else
		{
			if( (*iter)->mIsCollapsed )
				(*iter)->resize( newChildPosition, newChildExtent);
			else
				canResize = false;
		}
	}
	return canResize;
}

void GuiWindowCollapseCtrl::parentResized(const RectI &oldParentRect, const RectI &newParentRect)
{
	if(!mCanResize)
		return;
	
	GuiControl *parent = getParent();
	if( !parent )
		return;

   // bail if were not sized both by windowrelative
   if( mHorizSizing != horizResizeWindowRelative || mHorizSizing != vertResizeWindowRelative )
      return Parent::parentResized( oldParentRect, newParentRect );

	Point2I newPosition = getPosition();
   Point2I newExtent = getExtent();
	
	bool doCollapse = false;

	S32 deltaX = newParentRect.extent.x - oldParentRect.extent.x;
 	S32 deltaY = newParentRect.extent.y - oldParentRect.extent.y + mProfile->mYPositionOffset;

	//this;//do this again for y see what happens
	if (oldParentRect.extent.x != 0)
	{
		if( newPosition.x > (oldParentRect.extent.x / 2) - 1 )
		{
	//		if( (newPosition.x + newExtent.x + deltaX) < newParentRect.extent.x)
				newPosition.x = newPosition.x + deltaX;
		}
	}

	if (oldParentRect.extent.y != 0)
	{
		// only if were apart of a group
		if ( mCollapseGroup >= 0 )
		{
			// setup parsing mechanisms
			typedef Vector< GuiWindowCollapseCtrl *>	CollapseGroupNumVec;
			CollapseGroupNumVec					mCollapseGroupNumVec;
			
			// lets grab the information we need (this should probably be already stored on each individual window object)
			S32 groupNum = mCollapseGroup;
			S32 groupMax = parent->mCollapseGroupVec[ groupNum ].size() - 1;

			// set up vars that we're going to be using
			S32 groupPos = 0;
			S32 groupExtent = 0;
			S32 tempGroupExtent = 0;

			// set up the vector/iterator combo we need
			mCollapseGroupNumVec = parent->mCollapseGroupVec[ groupNum ];
			CollapseGroupNumVec::iterator iter = mCollapseGroupNumVec.begin();

			// grab some more information we need later ( this info should also be located on each ind. window object )
			for( ; iter != mCollapseGroupNumVec.end(); iter++ )
			{
				if((*iter)->getCollapseGroupNum() == 0)
				{
					groupPos = (*iter)->getPosition().y;
				}

				groupExtent += (*iter)->getExtent().y;
			}
			
			// use the information we just gatherered; only enter this block if we need to
			tempGroupExtent = groupPos + groupExtent;
			if( tempGroupExtent > ( newParentRect.extent.y / 2 ) + mProfile->mYPositionOffset)
			{
				// lets size the collapse group
				S32 windowParser = groupMax;
				bool secondLoop = false;
				while(tempGroupExtent >= (newParentRect.extent.y - mProfile->mYPositionOffset) )
				{
					
					if( windowParser == -1)
					{
						if( !secondLoop )
						{
							secondLoop = true;
							windowParser = groupMax;
						}
						else
							break;
					}

					GuiWindowCollapseCtrl *tempWindow = mCollapseGroupNumVec[windowParser];
					if(tempWindow->mIsCollapsed)
					{
						windowParser--;
						continue;
					}
					
					// resizing block for the loop... if we can get away with just resizing the bottom window do it before
					// resizing the whole block. else, go through the group and start setting min extents. if that doesnt work
					// on the second go around, start collpsing the windows.
					if( tempGroupExtent - ( tempWindow->getExtent().y - tempWindow->getMinExtent().y ) <= newParentRect.extent.y - mProfile->mYPositionOffset)
					{
						if( this == tempWindow )
							newExtent.y = newExtent.y - ( tempGroupExtent - newParentRect.extent.y + mProfile->mYPositionOffset);
							
						tempGroupExtent = tempGroupExtent - newParentRect.extent.y + mProfile->mYPositionOffset ;
					}
					else
					{
						if( secondLoop )
						{
							tempGroupExtent = tempGroupExtent - (tempWindow->getExtent().y + 32);

							if( this == tempWindow )
								doCollapse = true;
						}
						else
						{
							tempGroupExtent = tempGroupExtent - (tempWindow->getExtent().y - tempWindow->getMinExtent().y);
							if( this == tempWindow )
								newExtent.y = tempWindow->getMinExtent().y;
						}
					}
					windowParser--;
				}
			}
		}
		else if( newPosition.y > (oldParentRect.extent.y / 2) - mProfile->mYPositionOffset)
		{
			newPosition.y = newPosition.y + deltaY;
		}
	}

   if( newExtent.x >= getMinExtent().x && newExtent.y >= getMinExtent().y )
	{
		// If we are already outside the reach of the main window, lets not place ourselves
		// further out; but if were trying to improve visibility, go for it
		if( (newPosition.x + newExtent.x) > newParentRect.extent.x )
		{
			if( (newPosition.x + newExtent.x) > (getPosition().x + getExtent().x) )
			{ 
				newPosition.x = getPosition().x;
				newExtent.x = getExtent().x;
			}
				//return;
		}
		if( (newPosition.y + newExtent.y) > newParentRect.extent.y + mProfile->mYPositionOffset)
		{
			if( (newPosition.y + newExtent.y) > (getPosition().y + getExtent().y) )
			{
				newPosition.y = getPosition().y;
				newExtent.y = getExtent().y;
			}//return; 
		}

		// Only for collpasing groups, if were not, then do it like normal windows
		if(mCollapseGroup >= 0)
		{	
			bool resizeMe = false;
			
			// Only the group window should control positioning
			if( mCollapseGroupNum == 0 )
			{
				resizeMe = resizeCollapseGroup( true, true, (getPosition() - newPosition), (getExtent() - newExtent) );
				if(resizeMe == true)
					resize(newPosition, newExtent);
			}
			else if( getExtent() != newExtent)
			{
				resizeMe = resizeCollapseGroup( false, true, (getPosition() - newPosition), (getExtent() - newExtent) );
				if(resizeMe == true)
					resize(getPosition(), newExtent);
			}
		}
		else
		{
			resize(newPosition, newExtent);
		}
	}

	if( !mIsCollapsed && doCollapse )
		toggleCollapseGroup();

	// if docking is invalid on this control, then bail out here
	if( getDocking() & Docking::dockInvalid || getDocking() & Docking::dockNone)
		return;

	// Update Self
   RectI oldThisRect = getBounds();
   anchorControl( this, Point2I( deltaX, deltaY ) );
   RectI newThisRect = getBounds();

   // Update Deltas to pass on to children
   deltaX = newThisRect.extent.x - oldThisRect.extent.x;
   deltaY = newThisRect.extent.y - oldThisRect.extent.y;

   // Iterate over all children and update their anchors
   iterator nI = begin();
   for( ; nI != end(); nI++ )
   {
      // Sanity
      GuiControl *control = dynamic_cast<GuiControl*>( (*nI) );
      if( control )
         control->parentResized( oldThisRect, newThisRect );
   }
}

//-----------------------------------------------------------------------------

ConsoleMethod( GuiWindowCollapseCtrl, setCollapseGroup, void, 3, 3, "(bool collapse) - Set the window's collapsing state." )
{
   object->setCollapseGroup(dAtob(argv[2]));
}

ConsoleMethod( GuiWindowCollapseCtrl, toggleCollapseGroup, void, 2, 2, "() - Toggle the window collapsing." )
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   object->toggleCollapseGroup();
}

//-----------------------------------------------------------------------------

ConsoleFunction(AttachWindows, void, 3, 3, " (GuiWindowCollapseCtrl #1, GuiWindowCollapseCtrl #2) #1 = bottom window, #2 = top window")
{
   GuiWindowCollapseCtrl* bottomWindow;
   Sim::findObject<GuiWindowCollapseCtrl>(argv[1], bottomWindow);
   
	if(bottomWindow == NULL)
	{
		Con::warnf("Warning: AttachWindows - could not find window \"%s\"",argv[2]);
		return;
	}

   GuiWindowCollapseCtrl* topWindow;
   Sim::findObject<GuiWindowCollapseCtrl>(argv[2], topWindow);
	
	if(bottomWindow == NULL)
	{
		Con::warnf("Warning: AttachWindows - could not find window \"%s\"",argv[1]);
		return;
	}
	

   bottomWindow->moveToCollapseGroup(topWindow, 1);
}