//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/levelInfo.h"

#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "sceneGraph/sceneGraph.h"
#include "lighting/advanced/advancedLightManager.h"
#include "lighting/advanced/advancedLightBinManager.h"

IMPLEMENT_CO_NETOBJECT_V1(LevelInfo);

/// The color used to clear the canvas.
/// @see GuiCanvas
extern ColorI gCanvasClearColor;

/// @see DecalManager
extern F32 gDecalBias;

LevelInfo::LevelInfo()
   :  mNearClip( 0.1f ),
      mVisibleDistance( 1000.0f ),
      mDecalBias( 0.0015f ),
      mCanvasClearColor( 255, 0, 255, 255 )
{
   mFogData.density = 0.0f;
   mFogData.densityOffset = 0.0f;
   mFogData.atmosphereHeight = 0.0f;
   mFogData.color.set( 128, 128, 128 ),

   mNetFlags.set( ScopeAlways | Ghostable );

   mAdvancedLightmapSupport = false;

   // Register with the light manager activation signal, and we need to do it first
   // so the advanced light bin manager can be instructed about MRT lightmaps
   LightManager::smActivateSignal.notify(this, &LevelInfo::_onLMActivate, 0.01f);
}

LevelInfo::~LevelInfo()
{
   LightManager::smActivateSignal.remove(this, &LevelInfo::_onLMActivate);
}

void LevelInfo::initPersistFields()
{
   addGroup( "Visibility" );

      addField( "nearClip", TypeF32, Offset( mNearClip, LevelInfo ) );
      addField( "visibleDistance", TypeF32, Offset( mVisibleDistance, LevelInfo ) );
      addField( "decalBias", TypeF32, Offset( mDecalBias, LevelInfo ),
         "NearPlane bias used when rendering Decal and DecalRoad. This should be tuned to the visibleDistance in your level." );

   endGroup( "Visibility" );

   addGroup( "Fog" );

      addField( "fogColor", TypeColorF, Offset( mFogData.color, LevelInfo ),
         "The default color for the scene fog." );

      addField( "fogDensity", TypeF32, Offset( mFogData.density, LevelInfo ),
         "The 0 to 1 density value for the exponential fog falloff." );

      addField( "fogDensityOffset", TypeF32, Offset( mFogData.densityOffset, LevelInfo ),
         "An offset from the camera in meters for moving the start of the fog effect." );

      addField( "fogAtmosphereHeight", TypeF32, Offset( mFogData.atmosphereHeight, LevelInfo ),
         "A height in meters for altitude fog falloff." );

   endGroup( "Fog" );

   addGroup( "LevelInfo" );

      addField( "canvasClearColor", TypeColorI, Offset( mCanvasClearColor, LevelInfo ),
         "The color used to clear the background before the scene or any GUIs are rendered." );

   endGroup( "LevelInfo" );

   addGroup( "Lightmap Support" );

   addField( "advancedLightmapSupport", TypeBool, Offset( mAdvancedLightmapSupport, LevelInfo ),
      "Enable expanded support for mixing static and dynamic lighting (more costly)" );

   endGroup( "Lightmap Support" );
}

void LevelInfo::inspectPostApply()
{
   _updateSceneGraph();
   setMaskBits( 0xFFFFFFFF );
}

U32 LevelInfo::packUpdate(NetConnection *conn, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(conn, mask, stream);

   stream->write( mNearClip );
   stream->write( mVisibleDistance );
   stream->write( mDecalBias );

   stream->write( mFogData.density );
   stream->write( mFogData.densityOffset );
   stream->write( mFogData.atmosphereHeight );
   stream->write( mFogData.color );

   stream->write( mCanvasClearColor );

   stream->writeFlag( mAdvancedLightmapSupport );

   return retMask;
}

void LevelInfo::unpackUpdate(NetConnection *conn, BitStream *stream)
{
   Parent::unpackUpdate(conn, stream);

   stream->read( &mNearClip );
   stream->read( &mVisibleDistance );
   stream->read( &mDecalBias );

   stream->read( &mFogData.density );
   stream->read( &mFogData.densityOffset );
   stream->read( &mFogData.atmosphereHeight );
   stream->read( &mFogData.color );

   stream->read( &mCanvasClearColor );

   mAdvancedLightmapSupport = stream->readFlag();

   if ( isProperlyAdded() )
      _updateSceneGraph();
}

bool LevelInfo::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   _updateSceneGraph();

   return true;
}

void LevelInfo::onRemove()
{
   Parent::onRemove();
}

void LevelInfo::_updateSceneGraph()
{
   // We must update both scene graphs.
   SceneGraph *sm;
   if ( isClientObject() )
      sm = gClientSceneGraph;
   else
      sm = gServerSceneGraph;

   // Clamp above zero before setting on the sceneGraph.
   // If we don't we get serious crashes.
   if ( mNearClip <= 0.0f )
      mNearClip = 0.001f;

   sm->setNearClip( mNearClip );
   sm->setVisibleDistance( mVisibleDistance );

   gDecalBias = mDecalBias;

   // Copy our AirFogData into the sceneGraph.
   sm->setFogData( mFogData );

   // If the level info specifies that MRT pre-pass should be used in this scene
   // enable it via the appropriate light manager
   // (Basic lighting doesn't do anything different right now)
#ifndef TORQUE_DEDICATED
   if(isClientObject())
   {
      LightManager* manager = gClientSceneGraph->getLightManager();
      _onLMActivate(manager->getId(), true);
   }
#endif

   // TODO: This probably needs to be moved.
   gCanvasClearColor = mCanvasClearColor;
}

void LevelInfo::_onLMActivate(const char *lm, bool enable)
{
#ifndef TORQUE_DEDICATED
   // Advanced light manager
   if(enable && String(lm) == String("ADVLM"))
   {
      LightManager* manager = gClientSceneGraph->getLightManager();
      AssertFatal(dynamic_cast<AdvancedLightManager *>(manager), "Bad light manager type!");
      AdvancedLightManager *lightMgr = static_cast<AdvancedLightManager *>(manager);
      lightMgr->getLightBinManager()->MRTLightmapsDuringPrePass(mAdvancedLightmapSupport);
   }
#endif
}