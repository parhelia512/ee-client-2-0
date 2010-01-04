//-----------------------------------------------------------------------------
// Torque 3D
//
// Copyright (c) 2001 GarageGames.Com
//-----------------------------------------------------------------------------


//-------------------------------------
//
// Bitmap Button Contrl
// Set 'bitmap' comsole field to base name of bitmaps to use.  This control will
// append '_n' for normal
// append '_h' for hilighted
// append '_d' for depressed
//
// if bitmap cannot be found it will use the default bitmap to render.
//
// if the extent is set to (0,0) in the gui editor and appy hit, this control will
// set it's extent to be exactly the size of the normal bitmap (if present)
//

#include "platform/platform.h"
#include "gui/buttons/guiBitmapButtonCtrl.h"

#include "console/console.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "gui/core/guiDefaultControlRender.h"
#include "gfx/gfxDrawUtil.h"


IMPLEMENT_CONOBJECT(GuiBitmapButtonCtrl);

//-------------------------------------
GuiBitmapButtonCtrl::GuiBitmapButtonCtrl()
{
   mBitmapName = StringTable->insert("");
   setExtent(140, 30);
}


//-------------------------------------
void GuiBitmapButtonCtrl::initPersistFields()
{
   addField("bitmap", TypeFilename, Offset(mBitmapName, GuiBitmapButtonCtrl));
   Parent::initPersistFields();
}


//-------------------------------------
bool GuiBitmapButtonCtrl::onWake()
{
   if (! Parent::onWake())
      return false;
   setActive(true);
   setBitmap(mBitmapName);
   return true;
}


//-------------------------------------
void GuiBitmapButtonCtrl::onSleep()
{
   if (dStricmp(mBitmapName, "texhandle") != 0)
   {
      mTextureNormal = NULL;
      mTextureHilight = NULL;
      mTextureDepressed = NULL;
      mTextureInactive = NULL;
   }

   Parent::onSleep();
}


//-------------------------------------

ConsoleMethod( GuiBitmapButtonCtrl, setBitmap, void, 3, 3, "(filepath name)")
{
   object->setBitmap(argv[2]);
}

//-------------------------------------
void GuiBitmapButtonCtrl::inspectPostApply()
{
   // if the extent is set to (0,0) in the gui editor and appy hit, this control will
   // set it's extent to be exactly the size of the normal bitmap (if present)
   Parent::inspectPostApply();

   if ((getWidth() == 0) && (getHeight() == 0) && mTextureNormal)
   {
      setExtent( mTextureNormal->getWidth(), mTextureNormal->getHeight());
   }
}


//-------------------------------------
void GuiBitmapButtonCtrl::setBitmap(const char *name)
{
   mBitmapName = StringTable->insert(name);
   if(!isAwake())
      return;

   if (*mBitmapName)
   {
      if (dStricmp(mBitmapName, "texhandle") != 0)
      {
         char buffer[1024];
         char *p;
         dStrcpy(buffer, name);
         p = buffer + dStrlen(buffer);

         mTextureNormal = GFXTexHandle(buffer, &GFXDefaultPersistentProfile, avar("%s() - mTextureNormal (line %d)", __FUNCTION__, __LINE__));
         if (!mTextureNormal)
         {
            dStrcpy(p, "_n");
            mTextureNormal = GFXTexHandle(buffer, &GFXDefaultPersistentProfile, avar("%s() - mTextureNormal (line %d)", __FUNCTION__, __LINE__));
         }
         dStrcpy(p, "_h");
         mTextureHilight = GFXTexHandle(buffer, &GFXDefaultPersistentProfile, avar("%s() - mTextureHighlight (line %d)", __FUNCTION__, __LINE__));
         if (!mTextureHilight)
            mTextureHilight = mTextureNormal;
         dStrcpy(p, "_d");
         mTextureDepressed = GFXTexHandle(buffer, &GFXDefaultPersistentProfile, avar("%s() - mTextureDepressed (line %d)", __FUNCTION__, __LINE__));
         if (!mTextureDepressed)
            mTextureDepressed = mTextureHilight;
         dStrcpy(p, "_i");
         mTextureInactive = GFXTexHandle(buffer, &GFXDefaultPersistentProfile, avar("%s() - mTextureInactive (line %d)", __FUNCTION__, __LINE__));
         if (!mTextureInactive)
            mTextureInactive = mTextureNormal;

         if (mTextureNormal.isNull() && mTextureHilight.isNull() && mTextureDepressed.isNull() && mTextureInactive.isNull())
		   {
            Con::warnf("GFXTextureManager::createTexture - Unable to load Texture: %s", buffer);
			   this->setBitmap("core/art/unavailable");
			   return;
		   }
      }
   }
   else
   {
      mTextureNormal = NULL;
      mTextureHilight = NULL;
      mTextureDepressed = NULL;
      mTextureInactive = NULL;
   }
   setUpdate();
}

