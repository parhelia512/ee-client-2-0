//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/3d/guiTSControl.h"

#include "console/consoleTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "lighting/lightManager.h"
#include "gfx/sim/debugDraw.h"
#include "math/mathUtils.h"
#include "gui/core/guiCanvas.h"
#include "sceneGraph/reflectionManager.h"
#include "postFx/postEffectManager.h"
#include "postFx/postEffect.h"

IMPLEMENT_CONOBJECT( GuiTSCtrl );

U32 GuiTSCtrl::smFrameCount = 0;

Vector<GuiTSCtrl*> GuiTSCtrl::smAwakeTSCtrls;


GuiTSCtrl::GuiTSCtrl()
{
   mCameraZRot = 0;
   mForceFOV = 0;
   mReflectPriority = 1.0f;

   mSaveModelview.identity();
   mSaveProjection.identity();
   mSaveViewport.set( 0, 0, 10, 10 );
   mSaveWorldToScreenScale.set( 0, 0 );

   mLastCameraQuery.cameraMatrix.identity();
   mLastCameraQuery.fov = 45.0f;
   mLastCameraQuery.object = NULL;
   mLastCameraQuery.farPlane = 10.0f;
   mLastCameraQuery.nearPlane = 0.01f;

   mLastCameraQuery.ortho = false;
}
void GuiTSCtrl::initPersistFields()
{
   addField("cameraZRot", TypeF32, Offset(mCameraZRot, GuiTSCtrl));
   addField("forceFOV",   TypeF32, Offset(mForceFOV,   GuiTSCtrl));
   addField("reflectPriority", TypeF32, Offset(mReflectPriority,   GuiTSCtrl));
   
   Parent::initPersistFields();
}

void GuiTSCtrl::consoleInit()
{
   Con::addVariable("$TSControl::frameCount", TypeS32, &smFrameCount);
}

bool GuiTSCtrl::onWake()
{
   if ( !Parent::onWake() )
      return false;

   // Add ourselves to the active viewport list.
   AssertFatal( !smAwakeTSCtrls.contains( this ), 
      "GuiTSCtrl::onWake - This control is already in the awake list!" );
   smAwakeTSCtrls.push_back( this );

   return true;
}

void GuiTSCtrl::onSleep()
{
   Parent::onSleep();

   AssertFatal( smAwakeTSCtrls.contains( this ), 
      "GuiTSCtrl::onSleep - This control is not in the awake list!" );
   smAwakeTSCtrls.remove( this );
}

void GuiTSCtrl::onPreRender()
{
   setUpdate();
}

bool GuiTSCtrl::processCameraQuery(CameraQuery *)
{
   return false;
}

void GuiTSCtrl::renderWorld(const RectI& /*updateRect*/)
{
}

F32 GuiTSCtrl::projectRadius( F32 dist, F32 radius ) const
{
   // Fixup any negative or zero distance so we
   // don't get a divide by zero.
   dist = dist > 0.0f ? dist : 0.001f;
   return ( radius / dist ) * mSaveWorldToScreenScale.y;   
}

bool GuiTSCtrl::project( const Point3F &pt, Point3F *dest ) const
{
   return MathUtils::mProjectWorldToScreen(pt,dest,mSaveViewport,mSaveModelview,mSaveProjection);
}

bool GuiTSCtrl::unproject( const Point3F &pt, Point3F *dest ) const
{
   MathUtils::mProjectScreenToWorld(pt,dest,mSaveViewport,mSaveModelview,mSaveProjection,mLastCameraQuery.farPlane,mLastCameraQuery.nearPlane);
   return true;
}


F32 GuiTSCtrl::calculateViewDistance(F32 radius)
{
   // Determine if we should use the width fov or height fov.
   // If the window is wider than tall, use the height fov to
   // keep the object completely in view.
   F32 fov = mLastCameraQuery.fov;
   F32 wwidth;
   F32 wheight;
   if(!mLastCameraQuery.ortho)
   {
      wwidth = mLastCameraQuery.nearPlane * mTan(fov / 2);
      wheight = F32(getHeight()) / F32(getWidth()) * wwidth;
   }
   else
   {
      wwidth = fov;
      wheight = F32(getHeight()) / F32(getWidth()) * wwidth;
   }
   if(wheight < wwidth)
   {
      fov = mAtan( wheight / mLastCameraQuery.nearPlane ) * 2.0f;
   }

   return (radius / mTan(fov * 0.5f));
}

