//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _RENDERGLOWMGR_H_
#define _RENDERGLOWMGR_H_

#ifndef _TEXTARGETBIN_MGR_H_
#include "renderInstance/renderTexTargetBinManager.h"
#endif


class PostEffect;


///
class RenderGlowMgr : public RenderTexTargetBinManager
{  
   typedef RenderTexTargetBinManager Parent;

public:

   RenderGlowMgr();
   virtual ~RenderGlowMgr();

   /// Returns true if the glow post effect is
   /// enabled and the glow buffer should be updated.
   bool isGlowEnabled();

   // RenderBinManager
   virtual RenderBinManager::AddInstResult addElement( RenderInst *inst );
   virtual void render( SceneState *state );

   // ConsoleObject
   DECLARE_CONOBJECT( RenderGlowMgr );

protected:

   class GlowMaterialHook : public MatInstanceHook
   {
   public:

      GlowMaterialHook( BaseMatInstance *matInst );
      virtual ~GlowMaterialHook();

      virtual BaseMatInstance *getMatInstance() { return mGlowMatInst; }

      virtual const MatInstanceHookType& getType() const { return Type; }

      /// Our material hook type.
      static const MatInstanceHookType Type;

   protected:

      static void _overrideFeatures(   ProcessedMaterial *mat,
                                       U32 stageNum,
                                       MaterialFeatureData &fd, 
                                       const FeatureSet &features );

      BaseMatInstance *mGlowMatInst; 
   };

   SimObjectPtr<PostEffect> mGlowEffect;

};


#endif // _RENDERGLOWMGR_H_
