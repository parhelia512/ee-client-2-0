//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _DEBUGDRAW_H_
#define _DEBUGDRAW_H_

#ifndef _SIMOBJECT_H_
#include "console/simObject.h"
#endif
#ifndef _GFXDEVICE_H_
#include "gfx/gfxDevice.h"
#endif
#ifndef _PRIMBUILDER_H_
#include "gfx/primBuilder.h"
#endif
#ifndef _GFONT_H_
#include "gfx/gFont.h"
#endif
#ifndef _DATACHUNKER_H_
#include "core/dataChunker.h"
#endif


class GFont;
class Frustum;

#ifdef TORQUE_DEBUG
//#define ENABLE_DEBUGDRAW
#endif

/// Debug output class.
///
/// This class provides you with a flexible means of drawing debug output. It is
/// often useful when debugging collision code or complex 3d algorithms to have
/// them draw debug information, like culling hulls or bounding volumes, normals,
/// simple lines, and so forth. In TGE1.2, which was based directly on a simple
/// OpenGL rendering layer, it was a simple matter to do debug rendering directly
/// inline.
///
/// Unfortunately, this doesn't hold true with more complex rendering scenarios,
/// where render modes and targets may be in abritrary states. In addition, it is
/// often useful to be able to freeze frame debug information for closer inspection.
///
/// Therefore, TSE provides a global DebugDrawer instance, called gDebugDraw, which
/// you can use to draw debug information. It exposes a number of methods for drawing
/// a variety of debug primitives, including lines, triangles and boxes.
/// Internally, DebugDrawer maintains a list of active debug primitives, and draws the
/// contents of the list after each frame is done rendering. This way, you can be
/// assured that your debug rendering won't interfere with TSE's various effect
/// rendering passes or render-to-target calls.
///
/// The DebugDrawer can also be used for more interesting uses, like freezing its
/// primitive list so you can look at a situation more closely, or dumping the
/// primitive list to disk for closer analysis.
///
/// DebugDrawer is accessible by script under the name DebugDrawer, and by C++ under
/// the symbol gDebugDraw. There are a variety of methods available for drawing
/// different sorts of output; see the class reference for more information.
///
/// DebugDrawer works solely in worldspace. Primitives are rendered with cull mode of
/// none.
///
/// @warning Be careful! This tool is useful for debug but it can also 


class DebugDrawer : public SimObject
{
public:
   DECLARE_CONOBJECT(DebugDrawer);

   DebugDrawer();
   ~DebugDrawer();

   static DebugDrawer* get();
   
   /// Called at engine init to set up the global debug draw object.
   static void init();

   /// Called globally to render debug draw state. Also does state updates.
   void render();

   void toggleFreeze()  { shouldToggleFreeze = true; };
   void toggleDrawing() 
   {
#ifdef ENABLE_DEBUGDRAW
      isDrawing = !isDrawing;
#endif
   };


   /// @name ddrawmeth Debug Draw Methods
   ///
   /// @{

   void drawBox   (const Point3F &a, const Point3F &b, const ColorF &color = ColorF(1.0f,1.0f,1.0f));
   void drawLine  (const Point3F &a, const Point3F &b, const ColorF &color = ColorF(1.0f,1.0f,1.0f));	
   void drawTri   (const Point3F &a, const Point3F &b, const Point3F &c, const ColorF &color = ColorF(1.0f,1.0f,1.0f));   
   void drawFrustum(const Frustum& f, const ColorF &color = ColorF(1.0f,1.0f,1.0f));
   void drawText(const Point3F& pos, const String& text, const ColorF &color = ColorF(1.0f,1.0f,1.0f));

   /// Set the TTL for the last item we entered...
   ///
   /// Primitives default to lasting one frame (ie, ttl=0)
   enum {
      DD_INFINITE = U32_MAX
   };
   // How long should this primitive be draw for, 0 = one frame, DD_INFINITE = draw forever
   void setLastTTL(U32 ms);

   /// Disable/enable z testing on the last primitive.
   ///
   /// Primitives default to z testing on.
   void setLastZTest(bool enabled);

   /// @}
private:
   typedef SimObject Parent;

   static DebugDrawer* sgDebugDrawer;

   struct DebugPrim
   {
      /// Color used for this primitive.
      ColorF color;

      /// Points used to store positional data. Exact semantics determined by type.
      Point3F a, b, c;
      enum {
         Tri,
         Box,
         Line,
         Text
      } type;	   ///< Type of the primitive. The meanings of a,b,c are determined by this.

      SimTime dieTime;   ///< Time at which we should remove this from the list.
      bool useZ; ///< If true, do z-checks for this primitive.      
      char mText[256];      // Text to display

      DebugPrim *next;
   };


   FreeListChunker<DebugPrim> mPrimChunker;
   DebugPrim *mHead;

   bool isFrozen;
   bool shouldToggleFreeze;
   bool isDrawing;   

   GFXStateBlockRef mRenderZOffSB;
   GFXStateBlockRef mRenderZOnSB;

   Resource<GFont> mFont;

   void setupStateBlocks();
};

#endif
