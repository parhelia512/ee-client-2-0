//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "renderInstance/renderPassStateToken.h"
#include "materials/matTextureTarget.h"

class PostEffect;

class RenderFormatToken : public RenderPassStateToken, public MatTextureTarget
{
   typedef RenderPassStateToken Parent;

public:
   enum FormatTokenState
   {
      FTSDisabled,
      FTSWaiting,
      FTSActive,
      FTSComplete,
   };

   const static U32 TargetChainLength = 1;

protected:
   FormatTokenState mFCState;
   GFXFormat mColorFormat;
   GFXFormat mDepthFormat;
   bool mTargetUpdatePending;
   U32 mTargetChainIdx;
   RectI mViewportRect;
   Point2I mTargetSize;
   S32 mTargetAALevel;
   SimObjectPtr<PostEffect> mCopyPostEffect;
   SimObjectPtr<PostEffect> mResolvePostEffect;

   GFXTexHandle mTargetColorTexture[TargetChainLength];
   GFXTexHandle mTargetDepthStencilTexture[TargetChainLength];
   GFXTextureTargetRef mTargetChain[TargetChainLength];

   GFXTexHandle mStoredPassZTarget;
   
   void _updateTargets();
   void _teardownTargets();

   void _onTextureEvent( GFXTexCallbackCode code );
   virtual bool _handleGFXEvent(GFXDevice::GFXDeviceEventType event);

   static bool _setFmt(void* obj, const char* data);
public:
   DECLARE_CONOBJECT(RenderFormatToken);
   static void initPersistFields();
   virtual bool onAdd();
   virtual void onRemove();

   RenderFormatToken();
   virtual ~RenderFormatToken();

   virtual void process(SceneState *state, RenderPassStateBin *callingBin);
   virtual void reset();
   virtual void enable(bool enabled = true);
   virtual bool isEnabled() const;

   virtual GFXTextureObject* getTargetTexture( U32 mrtIndex ) const;
   virtual const RectI& getTargetViewport() const { return mViewportRect; }
   virtual void setupSamplerState( GFXSamplerStateDesc *desc ) const;
   virtual ConditionerFeature* getTargetConditioner() const { return NULL; };
};