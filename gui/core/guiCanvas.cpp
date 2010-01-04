//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/core/guiCanvas.h"

#include "console/console.h"
#include "platform/profiler.h"
#include "platform/event.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"
#include "gui/core/guiTypes.h"
#include "gui/core/guiControl.h"
#include "console/consoleTypes.h"
#include "gfx/screenshot.h"

#ifndef TORQUE_TGB_ONLY
#  include "sceneGraph/sceneObject.h"
#endif

#include "gfx/gfxInit.h"
#include "core/util/journal/process.h"

#ifdef TORQUE_GFX_STATE_DEBUG
#  include "gfx/gfxDebugStateTracker.h"
#endif

IMPLEMENT_CONOBJECT(GuiCanvas);

ColorI gCanvasClearColor( 255, 0, 255 ); ///< For GFX->clear

/// This is triggered by the canvas before it starts the
/// rendering process which renders *everything*.
Signal<void()> gCanvasRenderSignal;

extern InputModifiers convertModifierBits(const U32 in);

//-----------------------------------------------------------------------------

GuiCanvas::GuiCanvas(): GuiControl(),
                        mCursorEnabled(true),
                        mForceMouseToGUI(false),
                        mClampTorqueCursor(true),
                        mShowCursor(true),
                        mLastCursorEnabled(false),
                        mMouseControl(NULL),
                        mMouseCapturedControl(NULL),
                        mMouseControlClicked(false),
                        mMouseButtonDown(false),
                        mMouseRightButtonDown(false),
                        mMouseMiddleButtonDown(false),
                        mDefaultCursor(NULL),
                        mLastCursor(NULL),
                        mLastCursorPt(0,0),
                        mCursorPt(0,0),
                        mLastMouseClickCount(0),
                        mLastMouseDownTime(0),
                        mPrevMouseTime(0),
                        mRenderFront(false),
                        mHoverControl(NULL),
                        mHoverPositionSet(false),
                        mHoverLeftControlTime(0),
                        mLeftMouseLast(false),
                        mMiddleMouseLast(false),
                        mRightMouseLast(false),
                        mPlatformWindow(NULL)
{
   setBounds(0, 0, 640, 480);
   mAwake = true;

   mHoverControlStart = Platform::getRealMilliseconds();
   mHoverPosition = getCursorPos();

   mFences = NULL;
   mNextFenceIdx = -1;

#ifndef _XBOX
   mNumFences = Con::getIntVariable( "$pref::Video::defaultFenceCount", 0 );
#else
   mNumFences = 0;
#endif

#ifdef TORQUE_DEMO_PURCHASE
   mPurchaseScreen = NULL;
#endif
}

GuiCanvas::~GuiCanvas()
{
   SAFE_DELETE(mPlatformWindow);
   SAFE_DELETE_ARRAY( mFences );

#ifdef TORQUE_DEMO_PURCHASE
   if (mPurchaseScreen)
      SAFE_DELETE(mPurchaseScreen);
#endif
}

//------------------------------------------------------------------------------

bool GuiCanvas::setProtectedNumFences( void *obj, const char *data )
{
   GuiCanvas *canvas = reinterpret_cast<GuiCanvas *>( obj );
   canvas->mNumFences = dAtoi( data );
   canvas->setupFences();
   
   return false;
}

void GuiCanvas::initPersistFields()
{
   //add the canSaveDynamicFields property:
   addGroup("Canvas Rendering");
   addProtectedField( "numFences", TypeS32, Offset( mNumFences, GuiCanvas ), &setProtectedNumFences, &defaultProtectedGetFn, "The number of GFX fences to use." );
   endGroup("Canvas Rendering");

   Parent::initPersistFields();
}

//------------------------------------------------------------------------------

bool GuiCanvas::onAdd()
{
   // ensure that we have a cursor
   setCursor(dynamic_cast<GuiCursor*>(Sim::findObject("DefaultCursor")));
   
   // Enumerate things for GFX before we have an active device.
   GFXInit::enumerateAdapters();

   // Create a device.
   GFXAdapter *a = GFXInit::getBestAdapterChoice();

   // Do we have a global device already? (This is the site if you want
   // to start rendering to multiple devices simultaneously)
   GFXDevice *newDevice = GFX;
   if(newDevice == NULL)
      newDevice = GFXInit::createDevice(a);

   newDevice->setAllowRender( false );

   // Initialize the window...
   GFXVideoMode vm = GFXInit::getInitialVideoMode();

   if (a && a->mType != NullDevice)
   {
      mPlatformWindow = WindowManager->createWindow(newDevice, vm);

      // Set a minimum on the window size so people can't break us by resizing tiny.
      mPlatformWindow->setMinimumWindowSize(Point2I(640,480));

      // Now, we have to hook in our event callbacks so we'll get
      // appropriate events from the window.
      mPlatformWindow->resizeEvent .notify(this, &GuiCanvas::handleResize);
      mPlatformWindow->appEvent    .notify(this, &GuiCanvas::handleAppEvent);
      mPlatformWindow->displayEvent.notify(this, &GuiCanvas::handlePaintEvent);
      mPlatformWindow->setInputController( dynamic_cast<IProcessInput*>(this) );
   }

   // Need to get painted, too! :)
   Process::notify(this, &GuiCanvas::paint, PROCESS_RENDER_ORDER);
   
   // Set up the fences
   setupFences();

   // Make sure we're able to render.
   newDevice->setAllowRender( true );

   // Propagate add to parents.
   // CodeReview - if GuiCanvas fails to add for whatever reason, what happens to
   // all the event registration above?
   bool parentRet = Parent::onAdd();

   // Define the menu bar for this canvas (if any)
   Con::executef(this, "onCreateMenu");

#ifdef TORQUE_DEMO_PURCHASE
   mPurchaseScreen = new PurchaseScreen;
   mPurchaseScreen->init();

   mLastPurchaseHideTime = 0;
#endif

   return parentRet;
}

void GuiCanvas::onRemove()
{
#ifdef TORQUE_DEMO_PURCHASE
   if (mPurchaseScreen && mPurchaseScreen->isAwake())
      removeObject(mPurchaseScreen);
#endif

   // And the process list
   Process::remove(this, &GuiCanvas::paint);

   // Destroy the menu bar for this canvas (if any)
   Con::executef(this, "onDestroyMenu");

   Parent::onRemove();
}

void GuiCanvas::setWindowTitle(const char *newTitle)
{
   if (mPlatformWindow)
      mPlatformWindow->setCaption(newTitle);
}

void GuiCanvas::handleResize( WindowId did, S32 width, S32 height )
{
   // Notify the scripts
   if ( isMethod( "onResize" ) )
      Con::executef( this, "onResize", Con::getIntArg( width ), Con::getIntArg( height ) );
}

void GuiCanvas::handlePaintEvent(WindowId did)
{
   renderFrame(false);
}

void GuiCanvas::handleAppEvent( WindowId did, S32 event )
{
   // Notify script if we gain or lose focus.
   if(event == LoseFocus)
   {
      if(isMethod("onLoseFocus"))
         Con::executef(this, "onLoseFocus");
   }

   if(event == GainFocus)
   {
      if(isMethod("onGainFocus"))
         Con::executef(this, "onGainFocus");
   }

   if(event == WindowClose || event == WindowDestroy)
   {
#ifdef TORQUE_DEMO_PURCHASE
      showPurchaseScreen(true, "exit", true);
      return;
#endif

      if(isMethod("onWindowClose"))
      {
         // First see if there is a method on this window to handle 
         //  it's closure
         Con::executef(this,"onWindowClose");
      }
      else if(Con::isFunction("onWindowClose"))
      {
         // otherwise check to see if there is a global function handling it
         Con::executef("onWindowClose", getIdString());
      }
      else
      {
         // Else just shutdown
         Process::requestShutdown();
      }
   }
}

Point2I GuiCanvas::getWindowSize()
{
   // CodeReview Asserting on this breaks previous logic
   // and code assumptions.  It seems logical that we would
   // handle this and return an error value rather than implementing
   // if(!mPlatformWindow) whenever we need to call getWindowSize.
   // This should help keep our API error free and easy to use, while
   // cutting down on code duplication for sanity checking.  [5/5/2007 justind]
   if( !mPlatformWindow )
      return Point2I(-1,-1);

   return mPlatformWindow->getClientExtent();
}

void GuiCanvas::enableKeyboardTranslation()
{
   AssertISV(mPlatformWindow, "GuiCanvas::enableKeyboardTranslation - no window present!");
   mPlatformWindow->setKeyboardTranslation(true);
}

