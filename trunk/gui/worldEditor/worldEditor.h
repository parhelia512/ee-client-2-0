//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _WORLDEDITOR_H_
#define _WORLDEDITOR_H_

#ifndef _EDITTSCTRL_H_
#include "gui/worldEditor/editTSCtrl.h"
#endif
#ifndef _CONSOLETYPES_H_
#include "console/consoleTypes.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif
#ifndef _CONSOLE_SIMOBJECTMEMENTO_H_
#include "console/simObjectMemento.h"
#endif
#ifndef _UNDO_H_
#include "util/undo.h"
#endif
#ifndef _SIMPATH_H_
#include "sceneGraph/simPath.h"
#endif

class SceneObject;

class WorldEditor : public EditTSCtrl
{
   typedef EditTSCtrl Parent;

	public:

   struct Triangle
   {
      Point3F p0;
      Point3F p1;
      Point3F p2;
   };

	void ignoreObjClass(U32 argc, const char** argv);
	void clearIgnoreList();

   static bool setObjectsUseBoxCenter( void* obj, const char* data ) { static_cast<WorldEditor*>(obj)->setObjectsUseBoxCenter( dAtob( data ) ); return false; };
   void setObjectsUseBoxCenter(bool state);
   bool getObjectsUseBoxCenter() { return mObjectsUseBoxCenter; }

	void clearSelection();
	void selectObject(const char* obj);
	void unselectObject(const char* obj);
	
	S32 getSelectionSize();
	S32 getSelectObject(S32 index);	
   const Point3F& getSelectionCentroid();
	const char* getSelectionCentroidText();
   const Box3F& getSelectionBounds();
   Point3F getSelectionExtent();
   F32 getSelectionRadius();
	
	void dropCurrentSelection( bool skipUndo );
	void copyCurrentSelection();	
   void cutCurrentSelection();	
	bool canPasteSelection();

   bool alignByBounds(S32 boundsAxis);
   bool alignByAxis(S32 axis);

   void transformSelection(bool position, Point3F& p, bool relativePos, bool rotate, EulerF& r, bool relativeRot, bool rotLocal, S32 scaleType, Point3F& s, bool sRelative, bool sLocal);

   void resetSelectedRotation();
   void resetSelectedScale();

   void addUndoState() { submitUndo( mSelected ); }
	void redirectConsole(S32 objID);

   public:

      class Selection : public SimObject
      {
         typedef SimObject    Parent;

         private:

            Point3F        mCentroid;
            Point3F        mBoxCentroid;
            Box3F          mBoxBounds;
            bool           mCentroidValid;
            bool           mContainsGlobalBounds;  // Does the selection contain at least one object with a global bounds
            SimObjectList  mObjectList;
            bool           mAutoSelect;
            Point3F        mPrevCentroid;

            void           updateCentroid();

         public:

            Selection();
            ~Selection();

            //
            U32 size()  { return(mObjectList.size()); }
            SimObject * operator[] (S32 index) { return ( ( SimObject* ) mObjectList[ index ] ); }

            bool objInSet( SimObject* );

            bool addObject( SimObject* );
            bool removeObject( SimObject* );
            void clear();

            void onDeleteNotify( SimObject* );

            void storeCurrentCentroid() { mPrevCentroid = getCentroid(); }
            bool hasCentroidChanged() { return (mPrevCentroid != getCentroid()); }

            bool containsGlobalBounds();

            const Point3F & getCentroid();
            const Point3F & getBoxCentroid();
            const Box3F & getBoxBounds();
            Point3F getBoxBottomCenter();

            void enableCollision();
            void disableCollision();

            //
            void autoSelect(bool b) { mAutoSelect = b; }
            void invalidateCentroid() { mCentroidValid = false; }

            //
            void offset(const Point3F &);
            void setPosition(const Point3F & pos);
            void setCentroidPosition(bool useBoxCenter, const Point3F & pos);

            void orient(const MatrixF &, const Point3F &);
            void rotate(const EulerF &);
            void rotate(const EulerF &, const Point3F &);
            void setRotate(const EulerF &);

            void scale(const VectorF &);
            void scale(const VectorF &, const Point3F &);
            void setScale(const VectorF &);
            void setScale(const VectorF &, const Point3F &);

            void addSize(const VectorF &);
            void setSize(const VectorF &);
      };

