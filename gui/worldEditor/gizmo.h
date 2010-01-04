//-----------------------------------------------------------------------------
// Copyright (C) Sickhead Games, LLC
//-----------------------------------------------------------------------------

#ifndef _GIZMO_H_
#define _GIZMO_H_

#ifndef _SIMBASE_H_
#include "console/simBase.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _GUITYPES_H_
#include "gui/core/guiTypes.h"
#endif
#ifndef _MATHUTILS_H_
#include "math/mathUtils.h"
#endif


enum Mode {
   NoneMode = 0,
   MoveMode,    // 1
   RotateMode,  // 2
   ScaleMode,   // 3
   ModeEnumCount 
};

enum Align {
   World = 0,
   Object,
   AlignEnumCount
}; 


//
class GizmoProfile : public SimObject
{
   typedef SimObject Parent;

public:

   GizmoProfile();
   //virtual ~GizmoProfile();

   DECLARE_CONOBJECT( GizmoProfile );

   bool onAdd();

   static void initPersistFields();

   // Data Fields

   Mode mode;
   Align alignment;

   F32 rotateScalar;
   F32 scaleScalar;
   U32 screenLen;
   ColorI axisColors[3];
   ColorI activeColor;   
   ColorI inActiveColor;
   ColorI centroidColor;
   ColorI centroidHighlightColor;
   Resource<GFont> font;
   
   // new stuff...

   bool snapToGrid;
   F32 scaleSnap;
   bool allowSnapScale;
   F32 rotationSnap;
   bool allowSnapRotations;

   Point3F gridSize;
   bool renderPlane;
   bool renderPlaneHashes;
   ColorI gridColor;
   F32 planeDim;      

   enum Flags {
      CanRotate         = 1 << 0, // 0
      CanRotateX        = 1 << 1,
      CanRotateY        = 1 << 2,
      CanRotateZ        = 1 << 3,
      CanRotateScreen   = 1 << 4,
      CanScale          = 1 << 5,
      CanScaleX         = 1 << 6,
      CanScaleY         = 1 << 7,
      CanScaleZ         = 1 << 8,
      CanScaleUniform   = 1 << 9,
      CanTranslate      = 1 << 10,
      CanTranslateX     = 1 << 11, 
      CanTranslateY     = 1 << 12, 
      CanTranslateZ     = 1 << 13,
      CanTranslateUniform = 1 << 14,
      PlanarHandlesOn   = 1 << 15
   };

   S32 flags;
};


// This class contains code for rendering and manipulating a 3D gizmo, it
// is usually used as a helper within a TSEdit-derived control.
//
// The Gizmo has a MatrixF transform and Point3F scale on which it will
// operate by passing it Gui3DMouseEvent(s).
//
// The idea is to set the Gizmo transform/scale to that of another 3D object 
// which is being manipulated, pass mouse events into the Gizmo, read the
// new transform/scale out, and set it to onto the object.
// And of course the Gizmo can be rendered.
//
// Gizmo derives from SimObject only because this allows its properties
// to be initialized directly from script via fields.

class Gizmo : public SimObject
{
   typedef SimObject Parent;

   friend class WorldEditor;

public:   

   enum Selection {
      None     = -1,
      Axis_X   = 0,
      Axis_Y   = 1,
      Axis_Z   = 2,
      Plane_XY = 3,  // Normal = Axis_Z
      Plane_XZ = 4,  // Normal = Axis_Y
      Plane_YZ = 5,  // Normal = Axis_X
      Custom0  = 6,
      Custom1  = 7
   };  

   Gizmo();
   ~Gizmo();

   DECLARE_CONOBJECT( Gizmo );

   // SimObject
   bool onAdd();
   void onRemove();
   static void initPersistFields();

   // Mutators
   void set( const MatrixF &objMat, const Point3F &worldPos, const Point3F &objScale ); 
   void setProfile( GizmoProfile *profile ) { mProfile = profile; }

   // Accessors
   const MatrixF& getTransform() const          { return mTransform; }
   Point3F        getPosition() const           { return mTransform.getPosition(); }
   Point3F        getScale() const;
   GizmoProfile*  getProfile()                  { return mProfile; }
   Point3F        getOffset() const             { return mDeltaPos; }
   Point3F        getProjectPoint() const       { return mProjPnt; }
   Point3F        getDeltaRot() const           { return mDeltaRot; }
   Point3F        getDeltaScale() const         { return mDeltaScale; }

   // Gizmo Interface methods...

   // Set the current highlight mode on the gizmo's centroid handle
   void setCentroidHandleHighlight( bool state ) { mHighlightCentroidHandle = state; }

