//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SHADOWMAPMANAGER_H_
#define _SHADOWMAPMANAGER_H_

#ifndef _TSINGLETON_H_
#include "core/util/tSingleton.h"
#endif
#ifndef _SHADOWMANAGER_H_
#include "lighting/shadowManager.h"
#endif
#ifndef _GFXENUMS_H_
#include "gfx/gfxEnums.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _MPOINT4_H_
#include "math/mPoint4.h"
#endif

class LightShadowMap;
class ShadowMapPass;
class LightInfo;

class SceneGraph;
class SceneState;


class ShadowMapManager : public ShadowManager
{
   typedef ShadowManager Parent;

   friend class ShadowMapPass;

public:

   ShadowMapManager();
   virtual ~ShadowMapManager();

   /// Sets the current shadowmap (used in setLightInfo/setTextureStage calls)
   void setLightShadowMap( LightShadowMap *lm ) { mCurrentShadowMap = lm; }
   
   /// Looks up the shadow map for the light then sets it.
   void setLightShadowMapForLight( LightInfo *light );

   /// Return the current shadow map
   LightShadowMap* getCurrentShadowMap() const { return mCurrentShadowMap; }

   ShadowMapPass* getShadowMapPass() const { return mShadowMapPass; }

   // Shadow manager
   virtual void activate();
   virtual void deactivate();

   GFXTextureObject* getTapRotationTex();

protected:

   void _onPreRender( SceneGraph *sg, const SceneState* state );

   ShadowMapPass *mShadowMapPass;
   LightShadowMap *mCurrentShadowMap;

   ///
   GFXTexHandle mTapRotationTex;

   bool mIsActive;
};


/// Returns the ShadowMapManager singleton.
#define SHADOWMGR Singleton<ShadowMapManager>::instance()

#endif // _SHADOWMAPMANAGER_H_