void GuiCanvas::disableKeyboardTranslation()
{
   AssertISV(mPlatformWindow, "GuiCanvas::disableKeyboardTranslation - no window present!");
   mPlatformWindow->setKeyboardTranslation(false);
}

void GuiCanvas::setNativeAcceleratorsEnabled( bool enabled )
{
   AssertISV(mPlatformWindow, "GuiCanvas::setNativeAcceleratorsEnabled - no window present!");
   mPlatformWindow->setAcceleratorsEnabled( enabled );
}

void GuiCanvas::setForceMouseToGUI(bool onOff)
{
   mForceMouseToGUI = onOff;
}

void GuiCanvas::setClampTorqueCursor(bool onOff)
{
   mClampTorqueCursor = onOff;
}

void GuiCanvas::setCursor(GuiCursor *curs)
{
   mDefaultCursor = curs;
}

void GuiCanvas::setCursorON(bool onOff)
{
   mCursorEnabled = onOff;
   if(!mCursorEnabled)
      mMouseControl = NULL;
}

Point2I GuiCanvas::getCursorPos()
{
   Point2I p( 0, 0 );
   
   if( mPlatformWindow )
      mPlatformWindow->getCursorPosition( p );
      
   return p;
}

void GuiCanvas::setCursorPos(const Point2I &pt)   
{
   AssertISV(mPlatformWindow, "GuiCanvas::setCursorPos - no window present!");
   mPlatformWindow->setCursorPosition( pt.x, pt.y );
}

void GuiCanvas::showCursor(bool state)
{ 
   mShowCursor = state;
   mPlatformWindow->setCursorVisible( state );
}

bool GuiCanvas::isCursorShown()
{
   if ( !mPlatformWindow->getCursorController() )
   {
      return mShowCursor;
   }

   return mPlatformWindow->isCursorVisible();
}

void GuiCanvas::addAcceleratorKey(GuiControl *ctrl, U32 index, U32 keyCode, U32 modifier)
{
   if (keyCode > 0 && ctrl)
   {
      AccKeyMap newMap;
      newMap.ctrl = ctrl;
      newMap.index = index;
      newMap.keyCode = keyCode;
      newMap.modifier = modifier;
      mAcceleratorMap.push_back(newMap);
   }
}

bool GuiCanvas::tabNext(void)
{
   GuiControl *ctrl = static_cast<GuiControl *>(last());
   if (ctrl)
   {
      //save the old
      GuiControl *oldResponder = mFirstResponder;

		GuiControl* newResponder = ctrl->findNextTabable(mFirstResponder);
      if ( !newResponder )
         newResponder = ctrl->findFirstTabable();

		if ( newResponder && newResponder != oldResponder )
		{
			newResponder->setFirstResponder();

         // CodeReview Can this get killed? Note tabPrev code. BJG - 3/25/07
//      	if ( oldResponder )
//         	oldResponder->onLoseFirstResponder();
         return true;
		}
   }
   return false;
}

bool GuiCanvas::tabPrev(void)
{
   GuiControl *ctrl = static_cast<GuiControl *>(last());
   if (ctrl)
   {
      //save the old
      GuiControl *oldResponder = mFirstResponder;

		GuiControl* newResponder = ctrl->findPrevTabable(mFirstResponder);
		if ( !newResponder )
         newResponder = ctrl->findLastTabable();

		if ( newResponder && newResponder != oldResponder )
		{
			newResponder->setFirstResponder();
	
         // CodeReview As with tabNext() above, looks like this can now go. DAW - 7/05/09
	      //if ( oldResponder )
	      //   oldResponder->onLoseFirstResponder();

         return true;
		}
   }
   return false;
}

bool GuiCanvas::processInputEvent(InputEventInfo &inputEvent)
{
	mConsumeLastInputEvent = true;
	// First call the general input handler (on the extremely off-chance that it will be handled):
	if (mFirstResponder &&  mFirstResponder->onInputEvent(inputEvent))
   {
	   return mConsumeLastInputEvent;  
		//return(true);
   }

   switch (inputEvent.deviceType)
   {
   case KeyboardDeviceType:
      return processKeyboardEvent(inputEvent);
      break;

   case GamepadDeviceType:
      return processGamepadEvent(inputEvent);
      break;

   case MouseDeviceType:
      if (mCursorEnabled || mForceMouseToGUI)
      {
         return processMouseEvent(inputEvent);
      }
      break;
   default:
      break;
   }

   return false;
}

bool GuiCanvas::processKeyboardEvent(InputEventInfo &inputEvent)
{
   mLastEvent.ascii    = inputEvent.ascii;
   mLastEvent.modifier = inputEvent.modifier;
   mLastEvent.keyCode  = inputEvent.objInst;

   // Combine left/right shift bits - if one shift modifier key
   // bit is set, then set the other one. This way we can simplify
   // our processing logic by treating the keys identically.
   U32 eventModifier = inputEvent.modifier;
   if(eventModifier & SI_SHIFT)
   {
      eventModifier |= SI_SHIFT;
   }
   if(eventModifier & SI_CTRL)
   {
      eventModifier |= SI_CTRL;
   }
   if(eventModifier & SI_ALT)
   {
      eventModifier |= SI_ALT;
   }

   if (inputEvent.action == SI_MAKE)
   {
      //see if we should now pass the event to the first responder
      if (mFirstResponder)
      {
         if(mFirstResponder->onKeyDown(mLastEvent))
             return mConsumeLastInputEvent;
      }

      //see if we should tab next/prev
      if ( isCursorON() && ( inputEvent.objInst == KEY_TAB ) )
      {
         if (size() > 0)
         {
            if (inputEvent.modifier & SI_SHIFT)
            {
               if(tabPrev())
                  return mConsumeLastInputEvent;
            }
            else if (inputEvent.modifier == 0)
            {
               if(tabNext())
                  return mConsumeLastInputEvent;
            }
         }
      }

      //if not handled, search for an accelerator
      for (U32 i = 0; i < mAcceleratorMap.size(); i++)
      {
         if ((U32)mAcceleratorMap[i].keyCode == (U32)inputEvent.objInst && (U32)mAcceleratorMap[i].modifier == eventModifier)
         {
            mAcceleratorMap[i].ctrl->acceleratorKeyPress(mAcceleratorMap[i].index);
            return mConsumeLastInputEvent;
         }
      }
   }
   else if(inputEvent.action == SI_BREAK)
   {
      if(mFirstResponder && mFirstResponder->onKeyUp(mLastEvent))
         return mConsumeLastInputEvent;

      //see if there's an accelerator
      for (U32 i = 0; i < mAcceleratorMap.size(); i++)
      {
         if ((U32)mAcceleratorMap[i].keyCode == (U32)inputEvent.objInst && (U32)mAcceleratorMap[i].modifier == eventModifier)
         {
            mAcceleratorMap[i].ctrl->acceleratorKeyRelease(mAcceleratorMap[i].index);
            return mConsumeLastInputEvent;
         }
      }
   }
   else if(inputEvent.action == SI_REPEAT)
   {
      //if not handled, search for an accelerator
      for (U32 i = 0; i < mAcceleratorMap.size(); i++)
      {
         if ((U32)mAcceleratorMap[i].keyCode == (U32)inputEvent.objInst && (U32)mAcceleratorMap[i].modifier == eventModifier)
         {
            mAcceleratorMap[i].ctrl->acceleratorKeyPress(mAcceleratorMap[i].index);
            return mConsumeLastInputEvent;
         }
      }

      if(mFirstResponder)
      {
		  bool ret = mFirstResponder->onKeyRepeat(mLastEvent);
		  return ret && mConsumeLastInputEvent;
      }
   }
   return false;
}

