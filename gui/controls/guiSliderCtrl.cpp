//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "console/consoleTypes.h"
#include "gfx/gfxTextureManager.h"
#include "gui/controls/guiSliderCtrl.h"
#include "gui/core/guiDefaultControlRender.h"
#include "platform/event.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxDrawUtil.h"
#include "sfx/sfxSystem.h"

IMPLEMENT_CONOBJECT(GuiSliderCtrl);

//----------------------------------------------------------------------------
GuiSliderCtrl::GuiSliderCtrl(void)
{
   mActive = true;
   mRange.set( 0.0f, 1.0f );
   mTicks = 10;
   mValue = 0.5f;
   mThumbSize.set(8,20);
   mShiftPoint = 5;
   mShiftExtent = 10;
   mIncAmount = 0.0f;
   mDisplayValue = false;
   mMouseOver = false;
   mDepressed = false;
}

//----------------------------------------------------------------------------
void GuiSliderCtrl::initPersistFields()
{
   addGroup( "Slider" );
   addField("range", TypePoint2F,   Offset(mRange, GuiSliderCtrl));
   addField("ticks", TypeS32,       Offset(mTicks, GuiSliderCtrl));
   addField("value", TypeF32,       Offset(mValue, GuiSliderCtrl));
   endGroup( "Slider" );

   Parent::initPersistFields();
}

//----------------------------------------------------------------------------
ConsoleMethod( GuiSliderCtrl, getValue, F32, 2, 2, "Get the position of the slider.")
{
   return object->getValue();
}

//----------------------------------------------------------------------------
ConsoleMethod( GuiSliderCtrl, setValue, void, 3, 3, "( float pos ) - Set position of the slider." )
{
   object->setValue( dAtof( argv[ 2 ] ) );
}

//----------------------------------------------------------------------------
void GuiSliderCtrl::setValue(F32 val)
{
   mValue = val;
   updateThumb(mValue, false, false, false);
}

//----------------------------------------------------------------------------
void GuiSliderCtrl::setActive( bool value )
{
   if( !value && mDepressed )
   {
      // We're in the middle of a drag.  Finish it here as once we've
      // been deactivated, we are not going to see a mouse-up event.

      mDepressed = false;
      mouseUnlock();
      execConsoleCallback();
   }

   Parent::setActive( value );
}

//----------------------------------------------------------------------------
bool GuiSliderCtrl::onWake()
{
   if (! Parent::onWake())
      return false;
      
   F32 value;
   if( mConsoleVariable[ 0 ] )
      value = getFloatVariable();
   else
      value = mValue;

   mValue = mClampF( value, mRange.x, mRange.y );

   // mouse scroll increment percentage is 5% of the range
   mIncAmount = ((mRange.y - mRange.x) * 0.05);

   if(mThumbSize.y + mProfile->mFont->getHeight()-4 <= getExtent().y)
      mDisplayValue = true;
   else
      mDisplayValue = false;

   updateThumb( mValue, false, true );

   mHasTexture = mProfile->constructBitmapArray() >= NumBitmaps;  
   if( mHasTexture )
      mBitmapBounds = mProfile->mBitmapArrayRects.address();

   return true;
}

//----------------------------------------------------------------------------
void GuiSliderCtrl::onMouseDown(const GuiEvent &event)
{
   if ( !mActive || !mAwake || !mVisible )
      return;

   mouseLock();
   setFirstResponder();
   mDepressed = true;

   Point2I curMousePos = globalToLocalCoord(event.mousePoint);
   F32 value;
   if (getWidth() >= getHeight())
      value = F32(curMousePos.x-mShiftPoint) / F32(getWidth()-mShiftExtent)*(mRange.y-mRange.x) + mRange.x;
   else
      value = F32(curMousePos.y) / F32(getHeight())*(mRange.y-mRange.x) + mRange.x;
   
   updateThumb( value, ( event.modifier & SI_SHIFT ) );
}

//----------------------------------------------------------------------------
void GuiSliderCtrl::onMouseDragged(const GuiEvent &event)
{
   if ( !mActive || !mAwake || !mVisible )
      return;

   Point2I curMousePos = globalToLocalCoord(event.mousePoint);
   F32 value;
   if (getWidth() >= getHeight())
      value = F32(curMousePos.x-mShiftPoint) / F32(getWidth()-mShiftExtent)*(mRange.y-mRange.x) + mRange.x;
   else
      value = F32(curMousePos.y) / F32(getHeight())*(mRange.y-mRange.x) + mRange.x;

   if (value > mRange.y)
      value = mRange.y;
   else if (value < mRange.x)
      value = mRange.x;

   if (!(event.modifier & SI_SHIFT) && mTicks > 2) {
      // If the shift key is held, snap to the nearest tick, if any are being drawn

      F32 tickStep = (mRange.y - mRange.x) / F32(mTicks + 1);

      F32 tickSteps = (value - mRange.x) / tickStep;
      S32 actualTick = S32(tickSteps + 0.5);

      value = actualTick * tickStep + mRange.x;
      AssertFatal(value <= mRange.y && value >= mRange.x, "Error, out of bounds value generated from shift-snap of slider");
   }

   updateThumb( value, ( event.modifier & SI_SHIFT ) );

   Con::executef(this, "onMouseDragged");
}