static void offsetMatrix(const MatrixF & mat , MatrixF & matLeft , MatrixF & matRight , const F32 & offset)
{
	matLeft = mat;
	matRight = mat;

	Point3F pos = mat.getPosition();
	Point3F x;
	mat.getColumn(0,&x);
	x.normalizeSafe();

	pos -= x * offset;
	matLeft.setColumn(3,pos);

	pos += x * offset * 2;
	matRight.setColumn(3,pos);
}
// -------------------------------------------------------------------
// onRender
// -------------------------------------------------------------------
extern ColorI gCanvasClearColor;
void GuiTSCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   if(!processCameraQuery(&mLastCameraQuery))
   {
      // We have no camera, but render the GUI children 
      // anyway.  This makes editing GuiTSCtrl derived
      // controls easier in the GuiEditor.
      renderChildControls( offset, updateRect );
      return;
   }

   if ( mReflectPriority > 0 )
   {
      // Get the total reflection priority.
      F32 totalPriority = 0;
      for ( U32 i=0; i < smAwakeTSCtrls.size(); i++ )
         if ( smAwakeTSCtrls[i]->isVisible() )
            totalPriority += smAwakeTSCtrls[i]->mReflectPriority;

      REFLECTMGR->update(  mReflectPriority / totalPriority,
                           getExtent(),
                           mLastCameraQuery );
   }

   if(mForceFOV != 0)
      mLastCameraQuery.fov = mDegToRad(mForceFOV);

   if(mCameraZRot)
   {
      MatrixF rotMat(EulerF(0, 0, mDegToRad(mCameraZRot)));
      mLastCameraQuery.cameraMatrix.mul(rotMat);
   }

   // set up the camera and viewport stuff:
   F32 wwidth;
   F32 wheight;
   if(!mLastCameraQuery.ortho)
   {
      wwidth = mLastCameraQuery.nearPlane * mTan(mLastCameraQuery.fov / 2);
      wheight = F32(getHeight()) / F32(getWidth()) * wwidth;
   }
   else
   {
      wwidth = mLastCameraQuery.fov;
      wheight = F32(getHeight()) / F32(getWidth()) * wwidth;
   }

   F32 hscale = wwidth * 2 / F32(getWidth());
   F32 vscale = wheight * 2 / F32(getHeight());

   F32 left = (updateRect.point.x - offset.x) * hscale - wwidth;
   F32 right = (updateRect.point.x + updateRect.extent.x - offset.x) * hscale - wwidth;
   F32 top = wheight - vscale * (updateRect.point.y - offset.y);
   F32 bottom = wheight - vscale * (updateRect.point.y + updateRect.extent.y - offset.y);
      
   RectI tempRect = updateRect;
   
#ifdef TORQUE_OS_MAC
   Point2I screensize = getRoot()->getWindowSize();
   tempRect.point.y = screensize.y - (tempRect.point.y + tempRect.extent.y);