bool GuiCanvas::processMouseEvent(InputEventInfo &inputEvent)
{
   // [rene 09/09/10] This custom mouse cursor tracking that is happening here is bad.  It will frequently
   //    get ouf of step with where the cursor actually is.  We really should *not* track the cursor; it's
   //    just another thing that can/will go wrong.  Let the input system pass us absolute screen coordinates
   //    for every mouse event instead and work off that.
   //
   //    'mCursorPt' basically is an accumulation of errors and the number of bugs that have cropped up with
   //    the GUI clicking stuff where it is not supposed to are probably all to blame on this.
   
   // Need to query platform for specific things
   AssertISV(mPlatformWindow, "GuiCanvas::processMouseEvent - no window present!");
   PlatformCursorController *pController = mPlatformWindow->getCursorController();
   AssertFatal(pController != NULL, "GuiCanvas::processInputEvent - No Platform Controller Found")

      //copy the modifier into the new event
      mLastEvent.modifier = inputEvent.modifier;

   if(inputEvent.objType == SI_AXIS && 
      (inputEvent.objInst == SI_XAXIS || inputEvent.objInst == SI_YAXIS))
   {

      // Set the absolute position if we get an SI_MAKE on an axis
      if( inputEvent.objInst == SI_XAXIS )
      {
         if( inputEvent.action == SI_MAKE )
            mCursorPt.x = (S32)inputEvent.fValue;
         else if( inputEvent.action == SI_MOVE )
            mCursorPt.x += (S32)inputEvent.fValue;
            mCursorPt.x = getMax(0, getMin((S32)mCursorPt.x, getBounds().extent.x - 1));
      }
      else if( inputEvent.objInst == SI_YAXIS )
      {
         if( inputEvent.action == SI_MAKE )
            mCursorPt.y = (S32)inputEvent.fValue;
         else if( inputEvent.action == SI_MOVE )
            mCursorPt.y += (S32)inputEvent.fValue;
            mCursorPt.y = getMax(0, getMin((S32)mCursorPt.y, getBounds().extent.y - 1));
      }

      // Store new cursor position.
      mLastEvent.mousePoint.x = S32(mCursorPt.x);
      mLastEvent.mousePoint.y = S32(mCursorPt.y);

      // See if we need to invalidate a possible dbl click due to the cursor
      // moving too much.
      Point2F movement = mMouseDownPoint - mCursorPt;

      if ((mAbs((S32)movement.x) > pController->getDoubleClickWidth()) || (mAbs((S32)movement.y) > pController->getDoubleClickHeight() ) )
      {
         mLeftMouseLast   = false;
         mMiddleMouseLast = false;
         mRightMouseLast  = false;
      }

      if (mMouseButtonDown)
         rootMouseDragged(mLastEvent);
      else if (mMouseRightButtonDown)
         rootRightMouseDragged(mLastEvent);
      else if(mMouseMiddleButtonDown)
         rootMiddleMouseDragged(mLastEvent);
      else
         rootMouseMove(mLastEvent);
      return mConsumeLastInputEvent;
   }
   else if ( inputEvent.objInst == SI_ZAXIS
             || inputEvent.objInst == SI_RZAXIS )
   {
      mLastEvent.mousePoint.x = S32( mCursorPt.x );
      mLastEvent.mousePoint.y = S32( mCursorPt.y );
      mLastEvent.fval = inputEvent.fValue;

      if( inputEvent.objInst == SI_ZAXIS )
         mLastEvent.mouseAxis = 1;
      else
         mLastEvent.mouseAxis = 0;

      if ( inputEvent.fValue < 0.0f )
         return rootMouseWheelDown( mLastEvent );
      else
         return rootMouseWheelUp( mLastEvent );
   }
   else if(inputEvent.objType == SI_BUTTON)
   {
      //copy the cursor point into the event
      mLastEvent.mousePoint.x = S32(mCursorPt.x);
      mLastEvent.mousePoint.y = S32(mCursorPt.y);
      mMouseDownPoint = mCursorPt;
      
      if(inputEvent.objInst == KEY_BUTTON0) // left button
      {
         //see if button was pressed
         if (inputEvent.action == SI_MAKE)
         {
            U32 curTime = Platform::getVirtualMilliseconds();
            mNextMouseTime = curTime + mInitialMouseDelay;

            //if the last button pressed was the left...
            if (mLeftMouseLast)
            {
               //if it was within the double click time count the clicks
               if (curTime - mLastMouseDownTime <= pController->getDoubleClickTime())
                  mLastMouseClickCount++;
               else
                  mLastMouseClickCount = 1;
            }
            else
            {
               mLeftMouseLast = true;
               mLastMouseClickCount = 1;
            }

            mLastMouseDownTime = curTime;
            mLastEvent.mouseClickCount = mLastMouseClickCount;

            rootMouseDown(mLastEvent);
         }
         //else button was released
         else
         {
            mNextMouseTime = 0xFFFFFFFF;
            rootMouseUp(mLastEvent);
         }

         return mConsumeLastInputEvent;
      }
      else if(inputEvent.objInst == KEY_BUTTON1) // right button
      {
         if(inputEvent.action == SI_MAKE)
         {
            U32 curTime = Platform::getVirtualMilliseconds();

            //if the last button pressed was the right...
            if (mRightMouseLast)
            {
               //if it was within the double click time count the clicks
               if (curTime - mLastMouseDownTime <= pController->getDoubleClickTime())
                  mLastMouseClickCount++;
               else
                  mLastMouseClickCount = 1;
            }
            else
            {
               mRightMouseLast = true;
               mLastMouseClickCount = 1;
            }

            mLastMouseDownTime = curTime;
            mLastEvent.mouseClickCount = mLastMouseClickCount;

            rootRightMouseDown(mLastEvent);
         }
         else // it was a mouse up
            rootRightMouseUp(mLastEvent);

        return mConsumeLastInputEvent;
      }
      else if(inputEvent.objInst == KEY_BUTTON2) // middle button
      {
         if(inputEvent.action == SI_MAKE)
         {
            U32 curTime = Platform::getVirtualMilliseconds();

            //if the last button pressed was the right...
            if (mMiddleMouseLast)
            {
               //if it was within the double click time count the clicks
               if (curTime - mLastMouseDownTime <= pController->getDoubleClickTime())
                  mLastMouseClickCount++;
               else
                  mLastMouseClickCount = 1;
            }
            else
            {
               mMiddleMouseLast = true;
               mLastMouseClickCount = 1;
            }

            mLastMouseDownTime = curTime;
            mLastEvent.mouseClickCount = mLastMouseClickCount;

            rootMiddleMouseDown(mLastEvent);
         }
         else // it was a mouse up
            rootMiddleMouseUp(mLastEvent);

         return mConsumeLastInputEvent;
      }
   }
   return false;
}

bool GuiCanvas::processGamepadEvent(InputEventInfo &inputEvent)
{
   if (! mFirstResponder)
   {
      // early out, no first responder to receive gamepad input
      return false;
   }

   if (inputEvent.deviceInst >= MAX_GAMEPADS)
   {
      // early out, we only support the first MAX_GAMEPADS gamepads
      return false;
   }

   mLastEvent.keyCode = inputEvent.objInst;

   if (inputEvent.objType == SI_BUTTON)
   {
      switch (inputEvent.action)
      {
      case SI_MAKE:
         switch (inputEvent.objInst)
         {
         case SI_UPOV:
            return mFirstResponder->onGamepadAxisUp(mLastEvent);

         case SI_DPOV:
            return mFirstResponder->onGamepadAxisDown(mLastEvent);

         case SI_LPOV:
            return mFirstResponder->onGamepadAxisLeft(mLastEvent);

         case SI_RPOV:
            return mFirstResponder->onGamepadAxisRight(mLastEvent);

         default:
            return mFirstResponder->onGamepadButtonDown(mLastEvent);
         }
         break;

      case SI_BREAK:
         return mFirstResponder->onGamepadButtonUp(mLastEvent);

      default:
         return false;
      }
   }
   else if (inputEvent.objType == SI_AXIS)
   {
      F32 incomingValue = mFabs(inputEvent.fValue);
      static const F32 DEAD_ZONE = 0.5f;
      static const F32 MIN_CLICK_TIME = 500.0f;
      static const F32 MAX_CLICK_TIME = 1000.0f;
      static F32 xDecay [] = {1.0f, 1.0f, 1.0f, 1.0f};
      static F32 yDecay [] = {1.0f, 1.0f, 1.0f, 1.0f};
      static F32 zDecay [] = {1.0f, 1.0f, 1.0f, 1.0f};
      static U32 xLastClickTime [] = {0, 0, 0, 0};
      static U32 yLastClickTime [] = {0, 0, 0, 0};
      static U32 zLastClickTime [] = {0, 0, 0, 0};
      U32 curTime = Platform::getRealMilliseconds();
      F32 *decay;
      U32 *lastClickTime;

      switch (inputEvent.objInst)
      {
      case SI_ZAXIS:
      case XI_LEFT_TRIGGER:
      case XI_RIGHT_TRIGGER:
         decay = &zDecay[inputEvent.deviceInst];
         lastClickTime = &zLastClickTime[inputEvent.deviceInst];
         break;

      case SI_YAXIS:
      case XI_THUMBLY:
      case XI_THUMBRY:
         decay = &yDecay[inputEvent.deviceInst];
         lastClickTime = &yLastClickTime[inputEvent.deviceInst];
         break;

      case SI_XAXIS:
      case XI_THUMBLX:
      case XI_THUMBRX:
      default:
         decay = &xDecay[inputEvent.deviceInst];
         lastClickTime = &xLastClickTime[inputEvent.deviceInst];
         break;
      }

      if (incomingValue < DEAD_ZONE)
      {
         // early out, control movement is within the deadzone
         *decay = 1.0f;
         *lastClickTime = 0;
         return false;
      }

      // Rescales the input between 0.0 and 1.0
      incomingValue = (incomingValue - DEAD_ZONE) * (1.0f / (1.0f - DEAD_ZONE));

      F32 clickTime = MIN_CLICK_TIME + (MAX_CLICK_TIME - MIN_CLICK_TIME) * (1.0f - incomingValue);
      clickTime *= *decay;

      if (clickTime < (curTime - *lastClickTime))
      {
         *decay *= 0.9f;
         if (*decay < 0.2f)
         {
            *decay = 0.2f;
         }

         *lastClickTime = curTime;

         bool negative = (inputEvent.fValue < 0.0f);

         switch (inputEvent.objInst)
         {
         case XI_LEFT_TRIGGER:
         case XI_RIGHT_TRIGGER:
            return mFirstResponder->onGamepadTrigger(mLastEvent);

         case SI_ZAXIS:
         case SI_YAXIS:
         case XI_THUMBLY:
         case XI_THUMBRY:
            if (negative)
            {
               return mFirstResponder->onGamepadAxisDown(mLastEvent);
            }
            else
            {
               return mFirstResponder->onGamepadAxisUp(mLastEvent);
            }

         case SI_XAXIS:
         case XI_THUMBLX:
         case XI_THUMBRX:
         default:
            if (negative)
            {
               return mFirstResponder->onGamepadAxisLeft(mLastEvent);
            }
            else
            {
               return mFirstResponder->onGamepadAxisRight(mLastEvent);
            }
         }
      }
   }  
   return false;
}

