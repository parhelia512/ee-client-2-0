//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TERRAINEDITOR_H_
#define _TERRAINEDITOR_H_

#ifndef _EDITTSCTRL_H_
#include "gui/worldEditor/editTSCtrl.h"
#endif
#ifndef _TERRDATA_H_
#include "terrain/terrData.h"
#endif
#ifndef _UNDO_H_
#include "util/undo.h"
#endif


//------------------------------------------------------------------------------

// Each 2D grid position must be associated with a terrainBlock
struct GridPoint
{
   Point2I        gridPos;
   TerrainBlock*  terrainBlock;

   GridPoint() { gridPos.set(0, 0); terrainBlock = NULL; };
};

class GridInfo
{
   public:
      GridPoint                  mGridPoint;
      U8                         mMaterial;
      //TerrainBlock::Material     mMaterial;
      //U8                         mMaterialAlpha[TerrainBlock::MaterialGroups];
      F32                        mHeight;
      //U8                         mMaterialGroup;
      F32                        mWeight;
      F32                        mStartHeight;

      bool                       mPrimarySelect;
      bool                       mMaterialChanged;

      // hash table
      S32                        mNext;
      S32                        mPrev;
};

//------------------------------------------------------------------------------

class Selection : public Vector<GridInfo>
{
   private:

      StringTableEntry     mName;
      BitSet32             mUndoFlags;

      // hash table
      S32 lookup(const Point2I & pos);
      void insert(GridInfo info);
      U32 getHashIndex(const Point2I & pos);
      bool validate();

      Vector<S32>          mHashLists;
      U32                  mHashListSize;

   public:

      Selection();
      virtual ~Selection();

      void reset();
      bool add(const GridInfo &info);
      bool getInfo(Point2I pos, GridInfo & info);
      bool setInfo(GridInfo & info);
      bool remove(const GridInfo &info);
      void setName(StringTableEntry name);
      StringTableEntry getName(){return(mName);}
      F32 getAvgHeight();
      F32 getMinHeight();
      F32 getMaxHeight();
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class TerrainEditor;
class Brush : public Selection
{
   protected:
      TerrainEditor *   mTerrainEditor;
      Point2I           mSize;
      GridPoint         mGridPoint;

   public:

      enum {
         MaxBrushDim    =  40
      };

      Brush(TerrainEditor * editor);
      virtual ~Brush(){};

      virtual const char *getType() const = 0;

      // Brush appears to intentionally bypass Selection's hash table, so we
      // override validate() here.
      bool validate() { return true; }
      void setPosition(const Point3F & pos);
      void setPosition(const Point2I & pos);
      const Point2I & getPosition();
      const GridPoint & getGridPoint();
      void setTerrain(TerrainBlock* terrain) { mGridPoint.terrainBlock = terrain; };

      void update();
      virtual void rebuild() = 0;

      virtual void render(Vector<GFXVertexPC> & vertexBuffer, S32 & verts, S32 & elems, S32 & prims, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone) const = 0;

      Point2I getSize() const {return(mSize);}
      virtual void setSize(const Point2I & size){mSize = size;}
};

class BoxBrush : public Brush
{
   public:
      BoxBrush(TerrainEditor * editor) : Brush(editor){}
      
      const char *getType() const { return "box"; }
      void rebuild();
      void render(Vector<GFXVertexPC> & vertexBuffer, S32 & verts, S32 & elems, S32 & prims, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone) const;
};

class EllipseBrush : public Brush
{
   protected:
      Vector<S32>   mRenderList;

   public:
      EllipseBrush(TerrainEditor * editor) : Brush(editor){}
      
      const char *getType() const { return "ellipse"; }
      void rebuild();
      void render(Vector<GFXVertexPC> & vertexBuffer, S32 & verts, S32 & elems, S32 & prims, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone) const;
};

class SelectionBrush : public Brush
{
   public:
      SelectionBrush(TerrainEditor * editor);

      const char *getType() const { return "selection"; }
      void rebuild();
      void render(Vector<GFXVertexPC> & vertexBuffer, S32 & verts, S32 & elems, S32 & prims, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone) const;
      void setSize(const Point2I &){}
};
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


class TerrainAction;

class TerrainEditor : public EditTSCtrl
{
	// XA: This methods where added to replace the friend consoleMethods.
	public:
		void attachTerrain(TerrainBlock *terrBlock);
      void detachTerrain(TerrainBlock *terrBlock);
		
		void setBrushType(const char* type);
      const char* getBrushType() const;

