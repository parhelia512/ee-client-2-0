//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "renderInstance/renderTexTargetBinManager.h"

#include "shaderGen/conditionerFeature.h"
#include "core/util/safeDelete.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "sceneGraph/sceneState.h"
#include "sceneGraph/sceneGraph.h"
#include "lighting/shadowMap/blurTexture.h" // TODO: Move/generalize this file

IMPLEMENT_CONOBJECT(RenderTexTargetBinManager);


RenderTexTargetBinManager::RenderTexTargetBinManager(const GFXFormat targetFormat /* = DefaultTargetFormat */,
                                                     const Point2I &targetSize /* = Point2I */,
                                                     const U32 targetChainLength /* = DefaultTargetChainLength */)
: Parent(), mTargetFormat(targetFormat), mTargetSize(targetSize), mTargetScale(1.0f, 1.0f), mTargetSizeType(FixedSize),
  mTargetChainLength(targetChainLength), mTargetChainIdx(0), mNumRenderTargets(1),
  mTargetChain(NULL), mTargetChainTextures(NULL), mBlur(NULL), mScratchTexture(NULL),
#ifndef TORQUE_SHIPPING
  m_NeedsOnPostRender(false)
#endif
{
   GFXDevice::getDeviceEventSignal().notify(this, &RenderTexTargetBinManager::_handleGFXEvent);
   GFXTextureManager::addEventDelegate( this, &RenderTexTargetBinManager::_onTextureEvent );

   _setupTargets();
}

//------------------------------------------------------------------------------

RenderTexTargetBinManager::RenderTexTargetBinManager(const RenderInstType& ritype, F32 renderOrder, F32 processAddOrder,
                                                     const GFXFormat targetFormat /* = DefaultTargetFormat */,
                                                     const Point2I &targetSize /* = Point2I */,
                                                     const U32 targetChainLength /* = DefaultTargetChainLength */)
: Parent(ritype, renderOrder, processAddOrder), mTargetFormat(targetFormat), mTargetSize(targetSize), mTargetScale(1.0f, 1.0f), mTargetSizeType(FixedSize),
  mTargetChainLength(targetChainLength), mTargetChainIdx(0), mNumRenderTargets(1),
  mTargetChain(NULL), mTargetChainTextures(NULL), mBlur(NULL), mScratchTexture(NULL),
#ifndef TORQUE_SHIPPING
  m_NeedsOnPostRender(false)
#endif
{
   GFXDevice::getDeviceEventSignal().notify(this, &RenderTexTargetBinManager::_handleGFXEvent);
   GFXTextureManager::addEventDelegate( this, &RenderTexTargetBinManager::_onTextureEvent );
}

//------------------------------------------------------------------------------

RenderTexTargetBinManager::~RenderTexTargetBinManager()
{
   _teardownTargets();

   GFXTextureManager::removeEventDelegate( this, &RenderTexTargetBinManager::_onTextureEvent );
   GFXDevice::getDeviceEventSignal().remove(this, &RenderTexTargetBinManager::_handleGFXEvent);
   SAFE_DELETE(mBlur);
   SAFE_DELETE(mScratchTexture);
}

//------------------------------------------------------------------------------

bool RenderTexTargetBinManager::onAdd()
{
   if(!Parent::onAdd())
      return false;

   _setupTargets();

   return true;
}

//------------------------------------------------------------------------------

static EnumTable::Enums gSizeTypeEnums[] =
{
   { RenderTexTargetBinManager::WindowSize, "windowsize" },
   { RenderTexTargetBinManager::WindowSizeScaled,  "windowsizescaled" },
   { RenderTexTargetBinManager::FixedSize,  "fixedsize" },
};
EnumTable gSizeTypeEnumTable( 2, gSizeTypeEnums );

void RenderTexTargetBinManager::initPersistFields()
{
   // TOM_TODO:
   //addField( "targetScale", mTargetScale );
   //addPropertyNOPS( "targetSizeType", mTargetSizeType)->setEnumTable(gSizeTypeEnumTable);
   //addPropertyNOPS<S32>( "targetFormat")->setEnumTable(gTextureFormatEnumTable)->addGet(this, &RenderTexTargetBinManager::getTargetFormatConsole)->addSet(this, &RenderTexTargetBinManager::setTargetFormatConsole);
   //addProperty<bool>( "blur" )->addSet(this, &RenderTexTargetBinManager::setBlur)->addGet(this, &RenderTexTargetBinManager::getBlur);

   Parent::initPersistFields();
}

