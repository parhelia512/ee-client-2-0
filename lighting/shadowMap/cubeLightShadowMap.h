//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _CUBELIGHTSHADOWMAP_H_
#define _CUBELIGHTSHADOWMAP_H_

#ifndef _LIGHTSHADOWMAP_H_
#include "lighting/shadowMap/lightShadowMap.h"
#endif
#ifndef _GFXCUBEMAP_H_
#include "gfx/gfxCubemap.h"
#endif


class CubeLightShadowMap : public LightShadowMap
{
   typedef LightShadowMap Parent;

public:

   CubeLightShadowMap( LightInfo *light );

   // LightShadowMap
   virtual bool hasShadowTex() const { return mCubemap.isValid(); }
   virtual ShadowType getShadowType() const { return ShadowType_CubeMap; }
   virtual void _render( SceneGraph *sceneManager, const SceneState *diffuseState );
   virtual void setShaderParameters( GFXShaderConstBuffer* params, LightingShaderConstants* lsc );
   virtual void releaseTextures();
   virtual bool setTextureStage( const SceneGraphData& sgData, 
                                 const U32 currTexFlag,
                                 const U32 textureSlot, 
                                 GFXShaderConstBuffer* shaderConsts,
                                 GFXShaderConstHandle* shadowMapSC );

protected:   

   /// The shadow cubemap.
   GFXCubemapHandle mCubemap;

};

#endif // _CUBELIGHTSHADOWMAP_H_