		void setBrushSize(S32 w, S32 h);
		const char* getBrushPos();
		void setBrushPos(Point2I pos);
		
		void setAction(const char* action);
		const char* getActionName(U32 index);
		const char* getCurrentAction() const;
		S32 getNumActions();
		void processAction(const char* sAction);
			
		void resetSelWeights(bool clear);
		void clearSelection();
		
		S32 getNumTextures();

		void markEmptySquares();
		
		void mirrorTerrain(S32 mirrorIndex);
		
      TerrainBlock* getActiveTerrain() { return mActiveTerrain; };

      void scheduleGridUpdate() { mNeedsGridUpdate = true; }
      void scheduleMaterialUpdate() { mNeedsMaterialUpdate = true; }

	private:	

      typedef EditTSCtrl Parent;

      TerrainBlock* mActiveTerrain;

      // A list of all of the TerrainBlocks this editor can edit
      VectorPtr<TerrainBlock*> mTerrainBlocks;
      
      Point2I  mGridUpdateMin;
      Point2I  mGridUpdateMax;
      U32 mMouseDownSeq;

      /// If one of these flags when the terrainEditor goes to render
      /// an appropriate update method will be called on the terrain.
      /// This prevents unnecessary work from happening from directly
      /// within an editor event's process method.
      bool mNeedsGridUpdate;
      bool mNeedsMaterialUpdate;

      Point3F                    mMousePos;
      Brush *                    mMouseBrush;
      bool                       mBrushChanged;
      bool                       mRenderBrush;
      F32                        mBrushPressure;
      Point2I                    mBrushSize;
      F32                        mBrushSoftness;
      Vector<TerrainAction *>    mActions;
      TerrainAction *            mCurrentAction;
      bool                       mInAction;
      Selection                  mDefaultSel;
      bool                       mSelectionLocked;
      GuiCursor *                mDefaultCursor;
      GuiCursor *                mCurrentCursor;
      bool                       mCursorVisible;
      
      S32                        mPaintIndex;

      Selection *                mCurrentSel;

      class TerrainEditorUndoAction : public UndoAction
      {
      public:

         TerrainEditorUndoAction( const UTF8* actionName ) 
            :  UndoAction( actionName ),
               mTerrainEditor( NULL ),
               mSel( NULL )
         {
         }

         virtual ~TerrainEditorUndoAction()
         {
            delete mSel;
         }

         TerrainEditor *mTerrainEditor;

         Selection *mSel;
         
         virtual void undo();
         virtual void redo() { undo(); }
      };

      void submitUndo( Selection *sel );

      Selection *mUndoSel;

      bool mIsDirty; // dirty flag for writing terrain.
      bool mIsMissionDirty; // dirty flag for writing mission.

      GFXStateBlockRef mStateBlock;

   public:

      TerrainEditor();
      ~TerrainEditor();

      // conversion functions
      // Returns true if the grid position is on the main tile
      bool isMainTile(const Point2I & gPos) const;

      // Takes a world point and find the "highest" terrain underneath it
      // Returns true if the returned GridPoint includes a valid terrain and grid position
      TerrainBlock* getTerrainUnderWorldPoint(const Point3F & wPos);

      // Converts a GridPoint to a world position
      bool gridToWorld(const GridPoint & gPoint, Point3F & wPos);
      bool gridToWorld(const Point2I & gPos, Point3F & wPos, TerrainBlock* terrain);

      // Converts a world position to a grid point
      // If the grid point does not have a TerrainBlock already it will find the nearest
      // terrian under the world position
      bool worldToGrid(const Point3F & wPos, GridPoint & gPoint);
      bool worldToGrid(const Point3F & wPos, Point2I & gPos, TerrainBlock* terrain = NULL);

      // Converts any point that is off of the main tile to its equivalent on the main tile
      // Returns true if the point was already on the main tile
      bool gridToCenter(const Point2I & gPos, Point2I & cPos) const;

      //bool getGridInfo(const Point3F & wPos, GridInfo & info);
      // Gets the grid info for a point on a TerrainBlock's grid
      bool getGridInfo(const GridPoint & gPoint, GridInfo & info);
      bool getGridInfo(const Point2I & gPos, GridInfo & info, TerrainBlock* terrain);

      // Returns a list of infos for all points on the terrain that are at that point in space
      void getGridInfos(const GridPoint & gPoint, Vector<GridInfo>& infos);