//------------------------------------------------------------------------------

bool RenderTexTargetBinManager::setTargetSize(const Point2I &newTargetSize)
{
   if ( mTargetSize.x >= newTargetSize.x &&
        mTargetSize.y >= newTargetSize.y )
      return true;

   mTargetSize = newTargetSize;
   mTargetViewport.set( Point2I::Zero, mTargetSize );

   return _updateTargets();
}

//------------------------------------------------------------------------------

bool RenderTexTargetBinManager::setTargetFormat(const GFXFormat &newTargetFormat)
{
   if(mTargetFormat == newTargetFormat)
      return true;

   mTargetFormat = newTargetFormat;
   ConditionerFeature *conditioner = getTargetConditioner();
   if(conditioner)
      conditioner->setBufferFormat(mTargetFormat);

   return _updateTargets();
}

//------------------------------------------------------------------------------

void RenderTexTargetBinManager::setTargetChainLength(const U32 chainLength)
{
   if(mTargetChainLength != chainLength)
   {
      mTargetChainLength = chainLength;
      _setupTargets();
   }
}

GFXTextureObject* RenderTexTargetBinManager::getTargetTexture( U32 mrtIndex, S32 chainIndex ) const
{
   const U32 chainIdx = ( chainIndex > -1 ) ? chainIndex : mTargetChainIdx;
   if(chainIdx < mTargetChainLength)
      return mTargetChainTextures[chainIdx][mrtIndex];
   return NULL;
}

//------------------------------------------------------------------------------

bool RenderTexTargetBinManager::_updateTargets()
{
   PROFILE_SCOPE(RenderTexTargetBinManager_updateTargets);

   bool ret = true;

   // Update the target size
   for( U32 i = 0; i < mTargetChainLength; i++ )
   {
      if( !mTargetChain[i] )
         mTargetChain[i] = GFX->allocRenderToTextureTarget();

      for( U32 j = 0; j < mNumRenderTargets; j++ )
      {
         ret &= mTargetChainTextures[i][j].set( mTargetSize.x, mTargetSize.y, mTargetFormat,
            &GFXDefaultRenderTargetProfile, avar( "%s() - (line %d)", __FUNCTION__, __LINE__ ),
            1, GFXTextureManager::AA_MATCH_BACKBUFFER );

         mTargetChain[i]->attachTexture( GFXTextureTarget::RenderSlot(GFXTextureTarget::Color0 + j), mTargetChainTextures[i][j] );
      }
   }

   // Update the scratch texture
   if(mScratchTexture)
      ret &= mScratchTexture->set( mTargetSize.x, mTargetSize.y, mTargetFormat,
         &GFXDefaultRenderTargetProfile, avar( "%s() - (line %d)", __FUNCTION__, __LINE__ )  );

   return ret;
}

//------------------------------------------------------------------------------

bool RenderTexTargetBinManager::_handleGFXEvent(GFXDevice::GFXDeviceEventType event)
{
#define _SWAP_CHAIN_HELPER(swapchainidx, swapchainsz) { swapchainidx++; swapchainidx = swapchainidx < swapchainsz ? swapchainidx : 0; }
   switch(event)
   {
      case GFXDevice::deStartOfFrame:
         _SWAP_CHAIN_HELPER( mTargetChainIdx, mTargetChainLength );
         break;
      default:
         break;
   }
#undef _SWAP_CHAIN_HELPER

   return true;
}

//------------------------------------------------------------------------------

void RenderTexTargetBinManager::_onTextureEvent( GFXTexCallbackCode code )
{
   switch(code)
   {
      case GFXZombify:
         _teardownTargets();
         break;

      case GFXResurrect:
         _setupTargets();
         break;
   }
}

//------------------------------------------------------------------------------

