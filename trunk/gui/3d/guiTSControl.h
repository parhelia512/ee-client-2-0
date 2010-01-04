//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUITSCONTROL_H_
#define _GUITSCONTROL_H_

#ifndef _GUICONTAINER_H_
#include "gui/containers/guiContainer.h"
#endif
#ifndef _MMATH_H_
#include "math/mMath.h"
#endif

struct CameraQuery
{
   SimObject*  object;
   F32         nearPlane;
   F32         farPlane;
   F32         fov;
   bool        ortho;
   MatrixF     cameraMatrix;
   MatrixF     cameraMatrixLeft;
   MatrixF     cameraMatrixRight;
};

class GuiTSCtrl : public GuiContainer
{
   typedef GuiContainer Parent;

   static U32     smFrameCount;
   F32            mCameraZRot;
   F32            mForceFOV;

protected:

   /// A list of GuiTSCtrl which are awake and 
   /// most likely rendering.
   static Vector<GuiTSCtrl*> smAwakeTSCtrls;

   /// A scalar which controls how much of the reflection
   /// update timeslice for this viewport to get.
   F32 mReflectPriority;

   F32            mOrthoWidth;
   F32            mOrthoHeight;

   MatrixF     mSaveModelview;
   MatrixF     mSaveProjection;
   RectI       mSaveViewport;
   
   /// The saved world to screen space scale.
   /// @see getWorldToScreenScale
   Point2F mSaveWorldToScreenScale;

   /// The last camera query set in onRender.
   /// @see getLastCameraQuery
   CameraQuery mLastCameraQuery;
   
public:
   
   GuiTSCtrl();

   void onPreRender();
   void onRender(Point2I offset, const RectI &updateRect);
   virtual bool processCameraQuery(CameraQuery *query);
   virtual void renderWorld(const RectI &updateRect);

   static void initPersistFields();
   static void consoleInit();

   virtual bool onWake();
   virtual void onSleep();

   /// Returns the last camera query set in onRender.
   const CameraQuery& getLastCameraQuery() const { return mLastCameraQuery; }
   
   /// Returns the screen space X,Y and Z for world space point.
   /// The input z coord is depth, from 0 to 1.
   bool project( const Point3F &pt, Point3F *dest ) const; 

   /// Returns the world space point for X, Y and Z.  The ouput
   /// z coord is depth, from 0 to 1
   bool unproject( const Point3F &pt, Point3F *dest ) const;

   ///
   F32 projectRadius( F32 dist, F32 radius ) const;

   /// Returns the scale for converting world space 
   /// units to screen space units... aka pixels.
   /// @see GFXDevice::getWorldToScreenScale
   const Point2F& getWorldToScreenScale() const { return mSaveWorldToScreenScale; }

   /// Returns the distance required to fit the given
   /// radius within the camera's view.
   F32 calculateViewDistance(F32 radius);

   DECLARE_CONOBJECT(GuiTSCtrl);
   DECLARE_CATEGORY( "Gui 3D" );
   DECLARE_DESCRIPTION( "A control that renders a 3D viewport." );
};

#endif // _GUITSCONTROL_H_