      void setGridInfo(const GridInfo & info, bool checkActive = false);
      void setGridInfoHeight(const GridInfo & info);
      void gridUpdateComplete( bool materialChanged = false );
      void materialUpdateComplete();
      void processActionTick(U32 sequence);

      TerrainBlock* collide(const Gui3DMouseEvent & event, Point3F & pos);
      void lockSelection(bool lock) { mSelectionLocked = lock; };

      Selection * getUndoSel(){return(mUndoSel);}
      Selection * getCurrentSel(){return(mCurrentSel);}
      void setCurrentSel(Selection * sel) { mCurrentSel = sel; }
      void resetCurrentSel() {mCurrentSel = &mDefaultSel; }

      S32 getPaintMaterialIndex() const { return mPaintIndex; }

      void setBrushPressure( F32 pressure );
      F32 getBrushPressure() const { return mBrushPressure; }
      
      void setBrushSoftness( F32 softness );
      F32 getBrushSoftness() const { return mBrushSoftness; }

      Point2I getBrushSize() { return(mBrushSize); }
      
      TerrainBlock* getTerrainBlock() const { return mActiveTerrain; }
      TerrainBlock* getClientTerrain( TerrainBlock *serverTerrain = NULL ) const;
      bool terrainBlockValid() { return(mActiveTerrain ? true : false); }
      void setCursor(GuiCursor * cursor);
      void getCursor(GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent);
      void setDirty() { mIsDirty = true; }
      void setMissionDirty()  { mIsMissionDirty = true; }

      TerrainAction * lookupAction(const char * name);

   private:


      // terrain interface functions
      // Returns the height at a grid point
      F32 getGridHeight(const GridPoint & gPoint);
      // Sets a height at a grid point
      void setGridHeight(const GridPoint & gPoint, const F32 height);

      ///
      U8 getGridMaterial( const GridPoint &gPoint ) const;

      ///
      void setGridMaterial( const GridPoint & gPoint, U8 index );

      // Gets the material group of a specific spot on a TerrainBlock's grid
      U8 getGridMaterialGroup(const GridPoint & gPoint);

      // Sets a material group for a spot on a TerrainBlock's grid
      void setGridMaterialGroup(const GridPoint & gPoint, U8 group);

      //
      void updateBrush(Brush & brush, const Point2I & gPos);

      //
      Point3F getMousePos(){return(mMousePos);};

      //
      void renderSelection(const Selection & sel, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone, bool renderFill, bool renderFrame);
      void renderBrush(const Brush & brush, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone, bool renderFill, bool renderFrame);
      void renderBorder();

   public:

      // persist field data - these are dynamic
      bool                 mRenderBorder;
      F32                  mBorderHeight;
      ColorI               mBorderFillColor;
      ColorI               mBorderFrameColor;
      bool                 mBorderLineMode;
      bool                 mSelectionHidden;
      bool                 mRenderVertexSelection;
      bool                 mRenderSolidBrush;
      bool                 mProcessUsesBrush;

      //
      F32                  mAdjustHeightVal;
      F32                  mSetHeightVal;
      F32                  mScaleVal;
      F32                  mSmoothFactor;
      F32                  mNoiseFactor;
      S32                  mMaterialGroup;
      F32                  mSoftSelectRadius;
      StringTableEntry     mSoftSelectFilter;
      StringTableEntry     mSoftSelectDefaultFilter;
      F32                  mAdjustHeightMouseScale;
      Point2I              mMaxBrushSize;

      F32 mSlopeMinAngle;
      F32 mSlopeMaxAngle;

   public:

      // SimObject
      bool onAdd();
      void onDeleteNotify(SimObject * object);

      static void initPersistFields();

      // EditTSCtrl
      virtual bool onInputEvent(const InputEventInfo & event);
      virtual void on3DMouseUp(const Gui3DMouseEvent & event);
      virtual void on3DMouseDown(const Gui3DMouseEvent & event);
      virtual void on3DMouseMove(const Gui3DMouseEvent & event);
      virtual void on3DMouseDragged(const Gui3DMouseEvent & event);
      virtual void on3DMouseWheelUp(const Gui3DMouseEvent & event);
      virtual void on3DMouseWheelDown(const Gui3DMouseEvent & event);
      void updateGuiInfo();
      void renderScene(const RectI & updateRect);

      DECLARE_CONOBJECT(TerrainEditor);
};

inline void TerrainEditor::setGridInfoHeight(const GridInfo & info)
{
   setGridHeight(info.mGridPoint, info.mHeight);
}

#endif