bool RenderTexTargetBinManager::_setupTargets()
{
   _teardownTargets();

   mTargetChain = new GFXTextureTargetRef[mTargetChainLength];
   mTargetChainTextures = new GFXTexHandle*[mTargetChainLength];

   for( U32 i = 0; i < mTargetChainLength; i++ )
      mTargetChainTextures[i] = new GFXTexHandle[mNumRenderTargets];

   mTargetChainIdx = 0;

   mTargetSize = Point2I::Zero;
   //_updateTargets();

   return true;
}

//------------------------------------------------------------------------------

void RenderTexTargetBinManager::_teardownTargets()
{
   SAFE_DELETE_ARRAY(mTargetChain);
   if(mTargetChainTextures != NULL)
   {
      for( U32 i = 0; i < mTargetChainLength; i++ )
         SAFE_DELETE_ARRAY(mTargetChainTextures[i]);
   }
   SAFE_DELETE_ARRAY(mTargetChainTextures);
}

//------------------------------------------------------------------------------

GFXTextureTargetRef RenderTexTargetBinManager::_getTextureTarget(const U32 idx /* = 0 */)
{
   return mTargetChain[idx];
}

//------------------------------------------------------------------------------

bool RenderTexTargetBinManager::_onPreRender(SceneState * state, bool preserve /* = false */)
{
   PROFILE_SCOPE(RenderTexTargetBinManager_onPreRender);

#ifndef TORQUE_SHIPPING
   AssertFatal( m_NeedsOnPostRender == false, "_onPostRender not called on RenderTexTargetBinManager, or sub-class." );
   m_NeedsOnPostRender = false;
#endif

   // Update the render target size
   const Point2I &rtSize = GFX->getActiveRenderTarget()->getSize();
   switch(mTargetSizeType)
   {
   case WindowSize:
      setTargetSize(rtSize);
      break;
   case WindowSizeScaled:
      {
         Point2I scaledTargetSize(mFloor(rtSize.x * mTargetScale.x), mFloor(rtSize.y * mTargetScale.y));
         setTargetSize(scaledTargetSize);
         break;
      }
   case FixedSize:
      // No adjustment necessary
      break;
   }

   if( mTargetChainLength == 0 )
      return false;

   GFXTextureTargetRef binTarget = _getTextureTarget(mTargetChainIdx);

   if( binTarget.isNull() )
      return false;

   // Attach active depth target texture
   binTarget->attachTexture(GFXTextureTarget::DepthStencil, getParentManager()->getDepthTargetTexture());

   // Preserve contents
   if(preserve)
      GFX->getActiveRenderTarget()->preserve();

   GFX->pushActiveRenderTarget();
   GFX->setActiveRenderTarget(binTarget);
   GFX->setViewport(mTargetViewport);

#ifndef TORQUE_SHIPPING
   m_NeedsOnPostRender = true;
#endif

   return true;
}

//------------------------------------------------------------------------------

void RenderTexTargetBinManager::_onPostRender()
{
   PROFILE_SCOPE(RenderTexTargetBinManager_onPostRender);

#ifndef TORQUE_SHIPPING
   m_NeedsOnPostRender = false;
#endif
   GFXTextureTargetRef binTarget = _getTextureTarget(mTargetChainIdx);
   binTarget->resolve();

   GFX->popActiveRenderTarget();

   // Apply blur
   if(mBlur && mScratchTexture)
      mBlur->blur( getTargetTexture( 0 ), *mScratchTexture );
}

void RenderTexTargetBinManager::setBlur( const bool enableBlur )
{
   if(enableBlur == (mBlur != NULL))
      return;

   SAFE_DELETE(mBlur);
   SAFE_DELETE(mScratchTexture);
#ifndef TORQUE_DEDICATED
   if(enableBlur)
   {
      mBlur = new BlurOp;
      mBlur->init("BlurDepthShader", mTargetSize.x, mTargetSize.y);
      mScratchTexture = new GFXTexHandle;
      _updateTargets();
   }
#endif
}

//------------------------------------------------------------------------------

void RenderTexTargetBinManager::setupSamplerState( GFXSamplerStateDesc *desc ) const
{
   desc->addressModeU = GFXAddressClamp;
   desc->addressModeV = GFXAddressClamp;
   desc->minFilter = GFXTextureFilterPoint;
   desc->magFilter = GFXTextureFilterPoint;
   desc->mipFilter = GFXTextureFilterPoint;
}
