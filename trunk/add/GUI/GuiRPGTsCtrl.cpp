#include "GuiRPGTsCtrl.h"
#include "gui/core/guiCanvas.h"
#include "T3D/objectTypes.h"
#include "T3D/gameFunctions.h"
#include "../RPGPack/RPGUtils.h"
#include "T3D/gameConnection.h"
#include "T3D/aiPlayer.h"
#include "../RPGPack/Constraint/CST_TerrainClickedPoint.h"

IMPLEMENT_CONOBJECT(RPGTSCtrl);

RPGTSCtrl::RPGTSCtrl()
{
	mMouse3DVec.zero();
	mMouse3DPos.zero();
	mouse_dn_timestamp = 0;
}

bool RPGTSCtrl::processCameraQuery(CameraQuery *camq)
{
	return Parent::processCameraQuery(camq);
}

void RPGTSCtrl::renderWorld(const RectI &updateRect)
{
	Parent::renderWorld(updateRect);
}

void RPGTSCtrl::onRender(Point2I offset, const RectI &updateRect)
{
	Parent::onRender(offset,updateRect);
}

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

void RPGTSCtrl::onMouseDown(const GuiEvent &evt)
{   
	//Con::printf("#### RPGTSCtrl::onLeftMouseDown() ####");
	GameConnection * pGameConn = GameConnection::getConnectionToServer();
	if (pGameConn)
	{
		AIPlayer * pPlayer = dynamic_cast<AIPlayer*>(pGameConn->getControlObject());
		if (pPlayer)
		{
			MatrixF mat;
			Point3F face;
			if (GameGetCameraTransform(&mat,&face))
			{
				Point3F camPos;
				mat.getColumn(3,&camPos);
				Point3F worldPos;
				Point3F screenPos(evt.mousePoint.x,evt.mousePoint.y,1);
				if (unproject(screenPos,&worldPos))
				{
					Point3F pt3DPos = camPos;
					Point3F pt3DVec = worldPos - camPos;
					pt3DVec.normalizeSafe();
					Point3F ptEnd = pt3DPos + pt3DVec * 2000;
					RayInfo ray;
					pPlayer->disableCollision();
					U32 mask = PlayerObjectType | ItemObjectType | AIObjectType | TerrainObjectType;
					if (gClientContainer.castRay(pt3DPos,ptEnd,mask,&ray))
					{
						if (ray.object->getType() & TerrainObjectType)
						{
							//pPlayer->setMoveDestination(ray.point,false);
							CST_TerrainClicked::smTerrainClicked = ray.point;
							char position[256];
							dSprintf(position,256,"%2f %2f %2f",ray.point.x,ray.point.y,ray.point.z);
							Con::executef("onClickTerrian",position);
						}
					}
					pPlayer->enableCollision();
				}
			}
		}
	}
	// save a timestamp so we can measure how long the button is down
	mouse_dn_timestamp = Platform::getRealMilliseconds();


	GuiCanvas* Canvas = getRoot();


	// clear button down status because the ActionMap is going to capture
	// the mouse and the button up will never happen
	Canvas->clearMouseButtonDown();

	// indicate that processing of event should continue (pass down to ActionMap)
	Canvas->setConsumeLastInputEvent(false);
}

void RPGTSCtrl::onMouseUp(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onLeftMouseUp() ####");
}

void RPGTSCtrl::onRightMouseDown(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onRightMouseDown() ####");
	GuiCanvas* Canvas = getRoot();


	// clear right button down status because the ActionMap is going to capture
	// the mouse and the right button up will never happen
	Canvas->clearMouseRightButtonDown();

	// indicate that processing of event should continue (pass down to ActionMap)
	Canvas->setConsumeLastInputEvent(false);
}

void RPGTSCtrl::onRightMouseUp(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onRightMouseUp() ####");
}

void RPGTSCtrl::onMouseMove(const GuiEvent& evt)
{
	MatrixF cam_xfm;
	Point3F dummy_pt;
	if (GameGetCameraTransform(&cam_xfm, &dummy_pt))    
	{      
		// get cam pos 
		Point3F cameraPoint; cam_xfm.getColumn(3,&cameraPoint); 
		// construct 3D screen point from mouse coords 
		Point3F screen_pt((F32)evt.mousePoint.x, (F32)evt.mousePoint.y, 1.0f);  
		// convert screen point to world point
		Point3F world_pt;      
		if (unproject(screen_pt, &world_pt))       
		{         
			Point3F mouseVec = world_pt - cameraPoint;         
			mouseVec.normalizeSafe();    

			mMouse3DPos = cameraPoint;
			mMouse3DVec = mouseVec;

			F32 selectRange = 200.f;//
			Point3F mouseScaled = mouseVec*selectRange;
			Point3F rangeEnd = cameraPoint + mouseScaled;

			U32 selectionMask = PlayerObjectType;
			RPGUtils::rolloverRayCast(cameraPoint, rangeEnd, selectionMask);
		}
	}
}

void RPGTSCtrl::onMouseEnter(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onMouseEnter() ####");
}

void RPGTSCtrl::onMouseDragged(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onMouseDragged() ####");
}


void RPGTSCtrl::onMouseLeave(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onMouseLeave() ####");
}

bool RPGTSCtrl::onMouseWheelUp(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onMouseWheelUp() ####");
	Con::executef(this, "onMouseWheelUp");
	return true;
}

bool RPGTSCtrl::onMouseWheelDown(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onMouseWheelDown() ####");
	Con::executef(this, "onMouseWheelDown");
	return true;
}


void RPGTSCtrl::onRightMouseDragged(const GuiEvent& evt)
{
	//Con::printf("#### RPGTSCtrl::onRightMouseDragged() ####");
}