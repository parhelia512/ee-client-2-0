//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "renderInstance/renderFormatChanger.h"
#include "console/consoleTypes.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/gfxDebugEvent.h"
#include "postFx/postEffect.h"
#include "postFx/postEffectManager.h"

extern ColorI gCanvasClearColor;

IMPLEMENT_CONOBJECT(RenderFormatToken);

RenderFormatToken::RenderFormatToken() : 
   Parent(), mFCState(FTSDisabled), 
   mColorFormat(GFXFormat_COUNT), 
   mDepthFormat(GFXFormat_COUNT),
   mTargetUpdatePending(true),
   mTargetChainIdx(0),
   mViewportRect(Point2I::Zero, Point2I::One),
   mTargetSize(Point2I::Zero),
   mTargetAALevel(GFXTextureManager::AA_MATCH_BACKBUFFER),
   mCopyPostEffect(NULL),
   mResolvePostEffect(NULL)
{
   GFXDevice::getDeviceEventSignal().notify(this, &RenderFormatToken::_handleGFXEvent);
   GFXTextureManager::addEventDelegate(this, &RenderFormatToken::_onTextureEvent);
}

RenderFormatToken::~RenderFormatToken()
{
   GFXTextureManager::removeEventDelegate(this, &RenderFormatToken::_onTextureEvent);
   GFXDevice::getDeviceEventSignal().remove(this, &RenderFormatToken::_handleGFXEvent);

   _teardownTargets();
}

void RenderFormatToken::process(SceneState *state, RenderPassStateBin *callingBin)
{
   switch(mFCState)
   {
   case FTSWaiting:
      {
         GFXDEBUGEVENT_SCOPE_EX(RFT_Waiting, ColorI::BLUE, avar("[%s Activate] (%s)", getName(), GFXStringTextureFormat[mColorFormat]));
         mFCState = FTSActive;

         mViewportRect = GFX->getViewport();

         // Update targets
         _updateTargets();

         // If we have a copy PostEffect then get the active backbuffer copy 
         // now before we swap the render targets.
         GFXTexHandle curBackBuffer;
         if(mCopyPostEffect.isValid())
            curBackBuffer = PFXMGR->getBackBufferTex();

         // Push target
         GFX->pushActiveRenderTarget();
         GFX->setActiveRenderTarget(mTargetChain[mTargetChainIdx]);

         // Set viewport
         GFX->setViewport(mViewportRect);

         // Clear
         GFX->clear(GFXClearTarget | GFXClearZBuffer | GFXClearStencil, gCanvasClearColor, 1.0f, 0);

         // Set active z target on render pass
         if(mTargetDepthStencilTexture[mTargetChainIdx].isValid())
         {
            if(callingBin->getParentManager()->getDepthTargetTexture() != GFXTextureTarget::sDefaultDepthStencil)
               mStoredPassZTarget = callingBin->getParentManager()->getDepthTargetTexture();
            else
               mStoredPassZTarget = NULL;

            callingBin->getParentManager()->setDepthTargetTexture(mTargetDepthStencilTexture[mTargetChainIdx]);
         }

         // Run the PostEffect which copies data into the new target.
         if ( mCopyPostEffect.isValid() )
            mCopyPostEffect->process( state, curBackBuffer, &mViewportRect );
      }
      break;

   case FTSActive:
      {
         GFXDEBUGEVENT_SCOPE_EX(RFT_Active, ColorI::BLUE, avar("[%s Deactivate]", getName()));
         mFCState = FTSComplete;

         // Pop target
         AssertFatal(GFX->getActiveRenderTarget() == mTargetChain[mTargetChainIdx], "Render target stack went wrong somewhere");
         mTargetChain[mTargetChainIdx]->resolve();
         GFX->popActiveRenderTarget();

         // This is the GFX viewport when we were first processed.
         GFX->setViewport(mViewportRect);

         // Restore active z-target
         if(mTargetDepthStencilTexture[mTargetChainIdx].isValid())
         {
            callingBin->getParentManager()->setDepthTargetTexture(mStoredPassZTarget.getPointer());
            mStoredPassZTarget = NULL;
         }

         // Run the PostEffect which copies data to the backbuffer
         if(mResolvePostEffect.isValid())
         {
		      // Need to create a texhandle here, since inOutTex gets assigned during process()
            GFXTexHandle inOutTex = mTargetColorTexture[mTargetChainIdx];
            mResolvePostEffect->process( state, inOutTex, &mViewportRect );
         }
      }
      break;

   case FTSComplete:
      AssertFatal(false, "process() called on a RenderFormatToken which was already complete.");
      // fall through
   case FTSDisabled:
      break;
   }
}

void RenderFormatToken::reset()
{
   AssertFatal(mFCState != FTSActive, "RenderFormatToken still active during reset()!");
   if(mFCState != FTSDisabled)
      mFCState = FTSWaiting;
}

