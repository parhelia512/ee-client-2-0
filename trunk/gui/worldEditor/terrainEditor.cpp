//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/worldEditor/terrainEditor.h"

#include "gui/core/guiCanvas.h"
#include "console/consoleTypes.h"
#include "gui/worldEditor/terrainActions.h"
#include "sim/netConnection.h"
#include "core/frameAllocator.h"
#include "gfx/primBuilder.h"
#include "console/simEvents.h"
#include "interior/interiorInstance.h"
#include "core/strings/stringUnit.h"
#include "terrain/terrMaterial.h"


IMPLEMENT_CONOBJECT(TerrainEditor);

Selection::Selection() :
   Vector<GridInfo>(__FILE__, __LINE__),
   mName(0),
   mUndoFlags(0),
   mHashListSize(1024)
{
   VECTOR_SET_ASSOCIATION(mHashLists);

   // clear the hash list
   mHashLists.setSize(mHashListSize);
   reset();
}

Selection::~Selection()
{
}

void Selection::reset()
{
   for(U32 i = 0; i < mHashListSize; i++)
      mHashLists[i] = -1;
   clear();
}

bool Selection::validate()
{
   // scan all the hashes and verify that the heads they point to point back to them
   U32 hashesProcessed = 0;
   for(U32 i = 0; i < mHashLists.size(); i++)
   {
      U32 entry = mHashLists[i];
      if(entry == -1)
         continue;
      
      GridInfo info = (*this)[entry];
      U32 hashIndex = getHashIndex(info.mGridPoint.gridPos);
      
      if( entry != mHashLists[hashIndex] )
      {
         AssertFatal(false, "Selection hash lists corrupted");
         return false;
      }
      hashesProcessed++;
   }

   // scan all the entries and verify that anything w/ a prev == -1 is correctly in the hash table
   U32 headsProcessed = 0;
   for(U32 i = 0; i < size(); i++)
   {
      GridInfo info = (*this)[i];
      if(info.mPrev != -1)
         continue;

      U32 hashIndex = getHashIndex(info.mGridPoint.gridPos);

      if(mHashLists[hashIndex] != i)
      {
         AssertFatal(false, "Selection list heads corrupted");       
         return false;
      }
      headsProcessed++;
   }
   AssertFatal(headsProcessed == hashesProcessed, "Selection's number of hashes and number of list heads differ.");
   return true;
}

U32 Selection::getHashIndex(const Point2I & pos)
{
   Point2F pnt = Point2F((F32)pos.x, (F32)pos.y) + Point2F(1.3f,3.5f);
   return( (U32)(mFloor(mHashLists.size() * mFmod(pnt.len() * 0.618f, 1.0f))) );
}

S32 Selection::lookup(const Point2I & pos)
{
   U32 index = getHashIndex(pos);

   S32 entry = mHashLists[index];

   while(entry != -1)
   {
      if((*this)[entry].mGridPoint.gridPos == pos)
         return(entry);

      entry = (*this)[entry].mNext;
   }

   return(-1);
}

void Selection::insert(GridInfo info)
{
   //validate();
   // get the index into the hash table
   U32 index = getHashIndex(info.mGridPoint.gridPos);

   // if there is an existing linked list, make it our next
   info.mNext = mHashLists[index];
   info.mPrev = -1;

   // if there is an existing linked list, make us it's prev
   U32 indexOfNewEntry = size();
   if(info.mNext != -1)
      (*this)[info.mNext].mPrev = indexOfNewEntry;

   // the hash table holds the heads of the linked lists. make us the head of this list.
   mHashLists[index] = indexOfNewEntry;

   // copy us into the vector
   push_back(info);
   //validate();
}

bool Selection::remove(const GridInfo &info)
{
   if(size() < 1)
      return false;

   //AssertFatal( validate(), "Selection hashLists corrupted before Selection.remove()");

   U32 hashIndex = getHashIndex(info.mGridPoint.gridPos);
   S32 listHead = mHashLists[hashIndex];
   //AssertFatal(listHead < size(), "A Selection's hash table is corrupt.");

   if(listHead == -1)
      return(false);

   const S32 victimEntry = lookup(info.mGridPoint.gridPos);
   if( victimEntry == -1 )
      return(false);

   const GridInfo victim = (*this)[victimEntry];
   const S32 vicPrev = victim.mPrev;
   const S32 vicNext = victim.mNext;
      
   // remove us from the linked list, if there is one.
   if(vicPrev != -1)
      (*this)[vicPrev].mNext = vicNext;
   if(vicNext != -1)
      (*this)[vicNext].mPrev = vicPrev;
   
   // if we were the head of the list, make our next the new head in the hash table.
   if(vicPrev == -1)
      mHashLists[hashIndex] = vicNext;

   // if we're not the last element in the vector, copy the last element to our position.
   if(victimEntry != size() - 1)
   {
      // copy last into victim, and re-cache next & prev
      const GridInfo lastEntry = last();
      const S32 lastPrev = lastEntry.mPrev;
      const S32 lastNext = lastEntry.mNext;
      (*this)[victimEntry] = lastEntry;
      
      // update the new element's next and prev, to reestablish it in it's linked list.
      if(lastPrev != -1)
         (*this)[lastPrev].mNext = victimEntry;
      if(lastNext != -1)
         (*this)[lastNext].mPrev = victimEntry;

      // if it was the head of it's list, update the hash table with its new position.
      if(lastPrev == -1)
      {
         const U32 lastHash = getHashIndex(lastEntry.mGridPoint.gridPos);
         AssertFatal(mHashLists[lastHash] == size() - 1, "Selection hashLists corrupted during Selection.remove() (oldmsg)");
         mHashLists[lastHash] = victimEntry;
      }
   }
   
   // decrement the vector, we're done here
   pop_back();
   //AssertFatal( validate(), "Selection hashLists corrupted after Selection.remove()");
   return true;
}

// add unique grid info into the selection - test uniqueness by grid position
bool Selection::add(const GridInfo &info)
{
   S32 index = lookup(info.mGridPoint.gridPos);
   if(index != -1)
      return(false);

   insert(info);
   return(true);
}

bool Selection::getInfo(Point2I pos, GridInfo & info)
{

   S32 index = lookup(pos);
   if(index == -1)
      return(false);

   info = (*this)[index];
   return(true);
}

bool Selection::setInfo(GridInfo & info)
{
   S32 index = lookup(info.mGridPoint.gridPos);
   if(index == -1)
      return(false);

   S32 next = (*this)[index].mNext;
   S32 prev = (*this)[index].mPrev;

   (*this)[index] = info;
   (*this)[index].mNext = next;
   (*this)[index].mPrev = prev;

   return(true);
}

F32 Selection::getAvgHeight()
{
   if(!size())
      return(0);

   F32 avg = 0.f;
   for(U32 i = 0; i < size(); i++)
      avg += (*this)[i].mHeight;

   return(avg / size());
}

F32 Selection::getMinHeight()
{
   if(!size())
      return(0);

   F32 minHeight = (*this)[0].mHeight;
   for(U32 i = 1; i < size(); i++)
      minHeight = getMin(minHeight, (*this)[i].mHeight);

   return minHeight;
}

F32 Selection::getMaxHeight()
{
   if(!size())
      return(0);

   F32 maxHeight = (*this)[0].mHeight;
   for(U32 i = 1; i < size(); i++)
      maxHeight = getMax(maxHeight, (*this)[i].mHeight);

   return maxHeight;
}

//------------------------------------------------------------------------------

Brush::Brush(TerrainEditor * editor) :
   mTerrainEditor(editor)
{
   mSize = mTerrainEditor->getBrushSize();
}

const Point2I & Brush::getPosition()
{
   return(mGridPoint.gridPos);
}

const GridPoint & Brush::getGridPoint()
{
   return mGridPoint;
}

void Brush::setPosition(const Point3F & pos)
{
   mTerrainEditor->worldToGrid(pos, mGridPoint);
   update();
}

void Brush::setPosition(const Point2I & pos)
{
   mGridPoint.gridPos = pos;
   update();
}

//------------------------------------------------------------------------------

void Brush::update()
{
   rebuild();
}

//------------------------------------------------------------------------------

void BoxBrush::rebuild()
{
   reset();
   Filter filter;
   filter.set(1, &mTerrainEditor->mSoftSelectFilter);
   //
   // mSize should always be odd.

   S32 centerX = (mSize.x - 1) / 2;
   S32 centerY = (mSize.y - 1) / 2;

   F32 xFactorScale = F32(centerX) / (F32(centerX) + 0.5);
   F32 yFactorScale = F32(centerY) / (F32(centerY) + 0.5);
   
   const F32 softness = mTerrainEditor->getBrushSoftness();
   const F32 pressure = mTerrainEditor->getBrushPressure();

   for(S32 x = 0; x < mSize.x; x++)
   {
      for(S32 y = 0; y < mSize.y; y++)
      {
         Vector<GridInfo> infos;
         GridPoint gridPoint = mGridPoint;
         gridPoint.gridPos.set(mGridPoint.gridPos.x + x - centerX, mGridPoint.gridPos.y + y - centerY);

         mTerrainEditor->getGridInfos(gridPoint, infos);

         F32 xFactor = 0.0f;
         if(centerX > 0)
            xFactor = (mAbs(centerX - x) / F32(centerX)) * xFactorScale;

         F32 yFactor = 0.0f;
         if(centerY > 0)
            yFactor = (mAbs(centerY - y) / F32(centerY)) * yFactorScale;

         for (U32 z = 0; z < infos.size(); z++)
         {
            infos[z].mWeight = pressure *
               mLerp( infos[z].mWeight, filter.getValue(xFactor > yFactor ? xFactor : yFactor), softness );

            push_back(infos[z]);
         }
      }
   }
}