      //
      static SceneObject* getClientObj(SceneObject *);
      static void setClientObjInfo(SceneObject *, const MatrixF &, const VectorF &);
      static void updateClientTransforms(Selection &);

   protected:

      class WorldEditorUndoAction : public UndoAction
      {
      public:

         WorldEditorUndoAction( const UTF8* actionName ) : UndoAction( actionName )
         {
         }

         WorldEditor *mWorldEditor;

         struct Entry
         {
            MatrixF     mMatrix;
            VectorF     mScale;

            // validation
            U32         mObjId;
            U32         mObjNumber;
         };

         Vector<Entry>  mEntries;

         virtual void undo();
         virtual void redo() { undo(); }
      };

      void submitUndo( Selection &sel, const UTF8* label="World Editor Action" );

   public:

      /// The objects currently in the copy buffer.
      Vector<SimObjectMemento> mCopyBuffer;

      bool cutSelection(Selection &sel);
      bool copySelection(Selection & sel);
      bool pasteSelection(bool dropSel=true);
      void dropSelection(Selection & sel);
      void dropBelowSelection(Selection & sel, const Point3F & centroid, bool useBottomBounds=false);

      void terrainSnapSelection(Selection& sel, U8 modifier, Point3F gizmoPos, bool forceStick=false);
      void softSnapSelection(Selection& sel, U8 modifier, Point3F gizmoPos);

      // work off of mSelected
      void hideSelection(bool hide);
      void lockSelection(bool lock);

   public:
      bool objClassIgnored(const SimObject * obj);
      void renderObjectBox(SceneObject * obj, const ColorI & col);
      
   private:
      SceneObject * getControlObject();
      bool collide(const Gui3DMouseEvent & event, SceneObject **hitObj );

      // gfx methods
      //void renderObjectBox(SceneObject * obj, const ColorI & col);
      void renderObjectFace(SceneObject * obj, const VectorF & normal, const ColorI & col);
      void renderSelectionWorldBox(Selection & sel);

      void renderPlane(const Point3F & origin);
      void renderMousePopupInfo();
      void renderScreenObj(SceneObject * obj, Point3F sPos);

      void renderPaths(SimObject *obj);
      void renderSplinePath(SimPath::Path *path);

      // axis gizmo
      bool        mUsingAxisGizmo;

      GFXStateBlockRef mRenderObjectBoxSB;
      GFXStateBlockRef mRenderObjectFaceSB;
      GFXStateBlockRef mSplineSB;

      //
      bool                       mIsDirty;

      //
      bool                       mMouseDown;
      Selection                  mSelected;

      Selection                  mDragSelected;
      bool                       mDragSelect;
      RectI                      mDragRect;
      Point2I                    mDragStart;

      // modes for when dragging a selection
      enum {
         Move = 0,
         Rotate,
         Scale
      };

      //
      //U32                        mCurrentMode;
      //U32                        mDefaultMode;

      S32                        mRedirectID;


      struct IconObject
      {         
         SceneObject *object;
         F32 dist;         
         RectI rect;
      };
      
      Vector<IconObject> mIcons;

      SimObjectPtr<SceneObject>  mHitObject;
      SimObjectPtr<SceneObject>  mPossibleHitObject;
      bool                       mMouseDragged;
      Gui3DMouseEvent            mLastMouseEvent;
      Gui3DMouseEvent            mLastMouseDownEvent;

      //
      class ClassInfo
      {
         public:
            ~ClassInfo();

            struct Entry
            {
               StringTableEntry  mName;
               bool              mIgnoreCollision;
               GFXTexHandle      mDefaultHandle;
               GFXTexHandle      mSelectHandle;
               GFXTexHandle      mLockedHandle;
            };

            Vector<Entry*>       mEntries;
      };


      ClassInfo            mClassInfo;
      ClassInfo::Entry     mDefaultClassEntry;

      ClassInfo::Entry * getClassEntry(StringTableEntry name);
      ClassInfo::Entry * getClassEntry(const SimObject * obj);
      bool addClassEntry(ClassInfo::Entry * entry);

   // persist field data
   public:

      enum {
         DropAtOrigin = 0,
         DropAtCamera,
         DropAtCameraWithRot,
         DropBelowCamera,
         DropAtScreenCenter,
         DropAtCentroid,
         DropToTerrain,
         DropBelowSelection
      };

      // Snapping alignment mode
      enum {
         AlignNone = 0,
         AlignPosX,
         AlignPosY,
         AlignPosZ,
         AlignNegX,
         AlignNegY,
         AlignNegZ
      };