void RenderFormatToken::_updateTargets()
{
   if ( GFX->getActiveRenderTarget() == NULL )
      return;

   const Point2I &rtSize = GFX->getActiveRenderTarget()->getSize();

   if ( rtSize.x <= mTargetSize.x && 
        rtSize.y <= mTargetSize.y && 
        !mTargetUpdatePending )
      return;   

   mTargetSize = rtSize;
   mTargetUpdatePending = false;   
   mTargetChainIdx = 0;

   for( U32 i = 0; i < TargetChainLength; i++ )
   {
      if( !mTargetChain[i] )
         mTargetChain[i] = GFX->allocRenderToTextureTarget();

      // Update color target
      if(mColorFormat != GFXFormat_COUNT)
      {
         mTargetColorTexture[i].set( rtSize.x, rtSize.y, mColorFormat, 
            &GFXDefaultRenderTargetProfile, avar( "%s() - (line %d)", __FUNCTION__, __LINE__ ),
            1, mTargetAALevel );
         mTargetChain[i]->attachTexture( GFXTextureTarget::Color0, mTargetColorTexture[i] );
      }

      mTargetChain[i]->attachTexture( GFXTextureTarget::Color0, mTargetColorTexture[i] );
      

      // Update depth target
      if(mDepthFormat != GFXFormat_COUNT)
      {
         mTargetDepthStencilTexture[i].set( rtSize.x, rtSize.y, mDepthFormat, 
            &GFXDefaultZTargetProfile, avar( "%s() - (line %d)", __FUNCTION__, __LINE__ ),
            1, mTargetAALevel );
      }

      mTargetChain[i]->attachTexture( GFXTextureTarget::DepthStencil, mTargetDepthStencilTexture[i] );
   }
}

void RenderFormatToken::_teardownTargets()
{
   for(int i = 0; i < TargetChainLength; i++)
   {
      mTargetColorTexture[i] = NULL;
      mTargetDepthStencilTexture[i] = NULL;
      mTargetChain[i] = NULL;
   }
}

bool RenderFormatToken::_setFmt(void* obj, const char* data)
{
   // Flag update pending
   reinterpret_cast<RenderFormatToken *>(obj)->mTargetUpdatePending = true;

   // Allow console system to assign value
   return true;
}

void RenderFormatToken::enable( bool enabled /*= true*/ )
{
   AssertFatal(mFCState != FTSActive, "RenderFormatToken is active, cannot change state now!");

   if(enabled)
      mFCState = FTSWaiting;
   else
      mFCState = FTSDisabled;
}

bool RenderFormatToken::isEnabled() const
{
   return (mFCState != FTSDisabled);
}

void RenderFormatToken::initPersistFields()
{
   addProtectedField("format", TypeEnum, Offset(mColorFormat, RenderFormatToken), 
      &_setFmt, &defaultProtectedGetFn, 1, &gTextureFormatEnumTable, 
      "Sets the color buffer format for this token.");

   addProtectedField("depthFormat", TypeEnum, Offset(mDepthFormat, RenderFormatToken), 
      &_setFmt, &defaultProtectedGetFn, 1, &gTextureFormatEnumTable, 
      "Sets the depth/stencil buffer format for this token.");

   addField("copyEffect", TypeSimObjectPtr, Offset(mCopyPostEffect, RenderFormatToken),
      "This PostEffect will be run when the render target is changed to the format specified "
      "by this token. It is used to copy/format data into the token rendertarget");

   addField("resolveEffect", TypeSimObjectPtr, Offset(mResolvePostEffect, RenderFormatToken),
      "This PostEffect will be run when the render target is changed back to the format "
      "active prior to this token. It is used to copy/format data from the token rendertarget to the backbuffer.");

   addField("aaLevel", TypeS32, Offset(mTargetAALevel, RenderFormatToken), 
      "Anti-ailiasing level for the this token. 0 disables, -1 uses adapter default.");

   Parent::initPersistFields();
}


bool RenderFormatToken::_handleGFXEvent(GFXDevice::GFXDeviceEventType event)
{
#define _SWAP_CHAIN_HELPER(swapchainidx, swapchainsz) { swapchainidx++; swapchainidx = swapchainidx < swapchainsz ? swapchainidx : 0; }
   switch(event)
   {
   case GFXDevice::deStartOfFrame:
      _SWAP_CHAIN_HELPER( mTargetChainIdx, TargetChainLength );
      break;
   default:
      break;
   }
#undef _SWAP_CHAIN_HELPER

   return true;
}

void RenderFormatToken::_onTextureEvent( GFXTexCallbackCode code )
{
   if(code == GFXZombify)
   {
      _teardownTargets();
      mTargetUpdatePending = true;
   }
}

void RenderFormatToken::setupSamplerState( GFXSamplerStateDesc *desc ) const
{
   desc->addressModeU = GFXAddressClamp;
   desc->addressModeV = GFXAddressClamp;
   desc->minFilter = GFXTextureFilterPoint;
   desc->magFilter = GFXTextureFilterPoint;
   desc->mipFilter = GFXTextureFilterPoint;
}

GFXTextureObject* RenderFormatToken::getTargetTexture( U32 mrtIndex ) const
{
   return mTargetColorTexture[mTargetChainIdx].getPointer();
}

bool RenderFormatToken::onAdd()
{
   if(!Parent::onAdd())
      return false;

   if(getName())
      MatTextureTarget::registerTarget(getName(), this);

   return true;
}

void RenderFormatToken::onRemove()
{
   MatTextureTarget::unregisterTarget(getName(), this);

   Parent::onRemove();
}