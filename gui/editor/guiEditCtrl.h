//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIEDITCTRL_H_
#define _GUIEDITCTRL_H_

#ifndef _GUICONTROL_H_
   #include "gui/core/guiControl.h"
#endif
#ifndef _UNDO_H_
   #include "util/undo.h"
#endif
#ifndef _GFX_GFXDRAWER_H_
   #include "gfx/gfxDrawUtil.h"
#endif


/// Native side of the GUI editor.
class GuiEditCtrl : public GuiControl
{
   public:
   
      typedef GuiControl Parent;
      friend class GuiEditorRuler;

      enum Justification
      {
         JUSTIFY_LEFT,
         JUSTIFY_CENTER_VERTICAL,
         JUSTIFY_RIGHT,
         JUSTIFY_TOP,
         JUSTIFY_BOTTOM,
         SPACING_VERTICAL,
         SPACING_HORIZONTAL,
         JUSTIFY_CENTER_HORIZONTAL,
      };

      enum guideAxis { GuideVertical, GuideHorizontal };
      
   protected:
   
      enum
      {
         NUT_SIZE = 4,
         MIN_GRID_SIZE = 3
      };
   
      typedef Vector< GuiControl* > GuiControlVector;
      typedef SimObjectPtr< GuiControl > GuiControlPtr;

      enum mouseModes { Selecting, MovingSelection, SizingSelection, DragSelecting, DragGuide };
      enum sizingModes { sizingNone = 0, sizingLeft = 1, sizingRight = 2, sizingTop = 4, sizingBottom = 8 };
      
      enum snappingAxis { SnapVertical, SnapHorizontal };
      enum snappingEdges { SnapEdgeMin, SnapEdgeMid, SnapEdgeMax };

      bool                 mDrawBorderLines;
      bool                 mDrawGuides;
      bool                 mFullBoxSelection;
      GuiControlVector     mSelectedControls;
      GuiControlPtr        mCurrentAddSet;
      GuiControlPtr        mContentControl;
      Point2I              mLastMousePos;
      Point2I              mLastDragPos;
      Point2I              mSelectionAnchor;
      Point2I              mGridSnap;
      Point2I              mDragBeginPoint;
      Vector<Point2I>		mDragBeginPoints;
      bool                 mDragAddSelection;

      // Guides.
      
      bool                 mSnapToGuides;       ///< If true, edge and center snapping will work against guides.
      bool                 mDragGuide[ 2 ];
      U32                  mDragGuideIndex[ 2 ];///< Indices of the guide's being dragged in DragGuide mouse mode.
      Vector< U32 >        mGuides[ 2 ];        ///< The guides defined on the current content control.

      // Snapping
      
      bool                 mSnapToControls;     ///< If true, edge and center snapping will work against controls.
      bool                 mSnapToEdges;        ///< If true, selection edges will snap to other control edges.
      bool                 mSnapToCenters;      ///< If true, selection centers will snap to other control centers.
      S32                  mSnapSensitivity;    ///< Snap distance in pixels.
      
      bool                 mSnapped[ 2 ];       ///< Snap flag for each axis.  If true, we are currently snapped in a drag.
      S32                  mSnapOffset[ 2 ];    ///< Reference point on snap line for each axis.
      GuiControlVector     mSnapHits[ 2 ];      ///< Hit testing lists for each axis.
      snappingEdges        mSnapEdge[ 2 ];      ///< Identification of edge being snapped for each axis.
      GuiControlPtr        mSnapTargets[ 2 ];   ///< Controls providing the snap reference lines on each axis.

      // Undo
      UndoManager          mUndoManager;
      SimGroup             mTrash;
      SimSet               mSelectedSet;

      // grid drawing
      GFXVertexBufferHandle<GFXVertexPC> mDots;
      GFXStateBlockRef mDotSB;

      mouseModes           mMouseDownMode;
      sizingModes          mSizingMode;
      
      static StringTableEntry smGuidesPropertyName[ 2 ];

      // private methods
      void                 updateSelectedSet();
      
      S32                  findGuide( guideAxis axis, const Point2I& point, U32 tolerance = 0 );
      
      RectI                getSnapRegion( snappingAxis axis, const Point2I& center ) const;   
      void                 findSnaps( snappingAxis axis, const Point2I& mousePoint, const RectI& minRegion, const RectI& midRegion, const RectI& maxRegion );
      void                 registerSnap( snappingAxis axis, const Point2I& mousePoint, const Point2I& point, snappingEdges edge, GuiControl* ctrl = NULL );
      
      void                 doSnapping( const GuiEvent& event, const RectI& selectionBounds, Point2I& delta );
      void                 doGuideSnap( const GuiEvent& event, const RectI& selectionBounds, const RectI& selectionBoundsGlobal, Point2I& delta );
      void                 doControlSnap( const GuiEvent& event, const RectI& selectionBounds, const RectI& selectionBoundsGlobal, Point2I& delta );
      void                 doGridSnap( const GuiEvent& event, const RectI& selectionBounds, const RectI& selectionBoundsGlobal, Point2I& delta );
      void                 snapToGrid( Point2I& point );
      
      void                 startDragMove( const Point2I& startPoint );
      void                 startDragRectangle( const Point2I& startPoint );      
      void                 startMouseGuideDrag( guideAxis axis, U32 index, bool lockMouse = true );
            
      void onHierarchyChanged()
      {
         if( isMethod( "onHierarchyChanged" ) )
            Con::executef( this, "onHierarchyChanged" );
      }
      
