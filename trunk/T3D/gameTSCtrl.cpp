//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/gameTSCtrl.h"
#include "console/consoleTypes.h"
#include "T3D/gameBase.h"
#include "T3D/gameConnection.h"
#include "T3D/shapeBase.h"
#include "T3D/gameFunctions.h"

//---------------------------------------------------------------------------
// Debug stuff:
Point3F lineTestStart = Point3F(0, 0, 0);
Point3F lineTestEnd =   Point3F(0, 1000, 0);
Point3F lineTestIntersect =  Point3F(0, 0, 0);
bool gSnapLine = false;

//----------------------------------------------------------------------------
// Class: GameTSCtrl
//----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GameTSCtrl);

GameTSCtrl::GameTSCtrl()
{
}

//---------------------------------------------------------------------------
bool GameTSCtrl::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

#ifdef TORQUE_DEMO_WATERMARK
   mWatermark.init();
#endif

   return true;
}

//---------------------------------------------------------------------------
bool GameTSCtrl::processCameraQuery(CameraQuery *camq)
{
   GameUpdateCameraFov();
   return GameProcessCameraQuery(camq);
}

//---------------------------------------------------------------------------
void GameTSCtrl::renderWorld(const RectI &updateRect)
{
   GameRenderWorld();
}

//---------------------------------------------------------------------------
void GameTSCtrl::makeScriptCall(const char *func, const GuiEvent &evt) const
{
   // write screen position
   char *sp = Con::getArgBuffer(32);
   dSprintf(sp, 32, "%d %d", evt.mousePoint.x, evt.mousePoint.y);

   // write world position
   char *wp = Con::getArgBuffer(32);
   Point3F camPos;
   mLastCameraQuery.cameraMatrix.getColumn(3, &camPos);
   dSprintf(wp, 32, "%g %g %g", camPos.x, camPos.y, camPos.z);

   // write click vector
   char *vec = Con::getArgBuffer(32);
   Point3F fp(evt.mousePoint.x, evt.mousePoint.y, 1.0);
   Point3F ray;
   unproject(fp, &ray);
   ray -= camPos;
   ray.normalizeSafe();
   dSprintf(vec, 32, "%g %g %g", ray.x, ray.y, ray.z);

   Con::executef( (SimObject*)this, func, sp, wp, vec );
}

void GameTSCtrl::onMouseDown(const GuiEvent &evt)
{
   Parent::onMouseDown(evt);
   if( isMethod( "onMouseDown" ) )
      makeScriptCall( "onMouseDown", evt );
}

void GameTSCtrl::onRightMouseDown(const GuiEvent &evt)
{
   Parent::onRightMouseDown(evt);
   if( isMethod( "onRightMouseDown" ) )
      makeScriptCall( "onRightMouseDown", evt );
}

void GameTSCtrl::onMiddleMouseDown(const GuiEvent &evt)
{
   Parent::onMiddleMouseDown(evt);
   if( isMethod( "onMiddleMouseDown" ) )
      makeScriptCall( "onMiddleMouseDown", evt );
}

void GameTSCtrl::onMouseUp(const GuiEvent &evt)
{
   Parent::onMouseUp(evt);
   if( isMethod( "onMouseUp" ) )
      makeScriptCall( "onMouseUp", evt );
}

void GameTSCtrl::onRightMouseUp(const GuiEvent &evt)
{
   Parent::onRightMouseUp(evt);
   if( isMethod( "onRightMouseUp" ) )
      makeScriptCall( "onRightMouseUp", evt );
}

void GameTSCtrl::onMiddleMouseUp(const GuiEvent &evt)
{
   Parent::onMiddleMouseUp(evt);
   if( isMethod( "onMiddleMouseUp" ) )
      makeScriptCall( "onMiddleMouseUp", evt );
}

void GameTSCtrl::onMouseMove(const GuiEvent &evt)
{
   if(gSnapLine)
      return;

   MatrixF mat;
   Point3F vel;
   if ( GameGetCameraTransform(&mat, &vel) )
   {
      Point3F pos;
      mat.getColumn(3,&pos);
      Point3F screenPoint((F32)evt.mousePoint.x, (F32)evt.mousePoint.y, -1.0f);
      Point3F worldPoint;
      if (unproject(screenPoint, &worldPoint)) {
         Point3F vec = worldPoint - pos;
         lineTestStart = pos;
         vec.normalizeSafe();
         lineTestEnd = pos + vec * 1000;
      }
   }
}

void GameTSCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   // check if should bother with a render
   GameConnection * con = GameConnection::getConnectionToServer();
   bool skipRender = !con || (con->getWhiteOut() >= 1.f) || (con->getDamageFlash() >= 1.f) || (con->getBlackOut() >= 1.f);

   if(!skipRender || true)
      Parent::onRender(offset, updateRect);

#ifdef TORQUE_DEMO_WATERMARK
   mWatermark.render(getExtent());
#endif
}

//--------------------------------------------------------------------------
ConsoleFunction( snapToggle, void, 1, 1, "()" )
{
   gSnapLine = !gSnapLine;
}
