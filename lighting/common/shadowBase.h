//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _LIGHTINGSYSTEM_SHADOWBASE_H_
#define _LIGHTINGSYSTEM_SHADOWBASE_H_

class TSRenderState;

class ShadowBase
{
public:
   virtual ~ShadowBase() {}
   virtual bool shouldRender( const SceneState *state ) = 0;

   virtual void update( const SceneState *state ) = 0;
   virtual void render(F32 camDist, const TSRenderState &rdata ) = 0;
   virtual U32 getLastRenderTime() const = 0;
   virtual const F32 getScore() const = 0;
};

#endif