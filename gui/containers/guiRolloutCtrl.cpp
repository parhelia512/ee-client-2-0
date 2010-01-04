//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/containers/guiRolloutCtrl.h"
#include "gui/containers/guiScrollCtrl.h"
#include "gfx/gfxDrawUtil.h"

//-----------------------------------------------------------------------------
// GuiRolloutCtrl
//-----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiRolloutCtrl);

GuiRolloutCtrl::GuiRolloutCtrl()
{
   mExpanded.set(0,0,200,60);
   mCaption             = StringTable->insert("");
   mIsExpanded          = true;
   mIsAnimating         = false;
   mCollapsing          = false;
   mAnimateDestHeight   = 40;
   mAnimateStep         = 1;
   mDefaultHeight       = 40;
   mHideHeader          = false;
   mMargin.set( 0, 0, 0, 0 );
   mIsContainer = true;
   mCanCollapse = true;
   // Make sure we receive our ticks.
   setProcessTicks();
}

GuiRolloutCtrl::~GuiRolloutCtrl()
{
}

//-----------------------------------------------------------------------------
// Persistence 
//-----------------------------------------------------------------------------
void GuiRolloutCtrl::initPersistFields()
{
   addField( "Caption", TypeCaseString, Offset(mCaption, GuiRolloutCtrl) );
   addField( "Margin", TypeRectI, Offset(mMargin, GuiRolloutCtrl) );
   addField( "DefaultHeight", TypeS32, Offset(mDefaultHeight, GuiRolloutCtrl) );
   addProtectedField( "Expanded", TypeBool, Offset( mIsExpanded, GuiRolloutCtrl), &setExpanded, &defaultProtectedGetFn, "" );
   addField("ClickCollapse", TypeBool, Offset(mCanCollapse, GuiRolloutCtrl));
   addField("HideHeader", TypeBool, Offset(mHideHeader, GuiRolloutCtrl));

   Parent::initPersistFields();
}

//-----------------------------------------------------------------------------
// Scene Events
//-----------------------------------------------------------------------------
bool GuiRolloutCtrl::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   mHasTexture = ( mProfile ? mProfile->constructBitmapArray() > 0 : false );
   if ( mHasTexture )
      mBitmapBounds = mProfile->mBitmapArrayRects.address();

   // Calculate Heights for this control
   calculateHeights();

   return true;
}

bool GuiRolloutCtrl::onWake()
{
   if (! Parent::onWake())
      return false;

   if( !mIsAnimating && mIsExpanded )
      sizeToContents();

   return true;
}

void GuiRolloutCtrl::addObject( SimObject *obj )
{
   // Call Parent.
   Parent::addObject( obj );

   sizeToContents();
}

void GuiRolloutCtrl::removeObject( SimObject *obj )
{
   // Call Parent.
   Parent::removeObject( obj );

   // Recalculate our rectangles.
   calculateHeights();
}

//-----------------------------------------------------------------------------
// Mouse Events
//-----------------------------------------------------------------------------
void GuiRolloutCtrl::onMouseDown( const GuiEvent &event )
{
   Point2I localPoint = globalToLocalCoord( event.mousePoint );
   mouseLock();
}

void GuiRolloutCtrl::onMouseUp( const GuiEvent &event )
{
   Point2I localPoint = globalToLocalCoord( event.mousePoint );
   if( mCanCollapse && mHeader.pointInRect( localPoint ) && !mIsAnimating && isMouseLocked() )
   {
      // If Ctrl/Cmd-clicking a header, collapse all sibling GuiRolloutCtrls.
      
      if( event.modifier & SI_PRIMARY_CTRL )
      {
         for( SimSet::iterator iter = getParent()->begin(); iter != getParent()->end(); ++ iter )
         {
            GuiRolloutCtrl* ctrl = dynamic_cast< GuiRolloutCtrl* >( *iter );
            if( ctrl && ctrl != this && ctrl->mCanCollapse )
               ctrl->instantCollapse();
         }
         
         if( !mIsExpanded )
            expand();
      }
      else
      {
         // Toggle expansion.
         
         toggleExpanded( false );
      }
   }

   if( isMouseLocked() )
      mouseUnlock();
}

//-----------------------------------------------------------------------------
// Control Sizing Helpers
//-----------------------------------------------------------------------------
void GuiRolloutCtrl::calculateHeights()
{
   S32 barHeight = 20;

   if ( mHasTexture && mProfile && mProfile->mBitmapArrayRects.size() >= NumBitmaps )
   {
      // Store Header Rectangle
      mHeader.set( 0, 0, getWidth(), mProfile->mBitmapArrayRects[ CollapsedCenter ].extent.y );

      // Bottom Bar Max
      barHeight = mProfile->mBitmapArrayRects[ TopLeftHeader ].extent.y;
   }
   else
   {
      mHeader.set( 0, 0, getWidth(), barHeight );
   }
   
   if ( mHideHeader )
   {
      barHeight = 0;
      mHeader.extent.y = 0;
   }

   GuiControl *content = static_cast<GuiControl*>( at(0) );
   if ( content != NULL )
      mExpanded.set( 0, 0, getWidth(), barHeight + content->getHeight() + ( mMargin.point.y + mMargin.extent.y ) );
   else
      mExpanded.set( 0, 0, getWidth(), barHeight + mDefaultHeight );
}

