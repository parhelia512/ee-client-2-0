//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/consoleTypes.h"
#include "T3D/gameConnection.h"
#include "gui/worldEditor/editor.h"
#include "gui/worldEditor/editTSCtrl.h"
#include "gui/core/guiCanvas.h"
#include "terrain/terrData.h"
#include "T3D/sphere.h"
#include "T3D/missionArea.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxDrawUtil.h"
#include "gfx/gfxTransformSaver.h"
#include "sceneGraph/sceneGraph.h"


IMPLEMENT_CONOBJECT(EditTSCtrl);

Point3F  EditTSCtrl::smCamPos;
MatrixF  EditTSCtrl::smCamMatrix;
bool     EditTSCtrl::smCamOrtho = false;
F32      EditTSCtrl::smCamNearPlane;
F32      EditTSCtrl::smVisibleDistanceScale = 1.0f;
U32      EditTSCtrl::smSceneBoundsMask = EnvironmentObjectType | TerrainObjectType | WaterObjectType | CameraObjectType;
Point3F  EditTSCtrl::smMinSceneBounds = Point3F(500.0f, 500.0f, 500.0f);

EditTSCtrl::EditTSCtrl()
{
   mGizmoProfile = NULL;
   mGizmo = NULL;

   mRenderMissionArea = true;
   mMissionAreaFillColor.set(255,0,0,20);
   mMissionAreaFrameColor.set(255,0,0,128);

   mConsoleFrameColor.set(255,0,0,255);
   mConsoleFillColor.set(255,0,0,120);
   mConsoleSphereLevel = 1;
   mConsoleCircleSegments = 32;
   mConsoleLineWidth = 1;
   mRightMousePassThru = true;
   mMiddleMousePassThru = true;

   mConsoleRendering = false;

   mDisplayType = DisplayTypePerspective;
   mOrthoFOV = 50.0f;
   mOrthoCamTrans.set(0.0f, 0.0f, 0.0f);

   mIsoCamAngle = mDegToRad(45.0f);
   mIsoCamRot = EulerF(0, 0, 0);

   mRenderGridPlane = true;
   mGridPlaneOriginColor = ColorI(0, 0, 0, 255);
   mGridPlaneColor = ColorI(0, 0, 0, 255);
   mGridPlaneMinorTickColor = ColorI(102, 102, 102, 255);
   mGridPlaneMinorTicks = 9;
   mGridPlaneSize = 1.0f;
   mGridPlaneSizePixelBias = 2.0f;

   mLastMousePos.set(0, 0);

   mAllowBorderMove = false;
   mMouseMoveBorder = 20;
   mMouseMoveSpeed = 0.1f;
   mLastBorderMoveTime = 0;
   mLeftMouseDown = false;
   mRightMouseDown = false;
   mMiddleMouseDown = false;
   mMouseLeft = false;

   mBlendSB = NULL;

}

EditTSCtrl::~EditTSCtrl()
{
   mBlendSB = NULL;
}

//------------------------------------------------------------------------------

bool EditTSCtrl::onAdd()
{
   if(!Parent::onAdd())
      return(false);

   // give all derived access to the fields
   setModStaticFields(true);

   GFXStateBlockDesc blenddesc;
   blenddesc.setBlend(true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
   mBlendSB = GFX->createStateBlock( blenddesc );

   if ( !mGizmoProfile )
   {
      Con::errorf( "EditTSCtrl::onadd - gizmoProfile was not assigned, cannot create control!" );
      return false;
   }

   mGizmo = new Gizmo();
   mGizmo->setProfile( mGizmoProfile );
   mGizmo->registerObject();

   return true;
}

void EditTSCtrl::onRemove()
{
   Parent::onRemove();

   if ( mGizmo )
      mGizmo->deleteObject();
}

void EditTSCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   // Perform possible mouse border move...
   if(mAllowBorderMove && smCamOrtho && !mLeftMouseDown && !mRightMouseDown && !mMouseLeft)
   {
      Point2I ext = getExtent();

      U32 current = Platform::getRealMilliseconds();
      bool update = false;
      F32 movex = 0.0f;
      F32 movey = 0.0f;

      Point2I localMouse = globalToLocalCoord(mLastMousePos);

      if(localMouse.x <= mMouseMoveBorder || localMouse.x >= ext.x - mMouseMoveBorder)
      {
         if(mLastBorderMoveTime != 0)
         {
            U32 dt = current - mLastBorderMoveTime;
            if(localMouse.x <= mMouseMoveBorder)
            {
               movex = mMouseMoveSpeed * dt;
            }
            else
            {
               movex = -mMouseMoveSpeed * dt;
            }
         }
         update = true;
      }

      if(localMouse.y <= mMouseMoveBorder || localMouse.y >= ext.y - mMouseMoveBorder)
      {
         if(mLastBorderMoveTime != 0)
         {
            U32 dt = current - mLastBorderMoveTime;
            if(localMouse.y <= mMouseMoveBorder)
            {
               movey = mMouseMoveSpeed * dt;
            }
            else
            {
               movey = -mMouseMoveSpeed * dt;
            }
         }
         update = true;
      }

      if(update)
      {
         mLastBorderMoveTime = current;
         calcOrthoCamOffset(movex, movey);
      }
      else
      {
         mLastBorderMoveTime = 0;
      }
   }

   updateGuiInfo();
   Parent::onRender(offset, updateRect);

}