void GuiCanvas::rootMouseDown(const GuiEvent &event)
{
   mPrevMouseTime = Platform::getVirtualMilliseconds();
   mMouseButtonDown = true;

   //pass the event to the mouse locked control
   if (bool(mMouseCapturedControl))
      mMouseCapturedControl->onMouseDown(event);
   else
   {
      //else pass it to whoever is underneath the cursor
      iterator i;
      i = end();
      while (i != begin())
      {
         i--;
         GuiControl *ctrl = static_cast<GuiControl *>(*i);
         GuiControl *controlHit = ctrl->findHitControl(event.mousePoint);

         //see if the controlHit is a modeless dialog...
         if ((! controlHit->mActive) && (! controlHit->getControlProfile()->mModal))
            continue;
         else
         {
            controlHit->onMouseDown(event);
            break;
         }
      }
   }

   if (bool(mMouseControl))
      mMouseControlClicked = true;
}

void GuiCanvas::findMouseControl(const GuiEvent &event)
{
   // Any children at all?
   if(size() == 0)
   {
      mMouseControl = NULL;
      return;
   }

   // Otherwise, check the point and find the overlapped control.
   GuiControl *controlHit = findHitControl(event.mousePoint);
   if(controlHit != static_cast<GuiControl*>(mMouseControl))
   {
      if(bool(mMouseControl))
         mMouseControl->onMouseLeave(event);
      mMouseControl = controlHit;
      mMouseControl->onMouseEnter(event);
   }
}

void GuiCanvas::refreshMouseControl()
{
   GuiEvent evt;
   evt.mousePoint.x = S32(mCursorPt.x);
   evt.mousePoint.y = S32(mCursorPt.y);
   findMouseControl(evt);
}

void GuiCanvas::rootMouseUp(const GuiEvent &event)
{
   mPrevMouseTime = Platform::getVirtualMilliseconds();
   mMouseButtonDown = false;

   // pass the event to the mouse locked control
   if (bool(mMouseCapturedControl))
      mMouseCapturedControl->onMouseUp(event);
   else
   {
      findMouseControl(event);
      if(bool(mMouseControl))
         mMouseControl->onMouseUp(event);
   }
}

void GuiCanvas::checkLockMouseMove(const GuiEvent &event)
{
	GuiControl *controlHit = findHitControl(event.mousePoint);
   if(controlHit != mMouseControl)
   {
      if(mMouseControl == mMouseCapturedControl)
         mMouseCapturedControl->onMouseLeave(event);
      else if(controlHit == mMouseCapturedControl)
         mMouseCapturedControl->onMouseEnter(event);
      mMouseControl = controlHit;
   }
}

void GuiCanvas::rootMouseDragged(const GuiEvent &event)
{
   //pass the event to the mouse locked control
   if (bool(mMouseCapturedControl))
   {
      checkLockMouseMove(event);
      mMouseCapturedControl->onMouseDragged(event);
   }
   else
   {
      findMouseControl(event);
      if(bool(mMouseControl))
         mMouseControl->onMouseDragged(event);
   }
}

void GuiCanvas::rootMouseMove(const GuiEvent &event)
{   
   if (bool(mMouseCapturedControl))
   {
      checkLockMouseMove(event);
      mMouseCapturedControl->onMouseMove(event);
   }
   else
   {
      findMouseControl(event);
      if(bool(mMouseControl))
         mMouseControl->onMouseMove(event);
   }
}

void GuiCanvas::rootRightMouseDown(const GuiEvent &event)
{
   mPrevMouseTime = Platform::getVirtualMilliseconds();
   mMouseRightButtonDown = true;

   if (bool(mMouseCapturedControl))
      mMouseCapturedControl->onRightMouseDown(event);
   else
   {
      findMouseControl(event);

      if(bool(mMouseControl))
      {
         mMouseControl->onRightMouseDown(event);
      }
   }
}

void GuiCanvas::rootRightMouseUp(const GuiEvent &event)
{
   mPrevMouseTime = Platform::getVirtualMilliseconds();
   mMouseRightButtonDown = false;

   if (bool(mMouseCapturedControl))
      mMouseCapturedControl->onRightMouseUp(event);
   else
   {
      findMouseControl(event);

      if(bool(mMouseControl))
         mMouseControl->onRightMouseUp(event);
   }
}

void GuiCanvas::rootRightMouseDragged(const GuiEvent &event)
{
   mPrevMouseTime = Platform::getVirtualMilliseconds();

   if (bool(mMouseCapturedControl))
   {
      checkLockMouseMove(event);
      mMouseCapturedControl->onRightMouseDragged(event);
   }
   else
   {
      findMouseControl(event);

      if(bool(mMouseControl))
         mMouseControl->onRightMouseDragged(event);
   }
}

void GuiCanvas::rootMiddleMouseDown(const GuiEvent &event)
{
   mPrevMouseTime = Platform::getVirtualMilliseconds();
   mMouseMiddleButtonDown = true;

   if (bool(mMouseCapturedControl))
      mMouseCapturedControl->onMiddleMouseDown(event);
   else
   {
      findMouseControl(event);

      if(bool(mMouseControl))
      {
         mMouseControl->onMiddleMouseDown(event);
      }
   }
}

void GuiCanvas::rootMiddleMouseUp(const GuiEvent &event)
{
   mPrevMouseTime = Platform::getVirtualMilliseconds();
   mMouseMiddleButtonDown = false;

   if (bool(mMouseCapturedControl))
      mMouseCapturedControl->onMiddleMouseUp(event);
   else
   {
      findMouseControl(event);

      if(bool(mMouseControl))
         mMouseControl->onMiddleMouseUp(event);
   }
}

void GuiCanvas::rootMiddleMouseDragged(const GuiEvent &event)
{
   mPrevMouseTime = Platform::getVirtualMilliseconds();

   if (bool(mMouseCapturedControl))
   {
      checkLockMouseMove(event);
      mMouseCapturedControl->onMiddleMouseDragged(event);
   }
   else
   {
      findMouseControl(event);

      if(bool(mMouseControl))
         mMouseControl->onMiddleMouseDragged(event);
   }
}

