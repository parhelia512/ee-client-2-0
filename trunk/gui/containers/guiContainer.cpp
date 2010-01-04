//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/containers/guiContainer.h"

#include "gui/containers/guiPanel.h"
#include "console/consoleTypes.h"

//-----------------------------------------------------------------------------
// GuiContainer
//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT( GuiContainer );

static const EnumTable::Enums dockEnums[] =
{
   { Docking::dockNone,         "None"   },
   { Docking::dockClient,       "Client" },
   { Docking::dockTop,          "Top"    },
   { Docking::dockBottom,       "Bottom" },
   { Docking::dockLeft,         "Left"   },
   { Docking::dockRight,        "Right"  }
};
static const EnumTable gDockingTable(6, &dockEnums[0]);

GuiContainer::GuiContainer()
{
   mUpdateLayout = false;
   mValidDockingMask =  Docking::dockNone | Docking::dockBottom | 
      Docking::dockTop  | Docking::dockClient | 
      Docking::dockLeft | Docking::dockRight;

}

GuiContainer::~GuiContainer()
{
}

void GuiContainer::initPersistFields()
{
   Con::setIntVariable("$DOCKING_NONE",   Docking::dockNone);
   Con::setIntVariable("$DOCKING_CLIENT", Docking::dockClient);
   Con::setIntVariable("$DOCKING_TOP",    Docking::dockTop);
   Con::setIntVariable("$DOCKING_BOTTOM", Docking::dockBottom);
   Con::setIntVariable("$DOCKING_LEFT",   Docking::dockLeft);
   Con::setIntVariable("$DOCKING_RIGHT",  Docking::dockRight);

   addProtectedField("Docking",  TypeEnum,   Offset(mSizingOptions.mDocking, GuiContainer), &setDockingField, &defaultProtectedGetFn, 1, &gDockingTable, "");
   addField("Margin",         TypeRectSpacingI, Offset(mSizingOptions.mPadding, GuiContainer));
   addField("Padding",        TypeRectSpacingI, Offset(mSizingOptions.mInternalPadding, GuiContainer));
   addField("AnchorTop",      TypeBool,          Offset(mSizingOptions.mAnchorTop, GuiContainer));
   addField("AnchorBottom",   TypeBool,          Offset(mSizingOptions.mAnchorBottom, GuiContainer));
   addField("AnchorLeft",     TypeBool,          Offset(mSizingOptions.mAnchorLeft, GuiContainer));
   addField("AnchorRight",    TypeBool,          Offset(mSizingOptions.mAnchorRight, GuiContainer));

   Parent::initPersistFields();
}

void GuiContainer::onChildAdded(GuiControl* control)
{
   Parent::onChildAdded( control );
   setUpdateLayout();
}

void GuiContainer::onChildRemoved(GuiControl* control)
{
   Parent::onChildRemoved( control );
   setUpdateLayout();
}

bool GuiContainer::reOrder(SimObject* obj, SimObject* target)
{
   if ( !Parent::reOrder(obj, target) )
      return false;

   setUpdateLayout();
   return true;
}


bool GuiContainer::resize( const Point2I &newPosition, const Point2I &newExtent )
{
   RectI oldBounds = getBounds();
   Point2I minExtent = getMinExtent();

   if( !Parent::resize( newPosition, newExtent ) )
      return false;
   
   RectI clientRect = getClientRect();
   layoutControls( clientRect );

   GuiControl *parent = getParent();
   S32 docking = getDocking();
   if( parent && docking != Docking::dockNone && docking != Docking::dockInvalid )
      setUpdateLayout( updateParent );

   return true;
}

void GuiContainer::addObject(SimObject *obj)
{
   Parent::addObject(obj);

   setUpdateLayout();
}

void GuiContainer::removeObject(SimObject *obj)
{
   Parent::removeObject(obj);

   setUpdateLayout();
}