//----------------------------------------------------------------------------
void GuiSliderCtrl::onMouseUp(const GuiEvent &)
{
   if ( !mActive || !mAwake || !mVisible )
      return;

   if( mDepressed )
   {
      mDepressed = false;
      execConsoleCallback();
   }

	mouseUnlock();
}   

void GuiSliderCtrl::onMouseEnter(const GuiEvent &event)
{
   setUpdate();
   if(isMouseLocked())
   {
      mDepressed = true;
      mMouseOver = true;
   }
   else
   {
      if ( mActive && mProfile->mSoundButtonOver )
      {
         //F32 pan = (F32(event.mousePoint.x)/F32(getRoot()->getWidth())*2.0f-1.0f)*0.8f;
         SFX->playOnce(mProfile->mSoundButtonOver);
      }
      mMouseOver = true;
   }
}

void GuiSliderCtrl::onMouseLeave(const GuiEvent &)
{
   setUpdate();
   if(isMouseLocked())
      mDepressed = false;
   mMouseOver = false;
}
//----------------------------------------------------------------------------
bool GuiSliderCtrl::onMouseWheelUp(const GuiEvent &event)
{
   if ( !mActive || !mAwake || !mVisible )
      return Parent::onMouseWheelUp(event);

   mValue += mIncAmount;
   updateThumb( mValue, ( event.modifier & SI_SHIFT ) );

   return true;
}

bool GuiSliderCtrl::onMouseWheelDown(const GuiEvent &event)
{
   if ( !mActive || !mAwake || !mVisible )
      return Parent::onMouseWheelUp(event);

   mValue -= mIncAmount;
   updateThumb( mValue, ( event.modifier & SI_SHIFT ) );

   return true;
}
//----------------------------------------------------------------------------
void GuiSliderCtrl::updateThumb( F32 _value, bool snap, bool onWake, bool doCallback )
{
   if (snap && mTicks > 1) {
      // If the shift key is held, snap to the nearest tick, if any are being drawn

      F32 tickStep = (mRange.y - mRange.x) / F32(mTicks + 1);

      F32 tickSteps = (_value - mRange.x) / tickStep;
      S32 actualTick = S32(tickSteps + 0.5);

      _value = actualTick * tickStep + mRange.x;
   }
   
   mValue = _value;
   // clamp the thumb to legal values
   if (mValue < mRange.x)  mValue = mRange.x;
   if (mValue > mRange.y)  mValue = mRange.y;

   Point2I ext = getExtent();
	ext.x -= ( mShiftExtent + mThumbSize.x ) / 2;
   // update the bounding thumb rect
   if (getWidth() >= getHeight())
   {  // HORZ thumb
      S32 mx = (S32)((F32(ext.x) * (mValue-mRange.x) / (mRange.y-mRange.x)));
      S32 my = ext.y/2;
      if(mDisplayValue)
         my = mThumbSize.y/2;

      mThumb.point.x  = mx - (mThumbSize.x/2);
      mThumb.point.y  = my - (mThumbSize.y/2);
      mThumb.extent   = mThumbSize;
   }
   else
   {  // VERT thumb
      S32 mx = ext.x/2;
      S32 my = (S32)((F32(ext.y) * (mValue-mRange.x) / (mRange.y-mRange.x)));
      mThumb.point.x  = mx - (mThumbSize.y/2);
      mThumb.point.y  = my - (mThumbSize.x/2);
      mThumb.extent.x = mThumbSize.y;
      mThumb.extent.y = mThumbSize.x;
   }
   setFloatVariable(mValue);
   setUpdate();

   // Use the alt console command if you want to continually update:
   if ( !onWake && doCallback )
      execAltConsoleCallback();
}

