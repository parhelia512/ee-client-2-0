//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PARABOLOIDLIGHTSHADOWMAP_H_
#define _PARABOLOIDLIGHTSHADOWMAP_H_

#ifndef _LIGHTSHADOWMAP_H_
#include "lighting/shadowMap/lightShadowMap.h"
#endif


class ParaboloidLightShadowMap : public LightShadowMap
{
   typedef LightShadowMap Parent;
public:
   ParaboloidLightShadowMap( LightInfo *light );
   ~ParaboloidLightShadowMap();

   // LightShadowMap
   virtual ShadowType getShadowType() const;
   virtual void _render( SceneGraph *sceneManager, const SceneState *diffuseState );
   virtual void setShaderParameters(GFXShaderConstBuffer* params, LightingShaderConstants* lsc);

protected:
   Point2F mShadowMapScale;
   Point2F mShadowMapOffset;
};

#endif