      static bool inNut( const Point2I &pt, S32 x, S32 y )
      {
         S32 dx = pt.x - x;
         S32 dy = pt.y - y;
         return dx <= NUT_SIZE && dx >= -NUT_SIZE && dy <= NUT_SIZE && dy >= -NUT_SIZE;
      }

      static void drawCrossSection( U32 axis, S32 offset, const RectI& bounds, ColorI color, GFXDrawUtil* drawer )
      {
         Point2I start;
         Point2I end;
         
         if( axis == 0 )
         {
            start.x = end.x = offset;
            start.y = bounds.point.y;
            end.y = bounds.point.y + bounds.extent.y;
         }
         else
         {
            start.y = end.y = offset;
            start.x = bounds.point.x;
            end.x = bounds.point.x + bounds.extent.x;
         }
         
         drawer->drawLine( start, end, color );
      }
      
      static void snapDelta( snappingAxis axis, snappingEdges edge, S32 offset, const RectI& bounds, Point2I& delta )
      {
         switch( axis )
         {
            case SnapVertical:
               switch( edge )
               {
                  case SnapEdgeMin:
                     delta.x = offset - bounds.point.x;
                     break;
                     
                  case SnapEdgeMid:
                     delta.x = offset - bounds.point.x - bounds.extent.x / 2;
                     break;
                     
                  case SnapEdgeMax:
                     delta.x = offset - bounds.point.x - bounds.extent.x;
                     break;
               }
               break;
            
            case SnapHorizontal:
               switch( edge )
               {
                  case SnapEdgeMin:
                     delta.y = offset - bounds.point.y;
                     break;
                     
                  case SnapEdgeMid:
                     delta.y = offset - bounds.point.y - bounds.extent.y / 2;
                     break;
                     
                  case SnapEdgeMax:
                     delta.y = offset - bounds.point.y - bounds.extent.y;
                     break;
               }
               break;
         }
      }

   public:

      GuiEditCtrl();
      
      DECLARE_CONOBJECT(GuiEditCtrl);
      DECLARE_CATEGORY( "Gui Editor" );
      DECLARE_DESCRIPTION( "Implements the framework for the GUI editor." );

      bool onWake();
      void onSleep();
      
      static void initPersistFields();

      GuiControl*       getContentControl() const { return mContentControl; }
      void              setContentControl(GuiControl *ctrl);
      void              setEditMode(bool value);
      S32               getSizingHitKnobs(const Point2I &pt, const RectI &box);
      void              getDragRect(RectI &b);
      void              drawNut(const Point2I &nut, ColorI &outlineColor, ColorI &nutColor);
      void              drawNuts(RectI &box, ColorI &outlineColor, ColorI &nutColor);
      void              onPreRender();
      void              onRender(Point2I offset, const RectI &updateRect);
      void              addNewControl(GuiControl *ctrl);
      void              setCurrentAddSet(GuiControl *ctrl, bool clearSelection = true);
      const GuiControl* getCurrentAddSet() const;
      
      // Selections.

      void              select( GuiControl *ctrl );
      bool              selectionContains( GuiControl *ctrl );
      bool              selectionContainsParentOf( GuiControl* ctrl );
      void              setSelection( GuiControl *ctrl, bool inclusive = false );
      RectI             getSelectionBounds() const;
      RectI             getSelectionGlobalBounds() const;
      void              canHitSelectedControls( bool state = true );
      void              justifySelection( Justification j );
      void              moveSelection( const Point2I &delta );
      void              moveAndSnapSelection( const Point2I &delta );
      void              saveSelection( const char *filename );
      void              loadSelection( const char *filename );
      void              addSelection( S32 id );
      void              addSelection( GuiControl* ctrl );
      void              removeSelection( S32 id );
      void              removeSelection( GuiControl* ctrl );
      void              deleteSelection();
      void              clearSelection();
      void              selectAll();
      void              bringToFront();
      void              pushToBack();
      void              moveSelectionToCtrl( GuiControl* );
      void              addSelectControlsInRegion( const RectI& rect, U32 hitTestFlags = 0 );
      void              addSelectControlAt( const Point2I& pos );
      void              resizeControlsInSelectionBy( const Point2I& delta, U32 mode );
      
      // Guides.
      
      U32               addGuide( guideAxis axis, U32 offset ) { U32 index = mGuides[ axis ].size(); mGuides[ axis ].push_back( offset ); return index; }
      void              readGuides( guideAxis axis, GuiControl* ctrl );
      void              writeGuides( guideAxis axis, GuiControl* ctrl );
      void              clearGuides( guideAxis axis ) { mGuides[ axis ].clear(); }

      // Undo Access.
      
      void              undo();
      void              redo();
      UndoManager&      getUndoManager() { return mUndoManager; }

      // When a control is changed by the inspector
      void              controlInspectPreApply(GuiControl* object);
      void              controlInspectPostApply(GuiControl* object);

      // Sizing Cursors
      void              getCursor(GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent);

      U32               getSelectionSize() const { return mSelectedControls.size(); }
      const Vector<GuiControl *>& getSelected() const { return mSelectedControls; }
      const SimSet& getSelectedSet() { updateSelectedSet(); return mSelectedSet; }
      const SimGroup& getTrash() { return mTrash; }
      const GuiControl *getAddSet() const { return mCurrentAddSet; }; //JDD

      bool              onKeyDown(const GuiEvent &event);
      void              onMouseDown(const GuiEvent &event);
      void              onMouseUp(const GuiEvent &event);
      void              onMouseDragged(const GuiEvent &event);
      void              onRightMouseDown(const GuiEvent &event);

      virtual bool      onAdd();
      virtual void      onRemove();

      void              setSnapToGrid(U32 gridsize);
};

#endif //_GUI_EDIT_CTRL_H
