//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "console/console.h"
#include "console/consoleTypes.h"
#include "gfx/bitmap/gBitmap.h"
#include "gui/core/guiControl.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTextureHandle.h"
#include "gfx/gfxDrawUtil.h"


class GuiChunkedBitmapCtrl : public GuiControl
{
private:
   typedef GuiControl Parent;
   void renderRegion(const Point2I &offset, const Point2I &extent);

protected:
   StringTableEntry mBitmapName;
   GFXTexHandle mTexHandle;
   bool  mUseVariable;
   bool  mTile;

public:
   //creation methods
   DECLARE_CONOBJECT(GuiChunkedBitmapCtrl);
   DECLARE_CATEGORY( "Gui Images" );
   
   GuiChunkedBitmapCtrl();
   static void initPersistFields();

   //Parental methods
   bool onWake();
   void onSleep();

   void setBitmap(const char *name);

   void onRender(Point2I offset, const RectI &updateRect);
};

IMPLEMENT_CONOBJECT(GuiChunkedBitmapCtrl);

void GuiChunkedBitmapCtrl::initPersistFields()
{
   addGroup("GuiChunkedBitmapCtrl");		
   addField( "bitmap",        TypeFilename,  Offset( mBitmapName, GuiChunkedBitmapCtrl ) );
   addField( "useVariable",   TypeBool,      Offset( mUseVariable, GuiChunkedBitmapCtrl ) );
   addField( "tile",          TypeBool,      Offset( mTile, GuiChunkedBitmapCtrl ) );
   endGroup("GuiChunkedBitmapCtrl");
   Parent::initPersistFields();
}

ConsoleMethod( GuiChunkedBitmapCtrl, setBitmap, void, 3, 3, "(string filename)"
              "Set the bitmap contained in this control.")
{
   object->setBitmap( argv[2] );
}

GuiChunkedBitmapCtrl::GuiChunkedBitmapCtrl()
{
   mBitmapName = StringTable->insert("");
   mUseVariable = false;
   mTile = false;
}

void GuiChunkedBitmapCtrl::setBitmap(const char *name)
{
   bool awake = mAwake;
   if(awake)
      onSleep();

   mBitmapName = StringTable->insert(name);
   if(awake)
      onWake();
   setUpdate();
}

bool GuiChunkedBitmapCtrl::onWake()
{
   if(!Parent::onWake())
      return false;

   if( !mTexHandle
       && ( ( mBitmapName && mBitmapName[ 0 ] )
            || ( mUseVariable && mConsoleVariable && mConsoleVariable[ 0 ] ) ) )
   {
      if ( mUseVariable )
         mTexHandle.set( Con::getVariable( mConsoleVariable ), &GFXDefaultGUIProfile, avar("%s() - mTexHandle (line %d)", __FUNCTION__, __LINE__) );
      else
         mTexHandle.set( mBitmapName, &GFXDefaultGUIProfile, avar("%s() - mTexHandle (line %d)", __FUNCTION__, __LINE__) );
   }

   return true;
}

void GuiChunkedBitmapCtrl::onSleep()
{
   mTexHandle = NULL;
   Parent::onSleep();
}

void GuiChunkedBitmapCtrl::renderRegion(const Point2I &offset, const Point2I &extent)
{
/*
   U32 widthCount = mTexHandle.getTextureCountWidth();
   U32 heightCount = mTexHandle.getTextureCountHeight();
   if(!widthCount || !heightCount)
      return;

   F32 widthScale = F32(extent.x) / F32(mTexHandle.getWidth());
   F32 heightScale = F32(extent.y) / F32(mTexHandle.getHeight());
   GFX->setBitmapModulation(ColorF(1,1,1));
   for(U32 i = 0; i < widthCount; i++)
   {
      for(U32 j = 0; j < heightCount; j++)
      {
         GFXTexHandle t = mTexHandle.getSubTexture(i, j);
         RectI stretchRegion;
         stretchRegion.point.x = (S32)(i * 256 * widthScale  + offset.x);
         stretchRegion.point.y = (S32)(j * 256 * heightScale + offset.y);
         if(i == widthCount - 1)
            stretchRegion.extent.x = extent.x + offset.x - stretchRegion.point.x;
         else
            stretchRegion.extent.x = (S32)((i * 256 + t.getWidth() ) * widthScale  + offset.x - stretchRegion.point.x);
         if(j == heightCount - 1)
            stretchRegion.extent.y = extent.y + offset.y - stretchRegion.point.y;
         else
            stretchRegion.extent.y = (S32)((j * 256 + t.getHeight()) * heightScale + offset.y - stretchRegion.point.y);
         GFX->drawBitmapStretch(t, stretchRegion);
      }
   }
*/
}


void GuiChunkedBitmapCtrl::onRender(Point2I offset, const RectI &updateRect)
{

   if( mTexHandle )
   {
      RectI boundsRect( offset, getExtent());
      GFX->getDrawUtil()->drawBitmapStretch( mTexHandle, boundsRect, GFXBitmapFlip_None, GFXTextureFilterLinear );
   }

   renderChildControls(offset, updateRect);
}
