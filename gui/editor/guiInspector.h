//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUI_INSPECTOR_H_
#define _GUI_INSPECTOR_H_

#include "gui/containers/guiStackCtrl.h"


class GuiInspectorGroup;
class GuiInspectorField;
class GuiInspectorDatablockField;

class GuiInspector : public GuiStackControl
{
   typedef GuiStackControl Parent;

public:

   GuiInspector();
   virtual ~GuiInspector();

   DECLARE_CONOBJECT(GuiInspector);
   DECLARE_CATEGORY( "Gui Editor" );

   // Console Object

   bool onAdd();
   static void initPersistFields();

   // SimObject
   virtual void onDeleteNotify( SimObject *object );

   // GuiControl

   virtual void parentResized( const RectI &oldParentRect, const RectI &newParentRect );
   virtual bool resize( const Point2I &newPosition, const Point2I &newExtent );
   virtual GuiControl* findHitControl( const Point2I &pt, S32 initialLayer );   
   virtual void getCursor( GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent );
   virtual void onMouseMove( const GuiEvent &event );
   virtual void onMouseDown( const GuiEvent &event );
   virtual void onMouseUp( const GuiEvent &event );
   virtual void onMouseDragged( const GuiEvent &event );

   // GuiInspector

   /// Set the currently inspected object
   virtual void inspectObject( SimObject *object );

   /// Get the currently inspected object
   SimObject* getInspectObject() { return mTarget; }

   /// Set the currently inspected object name
   void setName( StringTableEntry newName );

   /// Deletes all GuiInspectorGroups
   void clearGroups();   

   /// Returns true if the named group exists
   /// Helper for inspectObject
   bool findExistentGroup( StringTableEntry groupName );

   /// Should there be a GuiInspectorField associated with this fieldName,
   /// update it to reflect actual/current value of that field in the inspected object.
   /// Added to support UndoActions
   void updateFieldValue( StringTableEntry fieldName, const char* arrayIdx );

   /// Divider position is interpreted as an offset 
   /// from the right edge of the field control.
   /// Divider margin is an offset on both left and right
   /// sides of the divider in which it can be selected
   /// with the mouse.
   void getDivider( S32 &pos, S32 &margin );   

   void updateDivider();

   bool collideDivider( const Point2I &localPnt );

   void setHighlightField( GuiInspectorField *field );

   // If returns true that group will not be inspected.
   bool isGroupFiltered( const char *groupName ) const;

protected:
      
   Vector<GuiInspectorGroup*> mGroups;

   SimObject *mTarget;
   F32 mDividerPos;   
   S32 mDividerMargin;
   bool mOverDivider;
   bool mMovingDivider;
   SimObjectPtr<GuiInspectorField> mHLField;
   String mGroupFilters;
};

#endif