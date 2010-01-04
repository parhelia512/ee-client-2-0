//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "gui/core/guiControl.h"
#include "console/consoleTypes.h"
#include "T3D/gameConnection.h"
#include "T3D/shapeBase.h"
#include "gfx/gfxDrawUtil.h"


//-----------------------------------------------------------------------------
/// A basic health bar control.
/// This gui displays the damage value of the current PlayerObjectType
/// control object.  The gui can be set to pulse if the health value
/// drops below a set value. This control only works if a server
/// connection exists and it's control object is a PlayerObjectType. If
/// either of these requirements is false, the control is not rendered.
class GuiHealthBarHud : public GuiControl
{
   typedef GuiControl Parent;

   bool     mShowFrame;
   bool     mShowFill;
   bool     mDisplayEnergy;

   ColorF   mFillColor;
   ColorF   mFrameColor;
   ColorF   mDamageFillColor;

   S32      mPulseRate;
   F32      mPulseThreshold;

   F32      mValue;

public:
   GuiHealthBarHud();

   void onRender( Point2I, const RectI &);
   static void initPersistFields();
   DECLARE_CONOBJECT( GuiHealthBarHud );
   DECLARE_CATEGORY( "Gui Game" );
};


//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT( GuiHealthBarHud );

GuiHealthBarHud::GuiHealthBarHud()
{
   mShowFrame = mShowFill = true;
   mDisplayEnergy = false;
   mFillColor.set(0, 0, 0, 0.5);
   mFrameColor.set(0, 1, 0, 1);
   mDamageFillColor.set(0, 1, 0, 1);

   mPulseRate = 0;
   mPulseThreshold = 0.3f;
   mValue = 0.2f;
}

void GuiHealthBarHud::initPersistFields()
{
   addGroup("Colors");		
   addField( "fillColor", TypeColorF, Offset( mFillColor, GuiHealthBarHud ) );
   addField( "frameColor", TypeColorF, Offset( mFrameColor, GuiHealthBarHud ) );
   addField( "damageFillColor", TypeColorF, Offset( mDamageFillColor, GuiHealthBarHud ) );
   endGroup("Colors");		

   addGroup("Pulse");		
   addField( "pulseRate", TypeS32, Offset( mPulseRate, GuiHealthBarHud ) );
   addField( "pulseThreshold", TypeF32, Offset( mPulseThreshold, GuiHealthBarHud ) );
   endGroup("Pulse");		

   addGroup("Misc");		
   addField( "showFill", TypeBool, Offset( mShowFill, GuiHealthBarHud ) );
   addField( "showFrame", TypeBool, Offset( mShowFrame, GuiHealthBarHud ) );
   addField( "displayEnergy", TypeBool, Offset( mDisplayEnergy, GuiHealthBarHud ) );
   endGroup("Misc");

   Parent::initPersistFields();
}


//-----------------------------------------------------------------------------
/**
   Gui onRender method.
   Renders a health bar with filled background and border.
*/
void GuiHealthBarHud::onRender(Point2I offset, const RectI &updateRect)
{
   // Must have a connection and player control object
   GameConnection* conn = GameConnection::getConnectionToServer();
   if (!conn)
      return;
   ShapeBase* control = dynamic_cast<ShapeBase*>(conn->getControlObject());
   if (!control || !(control->getType() & PlayerObjectType))
      return;

   if(mDisplayEnergy)
   {
      mValue = control->getEnergyValue();
   }
   else
   {
      // We'll just grab the damage right off the control object.
      // Damage value 0 = no damage.
      mValue = 1 - control->getDamageValue();
   }


   // Background first
   if (mShowFill)
      GFX->getDrawUtil()->drawRectFill(updateRect, mFillColor);

   // Pulse the damage fill if it's below the threshold
   if (mPulseRate != 0)
      if (mValue < mPulseThreshold) 
      {
         U32 time = Platform::getVirtualMilliseconds();
         F32 alpha = 2.0f * F32(time % mPulseRate) / F32(mPulseRate);
         mDamageFillColor.alpha = (alpha > 1.0f)? 2.0f - alpha: alpha;
      }
      else
         mDamageFillColor.alpha = 1;

   // Render damage fill %
   RectI rect(updateRect);
   if(getWidth() > getHeight())
      rect.extent.x = (S32)(rect.extent.x * mValue);
   else
   {
      S32 bottomY = rect.point.y + rect.extent.y;
      rect.extent.y = (S32)(rect.extent.y * mValue);
      rect.point.y = bottomY - rect.extent.y;
   }
   GFX->getDrawUtil()->drawRectFill(rect, mDamageFillColor);

   // Border last
   if (mShowFrame)
      GFX->getDrawUtil()->drawRect(updateRect, mFrameColor);
}
