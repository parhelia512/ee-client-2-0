//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _GUICONTAINER_H_
#define _GUICONTAINER_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif


/// @addtogroup gui_container_group Containers
///
/// @ingroup gui_group Gui System
/// @{
class  GuiContainer : public GuiControl
{
   S32 mUpdateLayout; ///< Layout Update Mask
   typedef GuiControl Parent;
protected:

   ControlSizing mSizingOptions; ///< Control Sizing Options
   S32 mValidDockingMask;
public:


   enum 
   {
      updateSelf = BIT(1),
      updateParent = BIT(2),
      updateNone = 0

   };
   
   DECLARE_CONOBJECT(GuiContainer);
   DECLARE_CATEGORY( "Gui Containers" );

   GuiContainer();
   virtual ~GuiContainer();

   static void initPersistFields();

   /// @name Container Sizing
   /// @{

   /// Returns the Mask of valid docking modes supported by this container
   inline S32 getValidDockingMask() { return mValidDockingMask; };

   /// Docking Accessors
   inline S32 getDocking() { return mSizingOptions.mDocking; };
   virtual void setDocking( S32 docking );

   /// Docking Protected Field Setter
   static bool setDockingField(void* obj, const char* data)
   {
      GuiContainer *pContainer = static_cast<GuiContainer*>(obj);
      pContainer->setUpdateLayout( updateParent );
      return true;
   }

   inline bool getAnchorTop() const { return mSizingOptions.mAnchorTop; }
   inline bool getAnchorBottom() const { return mSizingOptions.mAnchorBottom; }
   inline bool getAnchorLeft() const { return mSizingOptions.mAnchorLeft; }
   inline bool getAnchorRight() const { return mSizingOptions.mAnchorRight; }
   inline void setAnchorTop(bool val) { mSizingOptions.mAnchorTop = val; }
   inline void setAnchorBottom(bool val) { mSizingOptions.mAnchorBottom = val; }
   inline void setAnchorLeft(bool val) { mSizingOptions.mAnchorLeft = val; }
   inline void setAnchorRight(bool val) { mSizingOptions.mAnchorRight = val; }

   ControlSizing getSizingOptions() const { return mSizingOptions; }
   void setSizingOptions(ControlSizing val) { mSizingOptions = val; }

   /// @}   

   /// @name Sizing Constraints
   /// @{
   virtual const RectI getClientRect();
   /// @}

   /// @name Control Layout Methods
   /// @{

   /// Called when the Layout for a Container needs to be updated because of a resize call or a call to setUpdateLayout
   /// @param   clientRect The Client Rectangle that is available for this Container to layout it's children in
   virtual bool layoutControls( RectI &clientRect );

   /// Set the layout flag to Dirty on a Container, triggering an update to it's layout on the next onPreRender call.
   ///  @attention This can be called without regard to whether the flag is already set, as setting it
   ///    does not actually cause an update, but rather tells the container it should update the next
   ///    chance it gets
   /// @param   updateType   A Mask that indicates how the layout should be updated.
   inline void setUpdateLayout( S32 updateType = updateSelf ) { mUpdateLayout |= updateType; };

   /// @}

   /// @name Container Sizing Methods
   /// @{

   /// Dock a Control with the given docking mode inside the given client rect.
   /// @attention The clientRect passed in will be modified by the docking of 
   ///    the control.  It will return the rect that remains after the docking operation.
   virtual bool dockControl( GuiContainer *control, S32 dockingMode, RectI &clientRect );
   virtual bool anchorControl( GuiControl *control, const Point2I &deltaParentExtent );
   /// @}
public:
   /// @name GuiControl Inherited
   /// @{
   virtual void onChildAdded(GuiControl* control);
   virtual void onChildRemoved(GuiControl* control);
   virtual bool resize( const Point2I &newPosition, const Point2I &newExtent );
   virtual void parentResized(const RectI &oldParentRect, const RectI &newParentRect);
   virtual void childResized(GuiControl *child);
   virtual void addObject(SimObject *obj);
   virtual void removeObject(SimObject *obj);
   virtual bool reOrder(SimObject* obj, SimObject* target);
   virtual void onPreRender();
   /// @}
};
/// @}
#endif