//------------------------------------------------------------------------------

void EditTSCtrl::initPersistFields()
{
   addGroup("Mission Area");	
   addField("renderMissionArea", TypeBool, Offset(mRenderMissionArea, EditTSCtrl));
   addField("missionAreaFillColor", TypeColorI, Offset(mMissionAreaFillColor, EditTSCtrl));
   addField("missionAreaFrameColor", TypeColorI, Offset(mMissionAreaFrameColor, EditTSCtrl));
   endGroup("Mission Area");	

   addGroup("BorderMovement");	
   addField("allowBorderMove", TypeBool, Offset(mAllowBorderMove, EditTSCtrl));
   addField("borderMovePixelSize", TypeS32, Offset(mMouseMoveBorder, EditTSCtrl));
   addField("borderMoveSpeed", TypeF32, Offset(mMouseMoveSpeed, EditTSCtrl));
   endGroup("BorderMovement");	

   addGroup("Misc");	
   addField("consoleFrameColor", TypeColorI, Offset(mConsoleFrameColor, EditTSCtrl));
   addField("consoleFillColor", TypeColorI, Offset(mConsoleFillColor, EditTSCtrl));
   addField("consoleSphereLevel", TypeS32, Offset(mConsoleSphereLevel, EditTSCtrl));
   addField("consoleCircleSegments", TypeS32, Offset(mConsoleCircleSegments, EditTSCtrl));
   addField("consoleLineWidth", TypeS32, Offset(mConsoleLineWidth, EditTSCtrl));
   addField("gizmoProfile", TypeSimObjectPtr, Offset(mGizmoProfile, EditTSCtrl));
   endGroup("Misc");
   Parent::initPersistFields();
}

void EditTSCtrl::consoleInit()
{
   Con::addVariable("pref::WorldEditor::visibleDistanceScale", TypeF32, &EditTSCtrl::smVisibleDistanceScale);
}

//------------------------------------------------------------------------------

void EditTSCtrl::make3DMouseEvent(Gui3DMouseEvent & gui3DMouseEvent, const GuiEvent & event)
{
   (GuiEvent&)(gui3DMouseEvent) = event;
   gui3DMouseEvent.mousePoint = event.mousePoint;

   if(!smCamOrtho)
   {
      // get the eye pos and the mouse vec from that...
      Point3F screenPoint((F32)gui3DMouseEvent.mousePoint.x, (F32)gui3DMouseEvent.mousePoint.y, 1.0f);

      Point3F wp;
      unproject(screenPoint, &wp);

      gui3DMouseEvent.pos = smCamPos;
      gui3DMouseEvent.vec = wp - smCamPos;
      gui3DMouseEvent.vec.normalizeSafe();
   }
   else
   {
      // get the eye pos and the mouse vec from that...
      Point3F screenPoint((F32)gui3DMouseEvent.mousePoint.x, (F32)gui3DMouseEvent.mousePoint.y, 0.0f);

      Point3F np, fp;
      unproject(screenPoint, &np);

      gui3DMouseEvent.pos = np;
      smCamMatrix.getColumn( 1, &(gui3DMouseEvent.vec) );
   }
}

//------------------------------------------------------------------------------

TerrainBlock* EditTSCtrl::getActiveTerrain()
{
   // Find a terrain block
   SimSet* scopeAlwaysSet = Sim::getGhostAlwaysSet();
   for(SimSet::iterator itr = scopeAlwaysSet->begin(); itr != scopeAlwaysSet->end(); itr++)
   {
      TerrainBlock* block = dynamic_cast<TerrainBlock*>(*itr);
      if( block )
         return block;
   }

   return NULL;
}

//------------------------------------------------------------------------------

void EditTSCtrl::getCursor(GuiCursor *&cursor, bool &visible, const GuiEvent &event)
{
   make3DMouseEvent(mLastEvent, event);
   get3DCursor(cursor, visible, mLastEvent);
}

void EditTSCtrl::get3DCursor(GuiCursor *&cursor, bool &visible, const Gui3DMouseEvent &event)
{
   TORQUE_UNUSED(event);
   cursor = NULL;
   visible = false;
}

void EditTSCtrl::onMouseUp(const GuiEvent & event)
{
   mLeftMouseDown = false;
   make3DMouseEvent(mLastEvent, event);
   on3DMouseUp(mLastEvent);
}

void EditTSCtrl::onMouseDown(const GuiEvent & event)
{
   mLeftMouseDown = true;
   mLastBorderMoveTime = 0;
   make3DMouseEvent(mLastEvent, event);
   on3DMouseDown(mLastEvent);
   
   setFirstResponder();
}

void EditTSCtrl::onMouseMove(const GuiEvent & event)
{
   make3DMouseEvent(mLastEvent, event);
   on3DMouseMove(mLastEvent);

   mLastMousePos = event.mousePoint;
}

void EditTSCtrl::onMouseDragged(const GuiEvent & event)
{
   make3DMouseEvent(mLastEvent, event);
   on3DMouseDragged(mLastEvent);
}

