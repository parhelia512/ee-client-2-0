//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUI_INSPECTOR_FIELD_H_
#define _GUI_INSPECTOR_FIELD_H_

#include "gui/core/guiCanvas.h"
#include "gui/shiny/guiTickCtrl.h"
#include "gui/controls/guiTextEditCtrl.h"
#include "gui/buttons/guiBitmapButtonCtrl.h"
#include "gui/controls/guiPopUpCtrl.h"

#include "gui/containers/guiRolloutCtrl.h"

class GuiInspectorGroup;
class GuiInspector;


class GuiInspectorField : public GuiControl
{
   friend class GuiInspectorGroup;

public:

   typedef GuiControl Parent;


   GuiInspectorField( GuiInspector *inspector, GuiInspectorGroup* parent, SimObjectPtr<SimObject> target, AbstractClassRep::Field* field );
   GuiInspectorField();
   virtual ~GuiInspectorField();

   DECLARE_CONOBJECT(GuiInspectorField);
   DECLARE_CATEGORY( "Gui Editor" );

   // ConsoleObject
   virtual bool onAdd();

   // GuiControl
   virtual bool resize(const Point2I &newPosition, const Point2I &newExtent);
   virtual void onRender(Point2I offset, const RectI &updateRect);
   virtual void setFirstResponder( GuiControl *firstResponder );
   virtual void onMouseDown( const GuiEvent &event );

   // GuiInspectorField
   virtual void init( GuiInspector *inspector, GuiInspectorGroup *group, SimObjectPtr<SimObject> target );      
   virtual void setInspectorField( AbstractClassRep::Field *field, 
                                   StringTableEntry caption = NULL, 
                                   const char *arrayIndex = NULL );
   
   virtual GuiControl* constructEditControl();

   /// Sets the GuiControlProfile 
   virtual void setInspectorProfile();

   /// Sets this control's caption text, usually set within setInspectorField,
   /// this is exposed in case someone wants to override the normal caption.
   virtual void setCaption( StringTableEntry caption ) { mCaption = caption; }

   /// Returns pointer to this InspectorField's edit ctrl.
   virtual GuiControl* getEditCtrl() { return mEdit; }

   /// Sets the value of this GuiInspectorField (not the actual field)
   /// This means the EditCtrl unless overridden.
   virtual void setValue( StringTableEntry newValue );

   /// Get the currently value of this control (not the actual field)
   virtual const char* getValue() { return NULL; }

   /// Update this controls value to reflect that of the inspected field.
   virtual void updateValue();
   
   virtual StringTableEntry getFieldName();

   /// Called from within setData to allow child classes
   /// to perform their own verification.
   virtual bool verifyData( StringTableEntry data ) { return true; }

   /// Set value of the field we are inspecting
   virtual void setData( StringTableEntry data );

   /// Get value of the field we are inspecting
   virtual StringTableEntry getData();

   /// Update the inspected field to match the value of this control.
   virtual void updateData() {};

   virtual bool updateRects();   

   virtual void setHLEnabled( bool enabled );

protected:

   virtual void _registerEditControl( GuiControl *ctrl );
   virtual void _executeSelectedCallback();

protected:
   
   StringTableEntry           mCaption;
   GuiInspectorGroup*         mParent;
   GuiInspector*              mInspector;
   SimObjectPtr<SimObject>    mTarget;
   AbstractClassRep::Field*   mField;
   StringTableEntry           mFieldArrayIndex;
   GuiControl*                mEdit;
   RectI mCaptionRect;
   RectI mEditCtrlRect;
   bool mHighlighted;
};

#endif // _GUI_INSPECTOR_FIELD_H_