bool GuiRolloutCtrl::resize( const Point2I &newPosition, const Point2I &newExtent )
{
   if ( !Parent::resize( newPosition, newExtent ) )
      return false;

   // Recalculate Heights and resize ourself appropriately.
   calculateHeights();

   GuiControl *content = dynamic_cast<GuiControl*>( at(0) );
   
   // Size Content Properly?!
   if ( mNotifyChildrenResized && content != NULL )
   {
	  S32 barHeight = ( mHideHeader ) ? 0 : 20;
      if ( !mHideHeader && mHasTexture && mProfile && mProfile->mBitmapArrayRects.size() >= NumBitmaps )
      {
         barHeight = mProfile->mBitmapArrayRects[ TopLeftHeader ].extent.y;
      }

      mChildRect.set( mMargin.point.x, 
                      mHeader.extent.y + mMargin.point.y, 
                      getWidth() - ( mMargin.point.x + mMargin.extent.x ), 
                      getHeight() - ( barHeight + ( mMargin.point.y + mMargin.extent.y ) ) );

      if ( content->resize( mChildRect.point, mChildRect.extent ) )
         return true;
   }

   // Nothing sized
   return false;
}


void GuiRolloutCtrl::sizeToContents()
{
   calculateHeights();

   // Set destination height
   if ( size() > 0 )
      instantExpand();
   else
      instantCollapse();
}

void GuiRolloutCtrl::instantExpand()
{
   mAnimateDestHeight = mExpanded.extent.y;
   mCollapsing = false;
   mIsExpanded = true;
   mIsAnimating = false;
   resize( getPosition() + mExpanded.point, mExpanded.extent );

   if( isMethod( "onExpanded" ) )
      Con::executef( this, "onExpanded" );
}

void GuiRolloutCtrl::instantCollapse()
{
   mAnimateDestHeight = mHeader.extent.y;
   mCollapsing = false;
   mIsExpanded = false;
   mIsAnimating = false;
   resize( getPosition() + mHeader.point, mHeader.extent );
   
   if( isMethod( "onCollapsed" ) )
      Con::executef( this, "onCollapsed" );
}

void GuiRolloutCtrl::toggleExpanded( bool instant )
{   
   if ( mIsExpanded )
   {
      if ( instant )
         instantCollapse();
      else
         collapse();
   }
   else
   {
      if ( instant )
         instantExpand();
      else
         expand();
   }
}

void GuiRolloutCtrl::childResized( GuiControl *child )
{
    Parent::childResized( child );

    calculateHeights();

    // While we are animating we are constantly resizing our children
    // and therefore need to ignore this call to 'instantExpand' which would
    // halt the animation in some crappy intermediate stage.
    if ( mIsExpanded && !mIsAnimating )
    {
       mNotifyChildrenResized = false;
       instantExpand();
       mNotifyChildrenResized = true;
    }
}

//-----------------------------------------------------------------------------
// Control Sizing Animation Functions
//-----------------------------------------------------------------------------
void GuiRolloutCtrl::animateTo( S32 height )
{
   // We do nothing if we're already animating
   if( mIsAnimating )
      return;

   bool collapsing = (bool)( getHeight() > height );

   // If we're already at the destination height, bail
   if ( getHeight() >= height && !collapsing )
   {
      mIsExpanded = true;
      return;
   }

   // If we're already at the destination height, bail
   if ( getHeight() <= height && collapsing )
   {
      mIsExpanded = false;
      return;
   }

   // Set destination height
   mAnimateDestHeight = height;

   // Set Animation Mode
   mCollapsing = collapsing;

   // Set Animation Step (Increment)
   if ( collapsing )
      mAnimateStep = (S32)mFloor( (F32)( getHeight() - height ) / 3.f );
   else
      mAnimateStep = (S32)mFloor( (F32)( height - getHeight() ) / 3.f );

   // Start our animation
   mIsAnimating = true;
}

