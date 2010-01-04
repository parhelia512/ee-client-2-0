//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUISCRIPTNOTIFYCTRL_H_
#define _GUISCRIPTNOTIFYCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

class GuiScriptNotifyCtrl : public GuiControl
{
private:
   typedef GuiControl Parent;
public:

    /// @name Event Callbacks
    /// @{ 
   bool mOnChildAdded;         ///< Script Notify : onAddObject(%object)  
   bool mOnChildRemoved;       ///< Script Notify : onRemoveObject(%object)
   bool mOnResize;             ///< Script Notify : onResize()
   bool mOnChildResized;       ///< Script Notify : onChildResized(%child)
   bool mOnParentResized;      ///< Script Notify : onParentResized()
   bool mOnLoseFirstResponder; ///< Script Notify : onLoseFirstResponder()
   bool mOnGainFirstResponder; ///< Script Notify : onGainFirstResponder()
    /// @}

public:
    /// @name Initialization
    /// @{
    DECLARE_CONOBJECT(GuiScriptNotifyCtrl);
    DECLARE_CATEGORY( "Gui Other Script" );
    DECLARE_DESCRIPTION( "A control that implements various script callbacks for\n"
                         "certain GUI events." );
    
    GuiScriptNotifyCtrl();
    virtual ~GuiScriptNotifyCtrl();
    static void initPersistFields();

    virtual bool resize(const Point2I &newPosition, const Point2I &newExtent);
    virtual void childResized(GuiScriptNotifyCtrl *child);
    virtual void parentResized(const RectI &oldParentRect, const RectI &newParentRect);
    virtual void onChildRemoved( GuiControl *child );
    virtual void onChildAdded( GuiControl *child );
    //virtual void onMouseUp(const GuiEvent &event);
    //virtual void onMouseDown(const GuiEvent &event);
    //virtual void onMouseMove(const GuiEvent &event);
    //virtual void onMouseDragged(const GuiEvent &event);
    //virtual void onMouseEnter(const GuiEvent &event);
    //virtual void onMouseLeave(const GuiEvent &event);

    //virtual bool onMouseWheelUp(const GuiEvent &event);
    //virtual bool onMouseWheelDown(const GuiEvent &event);

    //virtual void onRightMouseDown(const GuiEvent &event);
    //virtual void onRightMouseUp(const GuiEvent &event);
    //virtual void onRightMouseDragged(const GuiEvent &event);

    //virtual void onMiddleMouseDown(const GuiEvent &event);
    //virtual void onMiddleMouseUp(const GuiEvent &event);
    //virtual void onMiddleMouseDragged(const GuiEvent &event);

    //virtual void onMouseDownEditor(const GuiEvent &event, Point2I offset);
    //virtual void onRightMouseDownEditor(const GuiEvent &event, Point2I offset);

    virtual void setFirstResponder(GuiControl *firstResponder);
    virtual void setFirstResponder();
    void clearFirstResponder();
    virtual void onLoseFirstResponder();

    //virtual void acceleratorKeyPress(U32 index);
    //virtual void acceleratorKeyRelease(U32 index);
    //virtual bool onKeyDown(const GuiEvent &event);
    //virtual bool onKeyUp(const GuiEvent &event);
    //virtual bool onKeyRepeat(const GuiEvent &event);

    virtual void onMessage(GuiScriptNotifyCtrl *sender, S32 msg);    ///< Receive a message from another control

    virtual void onDialogPush();
    virtual void onDialogPop();

};

#endif