bool GuiCanvas::rootMouseWheelUp(const GuiEvent &event)
{
   if (bool(mMouseCapturedControl))
      return mMouseCapturedControl->onMouseWheelUp(event);
   else
   {
      findMouseControl(event);

      if (bool(mMouseControl))
         return mMouseControl->onMouseWheelUp(event);
   }

   return false;
}

bool GuiCanvas::rootMouseWheelDown(const GuiEvent &event)
{
   if (bool(mMouseCapturedControl))
      return mMouseCapturedControl->onMouseWheelDown(event);
   else
   {
      findMouseControl(event);

      if (bool(mMouseControl))
         return mMouseControl->onMouseWheelDown(event);
   }

   return false;
}

void GuiCanvas::setContentControl(GuiControl *gui)
{
#ifdef TORQUE_DEMO_PURCHASE
   if (mPurchaseScreen->isForceExit())
      return;
#endif

   // Skip out if we got passed NULL (why would that happen?)
   if(!gui)
      return;

   GuiControl *oldContent = getContentControl();
   if(oldContent)
      Con::executef(oldContent, "onUnsetContent", Con::getIntArg(gui->getId()));

   //remove all dialogs on layer 0
   U32 index = 0;
   while (size() > index)
   {
      GuiControl *ctrl = static_cast<GuiControl*>((*this)[index]);
      if (ctrl == gui || ctrl->mLayer != 0)
         index++;

      removeObject(ctrl);
      Sim::getGuiGroup()->addObject(ctrl);
   }

   // lose the first responder from the old GUI
   GuiControl* responder = gui->findFirstTabable();
   if(responder)
      responder->setFirstResponder();

   //add the gui to the front
   if(!size() || gui != (*this)[0])
   {
      // automatically wakes objects in GuiControl::onWake
      addObject(gui);
      if (size() >= 2)
         reOrder(gui, *begin());
   }
   //refresh the entire gui
   resetUpdateRegions();

   //rebuild the accelerator map
   mAcceleratorMap.clear();

   for(iterator i = end(); i != begin() ; )
   {
      i--;
      GuiControl *ctrl = static_cast<GuiControl *>(*i);
      ctrl->buildAcceleratorMap();

      if (ctrl->getControlProfile()->mModal)
         break;
   }
   refreshMouseControl();

   // Do this last so onWake gets called first
   Con::executef(gui, "onSetContent", Con::getIntArg(oldContent ? oldContent->getId() : 0));

   // Force the canvas to update the sizing of the new content control
   maintainSizing();
}

GuiControl *GuiCanvas::getContentControl()
{
   if(size() > 0)
      return (GuiControl *) first();
   return NULL;
}

void GuiCanvas::pushDialogControl(GuiControl *gui, S32 layer)
{
#ifdef TORQUE_DEMO_PURCHASE
   if (mPurchaseScreen->isForceExit())
      return;
#endif

   //add the gui
   gui->mLayer = layer;

   // GuiControl::addObject wakes the object
   addObject(gui);

   //reorder it to the correct layer
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      GuiControl *ctrl = static_cast<GuiControl*>(*i);
      if (ctrl->mLayer > gui->mLayer)
      {
         reOrder(gui, ctrl);
         break;
      }
   }

   //call the dialog push method
   gui->onDialogPush();

   //find the first responder
   GuiControl* responder = gui->findFirstTabable();
   if(responder)
      responder->setFirstResponder();

   // call the 'onWake' method?
   //if(wakedGui)
   //   Con::executef(gui, 1, "onWake");

   //refresh the entire gui
   resetUpdateRegions();

   //rebuild the accelerator map
   mAcceleratorMap.clear();
   if (size() > 0)
   {
      GuiControl *ctrl = static_cast<GuiControl*>(last());
      ctrl->buildAcceleratorMap();
   }

   refreshMouseControl();

   // I don't see the purpose of this, and it's causing issues when showing, for instance the 
   //  metrics dialog while in a 3d scene, causing the cursor to be shown even when the mouse
   //  is locked [4/25/2007 justind]
   //if(gui->mProfile && gui->mProfile->mModal)
   //   mPlatformWindow->getCursorController()->pushCursor(PlatformCursorController::curArrow);
}

void GuiCanvas::popDialogControl(GuiControl *gui)
{
   if (size() < 1)
      return;

   //first, find the dialog, and call the "onDialogPop()" method
   GuiControl *ctrl = NULL;
   if (gui)
   {
      //make sure the gui really exists on the stack
      iterator i;
      bool found = false;
      for(i = begin(); i != end(); i++)
      {
         GuiControl *check = static_cast<GuiControl *>(*i);
         if (check == gui)
         {
            ctrl = check;
            found = true;
         }
      }

      if (!found)
         return;
   }
   else
      ctrl = static_cast<GuiControl*>(last());

   //call the "on pop" function
   ctrl->onDialogPop();

   //now pop the last child (will sleep if awake)
   removeObject(ctrl);

   Sim::getGuiGroup()->addObject(ctrl);

   if (size() > 0)
   {
      GuiControl *ctrl = static_cast<GuiControl *>(last());
      if(ctrl->mFirstResponder)
         ctrl->mFirstResponder->setFirstResponder();
   }
   else
   {
      setFirstResponder(NULL);
   }

   //refresh the entire gui
   resetUpdateRegions();

   //rebuild the accelerator map
   mAcceleratorMap.clear();

   if (size() > 0)
   {
      GuiControl *ctrl = static_cast<GuiControl*>(last());
      ctrl->buildAcceleratorMap();
   }
   refreshMouseControl();
}

void GuiCanvas::popDialogControl(S32 layer)
{
   if (size() < 1)
      return;

   GuiControl *ctrl = NULL;
   iterator i = end(); // find in z order (last to first)
   while (i != begin())
   {
      i--;
      ctrl = static_cast<GuiControl*>(*i);
      if (ctrl->mLayer == layer)
         break;
   }
   if (ctrl)
      popDialogControl(ctrl);
}

void GuiCanvas::mouseLock(GuiControl *lockingControl)
{
   if (bool(mMouseCapturedControl))
      return;

   mMouseCapturedControl = lockingControl;

   if(mMouseControl && mMouseControl != mMouseCapturedControl)
   {
      GuiEvent evt;
      evt.mousePoint.x = S32(mCursorPt.x);
      evt.mousePoint.y = S32(mCursorPt.y);

      mMouseControl->onMouseLeave(evt);
   }
}

void GuiCanvas::mouseUnlock(GuiControl *lockingControl)
{
   if (static_cast<GuiControl*>(mMouseCapturedControl) != lockingControl)
      return;

   GuiEvent evt;
   evt.mousePoint.x = S32(mCursorPt.x);
   evt.mousePoint.y = S32(mCursorPt.y);

   GuiControl * controlHit = findHitControl(evt.mousePoint);
   if(controlHit != mMouseCapturedControl)
   {
      mMouseControl = controlHit;
      mMouseControlClicked = false;
      if(bool(mMouseControl))
         mMouseControl->onMouseEnter(evt);
   }
   mMouseCapturedControl = NULL;
}

void GuiCanvas::paint()
{
   resetUpdateRegions();

   // inhibit explicit refreshes in the case we're swapped out
   if( mPlatformWindow && mPlatformWindow->isVisible() && GFX->allowRender())
      mPlatformWindow->displayEvent.trigger(mPlatformWindow->getWindowId());
}

void GuiCanvas::maintainSizing()
{
   Point2I size = getWindowSize();

   if(size.x == -1 || size.y == -1)
      return;

   RectI screenRect(0, 0, size.x, size.y);
   setBounds(screenRect);

   // all bottom level controls should be the same dimensions as the canvas
   // this is necessary for passing mouse events accurately
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      AssertFatal(static_cast<GuiControl*>((*i))->isAwake(), "GuiCanvas::maintainSizing - ctrl is not awake");
      GuiControl *ctrl = static_cast<GuiControl*>(*i);
      Point2I ext = ctrl->getExtent();
      Point2I pos = ctrl->getPosition();

      if(pos != screenRect.point || ext != screenRect.extent)
      {
         ctrl->resize(screenRect.point, screenRect.extent);
         resetUpdateRegions();
      }
   }

}

void GuiCanvas::setupFences()
{
   // Destroy old fences
   SAFE_DELETE_ARRAY( mFences );

   // Now create the new ones
   if( mNumFences > 0 )
   {
      mFences = new GFXFence*[mNumFences];

      // Allocate the new fences
      for( int i = 0; i < mNumFences; i++ )
         mFences[i] = GFX->createFence();
   }

   // Reset state
   mNextFenceIdx = 0;
}

