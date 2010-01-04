//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _GUI_INSPECTOR_TYPES_H_
#define _GUI_INSPECTOR_TYPES_H_

#ifndef _GUI_INSPECTOR_H_
#include "gui/editor/guiInspector.h"
#endif

#ifndef _GUI_INSPECTOR_FIELD_H_
#include "gui/editor/inspector/field.h"
#endif

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

#ifndef _GUICHECKBOXCTRL_H_
#include "gui/buttons/guiCheckBoxCtrl.h"
#endif

#ifndef _GUIBITMAPBUTTON_H_
#include "gui/buttons/guiBitmapButtonCtrl.h"
#endif

class GuiPopUpMenuCtrl;

/// A base class for other inspector field types which
/// wish to display a popup/dropdown menu.
class GuiInspectorTypeMenuBase : public GuiInspectorField
{
private:
   typedef GuiInspectorField Parent;
public:

   DECLARE_CONOBJECT(GuiInspectorTypeMenuBase);

   //-----------------------------------------------------------------------------
   // Override able methods for custom edit fields
   //-----------------------------------------------------------------------------
   virtual GuiControl* constructEditControl();
   virtual void        setValue( StringTableEntry newValue );
   virtual void        _populateMenu( GuiPopUpMenuCtrl *menu );
};

//-----------------------------------------------------------------------------
// TypeEnum GuiInspectorField Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeEnum : public GuiInspectorTypeMenuBase
{
private:
   typedef GuiInspectorTypeMenuBase Parent;
public:
   DECLARE_CONOBJECT(GuiInspectorTypeEnum);
   static void consoleInit();

   virtual void _populateMenu( GuiPopUpMenuCtrl *menu );
};

//-----------------------------------------------------------------------------
// TypeCubemapName GuiInspectorField Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeCubemapName : public GuiInspectorTypeMenuBase
{
private:
   typedef GuiInspectorTypeMenuBase Parent;
public:
   DECLARE_CONOBJECT(GuiInspectorTypeCubemapName);
   static void consoleInit();

   virtual void _populateMenu( GuiPopUpMenuCtrl *menu );
};

//--------------------------------------------------------------------------------
// TypeMaterialName GuiInspectorField Class
//--------------------------------------------------------------------------------
class GuiBitmapButtonCtrl;

class GuiInspectorTypeMaterialName : public GuiInspectorField
{
   typedef GuiInspectorField Parent;

public:

   GuiInspectorTypeMaterialName();

   DECLARE_CONOBJECT(GuiInspectorTypeMaterialName);
	static void consoleInit();

   GuiBitmapButtonCtrl *mBrowseButton;
   RectI mBrowseRect;

   //-----------------------------------------------------------------------------
   // Override able methods for custom edit fields
   //-----------------------------------------------------------------------------
   virtual GuiControl*        constructEditControl();
   virtual bool               updateRects();
};

class GuiInspectorTypeRegularMaterialName : public GuiInspectorTypeMaterialName
{
   typedef GuiInspectorTypeMaterialName Parent;
public:
   GuiInspectorTypeRegularMaterialName() {}
   DECLARE_CONOBJECT(GuiInspectorTypeRegularMaterialName);
   static void consoleInit();
   virtual void _populateMenu( GuiPopUpMenuCtrl *menu );
};

//-----------------------------------------------------------------------------
// GuiInspectorTypeGuiProfile Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeGuiProfile : public GuiInspectorTypeMenuBase
{
private:
   typedef GuiInspectorTypeMenuBase Parent;
public:
   DECLARE_CONOBJECT(GuiInspectorTypeGuiProfile);
   static void consoleInit();

   virtual void _populateMenu( GuiPopUpMenuCtrl *menu );
};