#endif

   GFX->setViewport( tempRect );

   // Clear the zBuffer so GUI doesn't hose object rendering accidentally
   GFX->clear( GFXClearZBuffer , ColorI(20,20,20), 1.0f, 0 );

   if(!mLastCameraQuery.ortho)
   {
      GFX->setFrustum( left, right, bottom, top,
                       mLastCameraQuery.nearPlane, mLastCameraQuery.farPlane );
   }
   else
   {
      GFX->setOrtho(left, right, bottom, top, mLastCameraQuery.nearPlane, mLastCameraQuery.farPlane, true);

      mOrthoWidth = right - left;
      mOrthoHeight = top - bottom;
   }

   // We're going to be displaying this render at size of this control in
   // pixels - let the scene know so that it can calculate e.g. reflections
   // correctly for that final display result.
   gClientSceneGraph->setDisplayTargetResolution(getExtent());

   // save the world matrix before attempting to draw anything
   GFX->pushWorldMatrix();

   // Set the GFX world matrix to the world-to-camera transform, but don't 
   // change the cameraMatrix in mLastCameraQuery. This is because 
   // mLastCameraQuery.cameraMatrix is supposed to contain the camera-to-world
   // transform. In-place invert would save a copy but mess up any GUIs that
   // depend on that value.
   MatrixF worldToCamera = mLastCameraQuery.cameraMatrix;
   worldToCamera.inverse();
   GFX->setWorldMatrix( worldToCamera );

   mSaveProjection = GFX->getProjectionMatrix();
   mSaveModelview = GFX->getWorldMatrix();
   mSaveViewport = updateRect;
   mSaveWorldToScreenScale = GFX->getWorldToScreenScale();

   // Set the default non-clip projection as some 
   // objects depend on this even in non-reflect cases.
   gClientSceneGraph->setNonClipProjection( mSaveProjection );

   // Give the post effect manager the worldToCamera, and cameraToScreen matrices
   PFXMGR->setFrameMatrices( mSaveModelview, mSaveProjection );

   if (!PostEffectManager::smRB3DEffects)
   {
	   renderWorld(updateRect);
   }
   else
   {
	   offsetMatrix(mLastCameraQuery.cameraMatrix,mLastCameraQuery.cameraMatrixLeft,mLastCameraQuery.cameraMatrixRight,0.025);
	   //==========left eye first==========
	   static PostEffect * pRB3DLeftEye = dynamic_cast<PostEffect *>( Sim::findObject("PFX_RB3D_LEFT") );
	   if (pRB3DLeftEye)
	   {
		   worldToCamera = mLastCameraQuery.cameraMatrixLeft;
		   worldToCamera.inverse();
		   GFX->setWorldMatrix( worldToCamera );
		   mSaveProjection = GFX->getProjectionMatrix();
		   mSaveModelview = GFX->getWorldMatrix();
		   mSaveViewport = updateRect;
		   mSaveWorldToScreenScale = GFX->getWorldToScreenScale();
		   gClientSceneGraph->setNonClipProjection( mSaveProjection );
		   PFXMGR->setFrameMatrices( mSaveModelview, mSaveProjection );
		   renderWorld(updateRect);
		   GFXTexHandle curBackBuffer;
		   curBackBuffer = PFXMGR->getBackBufferTex();
		   pRB3DLeftEye->process( gClientSceneGraph->getSceneState(), curBackBuffer);
	   }
	   //==========right eye============
	   GFX->clear( GFXClearZBuffer | GFXClearStencil | GFXClearTarget, gCanvasClearColor, 1.0f, 0 );
	   static PostEffect * pRB3DRightEye = dynamic_cast<PostEffect *>(Sim::findObject("PFX_RB3D_RIGHT"));
	   if (pRB3DRightEye)
	   {
		   worldToCamera = mLastCameraQuery.cameraMatrixRight;
		   worldToCamera.inverse();
		   GFX->setWorldMatrix( worldToCamera );
		   mSaveProjection = GFX->getProjectionMatrix();
		   mSaveModelview = GFX->getWorldMatrix();
		   mSaveViewport = updateRect;
		   mSaveWorldToScreenScale = GFX->getWorldToScreenScale();
		   gClientSceneGraph->setNonClipProjection( mSaveProjection );
		   PFXMGR->setFrameMatrices( mSaveModelview, mSaveProjection );
		   renderWorld(updateRect);
		   GFXTexHandle curBackBuffer;
		   curBackBuffer = PFXMGR->getBackBufferTex();
		   pRB3DRightEye->process( gClientSceneGraph->getSceneState(), curBackBuffer);
	   }
	   //==========combine============
	   //GFX->clear( GFXClearZBuffer | GFXClearStencil | GFXClearTarget, gCanvasClearColor, 1.0f, 0 );
	   static PostEffect * pRB3DCombine = dynamic_cast<PostEffect *>(Sim::findObject("PFX_RB3D_COMBINE"));
	   if (pRB3DCombine)
	   {
		   GFXTexHandle curBackBuffer;
		   curBackBuffer = PFXMGR->getBackBufferTex();
		   pRB3DCombine->process( gClientSceneGraph->getSceneState(), curBackBuffer);
	   }
   }
   DebugDrawer::get()->render();

   // restore the world matrix so the GUI will render correctly
   GFX->popWorldMatrix();

   renderChildControls(offset, updateRect);
   smFrameCount++;
}

// -------------------------------------------------------------------

ConsoleMethod(GuiTSCtrl, unproject, const char*, 3, 3, "unproject( %screenPosition ) : Transforms 3D screen space coordinates (x, y, depth) to world space.")
{
   char *returnBuffer = Con::getReturnBuffer(64);
   Point3F screenPos;
   Point3F worldPos;
   dSscanf(argv[2], "%g %g %g", &screenPos.x, &screenPos.y, &screenPos.z);
   object->unproject(screenPos, &worldPos);
   dSprintf(returnBuffer, 64, "%g %g %g", worldPos.x, worldPos.y, worldPos.z);
   return returnBuffer;
}

ConsoleMethod(GuiTSCtrl, project, const char*, 3, 3, "project( %worldPosition ) : Transforms world space coordinates to screen space (x, y, depth)")
{
   char *returnBuffer = Con::getReturnBuffer(64);
   Point3F screenPos;
   Point3F worldPos;
   dSscanf(argv[2], "%g %g %g", &worldPos.x, &worldPos.y, &worldPos.z);
   object->project(worldPos, &screenPos);
   dSprintf(returnBuffer, 64, "%g %g %g", screenPos.x, screenPos.y, screenPos.z);
   return returnBuffer;
}

ConsoleMethod(GuiTSCtrl, getWorldToScreenScale, const char*, 2, 2, "getWorldToScreenScale() : Returns the scale for converting world space units to pixels.")
{
   char *returnBuffer = Con::getReturnBuffer(64);
   const Point2F &scale = object->getWorldToScreenScale();
   dSprintf(returnBuffer, 64, "%g %g", scale.x, scale.y);
   return returnBuffer;
}

ConsoleMethod(GuiTSCtrl, calculateViewDistance, F32, 3, 3, "calculateViewDistance( radius ) : Returns the distance required to fit the given radius within the camera's view.")
{
   F32 radius = dAtof(argv[2]);
   return object->calculateViewDistance( radius );
}
