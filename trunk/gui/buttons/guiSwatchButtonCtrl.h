//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUISWATCHBUTTONCTRL_H_
#define _GUISWATCHBUTTONCTRL_H_

#ifndef _GUIBUTTONBASECTRL_H_
#include "gui/buttons/guiButtonBaseCtrl.h"
#endif

class GuiSwatchButtonCtrl : public GuiButtonBaseCtrl
{
   typedef GuiButtonBaseCtrl Parent;

public:
   
   GuiSwatchButtonCtrl();

   DECLARE_CONOBJECT( GuiSwatchButtonCtrl );

   // ConsoleObject...
   static void initPersistFields();

   // GuiControl...
   bool onWake();
   void onRender(Point2I offset, const RectI &updateRect);

   // GuiSwatchButtonCtrl...
   void setColor( const ColorF &color ) { mSwatchColor = color; }

protected:
   
   ColorF mSwatchColor;
   GFXTexHandle mGrid;
};

#endif // _GUISWATCHBUTTONCTRL_H_