//-----------------------------------------------------------------------------
// GuiInspectorTypeCheckBox Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeCheckBox : public GuiInspectorField
{
private:
   typedef GuiInspectorField Parent;
public:
   DECLARE_CONOBJECT(GuiInspectorTypeCheckBox);
   static void consoleInit();

   //-----------------------------------------------------------------------------
   // Override able methods for custom edit fields
   //-----------------------------------------------------------------------------
   virtual GuiControl* constructEditControl();
   virtual void setValue( StringTableEntry newValue );
   virtual const char* getValue();
};

//-----------------------------------------------------------------------------
// TypeCommand GuiInspectorField Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeCommand : public GuiInspectorField
{
private:
   typedef GuiInspectorField Parent;
   StringTableEntry mTextEditorCommand;
   void _setCommand( GuiButtonCtrl *ctrl, StringTableEntry command );
public:
   DECLARE_CONOBJECT(GuiInspectorTypeCommand);
   GuiInspectorTypeCommand();
   static void consoleInit();

   //-----------------------------------------------------------------------------
   // Override able methods for custom edit fields
   //-----------------------------------------------------------------------------
   virtual GuiControl*        constructEditControl();
   virtual void               setValue( StringTableEntry data );
};

//-----------------------------------------------------------------------------
// TypeFileName GuiInspectorField Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeFileName : public GuiInspectorField
{
private:
   typedef GuiInspectorField Parent;
public:
   DECLARE_CONOBJECT(GuiInspectorTypeFileName);
   static void consoleInit();

   SimObjectPtr<GuiButtonCtrl>   mBrowseButton;
   RectI mBrowseRect;

   //-----------------------------------------------------------------------------
   // Override able methods for custom edit fields
   //-----------------------------------------------------------------------------
   virtual GuiControl*        constructEditControl();
   virtual bool               resize(const Point2I &newPosition, const Point2I &newExtent);
   virtual bool               updateRects();
   virtual void               updateValue();
};


//-----------------------------------------------------------------------------
// TypeImageFileName GuiInspectorField Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeImageFileName : public GuiInspectorTypeFileName
{
   typedef GuiInspectorTypeFileName Parent;
public:

   DECLARE_CONOBJECT(GuiInspectorTypeImageFileName);
   static void consoleInit();

   virtual GuiControl* constructEditControl();
   bool renderTooltip( const Point2I &hoverPos, const Point2I &cursorPos, const char *tipText = NULL );
};


//-----------------------------------------------------------------------------
// TypeColor GuiInspectorField Class (Base for ColorI/ColorF)
//-----------------------------------------------------------------------------

class GuiSwatchButtonCtrl;

class GuiInspectorTypeColor : public GuiInspectorField
{
   typedef GuiInspectorField Parent;
   
protected:

   /// Return the name of a function that will be used to convert the
   /// floating-point color of the swatch button to the form used by the
   /// data field.
   virtual const char* _getColorConversionFunction() const { return NULL; }

public:

   GuiInspectorTypeColor();

   DECLARE_CONOBJECT(GuiInspectorTypeColor);

   StringTableEntry  mColorFunction;
   GuiSwatchButtonCtrl *mBrowseButton;
   RectI mBrowseRect;

   //-----------------------------------------------------------------------------
   // Override able methods for custom edit fields
   //-----------------------------------------------------------------------------
   virtual GuiControl*        constructEditControl();
   virtual bool               resize(const Point2I &newPosition, const Point2I &newExtent);
   virtual bool               updateRects();
};

//-----------------------------------------------------------------------------
// TypeColorI GuiInspectorField Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeColorI : public GuiInspectorTypeColor
{
   typedef GuiInspectorTypeColor Parent;
   
protected:

   virtual const char* _getColorConversionFunction() const { return "ColorFloatToInt"; }

public:

   GuiInspectorTypeColorI();

   DECLARE_CONOBJECT(GuiInspectorTypeColorI);

   static void consoleInit();
   void setValue( StringTableEntry newValue );
};