void GuiCanvas::renderFrame(bool preRenderOnly, bool bufferSwap /* = true */)
{
   AssertISV(mPlatformWindow, "GuiCanvas::renderFrame - no window present!");
   if(!mPlatformWindow->isVisible() || !GFX->allowRender() || GFX->canCurrentlyRender())
      return;

   PROFILE_START(CanvasPreRender);

   // Set our window as the current render target so we can see outputs.
   GFX->setActiveRenderTarget(mPlatformWindow->getGFXTarget());

   if (!GFX->getActiveRenderTarget())
   {
      PROFILE_END();
      return;
   }

#ifdef TORQUE_GFX_STATE_DEBUG
   GFX->getDebugStateManager()->startFrame();
#endif

   GFXTarget* renderTarget = GFX->getActiveRenderTarget();
   if (renderTarget == NULL)
   {
      PROFILE_END();
      return;
   }

   // Make sure the root control is the size of the canvas.
   Point2I size = renderTarget->getSize();

   if(size.x == 0 || size.y == 0)
   {
      PROFILE_END();
      return;
   }

   RectI screenRect(0, 0, size.x, size.y);

   maintainSizing();

   //preRender (recursive) all controls
   preRender();

   PROFILE_END();

   // Are we just doing pre-render?
   if(preRenderOnly)
      return;

   // Signal the interested parties.
   gCanvasRenderSignal.trigger();

   // for now, just always reset the update regions - this is a
   // fix for FSAA on ATI cards
   resetUpdateRegions();

   PROFILE_START(CanvasRenderControls);

   // Draw the mouse
   GuiCursor *mouseCursor = NULL;
   bool cursorVisible = true;

   if(bool(mMouseCapturedControl))
      mMouseCapturedControl->getCursor(mouseCursor, cursorVisible, mLastEvent);
   else if(bool(mMouseControl))
      mMouseControl->getCursor(mouseCursor, cursorVisible, mLastEvent);

   Point2I cursorPos((S32)mCursorPt.x, (S32)mCursorPt.y);
   if(!mouseCursor)
      mouseCursor = mDefaultCursor;

   if(mLastCursorEnabled && mLastCursor)
   {
      Point2I spot = mLastCursor->getHotSpot();
      Point2I cext = mLastCursor->getExtent();
      Point2I pos = mLastCursorPt - spot;
      addUpdateRegion(pos - Point2I(2, 2), Point2I(cext.x + 4, cext.y + 4));
   }

   if(cursorVisible && mouseCursor)
   {
      Point2I spot = mouseCursor->getHotSpot();
      Point2I cext = mouseCursor->getExtent();
      Point2I pos = cursorPos - spot;

      addUpdateRegion(pos - Point2I(2, 2), Point2I(cext.x + 4, cext.y + 4));
   }

	mLastCursorEnabled = cursorVisible;
	mLastCursor = mouseCursor;
	mLastCursorPt = cursorPos;

   // Begin GFX
   PROFILE_START(GFXBeginScene);

   GFX->beginScene();

   PROFILE_END();

   // Clear the current viewport area
   GFX->setViewport( screenRect );
   GFX->clear( GFXClearZBuffer | GFXClearStencil | GFXClearTarget, gCanvasClearColor, 1.0f, 0 );

   resetUpdateRegions();

   RectI updateUnion;
   buildUpdateUnion(&updateUnion);
   if (updateUnion.intersect(screenRect))
   {
      // Render active GUI Dialogs
      for(iterator i = begin(); i != end(); i++)
      {
         // Get the control
         GuiControl *contentCtrl = static_cast<GuiControl*>(*i);
         
         GFX->setClipRect( updateUnion );
         GFX->setStateBlock(mDefaultGuiSB);
         
         contentCtrl->onRender(contentCtrl->getPosition(), updateUnion);
      }

      // Fill Black if no Dialogs
      if(this->size() == 0)
         GFX->clear( GFXClearTarget, ColorI(0,0,0,0), 1.0f, 0 );

      // Tooltip resource
      if(bool(mMouseControl))
      {
         U32 curTime = Platform::getRealMilliseconds();
         if(mHoverControl == mMouseControl)
         {
            if(mHoverPositionSet || (curTime - mHoverControlStart) >= mHoverControl->mTipHoverTime || (curTime - mHoverLeftControlTime) <= mHoverControl->mTipHoverTime)
            {
               if(!mHoverPositionSet)
               {
                  mHoverPosition = cursorPos;
               }
               mHoverPositionSet = mMouseControl->mRenderTooltipDelegate( mHoverPosition, cursorPos, NULL );
            }

         } 
         else
         {
            if(mHoverPositionSet)
            {
               mHoverLeftControlTime = curTime;
               mHoverPositionSet = false;
            }
            mHoverControl = mMouseControl;
            mHoverControlStart = curTime;
         }
      }

      GFX->setClipRect( updateUnion );

      // Draw an ugly box if we don't have a cursor available...
      //if (mCursorEnabled && mShowCursor && !mouseCursor)
      //{
      //   GFX->drawRectFill( RectI( mCursorPt.x, mCursorPt.y, mCursorPt.x + 2, mCursorPt.y + 2 ), ColorI( 255, 0, 0 ) );
      //}


      // CodeReview - Make sure our bitmap modulation is clear or else there's a black modulation
      // that ruins rendering of textures at startup.. This was done in mouseCursor 
      // onRender and so at startup when it wasn't called the modulation was black, ruining
      // the loading screen display. This fixes the issue, but is it only masking a deeper issue
      // in GFX with regard to gui rendering? [5/3/2007 justind]
      GFX->getDrawUtil()->clearBitmapModulation();

      // Really draw the cursor. :)
      // Only if the platform cursor controller is missing or the platform cursor
      // isn't visible.  
//       if (!mPlatformWindow->getCursorController() || (mCursorEnabled && mouseCursor && mShowCursor && 
//          !mPlatformWindow->getCursorController()->isCursorVisible()))
	  if (mCursorEnabled && mouseCursor && mShowCursor)
      {
         Point2I pos((S32)mCursorPt.x, (S32)mCursorPt.y);
		 if (mPlatformWindow->getCursorController())
		 {
			 mPlatformWindow->getCursorController()->getCursorPosition(pos);
			 pos = mPlatformWindow->screenToClient(pos);
			 mCursorPt.x = pos.x;
			 mCursorPt.y = pos.y;
		 }
         Point2I spot = mouseCursor->getHotSpot();

         pos -= spot;
         mouseCursor->render(pos);
      }
   }

   // Render all RTT end of frame updates HERE
   //DynamicTexture::updateScreenTextures();
   //DynamicTexture::updateEndOfFrameTextures();
   // mPending is set when the console function "screenShot()" is called
   // this situation is necessary because it needs to take the screenshot
   // before the buffers swap

#ifdef TORQUE_DEMO_TIMEOUT
   checkTimeOut();
#endif

   if( gScreenShot != NULL && gScreenShot->mPending )
      gScreenShot->captureStandard();

   PROFILE_END();

   // Fence logic here, because this is where endScene is called.
   if( mNumFences > 0 )
   {
      // Issue next fence
      mFences[mNextFenceIdx]->issue();

      mNextFenceIdx++;
      
      // Wrap the next fence around to first if we're maxxed
      if( mNextFenceIdx >= mNumFences )
         mNextFenceIdx = 0;

      // Block on previous fence
      mFences[mNextFenceIdx]->block();
   }

   PROFILE_START(GFXEndScene);
   GFX->endScene();
   PROFILE_END();

   swapBuffers();

#ifdef TORQUE_GFX_STATE_DEBUG
   GFX->getDebugStateManager()->endFrame();
#endif
}

void GuiCanvas::swapBuffers()
{
   AssertISV(mPlatformWindow, "GuiCanvas::swapBuffers - no window present!");
   if(!mPlatformWindow->isVisible())
      return;

   PROFILE_START(SwapBuffers);
   mPlatformWindow->getGFXTarget()->present();
   PROFILE_END();
}

