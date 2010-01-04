//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _LEVELINFO_H_
#define _LEVELINFO_H_

#ifndef _NETOBJECT_H_
#include "sim/netObject.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _FOGSTRUCTS_H_
#include "sceneGraph/fogStructs.h"
#endif

class LevelInfo : public NetObject
{
   typedef NetObject Parent;
   
private:

   FogData mFogData;
   
   F32 mNearClip;

   F32 mVisibleDistance;

   F32 mDecalBias;

   ColorI mCanvasClearColor;

   bool mAdvancedLightmapSupport;
   
   /// Responsible for passing on
   /// the LevelInfo settings to the
   /// client SceneGraph, from which
   /// other systems can get at them.
   void _updateSceneGraph();

   void _onLMActivate(const char *lm, bool enable);

public:

   LevelInfo();
   virtual ~LevelInfo();

   DECLARE_CONOBJECT(LevelInfo);

   /// @name SimObject Inheritance
   /// @{
   bool onAdd();
   void onRemove();
   static void initPersistFields();
   void inspectPostApply();
   /// @}

   /// @name NetObject Inheritance
   /// @{
   enum NetMaskBits 
   {
      UpdateMask = BIT(0)
   };

   U32  packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   void unpackUpdate( NetConnection *conn, BitStream *stream );
   /// @}
};

#endif // _LEVELINFO_H_