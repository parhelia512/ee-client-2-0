//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/controls/guiTextEditSliderBitmapCtrl.h"

#include "console/consoleTypes.h"
#include "console/console.h"
#include "gui/core/guiCanvas.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"


IMPLEMENT_CONOBJECT(GuiTextEditSliderBitmapCtrl);

GuiTextEditSliderBitmapCtrl::GuiTextEditSliderBitmapCtrl()
{
   mRange.set(0.0f, 1.0f);
   mIncAmount = 1.0f;
   mValue = 0.0f;
   mMulInc = 0;
   mIncCounter = 0.0f;
   mFormat = StringTable->insert("%3.2f");
   mTextAreaHit = None;
   mFocusOnMouseWheel = false;
   mBitmapName = StringTable->insert( "" );
}

GuiTextEditSliderBitmapCtrl::~GuiTextEditSliderBitmapCtrl()
{
}

void GuiTextEditSliderBitmapCtrl::initPersistFields()
{
   addField("format",    TypeString,  Offset(mFormat, GuiTextEditSliderBitmapCtrl));
   addField("range",     TypePoint2F, Offset(mRange, GuiTextEditSliderBitmapCtrl));
   addField("increment", TypeF32,     Offset(mIncAmount,     GuiTextEditSliderBitmapCtrl));
   addField("focusOnMouseWheel", TypeBool, Offset(mFocusOnMouseWheel, GuiTextEditSliderBitmapCtrl));
	addField("bitmap",	 TypeFilename,Offset(mBitmapName, GuiTextEditSliderBitmapCtrl));

   Parent::initPersistFields();
}

void GuiTextEditSliderBitmapCtrl::getText(char *dest)
{
   Parent::getText(dest);
}

void GuiTextEditSliderBitmapCtrl::setText(const char *txt)
{
   mValue = dAtof(txt);
   checkRange();
   setValue();
}

bool GuiTextEditSliderBitmapCtrl::onKeyDown(const GuiEvent &event)
{
   return Parent::onKeyDown(event);
}

void GuiTextEditSliderBitmapCtrl::checkRange()
{
   if(mValue < mRange.x)
      mValue = mRange.x;
   else if(mValue > mRange.y)
      mValue = mRange.y;
}

void GuiTextEditSliderBitmapCtrl::setValue()
{
   char buf[20];
   // For some reason this sprintf is failing to convert
   // a floating point number to anything with %d, so cast it.
   if( dStricmp( mFormat, "%d" ) == 0 )
      dSprintf(buf,sizeof(buf),mFormat, (S32)mValue);
   else
      dSprintf(buf,sizeof(buf),mFormat, mValue);
   Parent::setText(buf);
}

void GuiTextEditSliderBitmapCtrl::onMouseDown(const GuiEvent &event)
{
   // If we're not active then skip out.
   if ( !mActive || !mAwake || !mVisible )
   {
      Parent::onMouseDown(event);
      return;
   }

   char txt[20];
   Parent::getText(txt);
   mValue = dAtof(txt);

   mMouseDownTime = Sim::getCurrentTime();
   GuiControl *parent = getParent();
   if(!parent)
      return;
   Point2I camPos  = event.mousePoint;
   Point2I point = parent->localToGlobalCoord(getPosition());

   if(camPos.x > point.x + getExtent().x - 14)
   {
      if(camPos.y > point.y + (getExtent().y/2))
      {
         mValue -=mIncAmount;
         mTextAreaHit = ArrowDown;
         mMulInc = -0.15f;
      }
      else
      {
         mValue +=mIncAmount;
         mTextAreaHit = ArrowUp;
         mMulInc = 0.15f;
      }

      checkRange();
      setValue();
      mouseLock();

      // We should get the focus and set the 
      // cursor to the start of the text to 
      // mimic the standard Windows behavior.
      setFirstResponder();
      mCursorPos = mBlockStart = mBlockEnd = 0;
      setUpdate();

      return;
   }

   Parent::onMouseDown(event);
}

