//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _SINGLELIGHTSHADOWMAP_H_
#define _SINGLELIGHTSHADOWMAP_H_

#ifndef _LIGHTSHADOWMAP_H_
#include "lighting/shadowMap/lightShadowMap.h"
#endif

//
// SingleLightShadowMap, holds the shadow map and various other things for a light.
//
// This represents everything we need to render the shadowmap for one light.
class SingleLightShadowMap : public LightShadowMap
{
public:
   SingleLightShadowMap( LightInfo *light );
   ~SingleLightShadowMap();

   // LightShadowMap
   virtual ShadowType getShadowType() const { return ShadowType_Spot; }
   virtual void _render( SceneGraph *sceneManager, const SceneState *diffuseState );
   virtual void setShaderParameters(GFXShaderConstBuffer* params, LightingShaderConstants* lsc);
};


#endif // _SINGLELIGHTSHADOWMAP_H_