void BoxBrush::render(Vector<GFXVertexPC> & vertexBuffer, S32 & verts, S32 & elems, S32 & prims, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone) const
{
   vertexBuffer.setSize(size() * 5);

   verts = 5;
   elems = 4;
   prims = size();

   ColorF color;
   U32 bindex;
   F32 weight[4];
   U32 vindex = 0;
   for(U32 i=0; i<mSize.x-1; ++i)
   {
      for(U32 j=0; j<mSize.y-1; ++j)
      {
         GFXVertexPC *verts = &(vertexBuffer[vindex]);

         bindex = i*mSize.x + j;
         mTerrainEditor->gridToWorld((*this)[bindex].mGridPoint, verts[0].point);
         weight[0] = (*this)[bindex].mWeight;
         mTerrainEditor->gridToWorld((*this)[bindex+1].mGridPoint, verts[1].point);
         weight[1] = (*this)[bindex+1].mWeight;
         bindex = (i+1)*mSize.x + j;
         mTerrainEditor->gridToWorld((*this)[bindex+1].mGridPoint, verts[2].point);
         weight[2] = (*this)[bindex+1].mWeight;
         mTerrainEditor->gridToWorld((*this)[bindex].mGridPoint, verts[3].point);
         weight[3] = (*this)[bindex].mWeight;

         for(U32 k=0; k<4; ++k)
         {
            if( !mTerrainEditor->mRenderSolidBrush )
            {
               if ( weight[k] < 0.f || weight[k] > 1.f )
                  color = inColorFull;
               else
                  color.interpolate(inColorNone, inColorFull, weight[k] );
            }
            else
            {
               color = inColorFull;
            }

            verts[k].color = color;
         }

         verts[4].point = verts[0].point;
         verts[4].color = verts[0].color;

         vindex += 5;
      }
   }
}

//------------------------------------------------------------------------------

void EllipseBrush::rebuild()
{
   reset();
   mRenderList.setSize(mSize.x*mSize.y);
   Point3F center(F32(mSize.x - 1) / 2, F32(mSize.y - 1) / 2, 0);
   Filter filter;
   filter.set(1, &mTerrainEditor->mSoftSelectFilter);

   // a point is in a circle if:
   // x^2 + y^2 <= r^2
   // a point is in an ellipse if:
   // (ax)^2 + (by)^2 <= 1
   // where a = 1/halfEllipseWidth and b = 1/halfEllipseHeight

   // for a soft-selected ellipse,
   // the factor is simply the filtered: ((ax)^2 + (by)^2)

   F32 a = 1 / (F32(mSize.x) * 0.5);
   F32 b = 1 / (F32(mSize.y) * 0.5);
   
   const F32 softness = mTerrainEditor->getBrushSoftness();
   const F32 pressure = mTerrainEditor->getBrushPressure();

   for(S32 x = 0; x < mSize.x; x++)
   {
      for(S32 y = 0; y < mSize.y; y++)
      {
         F32 xp = center.x - x;
         F32 yp = center.y - y;

         F32 factor = (a * a * xp * xp) + (b * b * yp * yp);
         if(factor > 1)
         {
            mRenderList[x*mSize.x+y] = -1;
            continue;
         }

         Vector<GridInfo> infos;
         GridPoint gridPoint = mGridPoint;
         gridPoint.gridPos.set((S32)(mGridPoint.gridPos.x + x - (S32)center.x), (S32)(mGridPoint.gridPos.y + y - (S32)center.y));

         mTerrainEditor->getGridInfos(gridPoint, infos);

         for (U32 z = 0; z < infos.size(); z++)
         {
            infos[z].mWeight = pressure * mLerp( infos[z].mWeight, filter.getValue( factor ), softness ); 
            push_back(infos[z]);
         }

         mRenderList[x*mSize.x+y] = size()-1;
      }
   }
}

void EllipseBrush::render(Vector<GFXVertexPC> & vertexBuffer, S32 & verts, S32 & elems, S32 & prims, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone) const
{
   vertexBuffer.setSize(size() * 5);

   verts = 5;
   elems = 4;
   prims = 0;

   ColorF color;
   F32 weight[4];
   U32 vindex = 0;
   for(U32 i=0; i<mSize.x-1; ++i)
   {
      for(U32 j=0; j<mSize.y-1; ++j)
      {
         // Make sure that all four corners of the quad are valid
         if(mRenderList[i*mSize.x+j] == -1)
            continue;
         if(mRenderList[i*mSize.x+j+1] == -1)
            continue;
         if(mRenderList[(i+1)*mSize.x+j] == -1)
            continue;
         if(mRenderList[(i+1)*mSize.x+j+1] == -1)
            continue;

         GFXVertexPC *verts = &(vertexBuffer[vindex]);

         mTerrainEditor->gridToWorld((*this)[mRenderList[i*mSize.x+j]].mGridPoint, verts[0].point);
         weight[0] = (*this)[mRenderList[i*mSize.x+j]].mWeight;

         mTerrainEditor->gridToWorld((*this)[mRenderList[i*mSize.x+j+1]].mGridPoint, verts[1].point);
         weight[1] = (*this)[mRenderList[i*mSize.x+j+1]].mWeight;

         mTerrainEditor->gridToWorld((*this)[mRenderList[(i+1)*mSize.x+j+1]].mGridPoint, verts[2].point);
         weight[2] = (*this)[mRenderList[(i+1)*mSize.x+j+1]].mWeight;

         mTerrainEditor->gridToWorld((*this)[mRenderList[(i+1)*mSize.x+j]].mGridPoint, verts[3].point);
         weight[3] = (*this)[mRenderList[(i+1)*mSize.x+j]].mWeight;

         for(U32 k=0; k<4; ++k)
         {
            if( !mTerrainEditor->mRenderSolidBrush )
            {
               if ( weight[k] < 0.f || weight[k] > 1.f )
                  color = inColorFull;
               else
                  color.interpolate(inColorNone, inColorFull, weight[k] );
            }
            else
            {
               color = inColorFull;
            }

            verts[k].color = color;
         }

         verts[4].point = verts[0].point;
         verts[4].color = verts[0].color;

         vindex += 5;
         ++prims;
      }
   }
}

//------------------------------------------------------------------------------

SelectionBrush::SelectionBrush(TerrainEditor * editor) :
   Brush(editor)
{
   //... grab the current selection
}

void SelectionBrush::rebuild()
{
   reset();
   //... move the selection
}