void GuiRolloutCtrl::processTick()
{
   // We do nothing here if we're NOT animating
   if ( !mIsAnimating )
      return;

   // Sanity check to fix non collapsing panels.
   if ( mAnimateStep == 0 )
      mAnimateStep = 1;

   S32 newHeight = getHeight();
   // We're collapsing ourself down (Hiding our contents)
   if( mCollapsing )
   {
      if ( newHeight < mAnimateDestHeight )
         newHeight = mAnimateDestHeight;
      else if ( ( newHeight - mAnimateStep ) < mAnimateDestHeight )
         newHeight = mAnimateDestHeight;

      if ( newHeight == mAnimateDestHeight )
         mIsAnimating = false;
      else
         newHeight -= mAnimateStep;

      if( !mIsAnimating )
	  {
         mIsExpanded = false;
	  }
   }
   else // We're expanding ourself (Showing our contents)
   {
      if ( newHeight > mAnimateDestHeight )
         newHeight = mAnimateDestHeight;
      else if ( ( newHeight + mAnimateStep ) > mAnimateDestHeight )
         newHeight = mAnimateDestHeight;

      if ( newHeight == mAnimateDestHeight )
         mIsAnimating = false;
      else
         newHeight += mAnimateStep;

      if ( !mIsAnimating )
         mIsExpanded = true;
   }

   if ( newHeight != getHeight() )
      setHeight( newHeight );

   if ( !mIsAnimating )
   {
      if( mCollapsing && isMethod( "onCollapsed" ) )
         Con::executef( this, "onCollapsed" );
      else if( !mCollapsing && isMethod( "onExpanded" ) )
         Con::executef( this, "onExpanded" );

      calculateHeights();
   }

   GuiControl* parent = getParent();
   if ( parent )
   {
      parent->childResized( this );
      // if our parent's parent is a scroll control, scrollvisible.
      GuiScrollCtrl* scroll = dynamic_cast<GuiScrollCtrl*>( parent->getParent() );
      if ( scroll )
      {
         scroll->scrollRectVisible( getBounds() );
      }
   }
}

//-----------------------------------------------------------------------------
// Control Rendering
//-----------------------------------------------------------------------------
void GuiRolloutCtrl::onRender( Point2I offset, const RectI &updateRect )
{
   if( !mProfile || mProfile->mFont == NULL )
      return;

   // Calculate actual world bounds for rendering
   RectI worldBounds( offset, getExtent() );

   // if opaque, fill the update rect with the fill color
   if ( mProfile->mOpaque )
      GFX->getDrawUtil()->drawRectFill( worldBounds, mProfile->mFillColor );

   if ( mProfile->mBitmapArrayRects.size() >= NumBitmaps )
   {
      GFX->getDrawUtil()->clearBitmapModulation();

      // Draw Rollout From Skin
      if ( !mIsExpanded && !mIsAnimating )
         renderFixedBitmapBordersFilled( worldBounds, 1, mProfile );
      else if ( mHideHeader )
         renderSizableBitmapBordersFilledIndex( worldBounds, MidPageLeft, mProfile );
      else
         renderSizableBitmapBordersFilledIndex( worldBounds, TopLeftHeader, mProfile );
   }

   if ( !(mIsExpanded && mHideHeader ) )
   {
      // Draw Caption ( Vertically Centered )
      ColorI currColor;
      GFX->getDrawUtil()->getBitmapModulation( &currColor );
      Point2I textPosition = mHeader.point + offset + mProfile->mTextOffset;
      GFX->getDrawUtil()->setBitmapModulation( mProfile->mFontColor );
      renderJustifiedText( textPosition, mHeader.extent, mCaption );
      GFX->getDrawUtil()->setBitmapModulation( currColor );
   }

   // If we're collapsed we contain the first child as our content
   // thus we don't render it when collapsed.  but to support modified
   // rollouts with custom header buttons etc we still render our other
   // children. -JDD
   GuiControl *pChild = dynamic_cast<GuiControl*>( at(0) );
   if ( pChild )
   {
      if ( !mIsExpanded && !mIsAnimating && pChild->isVisible() )
	  {
         pChild->setVisible( false );
	  }
      else if ( (mIsExpanded || mIsAnimating) && !pChild->isVisible() )
	  {
         pChild->setVisible( true );
	  }
   }
   renderChildControls( offset, updateRect );

   // Render our border should we have it specified in our profile.
   renderBorder(worldBounds, mProfile);
}

//-----------------------------------------------------------------------------
// Console
//-----------------------------------------------------------------------------
ConsoleMethod( GuiRolloutCtrl, isExpanded, bool, 2, 2, "isExpanded(); - returns true/false")
{
   return object->isExpanded();
}

ConsoleMethod( GuiRolloutCtrl, collapse, void, 2, 2, "%rollout.collapse();")
{
   object->collapse();
}

ConsoleMethod( GuiRolloutCtrl, expand, void, 2, 2, "%rollout.expand();")
{
   object->expand();
}

ConsoleMethod( GuiRolloutCtrl, toggleCollapse, void, 2, 2, "%rollout.toggle();")
{
   if( object->isExpanded() )
      object->collapse();
   else
	  object->expand();
}

ConsoleMethod( GuiRolloutCtrl, instantCollapse, void, 2, 2, "%rollout.collapse();")
{
   object->instantCollapse();
}

ConsoleMethod( GuiRolloutCtrl, instantExpand, void, 2, 2, "%rollout.expand();")
{
   object->instantExpand();
}

ConsoleMethod( GuiRolloutCtrl, toggleExpanded, void, 2, 3, "toggleExpanded( bool instant )" )
{
   object->toggleExpanded( dAtob(argv[2]) );   
}

ConsoleMethod( GuiRolloutCtrl, sizeToContents, void, 2, 2, "%rollout.sizeToContents()")
{
   object->sizeToContents();
}