void EditTSCtrl::onMouseEnter(const GuiEvent & event)
{
   mMouseLeft = false;
   make3DMouseEvent(mLastEvent, event);
   on3DMouseEnter(mLastEvent);
}

void EditTSCtrl::onMouseLeave(const GuiEvent & event)
{
   mMouseLeft = true;
   mLastBorderMoveTime = 0;
   make3DMouseEvent(mLastEvent, event);
   on3DMouseLeave(mLastEvent);
}

void EditTSCtrl::onRightMouseDown(const GuiEvent & event)
{
   // always process the right mouse event first...

   mRightMouseDown = true;
   mLastBorderMoveTime = 0;

   make3DMouseEvent(mLastEvent, event);
   on3DRightMouseDown(mLastEvent);

   if(mRightMousePassThru && mProfile->mCanKeyFocus)
   {
      GuiCanvas *pCanvas = getRoot();
      if( !pCanvas )
         return;

      PlatformWindow *pWindow = static_cast<GuiCanvas*>(getRoot())->getPlatformWindow();
      if( !pWindow )
         return;

      PlatformCursorController *pController = pWindow->getCursorController();
      if( !pController )
         return;

      // ok, gotta disable the mouse
      // script functions are lockMouse(true); Canvas.cursorOff();
      pWindow->setMouseLocked(true);
      pCanvas->setCursorON( false );

      if(mDisplayType != DisplayTypePerspective)
      {
         mouseLock();
         mLastMousePos = event.mousePoint;
         pCanvas->setForceMouseToGUI(true);
         mLastMouseClamping = pCanvas->getClampTorqueCursor();
         pCanvas->setClampTorqueCursor(false);
      }

      if(mDisplayType == DisplayTypeIsometric)
      {
         // Store the screen center point on the terrain for a possible rotation
         TerrainBlock* activeTerrain = getActiveTerrain();
         if( activeTerrain )
         {
            F32 extx, exty;
            if(event.modifier & SI_SHIFT)
            {
               extx = F32(event.mousePoint.x);
               exty = F32(event.mousePoint.y);
            }
            else
            {
               extx = getExtent().x * 0.5;
               exty = getExtent().y * 0.5;
            }
            Point3F sp(extx, exty, 0.0f); // Near plane projection
            Point3F start;
            unproject(sp, &start);

            Point3F end = start + mLastEvent.vec * 4000.0f;
            Point3F tStartPnt, tEndPnt;
            activeTerrain->getTransform().mulP(start, &tStartPnt);
            activeTerrain->getTransform().mulP(end, &tEndPnt);

            RayInfo info;
            bool result = activeTerrain->castRay(tStartPnt, tEndPnt, &info);
            if(result)
            {
               info.point.interpolate(start, end, info.t);
               mIsoCamRotCenter = info.point;
            }
            else
            {
               mIsoCamRotCenter = start;
            }
         }
         else
         {
            F32 extx = getExtent().x * 0.5;
            F32 exty = getExtent().y * 0.5;
            Point3F sp(extx, exty, 0.0f); // Near plane projection
            unproject(sp, &mIsoCamRotCenter);
         }
      }

      setFirstResponder();
   }
}

void EditTSCtrl::onRightMouseUp(const GuiEvent & event)
{
   mRightMouseDown = false;
   make3DMouseEvent(mLastEvent, event);
   on3DRightMouseUp(mLastEvent);
}

void EditTSCtrl::onRightMouseDragged(const GuiEvent & event)
{
   make3DMouseEvent(mLastEvent, event);
   on3DRightMouseDragged(mLastEvent);

   // Handle translation of orthographic views
   if(mDisplayType != DisplayTypePerspective)
   {
      calcOrthoCamOffset((event.mousePoint.x - mLastMousePos.x), (event.mousePoint.y - mLastMousePos.y), event.modifier);

      mLastMousePos = event.mousePoint;
   }
}

void EditTSCtrl::onMiddleMouseDown(const GuiEvent & event)
{
   mMiddleMouseDown = true;
   mLastBorderMoveTime = 0;

   if(mMiddleMousePassThru && mProfile->mCanKeyFocus)
   {
      GuiCanvas *pCanvas = getRoot();
      if( !pCanvas )
         return;

      PlatformWindow *pWindow = static_cast<GuiCanvas*>(getRoot())->getPlatformWindow();
      if( !pWindow )
         return;

      PlatformCursorController *pController = pWindow->getCursorController();
      if( !pController )
         return;

      // ok, gotta disable the mouse
      // script functions are lockMouse(true); Canvas.cursorOff();
      pWindow->setMouseLocked(true);
      pCanvas->setCursorON( false );

      // Trigger 2 is used by the camera
      MoveManager::mTriggerCount[2]++;

      setFirstResponder();
   }
}

void EditTSCtrl::onMiddleMouseUp(const GuiEvent & event)
{
   // Trigger 2 is used by the camera
   MoveManager::mTriggerCount[2]++;

   mMiddleMouseDown = false;
}

void EditTSCtrl::onMiddleMouseDragged(const GuiEvent & event)
{
}

bool EditTSCtrl::onMouseWheelUp( const GuiEvent &event )
{
   if(mDisplayType != DisplayTypePerspective && !event.modifier)
   {
      mOrthoFOV -= 2.0f;
      if(mOrthoFOV < 1.0f)
         mOrthoFOV = 1.0f;
      return true;
   }

   make3DMouseEvent(mLastEvent, event);
   on3DMouseWheelUp(mLastEvent);

   return false;
}

