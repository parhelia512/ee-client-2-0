//-----------------------------------------------------------------------------
// Torque 3D Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/worldEditor/missionAreaEditor.h"
#include "terrain/terrData.h"
#include "sceneGraph/sceneObject.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "gui/3d/guiTSControl.h"
#include "T3D/gameConnection.h"
#include "T3D/shapeBase.h"
#include "T3D/gameFunctions.h"
#include "gfx/primBuilder.h"
#include "gfx/bitmap/gBitmap.h"
#include "gfx/gfxDrawUtil.h"

IMPLEMENT_CONOBJECT(MissionAreaEditor);

// unnamed namespace for static data
namespace {
   static const Point3F BoxNormals[] =
   {
      Point3F( 1, 0, 0),
      Point3F(-1, 0, 0),
      Point3F( 0, 1, 0),
      Point3F( 0,-1, 0),
      Point3F( 0, 0, 1),
      Point3F( 0, 0,-1)
   };

   static Point3F BoxPnts[] = {
      Point3F(0,0,0),
      Point3F(0,0,1),
      Point3F(0,1,0),
      Point3F(0,1,1),
      Point3F(1,0,0),
      Point3F(1,0,1),
      Point3F(1,1,0),
      Point3F(1,1,1)
   };

   F32 round_local(F32 val)
   {
      if(val >= 0.f)
      {
         F32 floor = mFloor(val);
         if((val - floor) >= 0.5f)
            return(floor + 1.f);
         return(floor);
      }
      else
      {
         F32 ceil = mCeil(val);
         if((val - ceil) <= -0.5f)
            return(ceil - 1.f);
         return(ceil);
      }
   }

   S32 clamp(S32 val, S32 resolution)
   {
      return(S32(round_local(F32(val) / F32(resolution))) * resolution);
   }
}


//------------------------------------------------------------------------------

MissionAreaEditor::MissionAreaEditor()
{
   //
   mMissionArea = 0;
   mTerrainBlock = 0;

   //
   mCurrentCursor = 0;
   mLastHitMode = 0;

   // field data
   mSquareBitmap = true;
   mEnableEditing = true;
   mRenderCamera = true;

   mHandleFrameColor.set(255,255,255);
   mHandleFillColor.set(0,0,0);
   mDefaultObjectColor.set(0,255,0,100);
   mWaterObjectColor.set(0,0,255,100);
   mMissionBoundsColor.set(255,0,0);
   mCameraColor.set(255,0,0);

   mEnableMirroring = false;
   mMirrorIndex = 0;
   mMirrorLineColor.set(255,0,255,128);
   mMirrorArrowColor.set(255,0,255,128);

   mBlendStateBlock = NULL;
   mSolidStateBlock = NULL;
}

//------------------------------------------------------------------------------

const RectI & MissionAreaEditor::getArea()
{
   AssertFatal(mMissionArea, "MissionAreaEditor::getArea: no MissionArea obj!");
   if(!bool(mMissionArea))
      return(MissionArea::smMissionArea);

   return(mMissionArea->getArea());
}

bool MissionAreaEditor::clampArea(RectI & area)
{
   if(!bool(mTerrainBlock))
      return(false);
//    S32 res = mTerrainBlock->getSquareSize();
//    area.point.x = clamp(area.point.x, res);
//    area.point.y = clamp(area.point.y, res);
//    area.extent.x = clamp(area.extent.x, res << 1);
//    area.extent.y = clamp(area.extent.y, res << 1);
   return(true);
}

void MissionAreaEditor::setArea(const RectI & area)
{
   AssertFatal(mMissionArea, "MissionAreaEditor::setArea: no MissionArea obj!");
   if(bool(mMissionArea))
   {
      RectI clamped = area;
      if(clampArea(clamped))
      {
         mMissionArea->setArea(clamped);
         onUpdate();
      }
   }
}

//------------------------------------------------------------------------------

void MissionAreaEditor::getCursor(GuiCursor *&cursor, bool &visible, const GuiEvent &)
{
   cursor = mCurrentCursor;
   visible = true;
}

void MissionAreaEditor::setCursor(U32 cursor)
{
   AssertFatal(cursor < NumCursors, "MissionAreaEditor::setCursor: invalid cursor");
   mCurrentCursor = mCursors[cursor];
}

//------------------------------------------------------------------------------