   // Must be called before on3DMouseDragged to save state
   void on3DMouseDown( const Gui3DMouseEvent &event );

   // So Gizmo knows the current mouse button state.
   void on3DMouseUp( const Gui3DMouseEvent &event );
   
   // Test Gizmo for collisions and set the Gizmo Selection (the part under the cursor)
   void on3DMouseMove( const Gui3DMouseEvent &event );
   
   // Make changes to the Gizmo transform/scale (depending on mode)
   void on3DMouseDragged( const Gui3DMouseEvent &event );

   // Returns an enum describing the part of the Gizmo that is Selected
   // ( under the cursor ). This should be called AFTER calling onMouseMove
   // or collideAxisGizmo
   //
   // -1 None
   // 0  Axis_X
   // 1  Axis_Y
   // 2  Axis_Z
   // 3  Plane_XY
   // 4  Plane_XZ
   // 5  Plane_YZ
   Selection getSelection();
   void setSelection( Selection sel ) { mSelectionIdx = sel; }

   // Returns the object space vector corresponding to a Selection.
   Point3F selectionToAxisVector( Selection axis ); 

   // These provide the user an easy way to check if the Gizmo's transform
   // or scale have changed by calling markClean prior to calling
   // on3DMouseDragged, and calling isDirty after.   
   bool isDirty() { return mDirty; }
   void markClean() { mDirty = false; } 

   // Renders the 3D Gizmo in the scene, GFX must be setup for proper
   // 3D rendering before calling this!
   // Calling this will change the GFXStateBlock!
   void renderGizmo(const MatrixF &cameraTransform);

   // Renders text associated with the Gizmo, GFX must be setup for proper
   // 2D rendering before calling this!
   // Calling this will change the GFXStateBlock!
   void renderText( const RectI &viewPort, const MatrixF &modelView, const MatrixF &projection );

   // Returns true if the mouse event collides with any part of the Gizmo
   // and sets the Gizmo's current Selection.
   // You can call this or on3DMouseMove, they are identical   
   bool collideAxisGizmo( const Gui3DMouseEvent & event );

protected:

   void _calcAxisInfo();
   void _setStateBlock();
   void _renderPrimaryAxis();
   void _renderAxisArrows();
   void _renderAxisBoxes();
   void _renderAxisCircles();
   void _renderAxisText();   
   void _renderPlane();
   Point3F _snapPoint( const Point3F &pnt ) const;
   F32 _snapFloat( const F32 &val, const F32 &snap ) const;
   Align _filteredAlignment();
   void _updateState( bool collideGizmo = true );
   void _updateEnabledAxices();
   
protected:

   GizmoProfile *mProfile;

   MatrixF mObjectMat;
   MatrixF mTransform;
   MatrixF mLastTransform;
   MatrixF mSavedTransform;
   MatrixF *mRenderTransform;
   
   Align mCurrentAlignment;
   Mode mCurrentMode;

   MatrixF mCameraMat;
   Point3F mCameraPos;

   Point3F mScale;
   Point3F mSavedScale;
   Point3F mDeltaScale;
   Point3F mLastScale;
	Point3F mScaleInfluence;
 
   EulerF mRot;
   EulerF mSavedRot;
   EulerF mDeltaRot;
   F32 mDeltaAngle;
   F32 mLastAngle;
   Point2I mMouseDownPos;
   Point3F mMouseDownProjPnt;
   Point3F mDeltaPos;   
   Point3F mProjPnt;
   Point3F mOrigin;
   Point3F mProjAxisVector[3];
   F32 mProjLen;
   S32 mSelectionIdx;
   bool mDirty;
   Gui3DMouseEvent mLastMouseEvent;
   GFXStateBlockRef mStateBlock;

   PlaneF mMouseCollidePlane;
   MathUtils::Line mMouseCollideLine;

   bool mMouseDown;

   F32 mSign;

   bool mAxisEnabled[3];
   bool mUniformHandleEnabled;
   
   bool mHighlightCentroidHandle;

   // Initialized in renderGizmo and saved for later use when projecting
   // to screen space for selection testing.
   MatrixF mLastWorldMat;
   MatrixF mLastProjMat;
   RectI mLastViewport;
   Point2F mLastWorldToScreenScale;

   // Screenspace cursor collision information used in rotation mode.
   Point3F mElipseCursorCollidePntSS;
   Point3F mElipseCursorCollideVecSS;

   /// A large hard coded distance used to test 
   /// gizmo axis selection.
   static F32 smProjectDistance;

};

#endif // _GIZMO_H_