bool EditTSCtrl::onMouseWheelDown( const GuiEvent &event )
{
   if(mDisplayType != DisplayTypePerspective && !event.modifier)
   {
      mOrthoFOV += 2.0f;
      return true;
   }

   make3DMouseEvent(mLastEvent, event);
   on3DMouseWheelDown(mLastEvent);

   return false;
}


bool EditTSCtrl::onInputEvent(const InputEventInfo & event)
{

   if(mRightMousePassThru && event.deviceType == MouseDeviceType &&
      event.objInst == KEY_BUTTON1 && event.action == SI_BREAK)
   {
      // if the right mouse pass thru is enabled,
      // we want to reactivate mouse on a right mouse button up
      GuiCanvas *pCanvas = getRoot();
      if( !pCanvas )
         return false;

      PlatformWindow *pWindow = static_cast<GuiCanvas*>(getRoot())->getPlatformWindow();
      if( !pWindow )
         return false;

      PlatformCursorController *pController = pWindow->getCursorController();
      if( !pController )
         return false;

      pWindow->setMouseLocked(false);
      pCanvas->setCursorON( true );

      if(mDisplayType != DisplayTypePerspective)
      {
         mouseUnlock();
         pCanvas->setForceMouseToGUI(false);
         pCanvas->setClampTorqueCursor(mLastMouseClamping);
      }
   }

   if(mMiddleMousePassThru && event.deviceType == MouseDeviceType &&
      event.objInst == KEY_BUTTON2 && event.action == SI_BREAK)
   {
      // if the middle mouse pass thru is enabled,
      // we want to reactivate mouse on a middle mouse button up
      GuiCanvas *pCanvas = getRoot();
      if( !pCanvas )
         return false;

      PlatformWindow *pWindow = static_cast<GuiCanvas*>(getRoot())->getPlatformWindow();
      if( !pWindow )
         return false;

      PlatformCursorController *pController = pWindow->getCursorController();
      if( !pController )
         return false;

      pWindow->setMouseLocked(false);
      pCanvas->setCursorON( true );
   }

   // we return false so that the canvas can properly process the right mouse button up...
   return false;
}

//------------------------------------------------------------------------------

void EditTSCtrl::calcOrthoCamOffset(F32 mousex, F32 mousey, U8 modifier)
{
   F32 camScale = 0.01f;

   switch(mDisplayType)
   {
      case DisplayTypeTop:
         mOrthoCamTrans.x -= mousex * mOrthoFOV * camScale;
         mOrthoCamTrans.y += mousey * mOrthoFOV * camScale;
         break;

      case DisplayTypeBottom:
         mOrthoCamTrans.x -= mousex * mOrthoFOV * camScale;
         mOrthoCamTrans.y -= mousey * mOrthoFOV * camScale;
         break;

      case DisplayTypeFront:
         mOrthoCamTrans.x += mousex * mOrthoFOV * camScale;
         mOrthoCamTrans.z += mousey * mOrthoFOV * camScale;
         break;

      case DisplayTypeBack:
         mOrthoCamTrans.x -= mousex * mOrthoFOV * camScale;
         mOrthoCamTrans.z += mousey * mOrthoFOV * camScale;
         break;

      case DisplayTypeLeft:
         mOrthoCamTrans.y += mousex * mOrthoFOV * camScale;
         mOrthoCamTrans.z += mousey * mOrthoFOV * camScale;
         break;

      case DisplayTypeRight:
         mOrthoCamTrans.y -= mousex * mOrthoFOV * camScale;
         mOrthoCamTrans.z += mousey * mOrthoFOV * camScale;
         break;

      case DisplayTypeIsometric:
         if(modifier & SI_PRIMARY_CTRL)
         {
            // NOTE: Maybe move the center of rotation code to right mouse down to avoid compound errors?
            F32 rot = mDegToRad(mousex);

            Point3F campos = (mRawCamPos + mOrthoCamTrans) - mIsoCamRotCenter;
            MatrixF mat(EulerF(0, 0, rot));
            mat.mulP(campos);
            mOrthoCamTrans = (campos + mIsoCamRotCenter) - mRawCamPos;
            mIsoCamRot.z += rot;

         }
         else
         {
            mOrthoCamTrans.x -= mousex * mOrthoFOV * camScale * mCos(mIsoCamRot.z) - mousey * mOrthoFOV * camScale * mSin(mIsoCamRot.z);
            mOrthoCamTrans.y += mousex * mOrthoFOV * camScale * mSin(mIsoCamRot.z) + mousey * mOrthoFOV * camScale * mCos(mIsoCamRot.z);
         }
         break;
   }
}

//------------------------------------------------------------------------------