void GuiBitmapButtonCtrl::setBitmapHandles(GFXTexHandle normal, GFXTexHandle highlighted, GFXTexHandle depressed, GFXTexHandle inactive)
{
   mTextureNormal = normal;
   mTextureHilight = highlighted;
   mTextureDepressed = depressed;
   mTextureInactive = inactive;

   if (!mTextureHilight)
      mTextureHilight = mTextureNormal;
   if (!mTextureDepressed)
      mTextureDepressed = mTextureHilight;
   if (!mTextureInactive)
      mTextureInactive = mTextureNormal;

   if (mTextureNormal.isNull() && mTextureHilight.isNull() && mTextureDepressed.isNull() && mTextureInactive.isNull())
	{
      Con::warnf("GuiBitmapButtonCtrl::setBitmapHandles() - Invalid texture handles");
		setBitmap("core/art/unavailable");
		
      return;
	}

   mBitmapName = StringTable->insert("texhandle");
}


//-------------------------------------
void GuiBitmapButtonCtrl::onRender(Point2I offset, const RectI& updateRect)
{
   enum {
      NORMAL,
      HILIGHT,
      DEPRESSED,
      INACTIVE
   } state = NORMAL;

   if (mActive)
   {
      if (mMouseOver) state = HILIGHT;
      if (mDepressed || mStateOn) state = DEPRESSED;
   }
   else
      state = INACTIVE;

   switch (state)
   {
      case NORMAL:      renderButton(mTextureNormal, offset, updateRect); break;
      case HILIGHT:     renderButton(mTextureHilight ? mTextureHilight : mTextureNormal, offset, updateRect); break;
      case DEPRESSED:   renderButton(mTextureDepressed, offset, updateRect); break;
      case INACTIVE:    renderButton(mTextureInactive ? mTextureInactive : mTextureNormal, offset, updateRect); break;
   }
}

//------------------------------------------------------------------------------

void GuiBitmapButtonCtrl::renderButton(GFXTexHandle &texture, Point2I &offset, const RectI& updateRect)
{
   if (texture)
   {
      RectI rect(offset, getExtent());
      GFX->getDrawUtil()->clearBitmapModulation();
      GFX->getDrawUtil()->drawBitmapStretch(texture, rect);
      renderChildControls( offset, updateRect);
   }
   else
      Parent::onRender(offset, updateRect);
}

//------------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiBitmapButtonTextCtrl);

void GuiBitmapButtonTextCtrl::onRender(Point2I offset, const RectI& updateRect)
{
   enum {
      NORMAL,
      HILIGHT,
      DEPRESSED,
      INACTIVE
   } state = NORMAL;

   if (mActive)
   {
      if (mMouseOver) state = HILIGHT;
      if (mDepressed || mStateOn) state = DEPRESSED;
   }
   else
      state = INACTIVE;

   GFXTexHandle texture;

   switch (state)
   {
      case NORMAL:
         texture = mTextureNormal;
         break;
      case HILIGHT:
         texture = mTextureHilight;
         break;
      case DEPRESSED:
         texture = mTextureDepressed;
         break;
      case INACTIVE:
         texture = mTextureInactive;
         if(!texture)
            texture = mTextureNormal;
         break;
   }
   if (texture)
   {
      RectI rect(offset, getExtent());
      GFX->getDrawUtil()->clearBitmapModulation();
      GFX->getDrawUtil()->drawBitmapStretch(texture, rect);

      Point2I textPos = offset;
      if(mDepressed)
         textPos += Point2I(1,1);

      // Make sure we take the profile's textOffset into account.
      textPos += mProfile->mTextOffset;

      GFX->getDrawUtil()->setBitmapModulation( mProfile->mFontColor );
      renderJustifiedText(textPos, getExtent(), mButtonText);

      renderChildControls( offset, updateRect);
   }
   else
      Parent::onRender(offset, updateRect);
}