bool MissionAreaEditor::grabCursors()
{
   struct {
      U32 index;
      const char * name;
   } infos[] = {
      {DefaultCursor,            "DefaultCursor"         },
      {HandCursor,               "EditorHandCursor"      },
      {GrabCursor,               "EditorMoveCursor"      },
      {VertResizeCursor,         "EditorUpDownCursor"    },
      {HorizResizeCursor,        "EditorLeftRightCursor" },
      {DiagRightResizeCursor,    "EditorDiagRightCursor" },
      {DiagLeftResizeCursor,     "EditorDiagLeftCursor"  }
   };

   for(U32 i = 0; i < (sizeof(infos) / sizeof(infos[0])); i++)
   {
      SimObject * obj = Sim::findObject(infos[i].name);
      if(!obj)
      {
         Con::errorf(ConsoleLogEntry::Script, "MissionAreaEditor::grabCursors: failed to find cursor '%s'.", infos[i].name);
         return(false);
      }

      GuiCursor *cursor = dynamic_cast<GuiCursor*>(obj);
      if(!cursor)
      {
         Con::errorf(ConsoleLogEntry::Script, "MissionAreaEditor::grabCursors: object is not a cursor '%s'.", infos[i].name);
         return(false);
      }
      mCursors[infos[i].index] = cursor;
   }

   mCurrentCursor = mCursors[DefaultCursor];
   return(true);
}

//------------------------------------------------------------------------------

TerrainBlock * MissionAreaEditor::getTerrainObj()
{
   SimSet * scopeAlwaysSet = Sim::getGhostAlwaysSet();
   for(SimSet::iterator itr = scopeAlwaysSet->begin(); itr != scopeAlwaysSet->end(); itr++)
   {
      TerrainBlock * terrain = dynamic_cast<TerrainBlock*>(*itr);
      if(terrain)
         return(terrain);
   }
   return(0);
}

//------------------------------------------------------------------------------

GBitmap * MissionAreaEditor::createTerrainBitmap()
{
   // TODO: What is this for?
   /*
   GBitmap * bitmap = new GBitmap(TerrainBlock::BlockSize, TerrainBlock::BlockSize, false, GFXFormatR8G8B8 );
   if(!bitmap)
      return(0);

   U8 * pBits = bitmap->getAddress(0,0);

   // get the min/max
   GridSquare * gSquare = mTerrainBlock->findSquare(TerrainBlock::BlockShift, Point2I(0,0));

   F32 min = fixedToFloat(gSquare->minHeight);
   F32 max = fixedToFloat(gSquare->maxHeight);
   F32 diff = max - min;

   // This method allocates it's bitmap above, and does all assignment
   // in the following loop. It is not subject to 24-bit -> 32-bit conversion
   // problems, because the texture handle creation is where the conversion would
   // occur, if it occurs. Since the data in the texture is never read back, and
   // the bitmap is deleted after texture-upload, this is not a problem.
   for(U32 y = 0; y < TerrainBlock::BlockSize; y++)
      for(U32 x = 0; x < TerrainBlock::BlockSize; x++)
      {
         F32 height = fixedToFloat(mTerrainBlock->getHeight(x, y));

         U8 col = U8((height - min) / diff * 255.f);
         *pBits++ = col;
         *pBits++ = col;
         *pBits++ = col;
      }

   return(bitmap);
   */


   return NULL;
}

//------------------------------------------------------------------------------