void EditTSCtrl::renderWorld(const RectI & updateRect)
{
   gClientSceneGraph->setDisplayTargetResolution(getExtent());
   gClientSceneGraph->renderScene( SPT_Diffuse );

   // render the mission area...
   if ( mRenderMissionArea )
      renderMissionArea();

   // render through console callbacks
   SimSet * missionGroup = static_cast<SimSet*>(Sim::findObject("MissionGroup"));
   if(missionGroup)
   {
      mConsoleRendering = true;
 
      for(SimSetIterator itr(missionGroup); *itr; ++itr)
      {
         SimObject* object = *itr;
         const bool renderEnabled = object->getClassRep()->isRenderEnabled();
         
         if( renderEnabled )
         {
            char buf[2][16];
            dSprintf(buf[0], 16, object->isSelected() ? "true" : "false");
            dSprintf(buf[1], 16, object->isExpanded() ? "true" : "false");
            
            Con::executef( object, "onEditorRender", getIdString(), buf[0], buf[1] );
         }
      }

      mConsoleRendering = false;
   }

   // Draw the grid
   if ( mRenderGridPlane )
      renderGrid();

   // render the editor stuff
   renderScene(updateRect);

	// Draw the camera axis
   GFX->setClipRect(updateRect);
   GFX->setStateBlock(mDefaultGuiSB);
   renderCameraAxis();
}

void EditTSCtrl::renderMissionArea()
{
   // TODO: Fix me!
   /*
   MissionArea * obj = dynamic_cast<MissionArea*>(Sim::findObject("MissionAreaObject"));
   if( obj == NULL )
      return;

   //EditTSCTRL has a pointer to the currently active terrain. Going to use it
   //TerrainBlock * terrain = dynamic_cast<TerrainBlock*>(Sim::findObject("Terrain"));

   F32 height = 0.f;

   TerrainBlock* activeTerrain = getActiveTerrain();
   if( activeTerrain != NULL )
   {
      GridSquare * gs = activeTerrain->findSquare(TerrainBlock::BlockShift, Point2I(0,0));
      height = F32(gs->maxHeight) * 0.03125f + 10.f;
   }

   //
   const RectI &area = obj->getArea();
   Box3F areaBox( area.point.x, 
                  area.point.y, 
                  0, 
                  area.point.x + area.extent.x,
                  area.point.y + area.extent.y,
                  height );

   GFX->getDrawUtil()->drawSolidBox( areaBox, mMissionAreaFillColor );
   GFX->getDrawUtil()->drawWireBox( areaBox, mMissionAreaFrameColor );
   */
}

void EditTSCtrl::renderCameraAxis()
{
   static MatrixF sRotMat(EulerF( (M_PI_F / -2.0f), 0.0f, 0.0f));

   MatrixF camMat = mLastCameraQuery.cameraMatrix;
   camMat.mul(sRotMat);
   camMat.inverse();

   MatrixF axis;
   axis.setColumn(0, Point3F(1, 0, 0));
   axis.setColumn(1, Point3F(0, 0, 1));
   axis.setColumn(2, Point3F(0, -1, 0));
   axis.mul(camMat);

   Point3F forwardVec, upVec, rightVec;
	axis.getColumn( 2, &forwardVec );
	axis.getColumn( 1, &upVec );
	axis.getColumn( 0, &rightVec );

   Point2I pos = getPosition();
	F32 offsetx = pos.x + 20.0;
	F32 offsety = pos.y + getExtent().y - 42.0; // Take the status bar into account
	F32 scale = 15.0;

   // Generate correct drawing order
   ColorI c1(255,0,0);
   ColorI c2(0,255,0);
   ColorI c3(0,0,255);
	ColorI tc;
	Point3F *p1, *p2, *p3, *tp;
	p1 = &rightVec;
	p2 = &upVec;
	p3 = &forwardVec;
	if(p3->y > p2->y)
	{
		tp = p2; tc = c2;
		p2 = p3; c2 = c3;
		p3 = tp; c3 = tc;
	}
	if(p2->y > p1->y)
	{
		tp = p1; tc = c1;
		p1 = p2; c1 = c2;
		p2 = tp; c2 = tc;
	}

   PrimBuild::begin( GFXLineList, 6 );
		//*** Axis 1
		PrimBuild::color(c1);
		PrimBuild::vertex3f(offsetx, offsety, 0);
		PrimBuild::vertex3f(offsetx+p1->x*scale, offsety-p1->z*scale, 0);

		//*** Axis 2
		PrimBuild::color(c2);
		PrimBuild::vertex3f(offsetx, offsety, 0);
		PrimBuild::vertex3f(offsetx+p2->x*scale, offsety-p2->z*scale, 0);

		//*** Axis 3
		PrimBuild::color(c3);
		PrimBuild::vertex3f(offsetx, offsety, 0);
		PrimBuild::vertex3f(offsetx+p3->x*scale, offsety-p3->z*scale, 0);
   PrimBuild::end();
}