      /// A large hard coded distance used to test 
      /// object and icon selection.
      static F32 smProjectDistance;

      S32               mDropType;
      bool              mBoundingBoxCollision;
      bool              mObjectMeshCollision;
      bool              mRenderPopupBackground;
      ColorI            mPopupBackgroundColor;
      ColorI            mPopupTextColor;
      StringTableEntry  mSelectHandle;
      StringTableEntry  mDefaultHandle;
      StringTableEntry  mLockedHandle;
      ColorI            mObjectTextColor;
      bool              mObjectsUseBoxCenter;
      ColorI            mObjSelectColor;
      ColorI            mObjMouseOverSelectColor;
      ColorI            mObjMouseOverColor;
      bool              mShowMousePopupInfo;
      ColorI            mDragRectColor;
      bool              mRenderObjText;
      bool              mRenderObjHandle;
      StringTableEntry  mObjTextFormat;
      ColorI            mFaceSelectColor;
      bool              mRenderSelectionBox;
      ColorI            mSelectionBoxColor;
      bool              mSelectionLocked;
      bool              mPerformedDragCopy;
      bool              mToggleIgnoreList;
      bool              mNoMouseDrag;
      bool              mDropAtBounds;
      F32               mDropBelowCameraOffset;
      F32               mDropAtScreenCenterScalar;
      F32               mDropAtScreenCenterMax;

      bool              mStickToGround;
      bool              mStuckToGround;            ///< Selection is stuck to the ground
      S32               mTerrainSnapAlignment;     ///< How does the stickied object align to the terrain

      bool              mSoftSnap;                 ///< Allow soft snapping all of the time
      bool              mSoftSnapActivated;        ///< Soft snap has been activated by the user and allowed by the current rules
      bool              mSoftSnapIsStuck;          ///< Are we snapping?
      S32               mSoftSnapAlignment;        ///< How does the snapped object align to the snapped surface
      bool              mSoftSnapRender;           ///< Render the soft snapping bounds
      bool              mSoftSnapRenderTriangle;   ///< Render the soft snapped triangle
      Triangle          mSoftSnapTriangle;         ///< The triangle we are snapping to
      bool              mSoftSnapSizeByBounds;     ///< Use the selection bounds for the size
      F32               mSoftSnapSize;             ///< If not the selection bounds, use this size
      Box3F             mSoftSnapBounds;           ///< The actual bounds used for the soft snap
      Box3F             mSoftSnapPreBounds;        ///< The bounds prior to any soft snapping (used when rendering the snap bounds)
      F32               mSoftSnapBackfaceTolerance;   ///< Fraction of mSoftSnapSize for backface polys to have an influence

      bool              mSoftSnapDebugRender;      ///< Activates debug rendering
      Point3F           mSoftSnapDebugPoint;       ///< The point we're attempting to snap to
      Triangle          mSoftSnapDebugSnapTri;     ///< The triangle we are snapping to
      Vector<Triangle>  mSoftSnapDebugTriangles;   ///< The triangles that are considered for snapping

   protected:

      S32 mCurrentCursor;
      void setCursor(U32 cursor);
      void get3DCursor(GuiCursor *&cursor, bool &visible, const Gui3DMouseEvent &event);

   public:

      WorldEditor();
      ~WorldEditor();

      // SimObject
      bool onAdd();
      void onEditorEnable();
      void setDirty() { mIsDirty = true; }

      // EditTSCtrl
      void on3DMouseMove(const Gui3DMouseEvent & event);
      void on3DMouseDown(const Gui3DMouseEvent & event);
      void on3DMouseUp(const Gui3DMouseEvent & event);
      void on3DMouseDragged(const Gui3DMouseEvent & event);
      void on3DMouseEnter(const Gui3DMouseEvent & event);
      void on3DMouseLeave(const Gui3DMouseEvent & event);
      void on3DRightMouseDown(const Gui3DMouseEvent & event);
      void on3DRightMouseUp(const Gui3DMouseEvent & event);

      void updateGuiInfo();

      //
      void renderScene(const RectI & updateRect);

      Gizmo* getGizmo() { return mGizmo; };

      static void initPersistFields();

      DECLARE_CONOBJECT(WorldEditor);

	  static Signal<void(WorldEditor*)> smRenderSceneSignal;
};

#endif // _WORLDEDITOR_H_

