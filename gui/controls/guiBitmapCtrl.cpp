//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/controls/guiBitmapCtrl.h"

#include "console/console.h"
#include "console/consoleTypes.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"


IMPLEMENT_CONOBJECT(GuiBitmapCtrl);

GuiBitmapCtrl::GuiBitmapCtrl(void)
 : mBitmapName(),
   mStartPoint( 0, 0 ),
   mWrap( false )
{	
}

bool GuiBitmapCtrl::setBitmapName( void *obj, const char *data )
{
   // Prior to this, you couldn't do bitmap.bitmap = "foo.jpg" and have it work.
   // With protected console types you can now call the setBitmap function and
   // make it load the image.
   static_cast<GuiBitmapCtrl *>( obj )->setBitmap( data );

   // Return false because the setBitmap method will assign 'mBitmapName' to the
   // argument we are specifying in the call.
   return false;
}

void GuiBitmapCtrl::initPersistFields()
{
   addGroup("GuiBitmapCtrl");		
      addProtectedField( "bitmap", TypeImageFilename, Offset( mBitmapName, GuiBitmapCtrl ), &setBitmapName, &defaultProtectedGetFn, "" );
      addField("wrap",   TypeBool,     Offset(mWrap,       GuiBitmapCtrl));
   endGroup("GuiBitmapCtrl");

   Parent::initPersistFields();
}

ConsoleMethod( GuiBitmapCtrl, setValue, void, 4, 4, "(int xAxis, int yAxis)"
              "Set the offset of the bitmap.")
{
   object->setValue(dAtoi(argv[2]), dAtoi(argv[3]));
}

ConsoleMethod( GuiBitmapCtrl, setBitmap, void, 3, 4, "(string filename, bool resize=false)"
               "Set the bitmap displayed in the control. Note that it is limited in size, to 256x256.")
{
   char fileName[1024];
   Con::expandScriptFilename(fileName, sizeof(fileName), argv[2]);
   object->setBitmap(fileName, argc > 3 ? dAtob( argv[3] ) : false );
}

bool GuiBitmapCtrl::onWake()
{
   if (! Parent::onWake())
      return false;
   setActive(true);
   setBitmap(mBitmapName);
   return true;
}

void GuiBitmapCtrl::onSleep()
{
   if ( !mBitmapName.equal("texhandle", String::NoCase) )
      mTextureObject = NULL;

   Parent::onSleep();
}

//-------------------------------------
void GuiBitmapCtrl::inspectPostApply()
{
   // if the extent is set to (0,0) in the gui editor and appy hit, this control will
   // set it's extent to be exactly the size of the bitmap (if present)
   Parent::inspectPostApply();

   if (!mWrap && (getExtent().x == 0) && (getExtent().y == 0) && mTextureObject)
   {
      setExtent( mTextureObject->getWidth(), mTextureObject->getHeight());
   }
}

void GuiBitmapCtrl::setBitmap( const char *name, bool resize )
{
   mBitmapName = name;
   if ( !isAwake() )
      return;

   if ( mBitmapName.isNotEmpty() )
	{
      if ( !mBitmapName.equal("texhandle", String::NoCase) )
		   mTextureObject.set( mBitmapName, &GFXDefaultGUIProfile, avar("%s() - mTextureObject (line %d)", __FUNCTION__, __LINE__) );

      // Resize the control to fit the bitmap
      if ( mTextureObject && resize )
      {
         setExtent( mTextureObject->getWidth(), mTextureObject->getHeight() );
         updateSizing();
      }
   }
   else
      mTextureObject = NULL;

   setUpdate();
}

void GuiBitmapCtrl::updateSizing()
{
   if(!getParent())
      return;
   // updates our bounds according to our horizSizing and verSizing rules
   RectI fakeBounds( getPosition(), getParent()->getExtent());
   parentResized( fakeBounds, fakeBounds);
}

void GuiBitmapCtrl::setBitmapHandle(GFXTexHandle handle, bool resize)
{
   mTextureObject = handle;

   mBitmapName = String("texhandle");

   // Resize the control to fit the bitmap
   if (resize) 
   {
      setExtent(mTextureObject->getWidth(), mTextureObject->getHeight());
      updateSizing();
   }
}


void GuiBitmapCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   if (mTextureObject)
   {
      GFX->getDrawUtil()->clearBitmapModulation();
		if(mWrap)
		{
         // We manually draw each repeat because non power of two textures will 
         // not tile correctly when rendered with GFX->drawBitmapTile(). The non POT
         // bitmap will be padded by the hardware, and we'll see lots of slack
         // in the texture. So... lets do what we must: draw each repeat by itself:
 			GFXTextureObject* texture = mTextureObject;
			RectI srcRegion;
			RectI dstRegion;
			float xdone = ((float)getExtent().x/(float)texture->mBitmapSize.x)+1;
			float ydone = ((float)getExtent().y/(float)texture->mBitmapSize.y)+1;

			int xshift = mStartPoint.x%texture->mBitmapSize.x;
			int yshift = mStartPoint.y%texture->mBitmapSize.y;
			for(int y = 0; y < ydone; ++y)
				for(int x = 0; x < xdone; ++x)
				{
		 			srcRegion.set(0,0,texture->mBitmapSize.x,texture->mBitmapSize.y);
  					dstRegion.set( ((texture->mBitmapSize.x*x)+offset.x)-xshift,
								      ((texture->mBitmapSize.y*y)+offset.y)-yshift,
								      texture->mBitmapSize.x,
								      texture->mBitmapSize.y);
               GFX->getDrawUtil()->drawBitmapStretchSR(texture,dstRegion, srcRegion, GFXBitmapFlip_None, GFXTextureFilterLinear);
				}

		}
		else
      {
         RectI rect(offset, getExtent());
         GFX->getDrawUtil()->drawBitmapStretch(mTextureObject, rect, GFXBitmapFlip_None, GFXTextureFilterLinear);
      }
   }

   if (mProfile->mBorder || !mTextureObject)
   {
      RectI rect(offset.x, offset.y, getExtent().x, getExtent().y);
      GFX->getDrawUtil()->drawRect(rect, mProfile->mBorderColor);
   }

   renderChildControls(offset, updateRect);
}

void GuiBitmapCtrl::setValue(S32 x, S32 y)
{
   if (mTextureObject)
   {
		x += mTextureObject->getWidth() / 2;
		y += mTextureObject->getHeight() / 2;
  	}
  	while (x < 0)
  		x += 256;
  	mStartPoint.x = x % 256;

  	while (y < 0)
  		y += 256;
  	mStartPoint.y = y % 256;
}