void EditTSCtrl::renderGrid()
{
   if (  mDisplayType == DisplayTypePerspective ||
         mDisplayType == DisplayTypeIsometric )
      return;

   // Calculate the displayed grid size based on view
   F32 drawnGridSize = mGridPlaneSize;
   F32 gridPixelSize = projectRadius(1.0f, mGridPlaneSize);
   if(gridPixelSize < mGridPlaneSizePixelBias)
   {
      U32 counter = 1;
      while(gridPixelSize < mGridPlaneSizePixelBias)
      {
         drawnGridSize = mGridPlaneSize * counter * 10.0f;
         gridPixelSize = projectRadius(1.0f, drawnGridSize);

         ++counter;

         // No infinite loops here
         if(counter > 1000)
            break;
      }
   }

   F32 minorTickSize = 0;
   F32 gridSize = drawnGridSize;
   U32 minorTickMax = mGridPlaneMinorTicks + 1;
   if(minorTickMax > 0)
   {
      minorTickSize = drawnGridSize;
      gridSize = drawnGridSize * minorTickMax;
   }

   // Build the view-based origin
   VectorF dir;
   smCamMatrix.getColumn( 1, &dir );

   Point3F gridPlanePos = smCamPos + ( dir * smCamNearPlane );
   Point2F size(mOrthoWidth + 2 * gridSize, mOrthoHeight + 2 * gridSize);

   GFXStateBlockDesc desc;
   desc.setBlend( true );
   desc.setZReadWrite( true, false );

   GFX->getDrawUtil()->drawPlaneGrid( desc, gridPlanePos, size, Point2F( minorTickSize, minorTickSize ), mGridPlaneMinorTickColor );
   GFX->getDrawUtil()->drawPlaneGrid( desc, gridPlanePos, size, Point2F( gridSize, gridSize ), mGridPlaneColor );
}

struct SceneBoundsInfo 
{
   bool  mValid;
   Box3F mBounds;

   SceneBoundsInfo()
   {
      mValid = false;
      mBounds.minExtents.set(1e10, 1e10, 1e10);
      mBounds.maxExtents.set(-1e10, -1e10, -1e10);
   }
};

static void sceneBoundsCalcCallback(SceneObject* obj, void *key)
{
   // Early out for those objects that slipped through the mask check
   // because they belong to more than one type.
   if((obj->getType() & EditTSCtrl::smSceneBoundsMask) != 0)
      return;

   if(obj->isGlobalBounds())
      return;

   SceneBoundsInfo* bounds = (SceneBoundsInfo*)key;

   Point3F min = obj->getWorldBox().minExtents;
   Point3F max = obj->getWorldBox().maxExtents;

   if (min.x <= -5000.0f || min.y <= -5000.0f || min.z <= -5000.0f ||
       min.x >=  5000.0f || min.y >=  5000.0f || min.z >=  5000.0f)
       Con::errorf("SceneObject %d (%s : %s) has a bounds that could cause problems with a non-perspective view", obj->getId(), obj->getClassName(), obj->getName());

   bounds->mBounds.minExtents.setMin(min);
   bounds->mBounds.minExtents.setMin(max);
   bounds->mBounds.maxExtents.setMax(min);
   bounds->mBounds.maxExtents.setMax(max);

   bounds->mValid = true;
}

bool EditTSCtrl::processCameraQuery(CameraQuery * query)
{
   if(mDisplayType == DisplayTypePerspective)
   {
      query->ortho = false;
   }
   else
   {
      query->ortho = true;
   }

   GameConnection* connection = dynamic_cast<GameConnection *>(NetConnection::getConnectionToServer());
   if (connection)
   {
      if (connection->getControlCameraTransform(0.032f, &query->cameraMatrix)) 
      {
         query->farPlane = gClientSceneGraph->getVisibleDistance() * smVisibleDistanceScale;
         query->nearPlane = gClientSceneGraph->getNearClip();
         query->fov = mDegToRad(90.0f);

         if(query->ortho)
         {
            MatrixF camRot(true);
            SceneBoundsInfo sceneBounds;
            const F32 camBuffer = 1.0f;
            Point3F camPos = query->cameraMatrix.getPosition();

            F32 isocamplanedist = 0.0f;
            if(mDisplayType == DisplayTypeIsometric)
            {
               const RectI& vp = GFX->getViewport();
               isocamplanedist = 0.25 * vp.extent.y * mSin(mIsoCamAngle);
            }

            // Calculate the scene bounds
            gClientContainer.findObjects(~(smSceneBoundsMask), sceneBoundsCalcCallback, &sceneBounds);

            if(!sceneBounds.mValid)
            {
               sceneBounds.mBounds.maxExtents = camPos + smMinSceneBounds;
               sceneBounds.mBounds.minExtents = camPos - smMinSceneBounds;
            }
            else
            {
               query->farPlane = getMax(smMinSceneBounds.x * 2.0f, (sceneBounds.mBounds.maxExtents - sceneBounds.mBounds.minExtents).len() + camBuffer * 2.0f + isocamplanedist);
            }

            mRawCamPos = camPos;
            camPos += mOrthoCamTrans;

            switch(mDisplayType)
            {
               case DisplayTypeTop:
                  camRot.setColumn(0, Point3F(1.0, 0.0,  0.0));
                  camRot.setColumn(1, Point3F(0.0, 0.0, -1.0));
                  camRot.setColumn(2, Point3F(0.0, 1.0,  0.0));
                  camPos.z = getMax(camPos.z + smMinSceneBounds.z, sceneBounds.mBounds.maxExtents.z + camBuffer);
                  break;

               case DisplayTypeBottom:
                  camRot.setColumn(0, Point3F(1.0,  0.0,  0.0));
                  camRot.setColumn(1, Point3F(0.0,  0.0,  1.0));
                  camRot.setColumn(2, Point3F(0.0, -1.0,  0.0));
                  camPos.z = getMin(camPos.z - smMinSceneBounds.z, sceneBounds.mBounds.minExtents.z - camBuffer);
                  break;

               case DisplayTypeFront:
                  camRot.setColumn(0, Point3F(-1.0,  0.0,  0.0));
                  camRot.setColumn(1, Point3F( 0.0, -1.0,  0.0));
                  camRot.setColumn(2, Point3F( 0.0,  0.0,  1.0));
                  camPos.y = getMax(camPos.z + smMinSceneBounds.z, sceneBounds.mBounds.maxExtents.y + camBuffer);
                  break;

               case DisplayTypeBack:
                  //camRot.setColumn(0, Point3F(1.0,  0.0,  0.0));
                  //camRot.setColumn(1, Point3F(0.0,  1.0,  0.0));
                  //camRot.setColumn(2, Point3F(0.0,  0.0,  1.0));
                  camPos.y = getMin(camPos.y - smMinSceneBounds.y, sceneBounds.mBounds.minExtents.y - camBuffer);
                  break;

               case DisplayTypeLeft:
                  camRot.setColumn(0, Point3F( 0.0, -1.0,  0.0));
                  camRot.setColumn(1, Point3F( 1.0,  0.0,  0.0));
                  camRot.setColumn(2, Point3F( 0.0,  0.0,  1.0));
                  camPos.x = getMin(camPos.x - smMinSceneBounds.x, sceneBounds.mBounds.minExtents.x - camBuffer);
                  break;

               case DisplayTypeRight:
                  camRot.setColumn(0, Point3F( 0.0,  1.0,  0.0));
                  camRot.setColumn(1, Point3F(-1.0,  0.0,  0.0));
                  camRot.setColumn(2, Point3F( 0.0,  0.0,  1.0));
                  camPos.x = getMax(camPos.x + smMinSceneBounds.x, sceneBounds.mBounds.maxExtents.x + camBuffer);
                  break;

               case DisplayTypeIsometric:
                  camPos.z = sceneBounds.mBounds.maxExtents.z + camBuffer + isocamplanedist;
                  MatrixF angle(EulerF(mIsoCamAngle, 0, 0));
                  MatrixF rot(mIsoCamRot);
                  camRot.mul(rot, angle);
                  break;
            }

            query->cameraMatrix = camRot;
            query->cameraMatrix.setPosition(camPos);
            query->fov = mOrthoFOV;
         }

         smCamMatrix = query->cameraMatrix;
         smCamMatrix.getColumn(3,&smCamPos);
         smCamOrtho = query->ortho;
         smCamNearPlane = query->nearPlane;

         return true;
      }
   }
   return false;

}