void GuiCanvas::buildUpdateUnion(RectI *updateUnion)
{
   *updateUnion = mOldUpdateRects[0];

   //the update region should encompass the oldUpdateRects, and the curUpdateRect
   Point2I upperL;
   Point2I lowerR;

   upperL.x = getMin(mOldUpdateRects[0].point.x, mOldUpdateRects[1].point.x);
   upperL.x = getMin(upperL.x, mCurUpdateRect.point.x);

   upperL.y = getMin(mOldUpdateRects[0].point.y, mOldUpdateRects[1].point.y);
   upperL.y = getMin(upperL.y, mCurUpdateRect.point.y);

   lowerR.x = getMax(mOldUpdateRects[0].point.x + mOldUpdateRects[0].extent.x, mOldUpdateRects[1].point.x + mOldUpdateRects[1].extent.x);
   lowerR.x = getMax(lowerR.x, mCurUpdateRect.point.x + mCurUpdateRect.extent.x);

   lowerR.y = getMax(mOldUpdateRects[0].point.y + mOldUpdateRects[0].extent.y, mOldUpdateRects[1].point.y + mOldUpdateRects[1].extent.y);
   lowerR.y = getMax(lowerR.y, mCurUpdateRect.point.y + mCurUpdateRect.extent.y);

   updateUnion->point = upperL;
   updateUnion->extent = lowerR - upperL;

   //shift the oldUpdateRects
   mOldUpdateRects[0] = mOldUpdateRects[1];
   mOldUpdateRects[1] = mCurUpdateRect;

   mCurUpdateRect.point.set(0,0);
   mCurUpdateRect.extent.set(0,0);
}

void GuiCanvas::addUpdateRegion(Point2I pos, Point2I ext)
{
   if(mCurUpdateRect.extent.x == 0)
   {
      mCurUpdateRect.point = pos;
      mCurUpdateRect.extent = ext;
   }
   else
   {
      Point2I upperL;
      upperL.x = getMin(mCurUpdateRect.point.x, pos.x);
      upperL.y = getMin(mCurUpdateRect.point.y, pos.y);
      Point2I lowerR;
      lowerR.x = getMax(mCurUpdateRect.point.x + mCurUpdateRect.extent.x, pos.x + ext.x);
      lowerR.y = getMax(mCurUpdateRect.point.y + mCurUpdateRect.extent.y, pos.y + ext.y);
      mCurUpdateRect.point = upperL;
      mCurUpdateRect.extent = lowerR - upperL;
   }
}

void GuiCanvas::resetUpdateRegions()
{
   //DEBUG - get surface width and height
   mOldUpdateRects[0] = getBounds();
   mOldUpdateRects[1] = mOldUpdateRects[0];
   mCurUpdateRect = mOldUpdateRects[0];
}

void GuiCanvas::setFirstResponder( GuiControl* newResponder )
{
	GuiControl* oldResponder = mFirstResponder;
	Parent::setFirstResponder( newResponder );

	if ( oldResponder && ( oldResponder != mFirstResponder ) )
		oldResponder->onLoseFirstResponder();
}

GuiCursor * GuiCanvas::getCurrentCursor()
{
	return mDefaultCursor;
}

ConsoleMethod( GuiCanvas, getContent, S32, 2, 2, "Get the GuiControl which is being used as the content.")
{
   GuiControl *ctrl = object->getContentControl();
   if(ctrl)
      return ctrl->getId();
   return -1;
}

ConsoleMethod( GuiCanvas, setContent, void, 3, 3, "(GuiControl ctrl)"
              "Set the content of the canvas.")
{
   TORQUE_UNUSED(argc);

   GuiControl *gui = NULL;
   if(argv[2][0])
   {
      if (!Sim::findObject(argv[2], gui))
      {
         Con::printf("%s(): Invalid control: %s", argv[0], argv[2]);
         return;
      }
   }

   //set the new content control
   object->setContentControl(gui);
}

ConsoleMethod( GuiCanvas, pushDialog, void, 3, 4, "(GuiControl ctrl, int layer)")
{
   GuiControl *gui;

   if (!	Sim::findObject(argv[2], gui))
   {
      Con::printf("%s(): Invalid control: %s", argv[0], argv[2]);
      return;
   }

   //find the layer
   S32 layer = 0;
   if (argc == 4)
      layer = dAtoi(argv[3]);

   //set the new content control
   object->pushDialogControl(gui, layer);
}

ConsoleMethod( GuiCanvas, popDialog, void, 2, 3, "(GuiControl ctrl=NULL)" )
{
   GuiControl *gui = NULL;
   if (argc == 3)
   {
      if (!Sim::findObject(argv[2], gui))
      {
         Con::printf("%s(): Invalid control: %s", argv[0], argv[2]);
         return;
      }
   }

   if (gui)
      object->popDialogControl(gui);
   else
      object->popDialogControl();
}

ConsoleMethod( GuiCanvas, popLayer, void, 2, 3, "(int layer)" )
{
   S32 layer = 0;
   if (argc == 3)
      layer = dAtoi(argv[2]);

   object->popDialogControl(layer);
}

ConsoleMethod(GuiCanvas, cursorOn, void, 2, 2, "")
{
   TORQUE_UNUSED(argc);
   TORQUE_UNUSED(argv);
   object->setCursorON(true);
}

ConsoleMethod(GuiCanvas, cursorOff, void, 2, 2, "")
{
   TORQUE_UNUSED(argc);
   TORQUE_UNUSED(argv);
   object->setCursorON(false);
}

ConsoleMethod( GuiCanvas, setCursor, void, 3, 3, "(cursorName)")
{
   TORQUE_UNUSED(argc);

   GuiCursor *curs = NULL;
   if(argv[2][0])
   {
      if(!Sim::findObject(argv[2], curs))
      {
         Con::printf("%s is not a valid cursor.", argv[2]);
         return;
      }
   }
   object->setCursor(curs);
}

ConsoleMethod( GuiCanvas, renderFront, void, 3, 3, "(bool enable)")
{
   object->setRenderFront(dAtob(argv[2]));
}

ConsoleMethod( GuiCanvas, showCursor, void, 2, 2, "" )
{
   object->showCursor(true);
}

ConsoleMethod( GuiCanvas, hideCursor, void, 2, 2, "" )
{
   object->showCursor(false);
}

ConsoleMethod( GuiCanvas, isCursorOn, bool, 2, 2, "" )
{
   return object->isCursorON();
}

ConsoleMethod( GuiCanvas, isCursorShown, bool, 2, 2, "" )
{
	return object->isCursorShown();
}

ConsoleMethod( GuiCanvas, repaint, void, 2, 2, "Force canvas to redraw." )
{
   object->resetUpdateRegions();
   object->renderFrame(false);
}

ConsoleMethod( GuiCanvas, reset, void, 2, 2, "Reset the update regions for the canvas." )
{
   object->resetUpdateRegions();
}

ConsoleMethod( GuiCanvas, getCursorPos, const char*, 2, 2, "Get the current position of the cursor." )
{
   Point2I pos = object->getCursorPos();
   char * ret = Con::getReturnBuffer(32);
   dSprintf(ret, 32, "%d %d", pos.x, pos.y);
   return(ret);
}

ConsoleMethod( GuiCanvas, setCursorPos, void, 3, 4, "(Point2I pos)" )
{
   Point2I pos(0,0);

   if(argc == 4)
      pos.set(dAtoi(argv[2]), dAtoi(argv[3]));
   else
      dSscanf(argv[2], "%d %d", &pos.x, &pos.y);

   object->setCursorPos(pos);
}

ConsoleMethod( GuiCanvas, getMouseControl, S32, 2, 2, "Gets the gui control under the mouse.")
{
   GuiControl* control = object->getMouseControl();
   if (control)
      return control->getId();
   
   return NULL;
}

ConsoleFunction( excludeOtherInstance, bool, 1, 1, "(string appIdentifier) "
                 "@return false if another app is running that specified the "
                 "same string.")
{
   // mac/360 can only run one instance in general.
#if !defined(TORQUE_OS_MAC) && !defined(TORQUE_OS_XENON) && !defined(TORQUE_DEBUG)
   return Platform::excludeOtherInstances(argv[1]);
#else
   // We can just return true if we get here.
   return true;
#endif
}

ConsoleMethod( GuiCanvas, getExtent, const char*, 2, 2, "Returns the dimensions of the canvas.")
{
   char * ret = Con::getReturnBuffer(32);
   dSprintf(ret, 32, "%d %d", object->getWidth(), object->getHeight());
   return(ret);
}

ConsoleMethod( GuiCanvas, setWindowTitle, void, 3, 3, "(string newTitle)" )
{
   object->setWindowTitle(argv[2]);
}

