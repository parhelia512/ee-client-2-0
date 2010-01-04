//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/core/guiScriptNotifyControl.h"
#include "console/consoleTypes.h"
//------------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiScriptNotifyCtrl);

GuiScriptNotifyCtrl::GuiScriptNotifyCtrl()
{
   mOnChildAdded = false;
   mOnChildRemoved = false;
   mOnResize = false;
   mOnChildResized = false;
   mOnParentResized = false;
}

GuiScriptNotifyCtrl::~GuiScriptNotifyCtrl()
{
}

void GuiScriptNotifyCtrl::initPersistFields()
{
   // Callbacks Group
   addGroup("Callbacks");
   addField("onChildAdded", TypeBool, Offset( mOnChildAdded, GuiScriptNotifyCtrl ) );
   addField("onChildRemoved", TypeBool, Offset( mOnChildRemoved, GuiScriptNotifyCtrl ) );
   addField("onChildResized", TypeBool, Offset( mOnChildResized, GuiScriptNotifyCtrl ) );
   addField("onParentResized", TypeBool, Offset( mOnParentResized, GuiScriptNotifyCtrl ) );
   addField("onResize", TypeBool, Offset( mOnResize, GuiScriptNotifyCtrl ) );
   addField("onLoseFirstResponder", TypeBool, Offset( mOnLoseFirstResponder, GuiScriptNotifyCtrl ) );
   addField("onGainFirstResponder", TypeBool, Offset( mOnGainFirstResponder, GuiScriptNotifyCtrl ) );
   endGroup("Callbacks");

   Parent::initPersistFields();
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //
void GuiScriptNotifyCtrl::onChildAdded( GuiControl *child )
{
   Parent::onChildAdded( child );

   // Call Script.
   if( mOnChildAdded && isMethod( "onChildAdded" ) )
      Con::executef(this, "onChildAdded", child->getIdString() );
}

void GuiScriptNotifyCtrl::onChildRemoved( GuiControl *child )
{
   Parent::onChildRemoved( child );

   // Call Script.
   if( mOnChildRemoved && isMethod( "onChildRemoved" ) )
      Con::executef(this, "onChildRemoved", child->getIdString() ); 
}
//----------------------------------------------------------------

bool GuiScriptNotifyCtrl::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   if( !Parent::resize( newPosition, newExtent ) )
      return false;

   // Call Script.
   if( mOnResize && isMethod( "onResize" ) )
      Con::executef(this, "onResize" );

   return true;
}

void GuiScriptNotifyCtrl::childResized(GuiScriptNotifyCtrl *child)
{
   Parent::childResized( child );

   // Call Script.
   if( mOnChildResized && isMethod( "onChildResized" ) )
      Con::executef(this, "onChildResized", child->getIdString() );
}

void GuiScriptNotifyCtrl::parentResized(const RectI &oldParentRect, const RectI &newParentRect)
{
   Parent::parentResized( oldParentRect, newParentRect );

   // Call Script.
   if( mOnParentResized && isMethod( "onParentResized" ) )
      Con::executef(this, "onParentResized" );

}
 
void GuiScriptNotifyCtrl::onLoseFirstResponder()
{
   Parent::onLoseFirstResponder();

   // Call Script.
   if( mOnLoseFirstResponder && isMethod( "onLoseFirstResponder" ) )
      Con::executef(this, "onLoseFirstResponder" );

}

void GuiScriptNotifyCtrl::setFirstResponder( GuiControl* firstResponder )
{
   Parent::setFirstResponder( firstResponder );

   // Call Script.
   if( mOnGainFirstResponder && isFirstResponder() && isMethod( "onGainFirstResponder" ) )
      Con::executef(this, "onGainFirstResponder" );

}

void GuiScriptNotifyCtrl::setFirstResponder()
{
   Parent::setFirstResponder();

   // Call Script.
   if( mOnGainFirstResponder && isFirstResponder() && isMethod( "onGainFirstResponder" ) )
      Con::executef(this, "onGainFirstResponder" );
}

void GuiScriptNotifyCtrl::onMessage(GuiScriptNotifyCtrl *sender, S32 msg)
{
   Parent::onMessage( sender, msg );
}

void GuiScriptNotifyCtrl::onDialogPush()
{
   Parent::onDialogPush();
}

void GuiScriptNotifyCtrl::onDialogPop()
{
   Parent::onDialogPop();
}


//void GuiScriptNotifyCtrl::onMouseUp(const GuiEvent &event)
//{
//}
//
//void GuiScriptNotifyCtrl::onMouseDown(const GuiEvent &event)
//{
//}
//
//void GuiScriptNotifyCtrl::onMouseMove(const GuiEvent &event)
//{
//}
//
//void GuiScriptNotifyCtrl::onMouseDragged(const GuiEvent &event)
//{
//}
//
//void GuiScriptNotifyCtrl::onMouseEnter(const GuiEvent &)
//{
//}
//
//void GuiScriptNotifyCtrl::onMouseLeave(const GuiEvent &)
//{
//}
//
//bool GuiScriptNotifyCtrl::onMouseWheelUp( const GuiEvent &event )
//{
//}
//
//bool GuiScriptNotifyCtrl::onMouseWheelDown( const GuiEvent &event )
//{
//}
//
//void GuiScriptNotifyCtrl::onRightMouseDown(const GuiEvent &)
//{
//}
//
//void GuiScriptNotifyCtrl::onRightMouseUp(const GuiEvent &)
//{
//}
//
//void GuiScriptNotifyCtrl::onRightMouseDragged(const GuiEvent &)
//{
//}
//
//void GuiScriptNotifyCtrl::onMiddleMouseDown(const GuiEvent &)
//{
//}
//
//void GuiScriptNotifyCtrl::onMiddleMouseUp(const GuiEvent &)
//{
//}
//
//void GuiScriptNotifyCtrl::onMiddleMouseDragged(const GuiEvent &)
//{
//}
//void GuiScriptNotifyCtrl::onMouseDownEditor(const GuiEvent &event, Point2I offset)
//{
//}
//void GuiScriptNotifyCtrl::onRightMouseDownEditor(const GuiEvent &event, Point2I offset)
//{
//}

//bool GuiScriptNotifyCtrl::onKeyDown(const GuiEvent &event)
//{
//  if ( Parent::onKeyDown( event ) )
//     return true;
//}
//
//bool GuiScriptNotifyCtrl::onKeyRepeat(const GuiEvent &event)
//{
//   // default to just another key down.
//   return onKeyDown(event);
//}
//
//bool GuiScriptNotifyCtrl::onKeyUp(const GuiEvent &event)
//{
//  if ( Parent::onKeyUp( event ) )
//     return true;
//}