//------------------------------------------------------------------------------
ConsoleMethod(EditTSCtrl, getDisplayType, S32, 2, 2, "")
{
   return object->getDisplayType();
}

ConsoleMethod(EditTSCtrl, setDisplayType, void, 3, 3, "(int displayType)")
{
   object->setDisplayType(dAtoi(argv[2]));
}

ConsoleMethod( EditTSCtrl, renderBox, void, 4, 4, "( Point3F pos, Point3F size )" )
{
   if( !object->mConsoleRendering || !object->mConsoleFillColor.alpha )
      return;
      
   Point3F pos;
   Point3F size;
   
   dSscanf( argv[ 2 ], "%f %f %f", &pos.x, &pos.y, &pos.z );
   dSscanf( argv[ 3 ], "%f %f %f", &size.x, &size.y, &size.z );
      
   GFXStateBlockDesc desc;
   desc.setBlend( true );
   
   Box3F box;
   box.set( size );
   box.setCenter( pos );
   
   if( box.isContained( GFX->getWorldMatrix().getPosition() ) )
      desc.setCullMode( GFXCullNone );

   GFX->getDrawUtil()->drawCube( desc, size, pos, object->mConsoleFillColor );
}

ConsoleMethod(EditTSCtrl, renderSphere, void, 4, 5, "(Point3F pos, float radius, int subdivisions=NULL)")
{
   if ( !object->mConsoleRendering || !object->mConsoleFillColor.alpha )
      return;

   // TODO: We need to support subdivision levels in GFXDrawUtil!
   S32 sphereLevel = object->mConsoleSphereLevel;
   if(argc == 5)
      sphereLevel = dAtoi(argv[4]);

   Point3F pos;
   dSscanf(argv[2], "%f %f %f", &pos.x, &pos.y, &pos.z);

   F32 radius = dAtof(argv[3]);

   GFXStateBlockDesc desc;
   desc.setBlend( true );
   
   SphereF sphere( pos, radius );
   if( sphere.isContained( GFX->getWorldMatrix().getPosition() ) )
      desc.setCullMode( GFXCullNone );

   GFX->getDrawUtil()->drawSphere( desc, radius, pos, object->mConsoleFillColor );   
}

