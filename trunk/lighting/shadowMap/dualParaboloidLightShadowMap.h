//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _DUALPARABOLOIDLIGHTSHADOWMAP_H_
#define _DUALPARABOLOIDLIGHTSHADOWMAP_H_

#ifndef _PARABOLOIDLIGHTSHADOWMAP_H_
#include "lighting/shadowMap/paraboloidLightShadowMap.h"
#endif

class DualParaboloidLightShadowMap : public ParaboloidLightShadowMap
{
   typedef ParaboloidLightShadowMap Parent;

public:
   DualParaboloidLightShadowMap( LightInfo *light );

   virtual void _render( SceneGraph *sceneManager, const SceneState *diffuseState );
   virtual bool setTextureStage(const SceneGraphData& sgData, const U32 currTexFlag, 
      const U32 textureSlot, GFXShaderConstBuffer* shaderConsts, 
      GFXShaderConstHandle* shadowMapSC);
};

#endif // _DUALPARABOLOIDLIGHTSHADOWMAP_H_