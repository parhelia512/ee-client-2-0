//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/game/guiProgressBitmapCtrl.h"

#include "console/console.h"
#include "console/consoleTypes.h"
#include "gfx/gfxDrawUtil.h"


IMPLEMENT_CONOBJECT(GuiProgressBitmapCtrl);

GuiProgressBitmapCtrl::GuiProgressBitmapCtrl()
{
   mProgress = 0.0f;
	mBitmapName = StringTable->insert("");
   mUseVariable = false;
   mTile = false;
}

void GuiProgressBitmapCtrl::initPersistFields()
{
   addGroup("GuiProgressBitmapCtrl");
   addField( "bitmap",        TypeFilename,  Offset( mBitmapName, GuiProgressBitmapCtrl ) );
   endGroup("GuiProgressBitmapCtrl");
   Parent::initPersistFields();
}

void GuiProgressBitmapCtrl::setBitmap(const char *name)
{
   bool awake = mAwake;
   if(awake)
      onSleep();

   mBitmapName = StringTable->insert(name);
   if(awake)
      onWake();
   setUpdate();
}

const char* GuiProgressBitmapCtrl::getScriptValue()
{
   char * ret = Con::getReturnBuffer(64);
   dSprintf(ret, 64, "%g", mProgress);
   return ret;
}

void GuiProgressBitmapCtrl::setScriptValue(const char *value)
{
   //set the value
   if (! value)
      mProgress = 0.0f;
   else
      mProgress = dAtof(value);

   //validate the value
   mProgress = mClampF(mProgress, 0.f, 1.f);
   setUpdate();
}

void GuiProgressBitmapCtrl::onPreRender()
{
   const char * var = getVariable();
   if(var)
   {
      F32 value = mClampF(dAtof(var), 0.f, 1.f);
      if(value != mProgress)
      {
         mProgress = value;
         setUpdate();
      }
   }
}

void GuiProgressBitmapCtrl::onRender(Point2I offset, const RectI &updateRect)
{	
	RectI ctrlRect(offset, getExtent());
	
	//grab lowest dimension
	if(getHeight() <= getWidth())
		mDim = getHeight();
	else
		mDim = getWidth();
	
	GFX->getDrawUtil()->clearBitmapModulation();

	if(mNumberOfBitmaps == 1)
	{
		//draw the progress with image
		S32 width = (S32)((F32)(getWidth()) * mProgress);
		if (width > 0)
		{
			//drawing stretch bitmap
			RectI progressRect = ctrlRect;
			progressRect.extent.x = width;
			GFX->getDrawUtil()->drawBitmapStretchSR(mProfile->mTextureObject, progressRect, mProfile->mBitmapArrayRects[0]);
		}
	}
	else if(mNumberOfBitmaps >= 3)
	{
		//drawing left-end bitmap
		RectI progressRectLeft(ctrlRect.point.x, ctrlRect.point.y, mDim, mDim);
		GFX->getDrawUtil()->drawBitmapStretchSR(mProfile->mTextureObject, progressRectLeft, mProfile->mBitmapArrayRects[0]);

		//draw the progress with image
		S32 width = (S32)((F32)(getWidth()) * mProgress);
		if (width > mDim)
		{
			//drawing stretch bitmap
			RectI progressRect = ctrlRect;
			progressRect.point.x +=  mDim;
			progressRect.extent.x = (width - mDim - mDim);
			if (progressRect.extent.x < 0)
				progressRect.extent.x = 0;
			GFX->getDrawUtil()->drawBitmapStretchSR(mProfile->mTextureObject, progressRect, mProfile->mBitmapArrayRects[1]);
		
			//drawing right-end bitmap
			RectI progressRectRight(progressRect.point.x + progressRect.extent.x, ctrlRect.point.y, mDim, mDim );
			GFX->getDrawUtil()->drawBitmapStretchSR(mProfile->mTextureObject, progressRectRight, mProfile->mBitmapArrayRects[2]);
		}
	}
	else
		Con::warnf("guiProgressBitmapCtrl only processes an array of bitmaps == 1 or >= 3");

	//if there's a border, draw it
   if (mProfile->mBorder)
      GFX->getDrawUtil()->drawRect(ctrlRect, mProfile->mBorderColor);

   Parent::onRender( offset, updateRect );

   //render the children
   renderChildControls(offset, updateRect);
	
}

bool GuiProgressBitmapCtrl::onWake()
{
   if(!Parent::onWake())
      return false;

	mNumberOfBitmaps = mProfile->constructBitmapArray();

   return true;
}
void GuiProgressBitmapCtrl::onSleep()
{
   Parent::onSleep();
}

ConsoleMethod( GuiProgressBitmapCtrl, setBitmap, void, 3, 3, "(string filename)"
              "Set the bitmap contained in this control.")
{
   object->setBitmap( argv[2] );
}