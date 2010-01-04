//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUISHAPEEDPREVIEW_H_
#define _GUISHAPEEDPREVIEW_H_

#include "gui/worldEditor/editTSCtrl.h"
#include "gui/controls/guiSliderCtrl.h"
#include "ts/tsShapeInstance.h"

class LightInfo;

class GuiShapeEdPreview : public EditTSCtrl
{
private:
   typedef EditTSCtrl Parent;

   // View and node selection
   bool        mUsingAxisGizmo;

   S32         mSelectedNode;
   S32         mHoverNode;
   Vector<Point3F>   mProjectedNodes;

   // Camera
   EulerF      mCameraRot;
   Point3F     mOrbitPos;
   F32         mOrbitDist;
   F32         mMoveSpeed;
   F32         mZoomSpeed;

   // Rendering
   bool        mRenderGhost;
   bool        mRenderNodes;
   bool        mRenderBounds;
   TSShapeInstance*  mModel;

   LightInfo*  mFakeSun;

   // Animation and playback control
   TSThread *  mAnimThread;
   S32         mLastRenderTime;
   S32         mAnimationSeq;
   F32         mTimeScale;

   bool        mIsPlaying;
   F32         mSeqIn;
   F32         mSeqOut;

   GuiSliderCtrl  *mSliderCtrl;

   // Generic mouse event handlers
   void handleMouseDown(const GuiEvent& event, Mode mode);
   void handleMouseUp(const GuiEvent& event, Mode mode);
   void handleMouseMove(const GuiEvent& event, Mode mode);
   void handleMouseDragged(const GuiEvent& event, Mode mode);

   S32 collideNode(const Gui3DMouseEvent& event) const;
   void updateProjectedNodePoints();

public:
   bool onWake();

   // Keyboard and Mouse event handlers
   bool onKeyDown(const GuiEvent& event);
   bool onKeyUp(const GuiEvent& event);

   void onMouseDown(const GuiEvent& event) { handleMouseDown(event, NoneMode); }
   void onMouseUp(const GuiEvent& event) { handleMouseUp(event, NoneMode); }
   void onMouseMove(const GuiEvent& event) { handleMouseMove(event, NoneMode); }
   void onMouseDragged(const GuiEvent& event) { handleMouseDragged(event, NoneMode); }

   void onMiddleMouseDown(const GuiEvent& event) { handleMouseDown(event, MoveMode); }
   void onMiddleMouseUp(const GuiEvent& event) { handleMouseUp(event, MoveMode); }
   void onMiddleMouseDragged(const GuiEvent& event) { handleMouseDragged(event, MoveMode); }

   void onRightMouseDown(const GuiEvent& event) { handleMouseDown(event, RotateMode); }
   void onRightMouseUp(const GuiEvent& event) { handleMouseUp(event, RotateMode); }
   void onRightMouseDragged(const GuiEvent& event) { handleMouseDragged(event, RotateMode); }

   bool onMouseWheelUp(const GuiEvent& event);
   bool onMouseWheelDown(const GuiEvent& event);

   // Setters/Getters
   TSShapeInstance* getModel() { return mModel; }

   void setEditMode(bool editMode);
   void setObjectModel(const char * modelName);
   void setObjectAnimation(const char * seqName);
   void setSliderCtrl(GuiSliderCtrl* ctrl);
   void setTimeScale(F32 scale);
   void setOrbitDistance(F32 distance);
   void updateNodeTransforms();

   void get3DCursor(GuiCursor *& cursor, bool& visible, const Gui3DMouseEvent& event_);

   void fitToShape();

   // Rendering
   bool processCameraQuery(CameraQuery *query);
   void renderWorld(const RectI& updateRect);
   void renderNodes(const RectI& updateRect) const;
   void renderNodeAxes(S32 index, const ColorF& nodeColor) const;
   void renderNodeName(S32 index, const ColorF& textColor) const;

   DECLARE_CONOBJECT(GuiShapeEdPreview);
   DECLARE_CATEGORY( "Gui Editor" );
   
   static void initPersistFields();

   GuiShapeEdPreview();
   ~GuiShapeEdPreview();
};

#endif