ConsoleMethod( GuiCanvas, getVideoMode, const char*, 2, 2,
               "()\n"
               "Gets the current screen mode as a string.\n\n"
               "\\return (string) The current screen mode as \"(int)width (int)height (bool)fullscreen (int)bitdepth (int)refreshRate\"" )
{
   // Grab the video mode.
   if (!object->getPlatformWindow())
      return "";

   GFXVideoMode vm = object->getPlatformWindow()->getVideoMode();
   char* buf = Con::getReturnBuffer(vm.toString());
   return buf;
}

ConsoleMethod( GuiCanvas, getModeCount, S32, 2, 2,
               "()\n"
               "Gets the number of modes available on this device.\n\n"
               "\\return (int) The number of video modes supported by the device." )
{
   if (!object->getPlatformWindow())
      return 0;

   // Grab the available mode list from the device.
   const Vector<GFXVideoMode>* const modeList = 
      object->getPlatformWindow()->getGFXDevice()->getVideoModeList();

   // Return the number of resolutions.
   return modeList->size();
}

ConsoleMethod( GuiCanvas, getMode, const char*, 3, 3,
               "(int modeId)\n"
               "Gets information on the specified mode of this device.\n\n"
               "\\param modeId Index of the mode to get data from.\n"
               "\\return (string) A video mode string given an adapter and mode index. See GuiCanvas.getVideoMode()" )
{
   if (!object->getPlatformWindow())
      return 0;

   // Grab the available mode list from the device.
   const Vector<GFXVideoMode>* const modeList = 
      object->getPlatformWindow()->getGFXDevice()->getVideoModeList();

   // Get the desired index and confirm it's valid.
   S32 idx = dAtoi(argv[2]);
   if((idx < 0) || (idx >= modeList->size()))
   {
      Con::errorf("GuiCanvas::getResolution - You requested an out of range index of %d. Please specify an index in the range [0, %d).", idx, modeList->size());
      return "";
   }

   // Great - we got something valid, so convert the videomode into a 
   // string and return to the user.
   GFXVideoMode vm = (*modeList)[idx];

   char *retString = Con::getReturnBuffer(vm.toString());
   return retString;
}

ConsoleMethod( GuiCanvas, toggleFullscreen, void, 2, 2, "() - toggle canvas from fullscreen to windowed mode or back." )
{

   if (Platform::getWebDeployment())
      return;

   if (!object->getPlatformWindow())
      return;

   // Get the window's video mode.
   GFXVideoMode origMode = object->getPlatformWindow()->getVideoMode();
   
   // And grab the device its using.
   GFXDevice *device = object->getPlatformWindow()->getGFXDevice();

   // Toggle the fullscreen bit.
   GFXVideoMode newMode = origMode;
   newMode.fullScreen = !origMode.fullScreen;
   
   // CodeReview Toggling might be better served by reading the fullscreen
   //            or windowed video mode pref and setting that instead [bjg 5/2/07]

   if(newMode.fullScreen == true)
   {
      // Are we going to fullscreen? If so find the first matching resolution that
      // is equal to or bigger in size, and has same BPP - windows
      // are often strangely sized and will need to be adjusted to a viable
      // fullscreen res.

      for(S32 i=0; i<device->getVideoModeList()->size(); i++)
      {
         const GFXVideoMode &newVm = (*(device->getVideoModeList()))[i];

         if(newMode.resolution.x > newVm.resolution.x)
            continue;

         if(newMode.resolution.y > newVm.resolution.y)
            continue;

         if(newMode.bitDepth != newVm.bitDepth)
            continue;

         // Great - got a match.
         newMode = newVm;
         newMode.fullScreen = true;
         break;
      }
   }

   // Ok, we have new video mode. Set it!
   object->getPlatformWindow()->setVideoMode(newMode);
}

ConsoleMethod( GuiCanvas, isFullscreen, bool, 2, 2, "() - Is this canvas currently fullscreen?" )
{
   if (Platform::getWebDeployment())
      return false;

   if (!object->getPlatformWindow())
      return false;

   return object->getPlatformWindow()->getVideoMode().fullScreen;
}

ConsoleMethod( GuiCanvas, minimizeWindow, void, 2, 2, "() - minimize this canvas' window." )
{
   PlatformWindow* window = object->getPlatformWindow();
   if ( window )
      window->minimize();
}

ConsoleMethod( GuiCanvas, isMinimized, bool, 2, 2, "()" )
{
   PlatformWindow* window = object->getPlatformWindow();
   if ( window )
      return window->isMinimized();

   return false;
}

ConsoleMethod( GuiCanvas, isMaximized, bool, 2, 2, "()" )
{
   PlatformWindow* window = object->getPlatformWindow();
   if ( window )
      return window->isMaximized();

   return false;
}

ConsoleMethod( GuiCanvas, maximizeWindow, void, 2, 2, "() - maximize this canvas' window." )
{
   PlatformWindow* window = object->getPlatformWindow();
   if ( window )
      window->maximize();
}

ConsoleMethod( GuiCanvas, restoreWindow, void, 2, 2, "() - restore this canvas' window." )
{
   PlatformWindow* window = object->getPlatformWindow();
   if( window )
      window->restore();
}

ConsoleMethod( GuiCanvas, setVideoMode, void, 5, 8,
               "(int width, int height, bool fullscreen, [int bitDepth], [int refreshRate])\n"
               "Change the video mode of this canvas. This method has the side effect of setting the $pref::Video::mode to the new values.\n\n"
               "\\param width The screen width to set.\n"
               "\\param height The screen height to set.\n"
               "\\param fullscreen Specify true to run fullscreen or false to run in a window\n"
               "\\param bitDepth [optional] The desired bit-depth. Defaults to the current setting. This parameter is ignored if you are running in a window.\n"
               "\\param refreshRate [optional] The desired refresh rate. Defaults to the current setting. This parameter is ignored if you are running in a window"
					"\\param antialiasLevel [optional] The level of anti-aliasing to apply 0 = none" )
{
   if (!object->getPlatformWindow())
      return;

   // Update the video mode and tell the window to reset.
   GFXVideoMode vm = object->getPlatformWindow()->getVideoMode();

   U32 width = dAtoi(argv[2]);
   U32 height = dAtoi(argv[3]);

   bool changed = false;
   if (width == 0 && height > 0)
   {
      // Our width is 0 but our height isn't...
      // Try to find a matching width
      for(S32 i=0; i<object->getPlatformWindow()->getGFXDevice()->getVideoModeList()->size(); i++)
      {
         const GFXVideoMode &newVm = (*(object->getPlatformWindow()->getGFXDevice()->getVideoModeList()))[i];

         if(newVm.resolution.y == height)
         {
            width = newVm.resolution.x;
            changed = true;
            break;
         }
      }
   }
   else if (height == 0 && width > 0)
   {
      // Our height is 0 but our width isn't...
      // Try to find a matching height
      for(S32 i=0; i<object->getPlatformWindow()->getGFXDevice()->getVideoModeList()->size(); i++)
      {
         const GFXVideoMode &newVm = (*(object->getPlatformWindow()->getGFXDevice()->getVideoModeList()))[i];

         if(newVm.resolution.x == width)
         {
            height = newVm.resolution.y;
            changed = true;
            break;
         }
      }
   }

   if (width == 0 || height == 0)
   {
      // Got a bad size for both of our dimensions or one of our dimensions and
      // didn't get a match for the other default back to our current resolution
      width  = vm.resolution.x;
      height = vm.resolution.y;

      changed = true;
   }

   if (changed)
      Con::errorf("GuiCanvas::setVideoMode(): Error - Invalid resolution of (%d, %d) - attempting (%d, %d)", dAtoi(argv[2]), dAtoi(argv[3]), width, height);

   vm.resolution  = Point2I(width, height);
   vm.fullScreen  = dAtob(argv[4]);

   if (Platform::getWebDeployment())
      vm.fullScreen  = false;

   // These optional params are set to default at construction of vm. If they
   // aren't specified, just leave them at whatever they were set to.
   if ((argc > 5) && (dStrlen(argv[5]) > 0))
   {
      vm.bitDepth = dAtoi(argv[5]);
   }
   if ((argc > 6) && (dStrlen(argv[6]) > 0))
   {
      vm.refreshRate = dAtoi(argv[6]);
   }

   if ((argc > 7) && (dStrlen(argv[7]) > 0))
   {
      vm.antialiasLevel = dAtoi(argv[7]);
   }

#ifndef TORQUE_OS_XENON
   object->getPlatformWindow()->setVideoMode(vm);
#endif

   // Store the new mode into a pref.
   Con::setVariable( "$pref::Video::mode", vm.toString() );
}
