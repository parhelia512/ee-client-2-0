//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gfx/sim/debugDraw.h"

#include "gfx/gFont.h"
#include "gfx/gfxDrawUtil.h"
#include "gfx/gfxTransformSaver.h"
#include "math/mathUtils.h"
#include "math/util/frustum.h"
#include "console/console.h"
#include "sceneGraph/sceneGraph.h"


DebugDrawer* DebugDrawer::sgDebugDrawer = NULL;

IMPLEMENT_CONOBJECT(DebugDrawer);

DebugDrawer::DebugDrawer()
{
   mHead = NULL;
   isFrozen = false;
   shouldToggleFreeze = false;
   
#ifdef ENABLE_DEBUGDRAW
   isDrawing = true;
#else
   isDrawing = false;
#endif
}

DebugDrawer::~DebugDrawer()
{
}

DebugDrawer* DebugDrawer::get()
{
   if (sgDebugDrawer)
   {   
      return sgDebugDrawer;
   } else {
      DebugDrawer::init();
      return sgDebugDrawer;
   }
}

void DebugDrawer::init()
{
#ifdef ENABLE_DEBUGDRAW
   sgDebugDrawer = new DebugDrawer();
   sgDebugDrawer->registerObject("DebugDraw");

   #ifndef TORQUE_DEBUG
      Con::errorf("===============================================================");
      Con::errorf("=====  WARNING! DEBUG DRAWER ENABLED!                       ===");
      Con::errorf("=====       Turn me off in final build, thanks.             ===");
      Con::errorf("=====        I will draw a gross line to get your attention.===");
      Con::errorf("=====                                      -- BJG           ===");
      Con::errorf("===============================================================");

      // You can disable this code if you know what you're doing. Just be sure not to ship with
      // DebugDraw enabled! 
      
      // DebugDraw can be used for all sorts of __cheats__ and __bad things__.
      sgDebugDrawer->drawLine(Point3F(-10000, -10000, -10000), Point3F(10000, 10000, 10000), ColorF(1, 0, 0));
      sgDebugDrawer->setLastTTL(15 * 60 * 1000);
   #else
      sgDebugDrawer->drawLine(Point3F(-10000, -10000, -10000), Point3F(10000, 10000, 10000), ColorF(0, 1, 0));
      sgDebugDrawer->setLastTTL(30*1000);
   #endif   
#endif

}

void DebugDrawer::setupStateBlocks()
{
   GFXStateBlockDesc d;

   d.setCullMode(GFXCullNone);
   mRenderZOnSB = GFX->createStateBlock(d);
   
   d.setZReadWrite(false);
   mRenderZOffSB = GFX->createStateBlock(d);
}

void DebugDrawer::render()
{
#ifdef ENABLE_DEBUGDRAW
   if(!isDrawing)
      return;

   if (!mRenderZOnSB)
   {
      setupStateBlocks();
      String fontCacheDir = Con::getVariable("$GUI::fontCacheDirectory");
      mFont = GFont::create("Arial", 12, fontCacheDir);
   }

   SimTime curTime = Sim::getCurrentTime();
  
   GFX->disableShaders();   

   for(DebugPrim **walk = &mHead; *walk; )
   {
      DebugPrim *p = *walk;

      // Set up Z testing...
      if(p->useZ)
         GFX->setStateBlock(mRenderZOnSB);
      else
         GFX->setStateBlock(mRenderZOffSB);

      Point3F d;

      switch(p->type)
      {
      case DebugPrim::Tri:
         PrimBuild::begin( GFXLineStrip, 4);

         PrimBuild::color(p->color);

         PrimBuild::vertex3fv(p->a);
         PrimBuild::vertex3fv(p->b);
         PrimBuild::vertex3fv(p->c);
         PrimBuild::vertex3fv(p->a);

         PrimBuild::end();
         break;
      case DebugPrim::Box:
         d = p->a - p->b;
         GFX->getDrawUtil()->drawWireCube(d * 0.5, (p->a + p->b) * 0.5, p->color);
         break;
      case DebugPrim::Line:
         PrimBuild::begin( GFXLineStrip, 2);

         PrimBuild::color(p->color);

         PrimBuild::vertex3fv(p->a);
         PrimBuild::vertex3fv(p->b);

         PrimBuild::end();
         break;
      case DebugPrim::Text:
         {
            GFXTransformSaver saver;            
            Point3F result;
            if (MathUtils::mProjectWorldToScreen(p->a, &result, GFX->getViewport(), GFX->getWorldMatrix(), GFX->getProjectionMatrix()))
            {
               GFX->setClipRect(GFX->getViewport());
               ColorI primColor(p->color.red * 255.0f, p->color.blue * 255.0f, p->color.green * 255.0f, p->color.alpha * 255.0f);
               GFX->getDrawUtil()->drawText(mFont, Point2I(result.x, result.y), p->mText, &primColor);
            }
         }
         break;
      }

      // Ok, we've got data, now freeze here if needed.
      if (shouldToggleFreeze)
      {
         isFrozen = !isFrozen;
         shouldToggleFreeze = false;
      }

      if(p->dieTime <= curTime && !isFrozen && p->dieTime != U32_MAX)
      {
         *walk = p->next;
         mPrimChunker.free(p);
      }
      else
         walk = &((*walk)->next);
   }
#endif
}