//----------------------------------------------------------------------------
void GuiSliderCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   Point2I pos(offset.x+mShiftPoint, offset.y);
   Point2I ext(getWidth() - mShiftExtent, getHeight());
   RectI thumb = mThumb;

   if( mHasTexture )
   {
      if(mTicks > 0)
      {
         // TODO: tick marks should be positioned based on the bitmap dimensions.
         Point2I mid(ext.x, ext.y/2);
         Point2I oldpos = pos;
         pos += Point2I(1, 0);

         PrimBuild::color4f( 0.f, 0.f, 0.f, 1.f );
         PrimBuild::begin( GFXLineList, ( mTicks + 2 ) * 2 );
         // tick marks
         for (U32 t = 0; t <= (mTicks+1); t++)
         {
            S32 x = (S32)(F32(mid.x+1)/F32(mTicks+1)*F32(t)) + pos.x;
            S32 y = pos.y + mid.y;
            PrimBuild::vertex2i(x, y + mShiftPoint);
            PrimBuild::vertex2i(x, y + mShiftPoint*2 + 2);
         }
         PrimBuild::end();
         // TODO: it would be nice, if the primitive builder were a little smarter,
         // so that we could change colors midstream.
         PrimBuild::color4f(0.9f, 0.9f, 0.9f, 1.0f);
         PrimBuild::begin( GFXLineList, ( mTicks + 2 ) * 2 );
         // tick marks
         for (U32 t = 0; t <= (mTicks+1); t++)
         {
            S32 x = (S32)(F32(mid.x+1)/F32(mTicks+1)*F32(t)) + pos.x + 1;
            S32 y = pos.y + mid.y + 1;
            PrimBuild::vertex2i(x, y + mShiftPoint );
            PrimBuild::vertex2i(x, y + mShiftPoint * 2 + 3);
         }
         PrimBuild::end();
         pos = oldpos;
      }

      S32 index = SliderButtonNormal;
      if(mMouseOver)
         index = SliderButtonHighlight;
      GFX->getDrawUtil()->clearBitmapModulation();

      //left border
      GFX->getDrawUtil()->drawBitmapSR(mProfile->mTextureObject, Point2I(offset.x,offset.y), mBitmapBounds[SliderLineLeft]);
      //right border
      GFX->getDrawUtil()->drawBitmapSR(mProfile->mTextureObject, Point2I(offset.x + getWidth() - mBitmapBounds[SliderLineRight].extent.x, offset.y), mBitmapBounds[SliderLineRight]);


      //draw our center piece to our slider control's border and stretch it
      RectI destRect;	
      destRect.point.x = offset.x + mBitmapBounds[SliderLineLeft].extent.x;
      destRect.extent.x = getWidth() - mBitmapBounds[SliderLineLeft].extent.x - mBitmapBounds[SliderLineRight].extent.x;
      destRect.point.y = offset.y;
      destRect.extent.y = mBitmapBounds[SliderLineCenter].extent.y;

      RectI stretchRect;
      stretchRect = mBitmapBounds[SliderLineCenter];
      stretchRect.inset(1,0);

      GFX->getDrawUtil()->drawBitmapStretchSR(mProfile->mTextureObject, destRect, stretchRect);

      //draw our control slider button	
      thumb.point += pos;
      GFX->getDrawUtil()->drawBitmapSR(mProfile->mTextureObject,Point2I(thumb.point.x,offset.y ),mBitmapBounds[index]);

   }
   else if (getWidth() >= getHeight())
   {
      Point2I mid(ext.x, ext.y/2);
      if(mDisplayValue)
         mid.set(ext.x, mThumbSize.y/2);

      PrimBuild::color4f( 0.f, 0.f, 0.f, 1.f );
      PrimBuild::begin( GFXLineList, ( mTicks + 2 ) * 2 + 2);
         // horz rule
         PrimBuild::vertex2i( pos.x, pos.y + mid.y );
         PrimBuild::vertex2i( pos.x + mid.x, pos.y + mid.y );

         // tick marks
         for( U32 t = 0; t <= ( mTicks + 1 ); t++ )
         {
            S32 x = (S32)( F32( mid.x - 1 ) / F32( mTicks + 1 ) * F32( t ) );
            PrimBuild::vertex2i( pos.x + x, pos.y + mid.y - mShiftPoint );
            PrimBuild::vertex2i( pos.x + x, pos.y + mid.y + mShiftPoint );
         }
         PrimBuild::end();
   }
   else
   {
      Point2I mid(ext.x/2, ext.y);

      PrimBuild::color4f( 0.f, 0.f, 0.f, 1.f );
      PrimBuild::begin( GFXLineList, ( mTicks + 2 ) * 2 + 2);
         // horz rule
         PrimBuild::vertex2i( pos.x + mid.x, pos.y );
         PrimBuild::vertex2i( pos.x + mid.x, pos.y + mid.y );

         // tick marks
         for( U32 t = 0; t <= ( mTicks + 1 ); t++ )
         {
            S32 y = (S32)( F32( mid.y - 1 ) / F32( mTicks + 1 ) * F32( t ) );
            PrimBuild::vertex2i( pos.x + mid.x - mShiftPoint, pos.y + y );
            PrimBuild::vertex2i( pos.x + mid.x + mShiftPoint, pos.y + y );
         }
         PrimBuild::end();
      mDisplayValue = false;
   }
   // draw the thumb
   thumb.point += pos;
   renderRaisedBox(thumb, mProfile);

   if(mDisplayValue)
   {
   	char buf[20];
  		dSprintf(buf,sizeof(buf),"%0.3f",mValue);

   	Point2I textStart = thumb.point;

      S32 txt_w = mProfile->mFont->getStrWidth((const UTF8 *)buf);

   	textStart.x += (S32)((thumb.extent.x/2.0f));
   	textStart.y += thumb.extent.y - 2; //19
   	textStart.x -=	(txt_w/2);
   	if(textStart.x	< offset.x)
   		textStart.x = offset.x;
   	else if(textStart.x + txt_w > offset.x+getWidth())
   		textStart.x -=((textStart.x + txt_w) - (offset.x+getWidth()));

    	GFX->getDrawUtil()->setBitmapModulation(mProfile->mFontColor);
    	GFX->getDrawUtil()->drawText(mProfile->mFont, textStart, buf, mProfile->mFontColors);
   }
   renderChildControls(offset, updateRect);
}

