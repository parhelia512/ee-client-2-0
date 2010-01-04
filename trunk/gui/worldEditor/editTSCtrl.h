//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _EDITTSCTRL_H_
#define _EDITTSCTRL_H_

#ifndef _GUITSCONTROL_H_
#include "gui/3d/guiTSControl.h"
#endif
#ifndef _GIZMO_H_
#include "gizmo.h"
#endif

class TerrainBlock;
class Gizmo;
class EditManager;

class EditTSCtrl : public GuiTSCtrl
{
      typedef GuiTSCtrl Parent;

   protected:

      void make3DMouseEvent(Gui3DMouseEvent & gui3Devent, const GuiEvent &event);

      // GuiControl
      virtual void getCursor(GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent);
      virtual void onMouseUp(const GuiEvent & event);
      virtual void onMouseDown(const GuiEvent & event);
      virtual void onMouseMove(const GuiEvent & event);
      virtual void onMouseDragged(const GuiEvent & event);
      virtual void onMouseEnter(const GuiEvent & event);
      virtual void onMouseLeave(const GuiEvent & event);
      virtual void onRightMouseDown(const GuiEvent & event);
      virtual void onRightMouseUp(const GuiEvent & event);
      virtual void onRightMouseDragged(const GuiEvent & event);
      virtual void onMiddleMouseDown(const GuiEvent & event);
      virtual void onMiddleMouseUp(const GuiEvent & event);
      virtual void onMiddleMouseDragged(const GuiEvent & event);
      virtual bool onInputEvent(const InputEventInfo & event);
      virtual bool onMouseWheelUp(const GuiEvent &event);
      virtual bool onMouseWheelDown(const GuiEvent &event);


      virtual void updateGuiInfo() {};
      virtual void renderScene(const RectI &){};
      void renderMissionArea();
      virtual void renderCameraAxis();
      virtual void renderGrid();

      // GuiTSCtrl
      void renderWorld(const RectI & updateRect);

   protected:
      enum DisplayType
      {
         DisplayTypeTop,
         DisplayTypeBottom,
         DisplayTypeFront,
         DisplayTypeBack,
         DisplayTypeLeft,
         DisplayTypeRight,
         DisplayTypePerspective,
         DisplayTypeIsometric,
      };

      S32      mDisplayType;
      F32      mOrthoFOV;
      Point3F  mOrthoCamTrans;
      EulerF   mIsoCamRot;
      Point3F  mIsoCamRotCenter;
      F32      mIsoCamAngle;
      Point3F  mRawCamPos;
      Point2I  mLastMousePos;
      bool     mLastMouseClamping;

      bool     mAllowBorderMove;
      S32      mMouseMoveBorder;
      F32      mMouseMoveSpeed;
      U32      mLastBorderMoveTime;

      Gui3DMouseEvent   mLastEvent;
      bool              mLeftMouseDown;
      bool              mRightMouseDown;
      bool              mMiddleMouseDown;
      bool              mMouseLeft;

      SimObjectPtr<Gizmo> mGizmo;
      GizmoProfile *mGizmoProfile;

   public:

      EditTSCtrl();
      ~EditTSCtrl();

      // SimObject
      bool onAdd();
      void onRemove();

      //
      bool        mRenderMissionArea;
      ColorI      mMissionAreaFillColor;
      ColorI      mMissionAreaFrameColor;

      //
      ColorI            mConsoleFrameColor;
      ColorI            mConsoleFillColor;
      S32               mConsoleSphereLevel;
      S32               mConsoleCircleSegments;
      S32               mConsoleLineWidth;

      static void initPersistFields();
      static void consoleInit();

      //
      bool              mConsoleRendering;
      bool              mRightMousePassThru;
      bool              mMiddleMousePassThru;

      // all editors will share a camera
      static Point3F    smCamPos;
      static MatrixF    smCamMatrix;
      static bool       smCamOrtho;
      static F32        smCamNearPlane;
      static F32        smVisibleDistanceScale;

      static U32        smSceneBoundsMask;
      static Point3F    smMinSceneBounds;

      bool              mRenderGridPlane;
      ColorI            mGridPlaneColor;
      F32               mGridPlaneSize;
      F32               mGridPlaneSizePixelBias;
      S32               mGridPlaneMinorTicks;
      ColorI            mGridPlaneMinorTickColor;
      ColorI            mGridPlaneOriginColor;

      GFXStateBlockRef  mBlendSB;

      // GuiTSCtrl
      bool processCameraQuery(CameraQuery * query);

      // guiControl
      virtual void onRender(Point2I offset, const RectI &updateRect);
      virtual void on3DMouseUp(const Gui3DMouseEvent &){};
      virtual void on3DMouseDown(const Gui3DMouseEvent &){};
      virtual void on3DMouseMove(const Gui3DMouseEvent &){};
      virtual void on3DMouseDragged(const Gui3DMouseEvent &){};
      virtual void on3DMouseEnter(const Gui3DMouseEvent &){};
      virtual void on3DMouseLeave(const Gui3DMouseEvent &){};
      virtual void on3DRightMouseDown(const Gui3DMouseEvent &){};
      virtual void on3DRightMouseUp(const Gui3DMouseEvent &){};
      virtual void on3DRightMouseDragged(const Gui3DMouseEvent &){};
      virtual void on3DMouseWheelUp(const Gui3DMouseEvent &){};
      virtual void on3DMouseWheelDown(const Gui3DMouseEvent &){};
      virtual void get3DCursor(GuiCursor *&cursor, bool &visible, const Gui3DMouseEvent &);

      virtual bool isMiddleMouseDown() {return mMiddleMouseDown;}

      virtual S32 getDisplayType() {return mDisplayType;}
      virtual void setDisplayType(S32 type) {mDisplayType = type;}

      virtual TerrainBlock* getActiveTerrain();

      virtual void calcOrthoCamOffset(F32 mousex, F32 mousey, U8 modifier=0);

      Gizmo* getGizmo() { return mGizmo; }

      DECLARE_CONOBJECT(EditTSCtrl);
      DECLARE_CATEGORY( "Gui Editor" );
};

#endif // _EDITTSCTRL_H_