void GuiTextEditSliderBitmapCtrl::onMouseDragged(const GuiEvent &event)
{
   // If we're not active then skip out.
   if ( !mActive || !mAwake || !mVisible )
   {
      Parent::onMouseDragged(event);
      return;
   }

   if(mTextAreaHit == None || mTextAreaHit == Slider)
   {
      mTextAreaHit = Slider;
      GuiControl *parent = getParent();
      if(!parent)
         return;
      Point2I camPos = event.mousePoint;
      Point2I point = parent->localToGlobalCoord(getPosition());
      F32 maxDis = 100;
      F32 val;
      if(camPos.y < point.y)
      {
         if((F32)point.y < maxDis)
            maxDis = (F32)point.y;

         val = point.y - maxDis;
         
         if(point.y > 0)
            mMulInc= 1.0f-(((float)camPos.y - val) / maxDis);
         else
            mMulInc = 1.0f;
         
         checkIncValue();
         
         return;
      }
      else if(camPos.y > point.y + getExtent().y)
      {
         GuiCanvas *root = getRoot();
         val = (F32)(root->getHeight() - (point.y + getHeight()));
         if(val < maxDis)
            maxDis = val;
         if( val > 0)
            mMulInc= -(F32)(camPos.y - (point.y + getHeight()))/maxDis;
         else
            mMulInc = -1.0f;
         checkIncValue();
         return;
      }
      mTextAreaHit = None;
      Parent::onMouseDragged(event);
   }
}

void GuiTextEditSliderBitmapCtrl::onMouseUp(const GuiEvent &event)
{
   // If we're not active then skip out.
   if ( !mActive || !mAwake || !mVisible )
   {
      Parent::onMouseUp(event);
      return;
   }

   mMulInc = 0.0f;
   mouseUnlock();

   if ( mTextAreaHit != None )

  //if we released the mouse within this control, then the parent will call
  //the mConsoleCommand other wise we have to call it.
   Parent::onMouseUp(event);

   //if we didn't release the mouse within this control, then perform the action
   // if (!cursorInControl())
   execConsoleCallback();   

   if (mAltConsoleCommand && mAltConsoleCommand != "" && mAltConsoleCommand[0])
      Con::evaluate(mAltConsoleCommand, false);

   mTextAreaHit = None;
}

bool GuiTextEditSliderBitmapCtrl::onMouseWheelUp(const GuiEvent &event)
{
   if ( !mActive || !mAwake || !mVisible )
      return Parent::onMouseWheelUp(event);

   if ( !isFirstResponder() && !mFocusOnMouseWheel )
      return false;

   mValue += mIncAmount;

   checkRange();
   setValue();
   
   setFirstResponder();
   mCursorPos = mBlockStart = mBlockEnd = 0;
   setUpdate();

   return true;
}

bool GuiTextEditSliderBitmapCtrl::onMouseWheelDown(const GuiEvent &event)
{
   if ( !mActive || !mAwake || !mVisible )
      return Parent::onMouseWheelDown(event);

   if ( !isFirstResponder() && !mFocusOnMouseWheel )
      return false;

   mValue -= mIncAmount;

   checkRange();
   setValue();

   setFirstResponder();
   mCursorPos = mBlockStart = mBlockEnd = 0;
   setUpdate();

   return true;
}

void GuiTextEditSliderBitmapCtrl::checkIncValue()
{
   if(mMulInc > 1.0f)
      mMulInc = 1.0f;
   else if(mMulInc < -1.0f)
      mMulInc = -1.0f;
}

void GuiTextEditSliderBitmapCtrl::timeInc(U32 elapseTime)
{
   S32 numTimes = elapseTime / 750;
   if(mTextAreaHit != Slider && numTimes > 0)
   {
      if(mTextAreaHit == ArrowUp)
         mMulInc = 0.15f * numTimes;
      else
         mMulInc = -0.15f * numTimes;

      checkIncValue();
   }
}
bool GuiTextEditSliderBitmapCtrl::onWake()
{
   if(!Parent::onWake())
      return false;

	mNumberOfBitmaps = mProfile->constructBitmapArray();

   return true;
}