void SelectionBrush::render(Vector<GFXVertexPC> & vertexBuffer, S32 & verts, S32 & elems, S32 & prims, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone) const
{
   //... render the selection
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

TerrainEditor::TerrainEditor() :
   mActiveTerrain(0),
   mMousePos(0,0,0),
   mMouseBrush(0),
   mInAction(false),
   mUndoSel(0),
   mGridUpdateMin( S32_MAX, S32_MAX ),
   mGridUpdateMax( 0, 0 ),
   mMaxBrushSize(48,48),
   mNeedsGridUpdate( false ),
   mNeedsMaterialUpdate( false )
{
   VECTOR_SET_ASSOCIATION(mActions);

   //
   resetCurrentSel();

   //
   mBrushPressure = 1.0f;
   mBrushSize.set(1,1);
   mBrushSoftness = 1.0f;
   mBrushChanged = true;
   mMouseBrush = new BoxBrush(this);
   mMouseDownSeq = 0;
   mIsDirty = false;
   mIsMissionDirty = false;
   mPaintIndex = -1;

   // add in all the actions here..
   mActions.push_back(new SelectAction(this));
   mActions.push_back(new DeselectAction(this));
   mActions.push_back(new ClearAction(this));
   mActions.push_back(new SoftSelectAction(this));
   mActions.push_back(new OutlineSelectAction(this));
   mActions.push_back(new PaintMaterialAction(this));
   mActions.push_back(new ClearMaterialsAction(this));
   mActions.push_back(new RaiseHeightAction(this));
   mActions.push_back(new LowerHeightAction(this));
   mActions.push_back(new SetHeightAction(this));
   mActions.push_back(new SetEmptyAction(this));
   mActions.push_back(new ClearEmptyAction(this));
   mActions.push_back(new ScaleHeightAction(this));
   mActions.push_back(new BrushAdjustHeightAction(this));
   mActions.push_back(new AdjustHeightAction(this));
   mActions.push_back(new FlattenHeightAction(this));
   mActions.push_back(new SmoothHeightAction(this));
   mActions.push_back(new PaintNoiseAction(this));
   //mActions.push_back(new ThermalErosionAction(this));


   // set the default action
   mCurrentAction = mActions[0];
   mRenderBrush = mCurrentAction->useMouseBrush();

   // persist data defaults
   mRenderBorder = true;
   mBorderHeight = 10;
   mBorderFillColor.set(0,255,0,20);
   mBorderFrameColor.set(0,255,0,128);
   mBorderLineMode = false;
   mSelectionHidden = false;
   mRenderVertexSelection = false;
   mRenderSolidBrush = false;
   mProcessUsesBrush = false;
   mCurrentCursor = NULL;
   mCursorVisible = true;

   //
   mAdjustHeightVal = 10;
   mSetHeightVal = 100;
   mScaleVal = 1;
   mSmoothFactor = 0.1f;
   mNoiseFactor = 1.0f;
   mMaterialGroup = 0;
   mSoftSelectRadius = 50.f;
   mAdjustHeightMouseScale = 0.1f;

   mSoftSelectDefaultFilter = StringTable->insert("1.000000 0.833333 0.666667 0.500000 0.333333 0.166667 0.000000");
   mSoftSelectFilter = mSoftSelectDefaultFilter;

   mSlopeMinAngle = 0.0f;
   mSlopeMaxAngle = 90.0f;
}

TerrainEditor::~TerrainEditor()
{
   // mouse
   delete mMouseBrush;

   // terrain actions
   U32 i;
   for(i = 0; i < mActions.size(); i++)
      delete mActions[i];

   // undo stuff
   delete mUndoSel;
}

//------------------------------------------------------------------------------

TerrainAction * TerrainEditor::lookupAction(const char * name)
{
   for(U32 i = 0; i < mActions.size(); i++)
      if(!dStricmp(mActions[i]->getName(), name))
         return(mActions[i]);
   return(0);
}

//------------------------------------------------------------------------------

bool TerrainEditor::onAdd()
{
   if(!Parent::onAdd())
      return(false);

   SimObject * obj = Sim::findObject("EditorArrowCursor");
   if(!obj)
   {
      Con::errorf(ConsoleLogEntry::General, "TerrainEditor::onAdd: failed to load cursor");
      return(false);
   }

   mDefaultCursor = dynamic_cast<GuiCursor*>(obj);

   GFXStateBlockDesc desc;
   desc.setZReadWrite( false );
   desc.zWriteEnable = false;
   desc.setCullMode( GFXCullNone );
   desc.setBlend( true, GFXBlendSrcAlpha, GFXBlendDestAlpha );
   mStateBlock = GFX->createStateBlock( desc );

   return true;
}

//------------------------------------------------------------------------------

void TerrainEditor::onDeleteNotify(SimObject * object)
{
   Parent::onDeleteNotify(object);

   for (U32 i = 0; i < mTerrainBlocks.size(); i++)
   {
      if(mTerrainBlocks[i] != dynamic_cast<TerrainBlock*>(object))
         continue;

      if (mTerrainBlocks[i] == mActiveTerrain)
         mActiveTerrain = NULL;
   }
}

void TerrainEditor::setCursor(GuiCursor * cursor)
{
   mCurrentCursor = cursor ? cursor : mDefaultCursor;
}

//------------------------------------------------------------------------------

TerrainBlock* TerrainEditor::getClientTerrain( TerrainBlock *serverTerrain ) const
{
   if ( !serverTerrain && !mActiveTerrain )
      return NULL;

   serverTerrain = mActiveTerrain; // WEIRD STUFF!

   return dynamic_cast<TerrainBlock*>( serverTerrain->getClientObject() );
}

//------------------------------------------------------------------------------

bool TerrainEditor::isMainTile(const Point2I & gPos) const
{
   return true;

   // TODO: How importiant is this... is this to do with
   // tiled terrains or to deal with megaterrain?
   /*
   Point2I testGridPos = gPos;

   if (!dStrcmp(getCurrentAction(),"paintMaterial"))
   {
      if (testGridPos.x == (1 << TerrainBlock::BlockShift))
         testGridPos.x--;
      if (testGridPos.y == (1 << TerrainBlock::BlockShift))
         testGridPos.y--;
   }

   return (!(testGridPos.x >> TerrainBlock::BlockShift || testGridPos.y >> TerrainBlock::BlockShift));
   */
}

TerrainBlock* TerrainEditor::getTerrainUnderWorldPoint(const Point3F & wPos)
{
   // Cast a ray straight down from the world position and see which
   // Terrain is the closest to our starting point
   Point3F startPnt = wPos;
   Point3F endPnt = wPos + Point3F(0.0f, 0.0f, -1000.0f);

   S32 blockIndex = -1;
   F32 nearT = 1.0f;

   for (U32 i = 0; i < mTerrainBlocks.size(); i++)
   {
      Point3F tStartPnt, tEndPnt;

      mTerrainBlocks[i]->getWorldTransform().mulP(startPnt, &tStartPnt);
      mTerrainBlocks[i]->getWorldTransform().mulP(endPnt, &tEndPnt);

      RayInfo ri;
      if (mTerrainBlocks[i]->castRayI(tStartPnt, tEndPnt, &ri, true))
      {
         if (ri.t < nearT)
         {
            blockIndex = i;
            nearT = ri.t;
         }
      }
   }

   if (blockIndex > -1)
      return mTerrainBlocks[blockIndex];

   return NULL;
}

bool TerrainEditor::gridToWorld(const GridPoint & gPoint, Point3F & wPos)
{
   const MatrixF & mat = gPoint.terrainBlock->getTransform();
   Point3F origin;
   mat.getColumn(3, &origin);

   wPos.x = gPoint.gridPos.x * gPoint.terrainBlock->getSquareSize() + origin.x;
   wPos.y = gPoint.gridPos.y * gPoint.terrainBlock->getSquareSize() + origin.y;
   wPos.z = getGridHeight(gPoint) + origin.z;

   return isMainTile(gPoint.gridPos);
}

bool TerrainEditor::gridToWorld(const Point2I & gPos, Point3F & wPos, TerrainBlock* terrain)
{
   GridPoint gridPoint;
   gridPoint.gridPos = gPos;
   gridPoint.terrainBlock = terrain;

   return gridToWorld(gridPoint, wPos);
}

bool TerrainEditor::worldToGrid(const Point3F & wPos, GridPoint & gPoint)
{
   // If the grid point TerrainBlock is NULL then find the closest Terrain underneath that
   // point - pad a little upward in case our incoming point already lies exactly on the terrain
   if (!gPoint.terrainBlock)
      gPoint.terrainBlock = getTerrainUnderWorldPoint(wPos + Point3F(0.0f, 0.0f, 0.05f));

   if (gPoint.terrainBlock == NULL)
      return false;

   const MatrixF & worldMat = gPoint.terrainBlock->getWorldTransform();
   Point3F tPos = wPos;
   worldMat.mulP(tPos);

   F32 squareSize = (F32) gPoint.terrainBlock->getSquareSize();
   F32 halfSquareSize = squareSize / 2;

   F32 x = (tPos.x + halfSquareSize) / squareSize;
   F32 y = (tPos.y + halfSquareSize) / squareSize;

   gPoint.gridPos.x = (S32)mFloor(x);
   gPoint.gridPos.y = (S32)mFloor(y);

   return isMainTile(gPoint.gridPos);
}

bool TerrainEditor::worldToGrid(const Point3F & wPos, Point2I & gPos, TerrainBlock* terrain)
{
   GridPoint gridPoint;
   gridPoint.terrainBlock = terrain;

   bool ret = worldToGrid(wPos, gridPoint);

   gPos = gridPoint.gridPos;

   return ret;
}

bool TerrainEditor::gridToCenter(const Point2I & gPos, Point2I & cPos) const
{
   // TODO: What is this for... megaterrain or tiled terrains?
   cPos.x = gPos.x; // & TerrainBlock::BlockMask;
   cPos.y = gPos.y;// & TerrainBlock::BlockMask;

   //if (gPos.x == TerrainBlock::BlockSize)
   //   cPos.x = gPos.x;
   //if (gPos.y == TerrainBlock::BlockSize)
   //   cPos.y = gPos.y;

   return isMainTile(gPos);
}

//------------------------------------------------------------------------------

//bool TerrainEditor::getGridInfo(const Point3F & wPos, GridInfo & info)
//{
//   Point2I gPos;
//   worldToGrid(wPos, gPos);
//   return getGridInfo(gPos, info);
//}

bool TerrainEditor::getGridInfo(const GridPoint & gPoint, GridInfo & info)
{
   //
   info.mGridPoint = gPoint;
   info.mMaterial = getGridMaterial(gPoint);
   info.mHeight = getGridHeight(gPoint);
   info.mWeight = 1.f;
   info.mPrimarySelect = true;
   info.mMaterialChanged = false;

   Point2I cPos;
   gridToCenter(gPoint.gridPos, cPos);

   return isMainTile(gPoint.gridPos);
}

bool TerrainEditor::getGridInfo(const Point2I & gPos, GridInfo & info, TerrainBlock* terrain)
{
   GridPoint gridPoint;
   gridPoint.gridPos = gPos;
   gridPoint.terrainBlock = terrain;

   return getGridInfo(gridPoint, info);
}

void TerrainEditor::getGridInfos(const GridPoint & gPoint, Vector<GridInfo>& infos)
{
   // First we test against the brush terrain so that we can
   // favor it (this should be the same as the active terrain)
   bool foundBrush = false;

   GridInfo baseInfo;
   if (getGridInfo(gPoint, baseInfo))
   {
      infos.push_back(baseInfo);

      foundBrush = true;
   }

   // We are going to need the world position to test against
   Point3F wPos;
   gridToWorld(gPoint, wPos);

   // Now loop through our terrain blocks and decide which ones hit the point
   // If we already found a hit against our brush terrain we only add points
   // that are relatively close to the found point
   for (U32 i = 0; i < mTerrainBlocks.size(); i++)
   {
      // Skip if we've already found the point on the brush terrain
      if (foundBrush && mTerrainBlocks[i] == baseInfo.mGridPoint.terrainBlock)
         continue;

      // Get our grid position
      Point2I gPos;
      worldToGrid(wPos, gPos, mTerrainBlocks[i]);

      GridInfo info;
      if (getGridInfo(gPos, info, mTerrainBlocks[i]))
      {
         // Skip adding this if we already found a GridInfo from the brush terrain
         // and the resultant world point isn't equivalent
         if (foundBrush)
         {
            // Convert back to world (since the height can be different)
            // Possibly use getHeight() here?
            Point3F testWorldPt;
            gridToWorld(gPos, testWorldPt, mTerrainBlocks[i]);

            if (mFabs( wPos.z - testWorldPt.z ) > 4.0f )
               continue;
         }

         infos.push_back(info);
      }
   }
}

void TerrainEditor::setGridInfo(const GridInfo & info, bool checkActive)
{
   setGridHeight(info.mGridPoint, info.mHeight);
   setGridMaterial(info.mGridPoint, info.mMaterial);
}

F32 TerrainEditor::getGridHeight(const GridPoint & gPoint)
{
   Point2I cPos;
   gridToCenter( gPoint.gridPos, cPos );
   const TerrainFile *file = gPoint.terrainBlock->getFile();
   return fixedToFloat( file->getHeight( cPos.x, cPos.y ) );
}

void TerrainEditor::gridUpdateComplete( bool materialChanged )
{
   // TODO: This updates all terrains and not just the ones
   // that were changed.  We should keep track of the mGridUpdate
   // in world space and transform it into terrain space.

   if(mGridUpdateMin.x <= mGridUpdateMax.x)
   {
      for (U32 i = 0; i < mTerrainBlocks.size(); i++)
      {
         TerrainBlock *clientTerrain = getClientTerrain( mTerrainBlocks[i] );
         if ( materialChanged )
            clientTerrain->updateGridMaterials(mGridUpdateMin, mGridUpdateMax);

         mTerrainBlocks[i]->updateGrid(mGridUpdateMin, mGridUpdateMax);
         clientTerrain->updateGrid(mGridUpdateMin, mGridUpdateMax);
      }
   }

   mGridUpdateMin.set( S32_MAX, S32_MAX );
   mGridUpdateMax.set( 0, 0 );
   mNeedsGridUpdate = false;
}

void TerrainEditor::materialUpdateComplete()
{
   if(mGridUpdateMin.x <= mGridUpdateMax.x)
   {
      TerrainBlock * clientTerrain = getClientTerrain(mActiveTerrain);
      clientTerrain->updateGridMaterials(mGridUpdateMin, mGridUpdateMax);
      //mActiveTerrain->updateGrid(mGridUpdateMin, mGridUpdateMax);
   }
   mGridUpdateMin.set( S32_MAX, S32_MAX );
   mGridUpdateMax.set( 0, 0 );
   mNeedsMaterialUpdate = false;
}

void TerrainEditor::setGridHeight(const GridPoint & gPoint, const F32 height)
{
   Point2I cPos;
   gridToCenter(gPoint.gridPos, cPos);

   mGridUpdateMin.setMin( cPos );
   mGridUpdateMax.setMax( cPos );

   gPoint.terrainBlock->setHeight(cPos, height);
}

U8 TerrainEditor::getGridMaterial( const GridPoint &gPoint ) const
{
   Point2I cPos;
   gridToCenter( gPoint.gridPos, cPos );
   const TerrainFile *file = gPoint.terrainBlock->getFile();
   return file->getLayerIndex( cPos.x, cPos.y );
}

void TerrainEditor::setGridMaterial( const GridPoint &gPoint, U8 index )
{
   Point2I cPos;
   gridToCenter( gPoint.gridPos, cPos );
   TerrainFile *file = gPoint.terrainBlock->getFile();

   // If we changed the empty state then we need
   // to do a grid update as well.
   U8 currIndex = file->getLayerIndex( cPos.x, cPos.y );
   if (  ( currIndex == (U8)-1 && index != (U8)-1 ) || 
         ( currIndex != (U8)-1 && index == (U8)-1 ) )
   {
      mGridUpdateMin.setMin( cPos );
      mGridUpdateMax.setMax( cPos );
      mNeedsGridUpdate = true;
   }

   file->setLayerIndex( cPos.x, cPos.y, index );
}

//------------------------------------------------------------------------------

TerrainBlock* TerrainEditor::collide(const Gui3DMouseEvent & event, Point3F & pos)
{
   if (mTerrainBlocks.size() == 0)
      return NULL;

   // call the terrain block's ray collision routine directly
   Point3F startPnt = event.pos;
   Point3F endPnt = event.pos + event.vec * 1000.0f;

   S32 blockIndex = -1;
   F32 nearT = 1.0f;

   for (U32 i = 0; i < mTerrainBlocks.size(); i++)
   {
      Point3F tStartPnt, tEndPnt;

      mTerrainBlocks[i]->getWorldTransform().mulP(startPnt, &tStartPnt);
      mTerrainBlocks[i]->getWorldTransform().mulP(endPnt, &tEndPnt);

      RayInfo ri;
      if (mTerrainBlocks[i]->castRayI(tStartPnt, tEndPnt, &ri, true))
      {
         if (ri.t < nearT)
         {
            blockIndex = i;
            nearT = ri.t;
         }
      }
   }

   if (blockIndex > -1)
   {
      pos.interpolate(startPnt, endPnt, nearT);

      return mTerrainBlocks[blockIndex];
   }

   return NULL;
}

//------------------------------------------------------------------------------

void TerrainEditor::updateGuiInfo()
{
   char buf[128];

   // mouse num grids
   // mouse avg height
   // selection num grids
   // selection avg height
   dSprintf(buf, sizeof(buf), "%d %g %g %g %d %g",
      mMouseBrush->size(), mMouseBrush->getMinHeight(),
      mMouseBrush->getAvgHeight(), mMouseBrush->getMaxHeight(),
      mDefaultSel.size(), mDefaultSel.getAvgHeight());
   Con::executef(this, "onGuiUpdate", buf);

   // If the brush setup has changed send out
   // a notification of that!
   if ( mBrushChanged && isMethod( "onBrushChanged" ) )
   {
      mBrushChanged = false;
      Con::executef( this, "onBrushChanged" );
   }
}

//------------------------------------------------------------------------------

void TerrainEditor::renderScene(const RectI &)
{
   if ( mNeedsGridUpdate )         
      gridUpdateComplete( mNeedsMaterialUpdate );         
   else if ( mNeedsMaterialUpdate )   
      materialUpdateComplete();

   if(mTerrainBlocks.size() == 0)
      return;

   if(!mSelectionHidden)
      renderSelection(mDefaultSel, ColorF(1,0,0), ColorF(0,1,0), ColorF(0,0,1), ColorF(0,0,1), true, false);

   if(mRenderBrush && mMouseBrush->size())
      renderBrush(*mMouseBrush, ColorF(0,1,0), ColorF(1,0,0), ColorF(0,0,1), ColorF(0,0,1), false, true);

   if(mRenderBorder)
      renderBorder();
}

//------------------------------------------------------------------------------

void TerrainEditor::renderSelection( const Selection & sel, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone, bool renderFill, bool renderFrame )
{
   // Draw nothing if nothing selected.
   if(sel.size() == 0)
      return;

   Vector<GFXVertexPC> vertexBuffer;
   ColorF color;
   ColorI iColor;

   vertexBuffer.setSize(sel.size() * 5);

   F32 squareSize = ( mActiveTerrain ) ? mActiveTerrain->getSquareSize() : 1;

   // 'RenderVertexSelection' looks really bad so just always use the good one.
   if( false && mRenderVertexSelection)
   {

      for(U32 i = 0; i < sel.size(); i++)
      {
         Point3F wPos;
         bool center = gridToWorld(sel[i].mGridPoint, wPos);

         F32 weight = sel[i].mWeight;

         if(center)
         {
            if ( weight < 0.f || weight > 1.f )
               color = inColorFull;
            else
               color.interpolate( inColorNone, inColorFull, weight );
         }
         else
         {
            if ( weight < 0.f || weight > 1.f)
               color = outColorFull;
            else
               color.interpolate( outColorFull, outColorNone, weight );
         }
         //
         iColor = color;

         GFXVertexPC *verts = &(vertexBuffer[i * 5]);

         verts[0].point = wPos + Point3F(-squareSize, -squareSize, 0);
         verts[0].color = iColor;
         verts[1].point = wPos + Point3F( squareSize, -squareSize, 0);
         verts[1].color = iColor;
         verts[2].point = wPos + Point3F( squareSize,  squareSize, 0);
         verts[2].color = iColor;
         verts[3].point = wPos + Point3F(-squareSize,  squareSize, 0);
         verts[3].color = iColor;
         verts[4].point = verts[0].point;
         verts[4].color = iColor;
      }
   }
   else
   {
      // walk the points in the selection
      for(U32 i = 0; i < sel.size(); i++)
      {
         Point2I gPos = sel[i].mGridPoint.gridPos;

         GFXVertexPC *verts = &(vertexBuffer[i * 5]);

         bool center = gridToWorld(sel[i].mGridPoint, verts[0].point);
         gridToWorld(Point2I(gPos.x + 1, gPos.y), verts[1].point, sel[i].mGridPoint.terrainBlock);
         gridToWorld(Point2I(gPos.x + 1, gPos.y + 1), verts[2].point, sel[i].mGridPoint.terrainBlock);
         gridToWorld(Point2I(gPos.x, gPos.y + 1), verts[3].point, sel[i].mGridPoint.terrainBlock);
         verts[4].point = verts[0].point;

         F32 weight = sel[i].mWeight;

         if( !mRenderSolidBrush )
         {
            if ( center )
            {
               if ( weight < 0.f || weight > 1.f )
                  color = inColorFull;
               else
                  color.interpolate(inColorNone, inColorFull, weight );
            }
            else
            {
               if( weight < 0.f || weight > 1.f )
                  color = outColorFull;
               else
                  color.interpolate(outColorFull, outColorNone, weight );
            }

            iColor = color;
         }
         else
         {
            if ( center )
            {
               iColor = inColorNone;
            }
            else
            {
               iColor = outColorFull;
            }
         }

         verts[0].color = iColor;
         verts[1].color = iColor;
         verts[2].color = iColor;
         verts[3].color = iColor;
         verts[4].color = iColor;
      }
   }

   // Render this bad boy, by stuffing everything into a volatile buffer
   // and rendering...
   GFXVertexBufferHandle<GFXVertexPC> selectionVB(GFX, vertexBuffer.size(), GFXBufferTypeStatic);

   selectionVB.lock(0, vertexBuffer.size());

   // Copy stuff
   dMemcpy((void*)&selectionVB[0], (void*)&vertexBuffer[0], sizeof(GFXVertexPC) * vertexBuffer.size());

   selectionVB.unlock();

   GFX->setupGenericShaders();
   GFX->setStateBlock( mStateBlock );
   GFX->setVertexBuffer(selectionVB);

   if(renderFill)
      for(U32 i=0; i < sel.size(); i++)
         GFX->drawPrimitive( GFXTriangleFan, i*5, 4);

   if(renderFrame)
      for(U32 i=0; i < sel.size(); i++)
         GFX->drawPrimitive( GFXLineStrip , i*5, 4);

}

void TerrainEditor::renderBrush( const Brush & brush, const ColorF & inColorFull, const ColorF & inColorNone, const ColorF & outColorFull, const ColorF & outColorNone, bool renderFill, bool renderFrame )
{
   // Draw nothing if nothing selected.
   if(brush.size() == 0)
      return;

   S32 verticesPerPrimitive;
   S32 elementsPerPrimitive;
   S32 numPrimitives = 0;

   Vector<GFXVertexPC> vertexBuffer;

   if(brush.size() == 1)
   {
      Point2I gPos = brush[0].mGridPoint.gridPos;

      vertexBuffer.setSize(6);
      GFXVertexPC *verts = &(vertexBuffer[0]);

      Point3F centerPos;
      bool center = gridToWorld(brush[0].mGridPoint, centerPos);

      gridToWorld(Point2I(gPos.x - 1, gPos.y), verts[0].point, brush[0].mGridPoint.terrainBlock);
      verts[1].point = centerPos;
      gridToWorld(Point2I(gPos.x, gPos.y + 1), verts[2].point, brush[0].mGridPoint.terrainBlock);

      gridToWorld(Point2I(gPos.x + 1, gPos.y), verts[3].point, brush[0].mGridPoint.terrainBlock);
      verts[4].point = centerPos;
      gridToWorld(Point2I(gPos.x, gPos.y - 1), verts[5].point, brush[0].mGridPoint.terrainBlock);

      F32 weight = brush[0].mWeight;

      ColorF color;
      ColorI iColor;
      if ( center )
      {
         if ( weight < 0.f || weight > 1.f )
            color = inColorFull;
         else
            color.interpolate(inColorNone, inColorFull, weight );
      }
      else
      {
         if( weight < 0.f || weight > 1.f )
            color = outColorFull;
         else
            color.interpolate(outColorFull, outColorNone, weight );
      }

      iColor = color;
      verts[1].color = iColor;
      verts[4].color = iColor;
      iColor = inColorNone;
      verts[0].color = iColor;
      verts[2].color = iColor;
      verts[3].color = iColor;
      verts[5].color = iColor;

      verticesPerPrimitive = 3;
      elementsPerPrimitive = 2;
      numPrimitives = 2;
   }
   else
   {
      brush.render(vertexBuffer, verticesPerPrimitive, elementsPerPrimitive, numPrimitives, inColorFull, inColorNone, outColorFull, outColorNone);
   }

   // Render this bad boy, by stuffing everything into a volatile buffer
   // and rendering...
   GFXVertexBufferHandle<GFXVertexPC> selectionVB(GFX, vertexBuffer.size(), GFXBufferTypeStatic);

   selectionVB.lock(0, vertexBuffer.size());

   // Copy stuff
   dMemcpy((void*)&selectionVB[0], (void*)&vertexBuffer[0], sizeof(GFXVertexPC) * vertexBuffer.size());

   selectionVB.unlock();

   GFX->setupGenericShaders();
   GFX->setStateBlock( mStateBlock );
   GFX->setVertexBuffer(selectionVB);

   if(renderFill)
      for(U32 i=0; i < numPrimitives; i++)
         GFX->drawPrimitive( GFXTriangleFan, i*verticesPerPrimitive, elementsPerPrimitive);

   if(renderFrame)
      for(U32 i=0; i < numPrimitives; i++)
         GFX->drawPrimitive( GFXLineStrip , i*verticesPerPrimitive, elementsPerPrimitive);
}

void TerrainEditor::renderBorder()
{
   // TODO: Disabled rendering the terrain borders... it was
   // very annoying getting a fullscreen green tint on things.
   //
   // We should consider killing this all together or coming
   // up with a new technique.
   /*
   Point2I pos(0,0);
   Point2I dir[4] = {
      Point2I(1,0),
      Point2I(0,1),
      Point2I(-1,0),
      Point2I(0,-1)
   };

   GFX->setStateBlock( mStateBlock );
   
   //
   if(mBorderLineMode)
   {
      PrimBuild::color(mBorderFrameColor);
      
      PrimBuild::begin( GFXLineStrip, TerrainBlock::BlockSize * 4 + 1 );
      for(U32 i = 0; i < 4; i++)
      {
         for(U32 j = 0; j < TerrainBlock::BlockSize; j++)
         {
            Point3F wPos;
            gridToWorld(pos, wPos, mActiveTerrain);
            PrimBuild::vertex3fv( wPos );
            pos += dir[i];
         }
      }

      Point3F wPos;
      gridToWorld(Point2I(0,0), wPos, mActiveTerrain);
      PrimBuild::vertex3fv( wPos );
      PrimBuild::end();
   }
   else
   {
      GridSquare * gs = mActiveTerrain->findSquare(TerrainBlock::BlockShift, Point2I(0,0));
      F32 height = F32(gs->maxHeight) * 0.03125f + mBorderHeight;

      const MatrixF & mat = mActiveTerrain->getTransform();
      Point3F pos;
      mat.getColumn(3, &pos);

      Point2F min(pos.x, pos.y);
      Point2F max(pos.x + TerrainBlock::BlockSize * mActiveTerrain->getSquareSize(),
                  pos.y + TerrainBlock::BlockSize * mActiveTerrain->getSquareSize());

      ColorI & a = mBorderFillColor;
      ColorI & b = mBorderFrameColor;

      for(U32 i = 0; i < 2; i++)
      {
         //
         if(i){ PrimBuild::color(a); PrimBuild::begin( GFXTriangleFan, 4 ); } else { PrimBuild::color(b); PrimBuild::begin( GFXLineStrip, 5 ); }

         PrimBuild::vertex3f(min.x, min.y, 0);
         PrimBuild::vertex3f(max.x, min.y, 0);
         PrimBuild::vertex3f(max.x, min.y, height);
         PrimBuild::vertex3f(min.x, min.y, height);
         if(!i) PrimBuild::vertex3f( min.x, min.y, 0.f );
         PrimBuild::end();

         //
         if(i){ PrimBuild::color(a); PrimBuild::begin( GFXTriangleFan, 4 ); } else { PrimBuild::color(b); PrimBuild::begin( GFXLineStrip, 5 ); }
         PrimBuild::vertex3f(min.x, max.y, 0);
         PrimBuild::vertex3f(max.x, max.y, 0);
         PrimBuild::vertex3f(max.x, max.y, height);
         PrimBuild::vertex3f(min.x, max.y, height);
         if(!i) PrimBuild::vertex3f( min.x, min.y, 0.f );
         PrimBuild::end();

         //
         if(i){ PrimBuild::color(a); PrimBuild::begin( GFXTriangleFan, 4 ); } else { PrimBuild::color(b); PrimBuild::begin( GFXLineStrip, 5 ); }
         PrimBuild::vertex3f(min.x, min.y, 0);
         PrimBuild::vertex3f(min.x, max.y, 0);
         PrimBuild::vertex3f(min.x, max.y, height);
         PrimBuild::vertex3f(min.x, min.y, height);
         if(!i) PrimBuild::vertex3f( min.x, min.y, 0.f );
         PrimBuild::end();

         //
         if(i){ PrimBuild::color(a); PrimBuild::begin( GFXTriangleFan, 4 ); } else { PrimBuild::color(b); PrimBuild::begin( GFXLineStrip, 5 ); }
         PrimBuild::vertex3f(max.x, min.y, 0);
         PrimBuild::vertex3f(max.x, max.y, 0);
         PrimBuild::vertex3f(max.x, max.y, height);
         PrimBuild::vertex3f(max.x, min.y, height);
         if(!i) PrimBuild::vertex3f( min.x, min.y, 0.f );
         PrimBuild::end();
      }
   }
   */
}

void TerrainEditor::submitUndo( Selection *sel )
{
   // Grab the mission editor undo manager.
   UndoManager *undoMan = NULL;
   if ( !Sim::findObject( "EUndoManager", undoMan ) )
   {
      Con::errorf( "TerrainEditor::submitUndo() - EUndoManager not found!" );
      return;     
   }

   // Create and submit the action.
   TerrainEditorUndoAction *action = new TerrainEditorUndoAction( "Terrain Editor Action" );
   action->mSel = sel;
   action->mTerrainEditor = this;
   undoMan->addAction( action );
   
   // Mark the editor as dirty!
   setDirty();
}

void TerrainEditor::TerrainEditorUndoAction::undo()
{
   // NOTE: This function also handles TerrainEditorUndoAction::redo().

   bool materialChanged = false;

   for (U32 i = 0; i < mSel->size(); i++)
   {
      // Grab the current grid info for this point.
      GridInfo info;
      mTerrainEditor->getGridInfo( (*mSel)[i].mGridPoint, info );
      info.mMaterialChanged = (*mSel)[i].mMaterialChanged;

      materialChanged |= info.mMaterialChanged;

      // Restore the previous grid info.      
      mTerrainEditor->setGridInfo( (*mSel)[i] );

      // Save the old grid info so we can 
      // restore it later.
      (*mSel)[i] = info;
   }

   // Mark the editor as dirty!
   mTerrainEditor->setDirty();
   mTerrainEditor->gridUpdateComplete( materialChanged );
}


class TerrainProcessActionEvent : public SimEvent
{
   U32 mSequence;
public:
   TerrainProcessActionEvent(U32 seq)
   {
      mSequence = seq;
   }
   void process(SimObject *object)
   {
      ((TerrainEditor *) object)->processActionTick(mSequence);
   }
};

void TerrainEditor::processActionTick(U32 sequence)
{
   if(mMouseDownSeq == sequence)
   {
      Sim::postEvent(this, new TerrainProcessActionEvent(mMouseDownSeq), Sim::getCurrentTime() + 30);
      mCurrentAction->process(mMouseBrush, mLastEvent, false, TerrainAction::Update);
   }
}

bool TerrainEditor::onInputEvent(const InputEventInfo & event)
{
   /*
   if (  mRightMousePassThru && 
         event.deviceType == KeyboardDeviceType &&
         event.objType == SI_KEY &&
         event.objInst == KEY_TAB && 
         event.action == SI_MAKE )
   {
      if ( isMethod( "onToggleToolWindows" ) )
         Con::executef( this, "onToggleToolWindows" );
   }
   */
   
   return Parent::onInputEvent( event );
}

void TerrainEditor::on3DMouseDown(const Gui3DMouseEvent & event)
{
   if(mTerrainBlocks.size() == 0)
      return;

   if (!dStrcmp(getCurrentAction(),"paintMaterial"))
   {
      Point3F pos;
      TerrainBlock* hitTerrain = collide(event, pos);

      if(!hitTerrain)
         return;

      // Set the active terrain
      bool changed = mActiveTerrain != hitTerrain;
      mActiveTerrain = hitTerrain;

      if (changed)
      {
         Con::executef(this, "onActiveTerrainChange", Con::getIntArg(hitTerrain->getId()));
         mMouseBrush->setTerrain(mActiveTerrain);
         if(mRenderBrush)
            mCursorVisible = false;
         mMousePos = pos;

         mMouseBrush->setPosition(mMousePos);
         return;
      }
   }


   mSelectionLocked = false;

   mouseLock();
   mMouseDownSeq++;
   mUndoSel = new Selection;
   mCurrentAction->process(mMouseBrush, event, true, TerrainAction::Begin);
   // process on ticks - every 30th of a second.
   Sim::postEvent(this, new TerrainProcessActionEvent(mMouseDownSeq), Sim::getCurrentTime() + 30);
}

void TerrainEditor::on3DMouseMove(const Gui3DMouseEvent & event)
{
   if(mTerrainBlocks.size() == 0)
      return;

   //if ( !isFirstResponder() )
      //setFirstResponder();

   Point3F pos;
   TerrainBlock* hitTerrain = collide(event, pos);

   if(!hitTerrain)
   {
      mMouseBrush->reset();
      mCursorVisible = true;
   }
   else
   {
      // We do not change the active terrain as the mouse moves when
      // in painting mode.  This is because it causes the material 
      // window to change as you cursor over to it.
      if ( dStrcmp(getCurrentAction(),"paintMaterial") != 0 )
      {
         // Set the active terrain
         bool changed = mActiveTerrain != hitTerrain;
         mActiveTerrain = hitTerrain;

         if (changed)
            Con::executef(this, "onActiveTerrainChange", Con::getIntArg(hitTerrain->getId()));
      }

      //
      if(mRenderBrush)
         mCursorVisible = false;
      mMousePos = pos;

      mMouseBrush->setTerrain(mActiveTerrain);
      mMouseBrush->setPosition(mMousePos);
   }  
}

void TerrainEditor::on3DMouseDragged(const Gui3DMouseEvent & event)
{
   if(mTerrainBlocks.size() == 0)
      return;

   if(!isMouseLocked())
      return;

   Point3F pos;
   if(!mSelectionLocked)
   {
      if(!collide(event, pos))
      {
         mMouseBrush->reset();
         return;
      }
   }

   // check if the mouse has actually moved in grid space
   bool selChanged = false;
   if(!mSelectionLocked)
   {
      Point2I gMouse;
      Point2I gLastMouse;
      worldToGrid(pos, gMouse);
      worldToGrid(mMousePos, gLastMouse);

      //
      mMousePos = pos;
      mMouseBrush->setPosition(mMousePos);

      selChanged = gMouse != gLastMouse;
   }
   if(selChanged)
      mCurrentAction->process(mMouseBrush, event, true, TerrainAction::Update);
}

void TerrainEditor::on3DMouseUp(const Gui3DMouseEvent & event)
{
   if(mTerrainBlocks.size() == 0)
      return;

   if(isMouseLocked())
   {
      mouseUnlock();
      mMouseDownSeq++;
      mCurrentAction->process(mMouseBrush, event, false, TerrainAction::End);
      setCursor(0);

      if(mUndoSel->size())
         submitUndo( mUndoSel );
      else
         delete mUndoSel;

      mUndoSel = 0;
      mInAction = false;
   }
}

void TerrainEditor::on3DMouseWheelDown( const Gui3DMouseEvent & event )
{
   if ( event.modifier & SI_PRIMARY_CTRL && event.modifier & SI_SHIFT )
      setBrushPressure( mBrushPressure - 0.1f );

   else if ( event.modifier & SI_SHIFT )
      setBrushSoftness( mBrushSoftness + 0.05f );

   else if ( event.modifier & SI_PRIMARY_CTRL )
   {
      Point2I newBrush = getBrushSize() - Point2I(1,1);  
      setBrushSize( newBrush.x, newBrush.y );
   }
   else if ( event.modifier & SI_SHIFT )
      setBrushSoftness( mBrushSoftness - 0.05f );
}

void TerrainEditor::on3DMouseWheelUp( const Gui3DMouseEvent & event )
{
   if ( event.modifier & SI_PRIMARY_CTRL && event.modifier & SI_SHIFT )
      setBrushPressure( mBrushPressure + 0.1f );

   else if ( event.modifier & SI_SHIFT )
      setBrushSoftness( mBrushSoftness - 0.05f );

   else if( event.modifier & SI_PRIMARY_CTRL )
   {
      Point2I newBrush = getBrushSize() + Point2I(1,1);
      setBrushSize( newBrush.x, newBrush.y );
   }
}

void TerrainEditor::getCursor(GuiCursor *&cursor, bool &visible, const GuiEvent &event)
{
   TORQUE_UNUSED(event);
   cursor = mCurrentCursor;
   visible = mCursorVisible;
}

//------------------------------------------------------------------------------
// any console function which depends on a terrainBlock attached to the editor
// should call this
bool checkTerrainBlock(TerrainEditor * object, const char * funcName)
{
   if(!object->terrainBlockValid())
   {
      Con::errorf(ConsoleLogEntry::Script, "TerrainEditor::%s: not attached to a terrain block!", funcName);
      return(false);
   }
   return(true);
}

void TerrainEditor::attachTerrain(TerrainBlock *terrBlock)
{
   mActiveTerrain = terrBlock;

   for (U32 i = 0; i < mTerrainBlocks.size(); i++)
   {
      if (mTerrainBlocks[i] == terrBlock)
         return;
   }

   mTerrainBlocks.push_back(terrBlock);
}

void TerrainEditor::detachTerrain(TerrainBlock *terrBlock)
{
   if (mActiveTerrain == terrBlock)
      mActiveTerrain = NULL; //do we want to set this to an existing terrain?

   if (mMouseBrush->getGridPoint().terrainBlock == terrBlock)
      mMouseBrush->setTerrain(NULL);

   // reset the brush as its gridinfos may still have references to the old terrain
   mMouseBrush->reset();

   for (U32 i = 0; i < mTerrainBlocks.size(); i++)
   {
      if (mTerrainBlocks[i] == terrBlock)
      {
         mTerrainBlocks.erase_fast(i);
         break;
      }
   }
}

void TerrainEditor::setBrushType( const char *type )
{
   if ( mMouseBrush && dStrcmp( mMouseBrush->getType(), type ) == 0 )
      return;

   if(!dStricmp(type, "box"))
   {
      delete mMouseBrush;
      mMouseBrush = new BoxBrush(this);
      mBrushChanged = true;
   }
   else if(!dStricmp(type, "ellipse"))
   {
      delete mMouseBrush;
      mMouseBrush = new EllipseBrush(this);
      mBrushChanged = true;
   }
   else if(!dStricmp(type, "selection"))
   {
      delete mMouseBrush;
      mMouseBrush = new SelectionBrush(this);
      mBrushChanged = true;
   }
   else {}   
}

const char* TerrainEditor::getBrushType() const
{
   if ( mMouseBrush )
      return mMouseBrush->getType();

   return "";
}

void TerrainEditor::setBrushSize( S32 w, S32 h )
{
   w = mClamp( w, 1, mMaxBrushSize.x );
   h = mClamp( h, 1, mMaxBrushSize.y );

   if ( w == mBrushSize.x && h == mBrushSize.y )
      return;

	mBrushSize.set( w, h );
   mBrushChanged = true;

   if ( mMouseBrush )
   {
   	mMouseBrush->setSize( mBrushSize );

      if ( mMouseBrush->getGridPoint().terrainBlock )
         mMouseBrush->rebuild();
   }
}

void TerrainEditor::setBrushPressure( F32 pressure )
{
   pressure = mClampF( pressure, 0.01f, 1.0f );
   
   if ( mBrushPressure == pressure )
      return;

   mBrushPressure = pressure;
   mBrushChanged = true;

   if ( mMouseBrush && mMouseBrush->getGridPoint().terrainBlock )
      mMouseBrush->rebuild();
}

void TerrainEditor::setBrushSoftness( F32 softness )
{
   softness = mClampF( softness, 0.01f, 1.0f );

   if ( mBrushSoftness == softness )
      return;

   mBrushSoftness = softness;
   mBrushChanged = true;

   if ( mMouseBrush && mMouseBrush->getGridPoint().terrainBlock )
      mMouseBrush->rebuild();
}

const char* TerrainEditor::getBrushPos()
{
   AssertFatal(mMouseBrush!=NULL, "TerrainEditor::getBrushPos: no mouse brush!");

   Point2I pos = mMouseBrush->getPosition();
   char * ret = Con::getReturnBuffer(32);
   dSprintf(ret, 32, "%d %d", pos.x, pos.y);
   return(ret);
}

void TerrainEditor::setBrushPos(Point2I pos)
{
   AssertFatal(mMouseBrush!=NULL, "TerrainEditor::setBrushPos: no mouse brush!");
   mMouseBrush->setPosition(pos);
}

void TerrainEditor::setAction(const char* action)
{
   for(U32 i = 0; i < mActions.size(); i++)
   {
      if(!dStricmp(mActions[i]->getName(), action))
      {
         mCurrentAction = mActions[i];

         //
         mRenderBrush = mCurrentAction->useMouseBrush();
         return;
      }
   }
}

const char* TerrainEditor::getActionName(U32 index)
{
   if(index >= mActions.size())
      return("");
   return(mActions[index]->getName());
}

const char* TerrainEditor::getCurrentAction() const
{
   return(mCurrentAction->getName());
}

S32 TerrainEditor::getNumActions()
{
   return(mActions.size());
}

void TerrainEditor::resetSelWeights(bool clear)
{
   //
   if(!clear)
   {
      for(U32 i = 0; i < mDefaultSel.size(); i++)
      {
         mDefaultSel[i].mPrimarySelect = false;
         mDefaultSel[i].mWeight = 1.f;
      }
      return;
   }

   Selection sel;

   U32 i;
   for(i = 0; i < mDefaultSel.size(); i++)
   {
      if(mDefaultSel[i].mPrimarySelect)
      {
         mDefaultSel[i].mWeight = 1.f;
         sel.add(mDefaultSel[i]);
      }
   }

   mDefaultSel.reset();

   for(i = 0; i < sel.size(); i++)
      mDefaultSel.add(sel[i]);
}

void TerrainEditor::clearSelection()
{
	mDefaultSel.reset();
}

void TerrainEditor::processAction(const char* sAction)
{
   if(!checkTerrainBlock(this, "processAction"))
      return;

   TerrainAction * action = mCurrentAction;
   if (dStrcmp(sAction, "") != 0)
   {
      action = lookupAction(sAction);

      if(!action)
      {
         Con::errorf(ConsoleLogEntry::General, "TerrainEditor::cProcessAction: invalid action name '%s'.", sAction);
         return;
      }
   }

   if(!getCurrentSel()->size() && !mProcessUsesBrush)
      return;

   mUndoSel = new Selection;

   Gui3DMouseEvent event;
   if(mProcessUsesBrush)
      action->process(mMouseBrush, event, true, TerrainAction::Process);
   else
      action->process(getCurrentSel(), event, true, TerrainAction::Process);

   // check if should delete the undo
   if(mUndoSel->size())
      submitUndo( mUndoSel );
   else
      delete mUndoSel;

   mUndoSel = 0;
}

S32 TerrainEditor::getNumTextures()
{
   if(!checkTerrainBlock(this, "getNumTextures"))
      return(0);

   // walk all the possible material lists and count them..
   U32 count = 0;
   for (U32 t = 0; t < mTerrainBlocks.size(); t++)
      count += mTerrainBlocks[t]->getMaterialCount();

   return count;
}

void TerrainEditor::markEmptySquares()
{
   if(!checkTerrainBlock(this, "markEmptySquares"))
      return;

   // TODO!
   /*
   // build a list of all the marked interiors
   Vector<InteriorInstance*> interiors;
   U32 mask = InteriorObjectType;
   gServerContainer.findObjects(mask, findObjectsCallback, &interiors);

   // walk the terrains and empty any grid which clips to an interior
   for (U32 i = 0; i < mTerrainBlocks.size(); i++)
   {
      for(U32 x = 0; x < TerrainBlock::BlockSize; x++)
      {
         for(U32 y = 0; y < TerrainBlock::BlockSize; y++)
         {
            TerrainBlock::Material * material = mTerrainBlocks[i]->getMaterial(x,y);
            material->flags |= ~(TerrainBlock::Material::Empty);

            Point3F a, b;
            gridToWorld(Point2I(x,y), a, mTerrainBlocks[i]);
            gridToWorld(Point2I(x+1,y+1), b, mTerrainBlocks[i]);

            Box3F box;
            box.minExtents = a;
            box.maxExtents = b;

            box.minExtents.setMin(b);
            box.maxExtents.setMax(a);

            const MatrixF & terrOMat = mTerrainBlocks[i]->getTransform();
            const MatrixF & terrWMat = mTerrainBlocks[i]->getWorldTransform();

            terrWMat.mulP(box.minExtents);
            terrWMat.mulP(box.maxExtents);

            for(U32 i = 0; i < interiors.size(); i++)
            {
               MatrixF mat = interiors[i]->getWorldTransform();
               mat.scale(interiors[i]->getScale());
               mat.mul(terrOMat);

               U32 waterMark = FrameAllocator::getWaterMark();
               U16* zoneVector = (U16*)FrameAllocator::alloc(interiors[i]->getDetailLevel(0)->getNumZones());
               U32 numZones = 0;
               interiors[i]->getDetailLevel(0)->scanZones(box, mat,
                                                          zoneVector, &numZones);
               if (numZones != 0)
               {
                  Con::printf("%d %d", x, y);
                  material->flags |= TerrainBlock::Material::Empty;
                  FrameAllocator::setWaterMark(waterMark);
                  break;
               }
               FrameAllocator::setWaterMark(waterMark);
            }
         }
      }
   }

   // rebuild stuff..
   for (U32 i = 0; i < mTerrainBlocks.size(); i++)
   {
      mTerrainBlocks[i]->buildGridMap();
      mTerrainBlocks[i]->rebuildEmptyFlags();
      mTerrainBlocks[i]->packEmptySquares();
   }
   */
}

void TerrainEditor::mirrorTerrain(S32 mirrorIndex)
{
   if(!checkTerrainBlock(this, "mirrorTerrain"))
      return;

   // TODO!
   /*
   TerrainBlock * terrain = mActiveTerrain;
   setDirty();

   //
   enum {
      top = BIT(0),
      bottom = BIT(1),
      left = BIT(2),
      right = BIT(3)
   };

   U32 sides[8] =
   {
      bottom,
      bottom | left,
      left,
      left | top,
      top,
      top | right,
      right,
      bottom | right
   };

   U32 n = TerrainBlock::BlockSize;
   U32 side = sides[mirrorIndex % 8];
   bool diag = mirrorIndex & 0x01;

   Point2I src((side & right) ? (n - 1) : 0, (side & bottom) ? (n - 1) : 0);
   Point2I dest((side & left) ? (n - 1) : 0, (side & top) ? (n - 1) : 0);
   Point2I origSrc(src);
   Point2I origDest(dest);

   // determine the run length
   U32 minStride = ((side & top) || (side & bottom)) ? n : n / 2;
   U32 majStride = ((side & left) || (side & right)) ? n : n / 2;

   Point2I srcStep((side & right) ? -1 : 1, (side & bottom) ? -1 : 1);
   Point2I destStep((side & left) ? -1 : 1, (side & top) ? -1 : 1);

   //
   U16 * heights = terrain->getHeightAddress(0,0);
   U8 * baseMaterials = terrain->getBaseMaterialAddress(0,0);
   TerrainBlock::Material * materials = terrain->getMaterial(0,0);

   // create an undo selection
   Selection * undo = new Selection;

   // walk through all the positions
   for(U32 i = 0; i < majStride; i++)
   {
      for(U32 j = 0; j < minStride; j++)
      {
         // skip the same position
         if(src != dest)
         {
            U32 si = src.x + (src.y << TerrainBlock::BlockShift);
            U32 di = dest.x + (dest.y << TerrainBlock::BlockShift);

            // add to undo selection
            GridInfo info;
            getGridInfo(dest, info, terrain);
            undo->add(info);

            //... copy info... (height, basematerial, material)
            heights[di] = heights[si];
            baseMaterials[di] = baseMaterials[si];
            materials[di] = materials[si];
         }

         // get to the new position
         src.x += srcStep.x;
         diag ? (dest.y += destStep.y) : (dest.x += destStep.x);
      }

      // get the next position for a run
      src.y += srcStep.y;
      diag ? (dest.x += destStep.x) : (dest.y += destStep.y);

      // reset the minor run
      src.x = origSrc.x;
      diag ? (dest.y = origDest.y) : (dest.x = origDest.x);

      // shorten the run length for diag runs
      if(diag)
         minStride--;
   }

   // rebuild stuff..
   terrain->buildGridMap();
   terrain->rebuildEmptyFlags();
   terrain->packEmptySquares();

   // add undo selection
   submitUndo( undo );
   */
}

//------------------------------------------------------------------------------

ConsoleMethod( TerrainEditor, attachTerrain, void, 2, 3, "(TerrainBlock terrain)")
{
   SimSet * missionGroup = dynamic_cast<SimSet*>(Sim::findObject("MissionGroup"));
   if (!missionGroup)
   {
      Con::errorf(ConsoleLogEntry::Script, "TerrainEditor::attach: no mission group found");
      return;
   }

   VectorPtr<TerrainBlock*> terrains;

   // attach to first found terrainBlock
   if (argc == 2)
   {
      for(SimSetIterator itr(missionGroup); *itr; ++itr)
      {
         TerrainBlock* terrBlock = dynamic_cast<TerrainBlock*>(*itr);

         if (terrBlock)
            terrains.push_back(terrBlock);
      }

      //if (terrains.size() == 0)
      //   Con::errorf(ConsoleLogEntry::Script, "TerrainEditor::attach: no TerrainBlock objects found!");
   }
   else  // attach to named object
   {
      TerrainBlock* terrBlock = dynamic_cast<TerrainBlock*>(Sim::findObject(argv[2]));

      if (terrBlock)
         terrains.push_back(terrBlock);

      if(terrains.size() == 0)
         Con::errorf(ConsoleLogEntry::Script, "TerrainEditor::attach: failed to attach to object '%s'", argv[2]);
   }

   if (terrains.size() > 0)
   {
      for (U32 i = 0; i < terrains.size(); i++)
      {
         if (!terrains[i]->isServerObject())
         {
            terrains[i] = NULL;

            Con::errorf(ConsoleLogEntry::Script, "TerrainEditor::attach: cannot attach to client TerrainBlock");
         }
      }
   }

   for (U32 i = 0; i < terrains.size(); i++)
   {
      if (terrains[i])
	      object->attachTerrain(terrains[i]);
   }
}

ConsoleMethod( TerrainEditor, setBrushType, void, 3, 3, "(string type)"
              "One of box, ellipse, selection.")
{
	object->setBrushType(argv[2]);
}

ConsoleMethod( TerrainEditor, getBrushType, const char*, 2, 2, "()")
{
   return object->getBrushType();
}

ConsoleMethod( TerrainEditor, setBrushSize, void, 3, 4, "(int w [, int h])")
{
   S32 w = dAtoi(argv[2]);
   S32 h = argc > 3 ? dAtoi(argv[3]) : w;
	object->setBrushSize( w, h );
}

ConsoleMethod( TerrainEditor, getBrushSize, const char*, 2, 2, "()")
{
   Point2I size = object->getBrushSize();

   char * ret = Con::getReturnBuffer(32);
   dSprintf(ret, 32, "%d %d", size.x, size.y);
   return ret;
}

ConsoleMethod( TerrainEditor, setBrushPressure, void, 3, 3, "(float pressure)")
{
   object->setBrushPressure( dAtof( argv[2] ) );
}

ConsoleMethod( TerrainEditor, getBrushPressure, F32, 2, 2, "()")
{
   return object->getBrushPressure();
}

ConsoleMethod( TerrainEditor, setBrushSoftness, void, 3, 3, "(float softness)")
{
   object->setBrushSoftness( dAtof( argv[2] ) );
}

ConsoleMethod( TerrainEditor, getBrushSoftness, F32, 2, 2, "()")
{
   return object->getBrushSoftness();
}

ConsoleMethod( TerrainEditor, getBrushPos, const char*, 2, 2, "Returns a Point2I.")
{
	return object->getBrushPos();
}

ConsoleMethod( TerrainEditor, setBrushPos, void, 3, 4, "(int x, int y)")
{
   //
   Point2I pos;
   if(argc == 3)
      dSscanf(argv[2], "%d %d", &pos.x, &pos.y);
   else
   {
      pos.x = dAtoi(argv[2]);
      pos.y = dAtoi(argv[3]);
   }

   object->setBrushPos(pos);
}

ConsoleMethod( TerrainEditor, setAction, void, 3, 3, "(string action_name)")
{
	object->setAction(argv[2]);
}

ConsoleMethod( TerrainEditor, getActionName, const char*, 3, 3, "(int num)")
{
	return (object->getActionName(dAtoi(argv[2])));
}

ConsoleMethod( TerrainEditor, getNumActions, S32, 2, 2, "")
{
	return(object->getNumActions());
}

ConsoleMethod( TerrainEditor, getCurrentAction, const char*, 2, 2, "")
{
	return object->getCurrentAction();
}

ConsoleMethod( TerrainEditor, resetSelWeights, void, 3, 3, "(bool clear)")
{
	object->resetSelWeights(dAtob(argv[2]));
}

ConsoleMethod( TerrainEditor, clearSelection, void, 2, 2, "")
{
   object->clearSelection();
}

ConsoleMethod( TerrainEditor, processAction, void, 2, 3, "(string action=NULL)")
{
	if(argc == 3)
		object->processAction(argv[2]);
	else object->processAction("");
}

ConsoleMethod( TerrainEditor, getActiveTerrain, S32, 2, 2, "")
{
   S32 ret = 0;

   TerrainBlock* terrain = object->getActiveTerrain();

   if (terrain)
      ret = terrain->getId();

	return ret;
}

ConsoleMethod( TerrainEditor, getNumTextures, S32, 2, 2, "")
{
	return object->getNumTextures();
}

ConsoleMethod( TerrainEditor, markEmptySquares, void, 2, 2, "")
{
	object->markEmptySquares();
}

ConsoleMethod( TerrainEditor, mirrorTerrain, void, 3, 3, "")
{
	object->mirrorTerrain(dAtoi(argv[2]));
}

ConsoleMethod(TerrainEditor, setTerraformOverlay, void, 3, 3, "(bool overlayEnable) - sets the terraformer current heightmap to draw as an overlay over the current terrain.")
{
   // XA: This one needs to be implemented :)
}

ConsoleMethod(TerrainEditor, updateMaterial, bool, 4, 4, 
   "( int index, string matName )\n"
   "Changes the material name at the index." )
{
   TerrainBlock *terr = object->getClientTerrain();
   if ( !terr )
      return false;
   
   U32 index = dAtoi( argv[2] );
   if ( index >= terr->getMaterialCount() )
      return false;

   terr->updateMaterial( index, argv[3] );

   object->setDirty();

   return true;
}

ConsoleMethod(TerrainEditor, addMaterial, S32, 3, 3, 
   "( string matName )\n"
   "Adds a new material." )
{
   TerrainBlock *terr = object->getClientTerrain();
   if ( !terr )
      return false;
   
   terr->addMaterial( argv[2] );

   object->setDirty();

   return true;
}

ConsoleMethod(TerrainEditor, getMaterialCount, S32, 2, 2, 
   "Returns the current material count." )
{
   TerrainBlock *terr = object->getClientTerrain();
   if ( terr )
      return terr->getMaterialCount();

   return 0;
}

ConsoleMethod(TerrainEditor, getMaterials, const char *, 2, 2, "() gets the list of current terrain materials.")
{
   TerrainBlock *terr = object->getClientTerrain();
   if ( !terr )
      return "";

   char *ret = Con::getReturnBuffer(4096);
   ret[0] = 0;
   for(U32 i = 0; i < terr->getMaterialCount(); i++)
   {
      dStrcat( ret, terr->getMaterialName(i) );
      dStrcat( ret, "\n" );
   }

   return ret;
}

ConsoleMethod(TerrainEditor, getTerrainUnderWorldPoint, S32, 3, 5, "(x/y/z) Gets the terrain block that is located under the given world point.\n"
                                                                           "@param x/y/z The world coordinates (floating point values) you wish to query at. " 
                                                                           "These can be formatted as either a string (\"x y z\") or separately as (x, y, z)\n"
                                                                           "@return Returns the ID of the requested terrain block (0 if not found).\n\n")
{   
   TerrainEditor *tEditor = (TerrainEditor *) object;
   if(tEditor == NULL)
      return 0;
   Point3F pos;
   if(argc == 3)
      dSscanf(argv[2], "%f %f %f", &pos.x, &pos.y, &pos.z);
   else if(argc == 5)
   {
      pos.x = dAtof(argv[2]);
      pos.y = dAtof(argv[3]);
      pos.z = dAtof(argv[4]);
   }

   else
   {
      Con::errorf("TerrainEditor.getTerrainUnderWorldPoint(): Invalid argument count! Valid arguments are either \"x y z\" or x,y,z\n");
      return 0;
   }

   TerrainBlock* terrain = tEditor->getTerrainUnderWorldPoint(pos);
   if(terrain != NULL)
   {
      return terrain->getId();
   }

   return 0;
}

//------------------------------------------------------------------------------

void TerrainEditor::initPersistFields()
{
   addGroup("Misc");	
   addField("isDirty", TypeBool, Offset(mIsDirty, TerrainEditor));
   addField("isMissionDirty", TypeBool, Offset(mIsMissionDirty, TerrainEditor));
   addField("renderBorder", TypeBool, Offset(mRenderBorder, TerrainEditor));                    ///< Not currently used
   addField("borderHeight", TypeF32, Offset(mBorderHeight, TerrainEditor));                     ///< Not currently used
   addField("borderFillColor", TypeColorI, Offset(mBorderFillColor, TerrainEditor));            ///< Not currently used
   addField("borderFrameColor", TypeColorI, Offset(mBorderFrameColor, TerrainEditor));          ///< Not currently used
   addField("borderLineMode", TypeBool, Offset(mBorderLineMode, TerrainEditor));                ///< Not currently used
   addField("selectionHidden", TypeBool, Offset(mSelectionHidden, TerrainEditor));
   addField("renderVertexSelection", TypeBool, Offset(mRenderVertexSelection, TerrainEditor));  ///< Not currently used
   addField("renderSolidBrush", TypeBool, Offset(mRenderSolidBrush, TerrainEditor));
   addField("processUsesBrush", TypeBool, Offset(mProcessUsesBrush, TerrainEditor));
   addField("maxBrushSize", TypePoint2I, Offset(mMaxBrushSize, TerrainEditor));

   // action values...
   addField("adjustHeightVal", TypeF32, Offset(mAdjustHeightVal, TerrainEditor));               ///< RaiseHeightAction and LowerHeightAction
   addField("setHeightVal", TypeF32, Offset(mSetHeightVal, TerrainEditor));                     ///< SetHeightAction
   addField("scaleVal", TypeF32, Offset(mScaleVal, TerrainEditor));                             ///< ScaleHeightAction
   addField("smoothFactor", TypeF32, Offset(mSmoothFactor, TerrainEditor));                     ///< SmoothHeightAction
   addField("noiseFactor", TypeF32, Offset(mNoiseFactor, TerrainEditor));                       ///< PaintNoiseAction
   addField("materialGroup", TypeS32, Offset(mMaterialGroup, TerrainEditor));                   ///< Not currently used
   addField("softSelectRadius", TypeF32, Offset(mSoftSelectRadius, TerrainEditor));             ///< SoftSelectAction
   addField("softSelectFilter", TypeString, Offset(mSoftSelectFilter, TerrainEditor));          ///< SoftSelectAction brush filtering
   addField("softSelectDefaultFilter", TypeString, Offset(mSoftSelectDefaultFilter, TerrainEditor));  ///< SoftSelectAction brush filtering
   addField("adjustHeightMouseScale", TypeF32, Offset(mAdjustHeightMouseScale, TerrainEditor)); ///< Not currently used
   addField("paintIndex", TypeS32, Offset(mPaintIndex, TerrainEditor));                         ///< PaintMaterialAction
   endGroup("Misc");

   Parent::initPersistFields();
}

ConsoleMethod( TerrainEditor, getSlopeLimitMinAngle, F32, 2, 2, 0)
{
   return object->mSlopeMinAngle;
}

ConsoleMethod( TerrainEditor, setSlopeLimitMinAngle, F32, 3, 3, 0)
{
	F32 angle = dAtof( argv[2] );	
	if ( angle < 0.0f )
		angle = 0.0f;
   if ( angle > object->mSlopeMaxAngle )
      angle = object->mSlopeMaxAngle;

	object->mSlopeMinAngle = angle;
	return angle;
}

ConsoleMethod( TerrainEditor, getSlopeLimitMaxAngle, F32, 2, 2, 0)
{
   return object->mSlopeMaxAngle;
}

ConsoleMethod( TerrainEditor, setSlopeLimitMaxAngle, F32, 3, 3, 0)
{
	F32 angle = dAtof( argv[2] );	
	if ( angle > 90.0f )
		angle = 90.0f;
   if ( angle < object->mSlopeMinAngle )
      angle = object->mSlopeMinAngle;
      
	object->mSlopeMaxAngle = angle;
	return angle;
}