ConsoleMethod( EditTSCtrl, renderCircle, void, 5, 6, "(Point3F pos, Point3F normal, float radius, int segments=NULL)")
{
   if(!object->mConsoleRendering)
      return;

   if(!object->mConsoleFrameColor.alpha && !object->mConsoleFillColor.alpha)
      return;

   Point3F pos, normal;
   dSscanf(argv[2], "%g %g %g", &pos.x, &pos.y, &pos.z);
   dSscanf(argv[3], "%g %g %g", &normal.x, &normal.y, &normal.z);

   F32 radius = dAtof(argv[4]);

   S32 segments = object->mConsoleCircleSegments;
   if(argc == 6)
      segments = dAtoi(argv[5]);

   normal.normalize();

   AngAxisF aa;
   mCross(normal, Point3F(0,0,1), &aa.axis);
   aa.axis.normalizeSafe();
   aa.angle = mAcos(mClampF(mDot(normal, Point3F(0,0,1)), -1.f, 1.f));

   if(aa.angle == 0.f)
      aa.axis.set(0,0,1);

   MatrixF mat;
   aa.setMatrix(&mat);

   F32 step = M_2PI / segments;
   F32 angle = 0.f;

   Vector<Point3F> points;
   segments--;
   for(U32 i = 0; i < segments; i++)
   {
      Point3F pnt(mCos(angle), mSin(angle), 0.f);

      mat.mulP(pnt);
      pnt *= radius;
      pnt += pos;

      points.push_back(pnt);
      angle += step;
   }

   GFX->setStateBlock(object->mBlendSB);
   //GFX->setAlphaBlendEnable( true );
   //GFX->setSrcBlend( GFXBlendSrcAlpha );
   //GFX->setDestBlend( GFXBlendInvSrcAlpha );

   // framed
   if(object->mConsoleFrameColor.alpha)
   {
      // TODO: Set GFX line width (when it exists) to the value of 'object->mConsoleLineWidth'

      PrimBuild::color( object->mConsoleFrameColor );

      PrimBuild::begin( GFXLineStrip, points.size() + 1 );

      for( int i = 0; i < points.size(); i++ )
         PrimBuild::vertex3fv( points[i] );

      // GFX does not have a LineLoop primitive, so connect the last line
      if( points.size() > 0 )
         PrimBuild::vertex3fv( points[0] );

      PrimBuild::end();

      // TODO: Reset GFX line width here
   }

   // filled
   if(object->mConsoleFillColor.alpha)
   {
      PrimBuild::color( object->mConsoleFillColor );

      PrimBuild::begin( GFXTriangleFan, points.size() + 1 );

      // Center point
      PrimBuild::vertex3fv( pos );

      // Edge verts
      for( int i = 0; i < points.size(); i++ )
         PrimBuild::vertex3fv( points[i] );

      PrimBuild::end();
   }

   //GFX->setAlphaBlendEnable( false );
}

ConsoleMethod( EditTSCtrl, renderTriangle, void, 5, 5, "(Point3F a, Point3F b, Point3F c)")
{
   
   if(!object->mConsoleRendering)
      return;

   if(!object->mConsoleFrameColor.alpha && !object->mConsoleFillColor.alpha)
      return;

   Point3F pnts[3];
   for(U32 i = 0; i < 3; i++)
      dSscanf(argv[i+2], "%f %f %f", &pnts[i].x, &pnts[i].y, &pnts[i].z);

   
   GFX->setStateBlock(object->mBlendSB);
   //GFX->setAlphaBlendEnable( true );
   //GFX->setSrcBlend( GFXBlendSrcAlpha );
   //GFX->setDestBlend( GFXBlendInvSrcAlpha );

   // frame
   if( object->mConsoleFrameColor.alpha )
   {
      PrimBuild::color( object->mConsoleFrameColor );
  
      // TODO: Set GFX line width (when it exists) to the value of 'object->mConsoleLineWidth'

      PrimBuild::begin( GFXLineStrip, 4 );
         PrimBuild::vertex3fv( pnts[0] );
         PrimBuild::vertex3fv( pnts[1] );
         PrimBuild::vertex3fv( pnts[2] );
         PrimBuild::vertex3fv( pnts[0] );
      PrimBuild::end();

      // TODO: Reset GFX line width here
   }

   // fill
   if( object->mConsoleFillColor.alpha )
   {
      PrimBuild::color( object->mConsoleFillColor );

      PrimBuild::begin( GFXTriangleList, 3 );
         PrimBuild::vertex3fv( pnts[0] );
         PrimBuild::vertex3fv( pnts[1] );
         PrimBuild::vertex3fv( pnts[2] );
      PrimBuild::end();
   }

   // GFX->setAlphaBlendEnable( false );
}

ConsoleMethod( EditTSCtrl, renderLine, void, 4, 5, "(Point3F start, Point3F end, int width)")
{
   if ( !object->mConsoleRendering || !object->mConsoleFrameColor.alpha )
      return;

   Point3F start, end;
   dSscanf(argv[2], "%f %f %f", &start.x, &start.y, &start.z);
   dSscanf(argv[3], "%f %f %f", &end.x, &end.y, &end.z);

   // TODO: We don't support 3d lines with width... fix this!
   S32 lineWidth = object->mConsoleLineWidth;
   if ( argc == 5 )
      lineWidth = dAtoi(argv[4]);

   GFX->getDrawUtil()->drawLine( start, end, object->mConsoleFrameColor );
}

ConsoleMethod( EditTSCtrl, getGizmo, S32, 2, 2, "" )
{
   return object->getGizmo()->getId();
}

ConsoleMethod(EditTSCtrl, isMiddleMouseDown, bool, 2, 2, "")
{
   return object->isMiddleMouseDown();
}