void GuiTextEditSliderBitmapCtrl::onPreRender()
{
   if (isFirstResponder())
   {
      U32 timeElapsed = Platform::getVirtualMilliseconds() - mTimeLastCursorFlipped;
      mNumFramesElapsed++;
      if ((timeElapsed > 500) && (mNumFramesElapsed > 3))
      {
         mCursorOn = !mCursorOn;
         mTimeLastCursorFlipped = Sim::getCurrentTime();
         mNumFramesElapsed = 0;
         setUpdate();
      }

      //update the cursor if the text is scrolling
      if (mDragHit)
      {
         if ((mScrollDir < 0) && (mCursorPos > 0))
         {
            mCursorPos--;
         }
         else if ((mScrollDir > 0) && (mCursorPos < (S32)dStrlen(mText)))
         {
            mCursorPos++;
         }
      }
   }
}

void GuiTextEditSliderBitmapCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   if(mTextAreaHit != None)
   {
      U32 elapseTime = Sim::getCurrentTime() - mMouseDownTime;
      if(elapseTime > 750 || mTextAreaHit == Slider)
      {
         timeInc(elapseTime);
         mIncCounter += mMulInc;
         if(mIncCounter >= 1.0f || mIncCounter <= -1.0f)
         {
            mValue = (mMulInc > 0.0f) ? mValue+mIncAmount : mValue-mIncAmount;
            mIncCounter = (mIncCounter > 0.0f) ? mIncCounter-1 : mIncCounter+1;
            checkRange();
            setValue();
            mCursorPos = 0;
         }
      }
   }

	Parent::onRender(offset, updateRect);

	// Arrow placement coordinates
   Point2I arrowUpStart(offset.x + getWidth() - 14, offset.y + 1 );
   Point2I arrowUpEnd(13, getExtent().y/2);

	Point2I arrowDownStart(offset.x + getWidth() - 14, offset.y + 1 + getExtent().y/2);
	Point2I arrowDownEnd(13, getExtent().y/2);
	
	// Draw the line that splits the number and bitmaps
	GFX->getDrawUtil()->drawLine(Point2I(offset.x + getWidth() - 14 -2, offset.y + 1 ),
		Point2I(arrowUpStart.x -2, arrowUpStart.y + getExtent().y),
		mProfile->mBorderColor);

	GFX->getDrawUtil()->clearBitmapModulation();
	
	if(mNumberOfBitmaps == 0)
		Con::warnf("No image provided for GuiTextEditSliderBitmapCtrl; do not render");
	else
	{
		// This control needs 4 images in order to render correctly
		if(mTextAreaHit == ArrowUp)
			GFX->getDrawUtil()->drawBitmapStretchSR( mProfile->mTextureObject, RectI(arrowUpStart,arrowUpEnd), mProfile->mBitmapArrayRects[0] );
		else
			GFX->getDrawUtil()->drawBitmapStretchSR( mProfile->mTextureObject, RectI(arrowUpStart,arrowUpEnd), mProfile->mBitmapArrayRects[1] );

		if(mTextAreaHit == ArrowDown)
			GFX->getDrawUtil()->drawBitmapStretchSR( mProfile->mTextureObject, RectI(arrowDownStart,arrowDownEnd), mProfile->mBitmapArrayRects[2] );
		else
			GFX->getDrawUtil()->drawBitmapStretchSR( mProfile->mTextureObject, RectI(arrowDownStart,arrowDownEnd), mProfile->mBitmapArrayRects[3] );
	}
}

void GuiTextEditSliderBitmapCtrl::setBitmap(const char *name)
{
   bool awake = mAwake;
   if(awake)
      onSleep();

   mBitmapName = StringTable->insert(name);
   if(awake)
      onWake();
   setUpdate();
}