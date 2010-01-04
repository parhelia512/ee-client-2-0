//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/shadowMap/shadowMapManager.h"

#include "lighting/shadowMap/blurTexture.h"
#include "lighting/shadowMap/shadowMapPass.h"
#include "lighting/shadowMap/lightShadowMap.h"
#include "materials/materialManager.h"
#include "lighting/lightManager.h"
#include "core/util/safeDelete.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxTextureManager.h"


ShadowMapManager::ShadowMapManager() 
:  mShadowMapPass(NULL), 
   mCurrentShadowMap(NULL),
   mIsActive(false)
{
}

ShadowMapManager::~ShadowMapManager()
{
}

void ShadowMapManager::setLightShadowMapForLight( LightInfo *light )
{
   ShadowMapParams *params = light->getExtended<ShadowMapParams>();
   if ( params )
      mCurrentShadowMap = params->getShadowMap();
   else 
      mCurrentShadowMap = NULL;
}

void ShadowMapManager::activate()
{
   ShadowManager::activate();

   if (!getSceneManager())
   {
      Con::errorf("This world has no scene manager!  Shadow manager not activating!");
      return;
   }

   LightManager *activeLM = getSceneManager()->getLightManager();

   mShadowMapPass = new ShadowMapPass(activeLM, this);

   getSceneManager()->getPreRenderSignal().notify( this, &ShadowMapManager::_onPreRender, 0.01f );

   mIsActive = true;
}

void ShadowMapManager::deactivate()
{
   getSceneManager()->getPreRenderSignal().remove( this, &ShadowMapManager::_onPreRender );

   SAFE_DELETE(mShadowMapPass);
   mTapRotationTex = NULL;

   // Clean up our shadow texture memory.
   LightShadowMap::releaseAllTextures();
   TEXMGR->cleanupPool();

   mIsActive = false;

   ShadowManager::deactivate();
}

void ShadowMapManager::_onPreRender( SceneGraph *sg, const SceneState *state )
{
   if ( mShadowMapPass && state->isDiffusePass() )
      mShadowMapPass->render( sg, state, (U32)-1 );
}

GFXTextureObject* ShadowMapManager::getTapRotationTex()
{
   if ( mTapRotationTex.isValid() )
      return mTapRotationTex;

   mTapRotationTex.set( 64, 64, GFXFormatR8G8B8A8, &GFXDefaultPersistentProfile, 
                        "ShadowMapManager::getTapRotationTex" );

   GFXLockedRect *rect = mTapRotationTex.lock();
   U8 *f = rect->bits;
   F32 angle;
   for( U32 i = 0; i < 64*64; i++, f += 4 )
   {         
      // We only pack the rotations into the red
      // and green channels... the rest are empty.
      angle = M_2PI_F * gRandGen.randF();
      f[0] = U8_MAX * ( ( 1.0f + mSin( angle ) ) * 0.5f );
      f[1] = U8_MAX * ( ( 1.0f + mCos( angle ) ) * 0.5f );
      f[2] = 0;
      f[3] = 0;
   }

   mTapRotationTex.unlock();

   return mTapRotationTex;
}