bool MissionAreaEditor::onAdd()
{
   if(!Parent::onAdd())
      return(false);
   if(!grabCursors())
      return(false);

   GFXStateBlockDesc desc;
   desc.setBlend(true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
   mBlendStateBlock = GFX->createStateBlock( desc );

   desc.setBlend(false, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
   mSolidStateBlock = GFX->createStateBlock( desc );

   return(true);
}

//------------------------------------------------------------------------------

void MissionAreaEditor::updateTerrainBitmap()
{
   GBitmap * bitmap = createTerrainBitmap();
   if( bitmap )
      setBitmapHandle( GFXTexHandle( bitmap, &GFXDefaultGUIProfile, true, String("Terrain Bitmap Update") ) );
}

bool MissionAreaEditor::onWake()
{
   if(!Parent::onWake())
      return(false);

   mMissionArea = const_cast<MissionArea*>(MissionArea::getServerObject());
   if(!bool(mMissionArea))
      Con::warnf(ConsoleLogEntry::General, "MissionAreaEditor::onWake: no MissionArea object.");

   mTerrainBlock = getTerrainObj();
   if(!bool(mTerrainBlock))
      Con::warnf(ConsoleLogEntry::General, "MissionAreaEditor::onWake: no TerrainBlock object.");

   if ( !bool(mMissionArea) || !bool(mTerrainBlock) )
      return true;

   updateTerrainBitmap();

   // make sure mission area is clamped
   setArea(getArea());

   onUpdate();
   setActive(true);

   return(true);
}

void MissionAreaEditor::onSleep()
{
   mTextureObject = NULL;
   mMissionArea = 0;
   mTerrainBlock = 0;

   Parent::onSleep();
}

//------------------------------------------------------------------------------

void MissionAreaEditor::onUpdate()
{
   if(!bool(mMissionArea))
      return;

   char buf[48];

   const RectI & area = mMissionArea->getArea();
   dSprintf(buf, sizeof(buf), "%d %d %d %d", area.point.x, area.point.y, area.extent.x, area.extent.y);
   Con::executef(this, "onUpdate", buf);
}

void MissionAreaEditor::parentResized(const Point2I & oldParentExtent, const Point2I & newParentExtent)
{
   static Point2I offset = (oldParentExtent - getPosition()) - getExtent();
   resize(getPosition(), newParentExtent - getPosition() - offset);
}


//------------------------------------------------------------------------------

Point2F MissionAreaEditor::worldToScreen(const Point2F & pos)
{
   return(Point2F(mCenterPos.x + (pos.x * mScale.x), mCenterPos.y + (pos.y * mScale.y)));
}

Point2I MissionAreaEditor::worldToScreen(const Point2I &pos)
{
   return(Point2I(S32(mCenterPos.x + (pos.x * mScale.x)), S32(mCenterPos.y + (pos.y * mScale.y))));
}

Point2F MissionAreaEditor::screenToWorld(const Point2F & pos)
{
   return(Point2F((pos.x - mCenterPos.x) / mScale.x, (pos.y - mCenterPos.y) / mScale.y));
}

Point2I MissionAreaEditor::screenToWorld(const Point2I &pos)
{
   return(Point2I(S32((pos.x - mCenterPos.x) / mScale.x), S32((pos.y - mCenterPos.y) / mScale.y)));
}

//------------------------------------------------------------------------------

void MissionAreaEditor::getScreenMissionArea(RectI & rect)
{
   RectI area = mMissionArea->getArea();
   Point2F pos = worldToScreen(Point2F(F32(area.point.x), F32(area.point.y)));
   Point2F end = worldToScreen(Point2F(F32(area.point.x + area.extent.x), F32(area.point.y + area.extent.y)));

   //
   rect.point.x = S32(round_local(pos.x));
   rect.point.y = S32(round_local(pos.y));
   rect.extent.x = S32(round_local(end.x - pos.x));
   rect.extent.y = S32(round_local(end.y - pos.y));
}

void MissionAreaEditor::getScreenMissionArea(RectF & rect)
{
   RectI area = mMissionArea->getArea();
   Point2F pos = worldToScreen(Point2F(F32(area.point.x), F32(area.point.y)));
   Point2F end = worldToScreen(Point2F(F32(area.point.x + area.extent.x), F32(area.point.y + area.extent.y)));

   //
   rect.point.x = pos.x;
   rect.point.y = pos.y;
   rect.extent.x = end.x - pos.x;
   rect.extent.y = end.y - pos.y;
}

//------------------------------------------------------------------------------

void MissionAreaEditor::setupScreenTransform(const Point2I & offset)
{
   const MatrixF & terrMat = mTerrainBlock->getTransform();
   Point3F terrPos;
   terrMat.getColumn(3, &terrPos);
   terrPos.z = 0;

   F32 terrDim = mTerrainBlock->getWorldBlockSize();

   const Point2I& extenti = getExtent( );
   Point2F extent( static_cast<F32>( extenti.x ), static_cast<F32>( extenti.y ) );

   if(mSquareBitmap)
      extent.x > extent.y ? extent.x = extent.y : extent.y = extent.x;

   //
   mScale.set(extent.x / terrDim, extent.y / terrDim, 0);

   Point3F terrOffset = -terrPos;
   terrOffset.convolve(mScale);

   //
   mCenterPos.set(terrOffset.x + F32(offset.x), terrOffset.y + F32(offset.y));
}

//------------------------------------------------------------------------------

static void findObjectsCallback(SceneObject* obj, void * val)
{
   Vector<SceneObject*> * list = (Vector<SceneObject*>*)val;
   list->push_back(obj);
}

void MissionAreaEditor::onRender(Point2I offset, const RectI & updateRect)
{

   RectI rect(offset, getExtent());

   setUpdate();

   GFX->setStateBlock(mSolidStateBlock);

   // draw an x
   if(!bool(mMissionArea) || !bool(mTerrainBlock))
   {
      PrimBuild::color3i( 0, 0, 0 );
      PrimBuild::begin( GFXLineList, 4 );

      PrimBuild::vertex2f( rect.point.x + GFX->getFillConventionOffset(), updateRect.point.y + GFX->getFillConventionOffset() );
      PrimBuild::vertex2f( rect.point.x + updateRect.extent.x + GFX->getFillConventionOffset(), updateRect.point.y + updateRect.extent.y + GFX->getFillConventionOffset() );
      PrimBuild::vertex2f( rect.point.x + GFX->getFillConventionOffset(), updateRect.point.y + updateRect.extent.y + GFX->getFillConventionOffset() );
      PrimBuild::vertex2f( rect.point.x + updateRect.extent.x + GFX->getFillConventionOffset(), updateRect.point.y + GFX->getFillConventionOffset() );

      PrimBuild::end();

      return;
   }

   //
   setupScreenTransform(offset);

   // draw the terrain
   if(mSquareBitmap)
      rect.extent.x > rect.extent.y ? rect.extent.x = rect.extent.y : rect.extent.y = rect.extent.x;

   GFXDrawUtil *drawer = GFX->getDrawUtil();
   drawer->clearBitmapModulation();
   drawer->drawBitmapStretch(mTextureObject, rect);

   // draw all the objects
   Vector<SceneObject*> objects;
   U32 mask = PlayerObjectType | VehicleObjectType | StaticShapeObjectType | WaterObjectType | TriggerObjectType;
   gServerContainer.findObjects(mask, findObjectsCallback, &objects);

   RectF area;
   getScreenMissionArea(area);

   // render the mission area box
   PrimBuild::color( mMissionBoundsColor );
   PrimBuild::begin( GFXLineStrip, 5 );
      PrimBuild::vertex2f(area.point.x + GFX->getFillConventionOffset(), area.point.y + GFX->getFillConventionOffset());
      PrimBuild::vertex2f(area.point.x + area.extent.x + GFX->getFillConventionOffset(), area.point.y + GFX->getFillConventionOffset());
      PrimBuild::vertex2f(area.point.x + area.extent.x + GFX->getFillConventionOffset(), area.point.y + area.extent.y + GFX->getFillConventionOffset());
      PrimBuild::vertex2f(area.point.x + GFX->getFillConventionOffset(), area.point.y + area.extent.y + GFX->getFillConventionOffset());
      PrimBuild::vertex2f(area.point.x + GFX->getFillConventionOffset(), area.point.y + GFX->getFillConventionOffset());
   PrimBuild::end();

   // render the handles
   RectI iArea;
   getScreenMissionArea(iArea);
   if(mEnableEditing && !mEnableMirroring)
      drawNuts(iArea);

   // render the camera
   if(mRenderCamera)
   {
      CameraQuery camera;
      GameProcessCameraQuery(&camera);

      // farplane too far, 90' looks wrong...
      camera.fov = mDegToRad(60.f);
      camera.farPlane = 500.f;

      //
      F32 rot = camera.fov / 2;

      //
      VectorF ray;
      VectorF projRayA, projRayB;

      ray.set(camera.farPlane * -mSin(rot), camera.farPlane * mCos(rot), 0);
      camera.cameraMatrix.mulV(ray, &projRayA);

      ray.set(camera.farPlane * -mSin(-rot), camera.farPlane * mCos(-rot), 0);
      camera.cameraMatrix.mulV(ray, &projRayB);

      Point3F camPos;
      camera.cameraMatrix.getColumn(3, &camPos);

      Point2F s = worldToScreen(Point2F(camPos.x, camPos.y));
      Point2F e1 = worldToScreen(Point2F(camPos.x + projRayA.x, camPos.y + projRayA.y));
      Point2F e2 = worldToScreen(Point2F(camPos.x + projRayB.x, camPos.y + projRayB.y));

      PrimBuild::color( mCameraColor );
      PrimBuild::begin( GFXLineList, 4 );
         PrimBuild::vertex2f( s.x + GFX->getFillConventionOffset(), s.y + GFX->getFillConventionOffset() );
         PrimBuild::vertex2f( e1.x + GFX->getFillConventionOffset(), e1.y + GFX->getFillConventionOffset() );
         PrimBuild::vertex2f( s.x + GFX->getFillConventionOffset(), s.y + GFX->getFillConventionOffset() );
         PrimBuild::vertex2f( e2.x + GFX->getFillConventionOffset(), e2.y + GFX->getFillConventionOffset() );
      PrimBuild::end();
   }

   // draw the mirroring info
   if(mEnableMirroring)
   {
      // mirror index is cw octant of source
      static Point2F octPoints[] =
      {
         Point2F(0.5, 0.0),
         Point2F(1.0, 0.0),
         Point2F(1.0, 0.5),
         Point2F(1.0, 1.0),
         Point2F(0.5, 1.0),
         Point2F(0.0, 1.0),
         Point2F(0.0, 0.5),
         Point2F(0.0, 0.0)
      };

      // render the line
      PrimBuild::color( mMirrorLineColor );
      PrimBuild::begin( GFXLineList, 2 );
         PrimBuild::vertex2f(
            ( rect.point.x + octPoints[(mMirrorIndex+6)%8].x * rect.extent.x ) + GFX->getFillConventionOffset(),
            ( rect.point.y + octPoints[(mMirrorIndex+6)%8].y * rect.extent.y ) + GFX->getFillConventionOffset() );
         PrimBuild::vertex2f(
            ( rect.point.x + octPoints[(mMirrorIndex+2)%8].x * rect.extent.x ) + GFX->getFillConventionOffset(), 
            ( rect.point.y + octPoints[(mMirrorIndex+2)%8].y * rect.extent.y ) + GFX->getFillConventionOffset() );
      PrimBuild::end();

      // render the arrow
      static Point2F arrow[8] = // points up
      {
         Point2F(-0.375, 0),
         Point2F(0, -0.375),
         Point2F(0.375, 0),
         Point2F(0.125, 0),
         Point2F(0.125, 0.375),
         Point2F(-0.125, 0.375),
         Point2F(-0.125, 0),
         Point2F(-0.375, 0)
      };

      static U32 arrow_tri[15] = // triangle verts
      {
         0, 1, 6,
         6, 1, 3,
         3, 1, 2,
         6, 3, 5,
         3, 4, 5
      };

      // rotate cw
      F32 angle = -(M_PI * ((mMirrorIndex+6) % 8) / 4);

      F32 sin = mCos(angle);
      F32 cos = mSin(angle);

      // rotate points..
      Point2F pnts[8];
      U32 i;
      for(i = 0; i < 8; i++)
      {
         pnts[i].x = arrow[i].x * cos - arrow[i].y * sin;
         pnts[i].y = arrow[i].x * sin + arrow[i].y * cos;
      }

      // draw it
      GFX->setStateBlock(mBlendStateBlock);

      PrimBuild::color( mMirrorArrowColor );
      PrimBuild::begin( GFXTriangleList, 15 );
      for(i = 0; i < 15; i++)
         PrimBuild::vertex2f(
            rect.point.x + pnts[arrow_tri[i]].x * rect.extent.x + (rect.extent.x / 2) + GFX->getFillConventionOffset(),
            rect.point.y + pnts[arrow_tri[i]].y * rect.extent.y + (rect.extent.y / 2) + GFX->getFillConventionOffset() );
      PrimBuild::end();

      // opaque
      PrimBuild::color4i( mMirrorArrowColor.red, mMirrorArrowColor.green, mMirrorArrowColor.blue, 0xff );
      PrimBuild::begin( GFXLineStrip, 8 );
      for(i = 0; i < 8; i++)
         PrimBuild::vertex2f(
            rect.point.x + pnts[i].x * rect.extent.x + (rect.extent.x / 2) + GFX->getFillConventionOffset(),
            rect.point.y + pnts[i].y * rect.extent.y + (rect.extent.y / 2) + GFX->getFillConventionOffset() );
      PrimBuild::end();
   }

   renderChildControls(offset, updateRect);
}

//------------------------------------------------------------------------------
// sometimes you feel like a.....
bool MissionAreaEditor::inNut(const Point2I & pt, S32 x, S32 y)
{
   S32 dx = pt.x - x;
   S32 dy = pt.y - y;
   return dx <= NUT_SIZE && dx >= -NUT_SIZE && dy <= NUT_SIZE && dy >= -NUT_SIZE;
}

S32 MissionAreaEditor::getSizingHitKnobs(const Point2I & pt, const RectI & box)
{
   if(!mEnableEditing || mEnableMirroring)
      return(nothing);

   S32 lx = box.point.x, rx = box.point.x + box.extent.x - 1;
   S32 cx = (lx + rx) >> 1;
   S32 ty = box.point.y, by = box.point.y + box.extent.y - 1;
   S32 cy = (ty + by) >> 1;

   if (inNut(pt, lx, ty))
      return sizingLeft | sizingTop;
   if (inNut(pt, cx, ty))
      return sizingTop;
   if (inNut(pt, rx, ty))
      return sizingRight | sizingTop;
   if (inNut(pt, lx, by))
      return sizingLeft | sizingBottom;
   if (inNut(pt, cx, by))
      return sizingBottom;
   if (inNut(pt, rx, by))
      return sizingRight | sizingBottom;
   if (inNut(pt, lx, cy))
      return sizingLeft;
   if (inNut(pt, rx, cy))
      return sizingRight;
   if(pt.x >= box.point.x && pt.x < box.point.x + box.extent.x &&
      pt.y >= box.point.y && pt.y < box.point.y + box.extent.y)
      return(moving);
   return nothing;
}

void MissionAreaEditor::drawNut(const Point2I & nut)
{
   RectI r(nut.x - NUT_SIZE, nut.y - NUT_SIZE, 2 * NUT_SIZE + 1, 2 * NUT_SIZE + 1);
   GFX->getDrawUtil()->drawRect(r, mHandleFrameColor);
   r.point += Point2I(1, 1);
   r.extent -= Point2I(1, 1);
   GFX->getDrawUtil()->drawRectFill(r, mHandleFillColor);
}

void MissionAreaEditor::drawNuts(RectI & box)
{
   S32 lx = box.point.x, rx = box.point.x + box.extent.x - 1;
   S32 cx = (lx + rx) >> 1;
   S32 ty = box.point.y, by = box.point.y + box.extent.y - 1;
   S32 cy = (ty + by) >> 1;
   drawNut(Point2I(lx, ty));
   drawNut(Point2I(lx, cy));
   drawNut(Point2I(lx, by));
   drawNut(Point2I(rx, ty));
   drawNut(Point2I(rx, cy));
   drawNut(Point2I(rx, by));
   drawNut(Point2I(cx, ty));
   drawNut(Point2I(cx, by));
}

//------------------------------------------------------------------------------

void MissionAreaEditor::updateCursor(S32 hit)
{
   if(hit)
   {
      if(hit == sizingTop || hit == sizingBottom)
         setCursor(VertResizeCursor);
      else if(hit == sizingLeft || hit == sizingRight)
         setCursor(HorizResizeCursor);
      else if(hit & sizingTop)
      {
         if(hit & sizingLeft)
            setCursor(DiagLeftResizeCursor);
         else
            setCursor(DiagRightResizeCursor);
      }
      else if(hit & sizingBottom)
      {
         if(hit & sizingLeft)
            setCursor(DiagRightResizeCursor);
         else
            setCursor(DiagLeftResizeCursor);
      }
      else if(hit == moving)
         setCursor(HandCursor);
   }
   else
      setCursor(DefaultCursor);
}

//------------------------------------------------------------------------------

void MissionAreaEditor::onMouseUp(const GuiEvent & event)
{
   if(!bool(mMissionArea))
      return;

   RectI box;
   getScreenMissionArea(box);
   S32 hit = getSizingHitKnobs(event.mousePoint, box);

   // set the current cursor
   updateCursor(hit);
   mLastHitMode = hit;
}

void MissionAreaEditor::onMouseDown(const GuiEvent & event)
{
   if(!bool(mMissionArea))
      return;

   if(!mEnableEditing || mEnableMirroring)
   {
      Point2F pos = screenToWorld(Point2F(F32(event.mousePoint.x), F32(event.mousePoint.y)));
      setControlObjPos(pos);
      return;
   }

   RectI box;
   getScreenMissionArea(box);

   mLastHitMode = getSizingHitKnobs(event.mousePoint, box);
   if(mLastHitMode == moving)
      setCursor(GrabCursor);
   mLastMousePoint = event.mousePoint;
}

void MissionAreaEditor::onMouseMove(const GuiEvent & event)
{
   if(!bool(mMissionArea))
      return;

   RectI box;
   getScreenMissionArea(box);
   S32 hit = getSizingHitKnobs(event.mousePoint, box);

   // set the current cursor...
   updateCursor(hit);
   mLastHitMode = hit;
}

// update the mission area here...
void MissionAreaEditor::onMouseDragged(const GuiEvent & event)
{
   if(!bool(mMissionArea))
      return;

   if(!mLastHitMode)
      return;

   RectI box;
   getScreenMissionArea(box);
   Point2I mouseDiff(event.mousePoint.x - mLastMousePoint.x,
      event.mousePoint.y - mLastMousePoint.y);

   // what we drag'n?
   if(mLastHitMode == moving)
      box.point += mouseDiff;
   else
   {
      // dont allow the box to be < 1x1 'pixels'
      if(mLastHitMode & sizingLeft)
      {
         if(mouseDiff.x >= box.extent.x)
            mouseDiff.x = box.extent.x - 1;

         box.point.x += mouseDiff.x;
         box.extent.x -= mouseDiff.x;
      }

      if(mLastHitMode & sizingRight)
      {
         if(mouseDiff.x + box.extent.x <= 0)
            mouseDiff.x = -(box.extent.x - 1);
         box.extent.x += mouseDiff.x;
      }

      if(mLastHitMode & sizingTop)
      {
         if(mouseDiff.y >= box.extent.y)
            mouseDiff.y = box.extent.y - 1;

         box.point.y += mouseDiff.y;
         box.extent.y -= mouseDiff.y;
      }

      if(mLastHitMode & sizingBottom)
      {
         if(mouseDiff.y + box.extent.y <= 0)
            mouseDiff.y = -(box.extent.y - 1);
         box.extent.y += mouseDiff.y;
      }
   }

   //
   Point2I min = screenToWorld(box.point);
   Point2I max = screenToWorld(box.point + box.extent);

   RectI iBox(min.x, min.y, max.x - min.x, max.y - min.y);
   setArea(iBox);

   mLastMousePoint = event.mousePoint;
}

void MissionAreaEditor::onMouseEnter(const GuiEvent &)
{
   mLastHitMode = nothing;
   setCursor(DefaultCursor);
}

void MissionAreaEditor::onMouseLeave(const GuiEvent &)
{
   mLastHitMode = nothing;
   setCursor(DefaultCursor);
}

//------------------------------------------------------------------------------

void MissionAreaEditor::setControlObjPos(const Point2F & pos)
{
   GameConnection * connection = GameConnection::getLocalClientConnection();

   ShapeBase * obj = 0;
   if(connection)
      obj = dynamic_cast<ShapeBase*>(connection->getControlObject());

   if(!obj)
   {
      Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::setControlObjPos: could not get a control object!");
      return;
   }

   // move it
   MatrixF mat = obj->getTransform();

   Point3F current;
   mat.getColumn(3, &current);

   //
   if(bool(mTerrainBlock))
   {
      F32 height;
      mTerrainBlock->getHeight(pos, &height);
      if(current.z < height)
         current.z = height + 10.f;
   }

   //
   current.set(pos.x, pos.y, current.z);
   mat.setColumn(3, current);
   obj->setTransform(mat);
}


//------------------------------------------------------------------------------
// the following globals allocations needed for cCenterWorld are allocated when first refd.
//static U16 *heights = NULL;
//static U8 *baseMaterials = NULL;
//static TerrainBlock::Material *materials = NULL;
//static bool cwAllocs = false;

//------------------------------------------------------------------------------
ConsoleMethod( MissionAreaEditor, centerWorld, void, 2, 2, "Realign the world so that the mission area is centered.\n\n"
              "This method moves every SceneObject (including terrain) in the world so that the center of the world is "
              "the center of the mission area.")
{
   // TODO: Fix me when terrain changes are finished!
   /*
   if(!object->missionAreaObjValid())
   {
      Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cCenterWorld: no MissionArea obj!");
      return;
   }

   //
   SimSet * missionGroup = dynamic_cast<SimSet*>(Sim::findObject("missionGroup"));
   if(!missionGroup)
   {
      Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cCenterWorld: no mission group found!");
      return;
   }

	// make sure we're allocated.
	// !!!!!TBD -- NOTE THAT THIS IS LEAKED ON EXIT!
	if (!cwAllocs)
	{
		U32 allocSize = TerrainBlock::BlockSize * TerrainBlock::BlockSize;
		heights = new U16[allocSize];
		if (NULL==heights)
		{
         Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cCenterWorld: out of memory!");
         return;
		}
		baseMaterials = new U8[allocSize];
		if (NULL==baseMaterials)
		{
         Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cCenterWorld: out of memory!");
         return;
		}
		materials = new TerrainBlock::Material[allocSize];
		if (NULL==materials)
		{
         Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cCenterWorld: out of memory!");
         return;
		}
		cwAllocs = true;
	}

   // make sure area is clamped to terrain square size!
   object->setArea(object->getArea());
   RectI area = object->getArea();

   // calc offset
   Point2I offset(area.point.x + (area.extent.x >> 1),
                  area.point.y + (area.extent.y >> 1));

   if(!offset.x || !offset.y)
      return;

   Point3F offset3F(F32(offset.x), F32(offset.y), 0.0f);

   // update all the scene objects
   for(SimSetIterator itr(missionGroup); *itr; ++itr)
   {
      SceneObject * obj = dynamic_cast<SceneObject*>(*itr);
      if(!obj)
         continue;

      // terrain is handled special like (mission area is forced to align to
      // terrain square sizes because terrain cannot move)
      TerrainBlock * terrain = dynamic_cast<TerrainBlock*>(*itr);
      if(terrain)
      {
         // get the new start location in gridSquare space
         Point2I start((offset.x / (S32) terrain->getSquareSize()) & TerrainBlock::BlockMask,
                       (offset.y / (S32) terrain->getSquareSize()) & TerrainBlock::BlockMask);

         // update the grid block
         if(start.x || start.y)
         {
            for(U32 y = 0; y < TerrainBlock::BlockSize; y++)
               for(U32 x = 0; x < TerrainBlock::BlockSize; x++)
               {
                  Point2I pos((start.x + x) & TerrainBlock::BlockMask,
                              (start.y + y) & TerrainBlock::BlockMask);

                  heights[x + y * TerrainBlock::BlockSize] = terrain->getHeight(pos.x, pos.y);
                  baseMaterials[x + y * TerrainBlock::BlockSize] = terrain->getBaseMaterial(pos.x, pos.y);
                  materials[x + y * TerrainBlock::BlockSize] = *terrain->getMaterial(pos.x, pos.y);
               }

            U16 * heightsAddr = terrain->getHeightAddress(0,0);
            U8 * baseMaterialsAddr = terrain->getBaseMaterialAddress(0,0);
            TerrainBlock::Material * materialsAddr = terrain->getMaterial(0,0);

            dMemcpy(heightsAddr, heights, sizeof(heights));
            dMemcpy(baseMaterialsAddr, baseMaterials, sizeof(baseMaterials));
            dMemcpy(materialsAddr, materials, sizeof(materials));

            terrain->buildGridMap();
            terrain->rebuildEmptyFlags();
            terrain->packEmptySquares();
         }

         object->updateTerrainBitmap();
      }
      else
      {
         MatrixF objMat = obj->getTransform();
         Point3F pos;
         objMat.getColumn(3, &pos);

         pos -= offset3F;
         objMat.setColumn(3, pos);

         obj->setTransform(objMat);
      }
   }

   char buf[64];
   dSprintf(buf, sizeof(buf), "%g %g %g", -offset3F.x, -offset3F.y, -offset3F.z);
   Con::executef(object, "onWorldOffset", buf);

   // move the mission area
   area.point.x -= offset.x;
   area.point.y -= offset.y;

   object->setArea(area);
   */
}

//------------------------------------------------------------------------------

ConsoleMethod(MissionAreaEditor, getArea, const char *, 2, 2, "Return a 4-tuple: area_x area_y area_width are_height")
{
   if(!object->missionAreaObjValid())
   {
      Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cGetArea: no MissionArea obj!");
      return("");
   }

   //
   RectI area = object->getArea();
   char * ret = Con::getReturnBuffer(64);
   dSprintf(ret, 64, "%d %d %d %d", area.point.x, area.point.y, area.extent.x, area.extent.y);

   return(ret);
}

ConsoleMethod( MissionAreaEditor, setArea, void, 3, 6, "(int x, int y, int w, int h)"
              "Set the mission area to the specified co-ordinates/extents.")
{
   if(!object->missionAreaObjValid())
   {
      Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cSetArea: no MissionArea obj!");
      return;
   }

   RectI area;

   //
   if(argc == 3)
      dSscanf(argv[2], "%d %d %d %d", &area.point.x, &area.point.y, &area.extent.x, &area.extent.y);
   else if(argc == 6)
   {
      area.point.x = dAtoi(argv[2]);
      area.point.y = dAtoi(argv[3]);
      area.extent.x = dAtoi(argv[4]);
      area.extent.y = dAtoi(argv[5]);
   }
   else
      Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cSetArea: invalid number of arguments!");

   //
   object->setArea(area);
}

ConsoleMethod( MissionAreaEditor, updateTerrain, void, 2, 2, "Update the terrain bitmap that is rendered as background in the control.")
{
   //
   if(!object->getTerrainObj())
   {
      Con::errorf(ConsoleLogEntry::General, "MissionAreaEditor::cUpdateTerrain: no terrain found!");
      return;
   }

   //
   object->updateTerrainBitmap();
}

//------------------------------------------------------------------------------

void MissionAreaEditor::initPersistFields()
{
   addGroup("Mirror");	
   addField("enableMirroring", TypeBool, Offset(mEnableMirroring, MissionAreaEditor));
   addField("mirrorIndex", TypeS32, Offset(mMirrorIndex, MissionAreaEditor));
   addField("mirrorLineColor", TypeColorI, Offset(mMirrorLineColor, MissionAreaEditor));
   addField("mirrorArrowColor", TypeColorI, Offset(mMirrorArrowColor, MissionAreaEditor));
   endGroup("Mirror");	

   addGroup("Misc");	
   addField("handleFrameColor", TypeColorI, Offset(mHandleFrameColor, MissionAreaEditor));
   addField("handleFillColor", TypeColorI, Offset(mHandleFillColor, MissionAreaEditor));
   addField("defaultObjectColor", TypeColorI, Offset(mDefaultObjectColor, MissionAreaEditor));
   addField("waterObjectColor", TypeColorI, Offset(mWaterObjectColor, MissionAreaEditor));
   addField("missionBoundsColor", TypeColorI, Offset(mMissionBoundsColor, MissionAreaEditor));
   addField("cameraColor", TypeColorI, Offset(mCameraColor, MissionAreaEditor));
   addField("squareBitmap", TypeBool, Offset(mSquareBitmap, MissionAreaEditor));
   addField("enableEditing", TypeBool, Offset(mEnableEditing, MissionAreaEditor));
   addField("renderCamera", TypeBool, Offset(mRenderCamera, MissionAreaEditor));
   endGroup("Misc");

   Parent::initPersistFields();
}