//-----------------------------------------------------------------------------
// TypeColorF GuiInspectorField Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeColorF : public GuiInspectorTypeColor
{
   typedef GuiInspectorTypeColor Parent;

public:

   GuiInspectorTypeColorF();

   DECLARE_CONOBJECT(GuiInspectorTypeColorF);

   static void consoleInit();
   void setValue( StringTableEntry newValue );
};

/* NOTE: Evidently this isn't used anywhere (or implemented) so i commented it out
//------------------------------------------------------------------------------
// TypeString GuiInspectorField class
//------------------------------------------------------------------------------
class GuiInspectorTypeString : public GuiInspectorField
{
private:
   typedef GuiInspectorField Parent;
public:
   DECLARE_CONOBJECT(GuiInspectorTypeString);
   static void consoleInit();

   SimObjectPtr<GuiButtonCtrl> mBrowseButton;

   virtual GuiControl*  constructEditControl();
   virtual bool         resize(const Point2I &newPosition, const Point2I &newExtent);
   virtual bool         updateRects();
};
*/


//------------------------------------------------------------------------------
// TypeS32 GuiInspectorField class
//------------------------------------------------------------------------------
class GuiInspectorTypeS32 : public GuiInspectorField
{
private:
   typedef GuiInspectorField Parent;
public:
   DECLARE_CONOBJECT(GuiInspectorTypeS32);
   static void consoleInit();

   virtual GuiControl*  constructEditControl();
   virtual void setValue( StringTableEntry newValue );
};


//------------------------------------------------------------------------------
// TypeBitMask32 GuiInspectorField class
//------------------------------------------------------------------------------
class GuiInspectorTypeBitMask32Helper;
class GuiDynamicCtrlArrayControl;

class GuiInspectorTypeBitMask32 : public GuiInspectorField
{
   typedef GuiInspectorField Parent;

public:

   GuiInspectorTypeBitMask32();
   virtual ~GuiInspectorTypeBitMask32() {}

   DECLARE_CONOBJECT( GuiInspectorTypeBitMask32 );

   // ConsoleObject
   bool onAdd();
   static void consoleInit();

   // GuiInspectorField
   virtual void childResized( GuiControl *child );
   virtual bool resize( const Point2I &newPosition, const Point2I &newExtent );
   virtual bool updateRects();
   virtual void updateData();
   virtual StringTableEntry getValue();
   virtual void setValue( StringTableEntry value );
   virtual void setData( StringTableEntry data );

protected:

   GuiInspectorTypeBitMask32Helper *mHelper;
   GuiRolloutCtrl *mRollout;
   GuiDynamicCtrlArrayControl *mArrayCtrl;
   Vector<GuiInspectorField*> mChildren;
};

class GuiInspectorTypeBitMask32Helper : public GuiInspectorField
{
   typedef GuiInspectorField Parent;

public:

   GuiInspectorTypeBitMask32Helper();

   DECLARE_CONOBJECT( GuiInspectorTypeBitMask32Helper );

   GuiBitmapButtonCtrl *mButton;
   GuiRolloutCtrl *mParentRollout;
   GuiInspectorTypeBitMask32 *mParentField;
   RectI mButtonRect;

   //-----------------------------------------------------------------------------
   // Override able methods for custom edit fields
   //-----------------------------------------------------------------------------
   virtual GuiControl*        constructEditControl();
   virtual bool               resize( const Point2I &newPosition, const Point2I &newExtent );
   virtual bool               updateRects();
   virtual void               setValue( StringTableEntry value );
};


//-----------------------------------------------------------------------------
// TypeName GuiInspectorField Class
//-----------------------------------------------------------------------------
class GuiInspectorTypeName : public GuiInspectorField
{
private:
   typedef GuiInspectorField Parent;
public:
   DECLARE_CONOBJECT(GuiInspectorTypeName);
   static void consoleInit();

   virtual bool verifyData( StringTableEntry data );
};

#endif // _GUI_INSPECTOR_TYPES_H_