void DebugDrawer::drawBox(const Point3F &a, const Point3F &b, const ColorF &color)
{
   if(isFrozen || !isDrawing)
      return;

   DebugPrim *n = mPrimChunker.alloc();

   n->useZ = true;
   n->dieTime = 0;
   n->a = a;
   n->b = b;
   n->color = color;
   n->type = DebugPrim::Box;

   n->next = mHead;
   mHead = n;
}

void DebugDrawer::drawLine(const Point3F &a, const Point3F &b, const ColorF &color)
{
   if(isFrozen || !isDrawing)
      return;

   DebugPrim *n = mPrimChunker.alloc();

   n->useZ = true;
   n->dieTime = 0;
   n->a = a;
   n->b = b;
   n->color = color;
   n->type = DebugPrim::Line;

   n->next = mHead;
   mHead = n;
}

void DebugDrawer::drawTri(const Point3F &a, const Point3F &b, const Point3F &c, const ColorF &color)
{
   if(isFrozen || !isDrawing)
      return;

   DebugPrim *n = mPrimChunker.alloc();

   n->useZ = true;
   n->dieTime = 0;
   n->a = a;
   n->b = b;
   n->c = c;
   n->color = color;
   n->type = DebugPrim::Tri;

   n->next = mHead;
   mHead = n;
}

void DebugDrawer::drawFrustum(const Frustum& f, const ColorF &color)
{
   // Draw near and far planes.
   for (U32 offset = 0; offset < 8; offset+=4)
   {      
      drawLine(f.getPoints()[offset+0], f.getPoints()[offset+1]);
      drawLine(f.getPoints()[offset+2], f.getPoints()[offset+3]);
      drawLine(f.getPoints()[offset+0], f.getPoints()[offset+2]);
      drawLine(f.getPoints()[offset+1], f.getPoints()[offset+3]);            
   }
   drawLine(f.getPoints()[Frustum::NearTopLeft], f.getPoints()[Frustum::FarTopLeft]);
   drawLine(f.getPoints()[Frustum::NearTopRight], f.getPoints()[Frustum::FarTopRight]);
   drawLine(f.getPoints()[Frustum::NearBottomLeft], f.getPoints()[Frustum::FarBottomLeft]);
   drawLine(f.getPoints()[Frustum::NearBottomRight], f.getPoints()[Frustum::FarBottomRight]);
}

void DebugDrawer::drawText(const Point3F& pos, const String& text, const ColorF &color)
{
   if(isFrozen || !isDrawing)
      return;

   DebugPrim *n = mPrimChunker.alloc();

   n->useZ = false;
   n->dieTime = 0;
   n->a = pos;
   n->color = color;
   dStrncpy(n->mText, text.c_str(), 256);   
   n->type = DebugPrim::Text;

   n->next = mHead;
   mHead = n;
}

void DebugDrawer::setLastTTL(U32 ms)
{
   AssertFatal(mHead, "Tried to set last with nothing in the list!");
   if (ms != U32_MAX)
      mHead->dieTime = Sim::getCurrentTime() + ms;
   else
      mHead->dieTime = U32_MAX;
}

void DebugDrawer::setLastZTest(bool enabled)
{
   AssertFatal(mHead, "Tried to set last with nothing in the list!");
   mHead->useZ = enabled;
}

//
// Script interface
//
ConsoleMethod(DebugDrawer, drawLine, void, 4, 4, "(Point3F a, Point3F b)")
{
   Point3F a, b;

   dSscanf(argv[2], "%f %f %f", &a.x, &a.y, &a.z);
   dSscanf(argv[3], "%f %f %f", &b.x, &b.y, &b.z);

   object->drawLine(a, b);
}

ConsoleMethod(DebugDrawer, setLastTTL, void, 3, 3, "(U32 ms)")
{
   object->setLastTTL(dAtoi(argv[2]));
}

ConsoleMethod(DebugDrawer, setLastZTest, void, 3, 3, "(bool enabled)")
{
   object->setLastZTest(dAtob(argv[2]));
}

ConsoleMethod(DebugDrawer, toggleFreeze, void, 2, 2, "() - Toggle freeze mode.")
{
   object->toggleFreeze();
}

ConsoleMethod(DebugDrawer, toggleDrawing, void, 2, 2, "() - Enabled/disable drawing.")
{
   object->toggleDrawing();
}
