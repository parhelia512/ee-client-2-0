//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _SCENEDATA_H_
#define _SCENEDATA_H_

#ifndef _SCENEGRAPH_H_
#include "sceneGraph/sceneGraph.h"
#endif
#ifndef _LIGHTMANAGER_H_
#include "lighting/lightManager.h"
#endif

struct VertexData;
class GFXTexHandle;
class GFXCubemap;

//**************************************************************************
// Scene graph data - temp - simulates data scenegraph will provide
//
// CodeReview [btr, 7/31/2007] I'm not sure how temporary this struct is.  But 
// it keeps the material system separate from the SceneGraph and RenderInst 
// systems.  Which is kind of nice.  I think eventually the RenderInst should 
// get rid of the duplicate variables and just contain a SceneGraphData.
//**************************************************************************
struct SceneGraphData
{
   // textures
   GFXTextureObject * lightmap;
   GFXTextureObject * backBuffTex;
   GFXTextureObject * reflectTex;
   GFXTextureObject * miscTex;
   
   /// The current lights to use in rendering
   /// in order of the light importance.
   LightInfo* lights[8];

   // fog      
   F32 fogDensity;
   F32 fogDensityOffset;
   F32 fogHeightFalloff;
   ColorF fogColor;

   /// The special bin types.
   enum BinType
   {
      /// A render bin that isn't one of the
      /// special bins we care about.
      OtherBin = 0,

      /// The glow render bin.
      /// @see RenderGlowMgr
      GlowBin,

      /// The prepass render bin.
      /// @RenderPrePassMgr
      PrePassBin,
   };
   
   /// This defines when we're rendering a special bin 
   /// type that the material or lighting system needs
   /// to know about.
   BinType binType;

   // misc
   MatrixF        objTrans;
   VertexData *   vertData;
   GFXCubemap *   cubemap;
   F32            visibility;

   /// Enables wireframe rendering for the object.
   bool wireframe;

   /// A generic hint value passed from the game
   /// code down to the material for use by shader 
   /// features.
   void *materialHint;

   //-----------------------------------------------------------------------
   // Constructor
   //-----------------------------------------------------------------------
   SceneGraphData() : lightmap()
   {
      reset();
   }

   inline void reset()
   {
      dMemset( this, 0, sizeof( SceneGraphData ) );
      visibility = 1.0f;
   }

   inline void setFogParams( const FogData &data )
   {
      // Fogging...
      fogDensity = data.density;
      fogDensityOffset = data.densityOffset;
      if ( !mIsZero( data.atmosphereHeight ) )
         fogHeightFalloff = 1.0f / data.atmosphereHeight;
      else
         fogHeightFalloff = 0.0f;

      fogColor = data.color;
   }
};

#endif // _SCENEDATA_H_
