//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _TEXTARGETBIN_MGR_H_
#define _TEXTARGETBIN_MGR_H_

#ifndef _RENDERBINMANAGER_H_
#include "renderInstance/renderBinManager.h"
#endif
#ifndef _MATTEXTURETARGET_H_
#include "materials/matTextureTarget.h"
#endif

class ConditionerFeature;
class BlurOp;


class RenderTexTargetBinManager : public RenderBinManager, public MatTextureTarget
{
   typedef RenderBinManager Parent;

public:
   static const GFXFormat DefaultTargetFormat = GFXFormatR8G8B8A8;
   static const U32 DefaultTargetChainLength = 1;
   static const U32 DefaultTargetSize = 256;

   enum TargetSizeType
   {
      WindowSize = 0,
      WindowSizeScaled,
      FixedSize
   };

public:

   RenderTexTargetBinManager(const GFXFormat targetFormat = DefaultTargetFormat, 
                              const Point2I &targetSize = Point2I(DefaultTargetSize, DefaultTargetSize),
                              const U32 targetChainLength = DefaultTargetChainLength );
   RenderTexTargetBinManager(const RenderInstType& ritype, F32 renderOrder, F32 processAddOrder,
                              const GFXFormat targetFormat = DefaultTargetFormat, 
                              const Point2I &targetSize = Point2I(DefaultTargetSize, DefaultTargetSize),
                              const U32 targetChainLength = DefaultTargetChainLength);

   virtual ~RenderTexTargetBinManager();

   // MatTextureTarget
   virtual const Point2I &getTargetSize() const { return mTargetSize; }
   virtual const RectI& getTargetViewport() const { return mTargetViewport; }
   virtual GFXTextureObject* getTargetTexture( U32 mrtIndex ) const { return getTargetTexture(mrtIndex, -1); }
   virtual GFXTextureObject* getTargetTexture( U32 mrtIndex, S32 chainIndex ) const;
   virtual void setupSamplerState( GFXSamplerStateDesc *desc ) const;
   virtual ConditionerFeature* getTargetConditioner() const { return NULL; }

   virtual bool setTargetSize(const Point2I &newTargetSize);

   /// Force a target update
   virtual bool updateTargets() { return _updateTargets(); }

   void setTargetFormatConsole(const S32 &fmt) { setTargetFormat(GFXFormat(fmt)); }
   virtual bool setTargetFormat(const GFXFormat &newTargetFormat);

   S32 getTargetFormatConsole() { return getTargetFormat(); }
   virtual const GFXFormat &getTargetFormat() const { return mTargetFormat; }

   virtual void setTargetChainLength(const U32 chainLength);
   virtual U32 getTargetChainLength() const { return mTargetChainLength; }

   virtual void setBlur(bool enableBlur);
   virtual bool getBlur() const { return mBlur != NULL; }

   DECLARE_CONOBJECT(RenderTexTargetBinManager);
   static void initPersistFields();
   virtual bool onAdd();

protected:

   GFXFormat mTargetFormat;
   Point2I mTargetSize;
   Point2F mTargetScale;
   RectI mTargetViewport;
   TargetSizeType mTargetSizeType;

   U32 mTargetChainLength;
   U32 mTargetChainIdx;
   U32 mNumRenderTargets;
   GFXTextureTargetRef *mTargetChain;
   GFXTexHandle **mTargetChainTextures;
   BlurOp *mBlur;
   GFXTexHandle *mScratchTexture;

#ifndef TORQUE_SHIPPING
   bool m_NeedsOnPostRender;
#endif
   bool mPreserve;

   virtual bool _handleGFXEvent(GFXDevice::GFXDeviceEventType event);
   virtual GFXTextureTargetRef _getTextureTarget(const U32 idx = 0);

   /// Pushes the active render target, and sets itself as a render target. The
   /// target is then cleared using 'mTargetClearColor', viewport is set properly,
   /// and true is returned, and '_onPostRender' must be called after rendering
   /// is complete. If the return value is false, than '_onPostRender'
   /// should not be called. 
   ///
   /// @param preserve  If set to true, the contents of the current render target 
   //                   will be the same when _onPostRender is called. Otherwise
   //                   the contents are undefined on console platforms.
   virtual bool _onPreRender(SceneState * state, bool preserve = false);

   /// Resolves the active render target, pops the render target from _onPreRender, and sets debug info. 
   virtual void _onPostRender();

   virtual bool _updateTargets();

   virtual bool _setupTargets();

   virtual void _teardownTargets();

   /// The callback used to get texture events.
   /// @see GFXTextureManager::addEventDelegate
   void _onTextureEvent( GFXTexCallbackCode code );
};

#endif // _TEXTARGETBIN_MGR_H_