/// GuiContainer deals with parentResized calls differently than GuiControl.  It will
/// update the layout for all of it's non-docked child controls.  parentResized calls
/// on the child controls will be handled by their default functions, but for our
/// purposes we want at least our immediate children to use the anchors that they have
/// set on themselves. - JDD [9/20/2006]
void GuiContainer::parentResized(const RectI &oldParentRect, const RectI &newParentRect)
{
	//if(!mCanResize)
	//	return;

   // If it's a control that specifies invalid docking, we'll just treat it as an old GuiControl
   if( getDocking() & Docking::dockInvalid || getDocking() & Docking::dockNone)
      return Parent::parentResized( oldParentRect, newParentRect );

   S32 deltaX = newParentRect.extent.x - oldParentRect.extent.x;
   S32 deltaY = newParentRect.extent.y - oldParentRect.extent.y;

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

void GuiContainer::childResized(GuiControl *child)
{
   Parent::childResized( child );
   setUpdateLayout();
}

bool GuiContainer::layoutControls(  RectI &clientRect )
{

   // This variable is set to the first 'Client' docking 
   //  control that is found.  We defer client docking until
   //  after all other docks have been made since it will consume
   //  the remaining client area available.
   GuiContainer *clientDocking = NULL;

   // Iterate over all children and perform docking
   iterator nI = begin();
   for( ; nI != end(); nI++ )
   {
      // Layout Content with proper docking (Client Default)
      GuiControl *control = static_cast<GuiControl*>(*nI);
      
      // If we're invisible we don't get counted in docking
      if( control == NULL || !control->isVisible() )
         continue;

	  S32 dockingMode = Docking::dockNone;
	  GuiContainer *container = dynamic_cast<GuiContainer*>(control);
	  if( container != NULL )
		  dockingMode = container->getDocking();
     else
        continue;

     // See above note about clientDocking pointer
     if( dockingMode & Docking::dockClient && clientDocking == NULL )
        clientDocking = container;

      // Dock Appropriately
     if( !(dockingMode & Docking::dockClient) )
        dockControl( container, dockingMode, clientRect );
   }

   // Do client dock
   if( clientDocking != NULL )
      dockControl( clientDocking, Docking::dockClient, clientRect );

   return true;
}
bool GuiContainer::dockControl( GuiContainer *control, S32 dockingMode, RectI &clientRect )
{

   if( !control )
      return false;

   // Make sure this class support docking of this type
   if( !(dockingMode & getValidDockingMask()))
      return false;

   // If our client rect has run out of room, we can't dock any more
   if( !clientRect.isValidRect() )
      return false;

   // Dock Appropriately
   RectI dockRect;
   RectSpacingI rectShrinker;
   ControlSizing sizingOptions = control->getSizingOptions();
   switch( dockingMode )
   {
   case Docking::dockClient:

      // Inset by padding 
      sizingOptions.mPadding.insetRect(clientRect);

      // Dock to entirety of client rectangle
      control->resize( clientRect.point, clientRect.extent );

      // Remove Client Rect, can only have one client dock
      clientRect.set(0,0,0,0);
      break;
   case Docking::dockTop:         

      dockRect = clientRect;
      dockRect.extent.y = getMin( control->getHeight() + sizingOptions.mPadding.top + sizingOptions.mPadding.bottom , clientRect.extent.y );

      // Subtract our rect
      clientRect.point.y += dockRect.extent.y;
      clientRect.extent.y -= dockRect.extent.y;

      // Inset by padding 
      sizingOptions.mPadding.insetRect(dockRect);

      // Resize
      control->resize( dockRect.point, dockRect.extent );

      break;
   case Docking::dockBottom:

      dockRect = clientRect;
      dockRect.extent.y = getMin( control->getHeight() + sizingOptions.mPadding.top + sizingOptions.mPadding.bottom, clientRect.extent.y );
      dockRect.point.y += clientRect.extent.y - dockRect.extent.y;

      // Subtract our rect
      clientRect.extent.y -= dockRect.extent.y;

      // Inset by padding 
      sizingOptions.mPadding.insetRect(dockRect);

      // Resize
      control->resize( dockRect.point, dockRect.extent );

      break;
   case Docking::dockLeft:

      dockRect = clientRect;
      dockRect.extent.x = getMin( control->getWidth() + sizingOptions.mPadding.left + sizingOptions.mPadding.right, clientRect.extent.x );

      // Subtract our rect
      clientRect.point.x += dockRect.extent.x;
      clientRect.extent.x -= dockRect.extent.x;

      // Inset by padding 
      sizingOptions.mPadding.insetRect(dockRect);

      // Resize
      control->resize( dockRect.point, dockRect.extent );

      break;
   case Docking::dockRight:

      dockRect = clientRect;
      dockRect.extent.x = getMin( control->getWidth() + sizingOptions.mPadding.left + sizingOptions.mPadding.right, clientRect.extent.x );
      dockRect.point.x += clientRect.extent.x - dockRect.extent.x;

      // Subtract our rect
      clientRect.extent.x -= dockRect.extent.x;

      // Inset by padding 
      sizingOptions.mPadding.insetRect(dockRect);

      // Resize
      control->resize( dockRect.point, dockRect.extent );

      break;
   case Docking::dockNone:
      control->setUpdateLayout();
      break;
   }

   return true;
}


// Update a Controls Anchor based on a delta sizing of it's parents extent
// This function should return true if the control was changed in size or position at all
bool GuiContainer::anchorControl( GuiControl *control, const Point2I &deltaParentExtent )
{
   GuiContainer *container = dynamic_cast<GuiContainer*>( control );
   if( !control || !container )
      return false;

   // If we're docked, we don't anchor to anything
   if( (container->getDocking() & Docking::dockAny) || !(container->getDocking() & Docking::dockInvalid)  )
      return false;

   if( deltaParentExtent.isZero() )
      return false;

   RectI oldRect = control->getBounds();
   RectI newRect = control->getBounds();

   F32 deltaBottom = mSizingOptions.mAnchorBottom ? (F32)deltaParentExtent.y : 0.0f;
   F32 deltaRight = mSizingOptions.mAnchorRight ? (F32)deltaParentExtent.x : 0.0f;
   F32 deltaLeft = mSizingOptions.mAnchorLeft ? 0.0f : (F32)deltaParentExtent.x;
   F32 deltaTop = mSizingOptions.mAnchorTop ? 0.0f : (F32)deltaParentExtent.y;

   // Apply Delta's to newRect
   newRect.point.x += (S32)deltaLeft;
   newRect.extent.x += (S32)(deltaRight - deltaLeft);
   newRect.point.y += (S32)deltaTop;
   newRect.extent.y += (S32)(deltaBottom - deltaTop);

   Point2I minExtent = control->getMinExtent();
   // Only resize if our minExtent is satisfied with it.
   if( !( newRect.extent.x >= control->getMinExtent().x && newRect.extent.y >= control->getMinExtent().y ) )
      return false;

   if( newRect.point == oldRect.point && newRect.extent == oldRect.extent )
      return false;

   // Finally Size the control
   control->resize( newRect.point, newRect.extent );

   // We made changes
   return true;
}

void GuiContainer::onPreRender()
{
   if( mUpdateLayout == updateNone )
      return;

   RectI clientRect = getClientRect();
   if( mUpdateLayout & updateSelf )
      layoutControls( clientRect );

   GuiContainer *parent = dynamic_cast<GuiContainer*>( getParent() );
   if( parent && ( mUpdateLayout & updateParent ) )
      parent->setUpdateLayout();

   // Always set AFTER layoutControls call to prevent recursive calling of layoutControls - JDD
   mUpdateLayout = updateNone;

   Parent::onPreRender();
}

const RectI GuiContainer::getClientRect()
{
   RectI resRect = RectI( Point2I(0,0), getExtent() );

   // Inset by padding
   mSizingOptions.mInternalPadding.insetRect( resRect ); 

   return resRect;
}

void GuiContainer::setDocking( S32 docking )
{
   mSizingOptions.mDocking = docking; 
   setUpdateLayout( updateParent );
}
