//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _GUISEPARATORCTRL_H_
#define _GUISEPARATORCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

/// Renders a separator line with optional text.
class GuiSeparatorCtrl : public GuiControl
{
private:
   typedef GuiControl Parent;

public:
   bool  mInvisible;
   StringTableEntry mText;
   S32   mTextLeftMargin;
   S32   mMargin;
   S32   mSeparatorType;

   enum separatorTypeOptions
   {
      separatorTypeVertical = 0, ///< Draw Vertical Separator
      separatorTypeHorizontal    ///< Horizontal Separator
   };

   //creation methods
   DECLARE_CONOBJECT(GuiSeparatorCtrl);
   DECLARE_CATEGORY( "Gui Other" );
   GuiSeparatorCtrl();

   static void initPersistFields();

   void onRender(Point2I offset, const RectI &updateRect);
};

#endif
