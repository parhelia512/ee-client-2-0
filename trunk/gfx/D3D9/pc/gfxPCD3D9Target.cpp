#ifdef _MSC_VER
#pragma warning(disable: 4996) // turn off "deprecation" warnings
#endif

#include "gfx/D3D9/gfxD3D9Device.h"
#include "gfx/D3D9/gfxD3D9TextureObject.h"
#include "gfx/D3D9/gfxD3D9Cubemap.h"
#include "gfx/D3D9/gfxD3D9EnumTranslate.h"
#include "gfx/D3D9/pc/gfxPCD3D9Device.h"
#include "gfx/D3D9/pc/gfxPCD3D9Target.h"
#include "windowManager/win32/win32Window.h"

GFXPCD3D9TextureTarget::GFXPCD3D9TextureTarget() 
   :  mTargetSize( Point2I::Zero ),
      mTargetFormat( GFXFormatR8G8B8A8 )
{
   for(S32 i=0; i<MaxRenderSlotId; i++)
   {
      mTargets[i] = NULL;
      mResolveTargets[i] = NULL;
   }
}

GFXPCD3D9TextureTarget::~GFXPCD3D9TextureTarget()
{
   // Release anything we might be holding.
   for(S32 i=0; i<MaxRenderSlotId; i++)
   {
      mResolveTargets[i] = NULL;

      if( GFXDevice::devicePresent() )
      {
         static_cast<GFXD3D9Device *>( GFX )->destroyD3DResource( mTargets[i] );
         mTargets[i] = NULL;
      }
      else
         SAFE_RELEASE( mTargets[i] );
   }

   zombify();
}

void GFXPCD3D9TextureTarget::attachTexture( RenderSlot slot, GFXTextureObject *tex, U32 mipLevel/*=0*/, U32 zOffset /*= 0*/ )
{
   AssertFatal(slot < MaxRenderSlotId, "GFXPCD3D9TextureTarget::attachTexture - out of range slot.");

   // Mark state as dirty so device can know to update.
   invalidateState();

   // Release what we had, it's definitely going to change.
   static_cast<GFXD3D9Device *>( GFX )->destroyD3DResource( mTargets[slot] );
   mTargets[slot] = NULL;
   mResolveTargets[slot] = NULL;

   if(slot == Color0)
   {
      mTargetSize = Point2I::Zero;
      mTargetFormat = GFXFormatR8G8B8A8;
   }

   // Are we clearing?
   if(!tex)
   {
      // Yup - just exit, it'll stay NULL.      
      return;
   }


   // Take care of default targets
   if( tex == GFXTextureTarget::sDefaultDepthStencil )
   {
      mTargets[slot] = static_cast<GFXD3D9Device *>(mDevice)->mDeviceDepthStencil;
      mTargets[slot]->AddRef();
   }
   else
   {
      // Cast the texture object to D3D...
      AssertFatal(dynamic_cast<GFXD3D9TextureObject*>(tex), 
         "GFXPCD3D9TextureTarget::attachTexture - invalid texture object.");

      GFXD3D9TextureObject *d3dto = static_cast<GFXD3D9TextureObject*>(tex);

      // Grab the surface level.
      if( slot == DepthStencil )
      {
         mTargets[slot] = d3dto->getSurface();
         if ( mTargets[slot] )
            mTargets[slot]->AddRef();
      }
      else
      {
         // getSurface will almost always return NULL. It will only return non-NULL
         // if the surface that it needs to render to is different than the mip level
         // in the actual texture. This will happen with MSAA.
         if( d3dto->getSurface() == NULL )
         {
            D3D9Assert(d3dto->get2DTex()->GetSurfaceLevel(mipLevel, &mTargets[slot]), 
               "GFXPCD3D9TextureTarget::attachTexture - could not get surface level for the passed texture!");
         } 
         else 
         {
            mTargets[slot] = d3dto->getSurface();
            mTargets[slot]->AddRef();

            // Only assign resolve target if d3dto has a surface to give us.
            //
            // That usually means there is an MSAA target involved, which is why
            // the resolve is needed to get the data out of the target.
            mResolveTargets[slot] = d3dto;

            if ( tex && slot == Color0 )
            {
               mTargetSize.set( tex->getSize().x, tex->getSize().y );
               mTargetFormat = tex->getFormat();
            }
         }           
      }

      // Update surface size
      if(slot == Color0)
      {
         IDirect3DSurface9 *surface = mTargets[Color0];
         if ( surface )
         {
            D3DSURFACE_DESC sd;
            surface->GetDesc(&sd);
            mTargetSize = Point2I(sd.Width, sd.Height);

            S32 format = sd.Format;
            GFXREVERSE_LOOKUP( GFXD3D9TextureFormat, GFXFormat, format );
            mTargetFormat = (GFXFormat)format;
         }
      }
   }
}

void GFXPCD3D9TextureTarget::attachTexture( RenderSlot slot, GFXCubemap *tex, U32 face, U32 mipLevel/*=0*/ )
{
   AssertFatal(slot < MaxRenderSlotId, "GFXPCD3D9TextureTarget::attachTexture - out of range slot.");

   // Mark state as dirty so device can know to update.
   invalidateState();

   // Release what we had, it's definitely going to change.
   static_cast<GFXD3D9Device *>( GFX )->destroyD3DResource( mTargets[slot] );
   mTargets[slot] = NULL;
   mResolveTargets[slot] = NULL;

   // Cast the texture object to D3D...
   AssertFatal(!tex || dynamic_cast<GFXD3D9Cubemap*>(tex), 
      "GFXD3DTextureTarget::attachTexture - invalid cubemap object.");

   GFXD3D9Cubemap *cube = static_cast<GFXD3D9Cubemap*>(tex);

   if(slot == Color0)
   {
      mTargetSize = Point2I::Zero;
      mTargetFormat = GFXFormatR8G8B8A8;
   }

   // Are we clearing?
   if(!tex)
   {
      // Yup - just exit, it'll stay NULL.      
      return;
   }

   D3D9Assert(cube->mCubeTex->GetCubeMapSurface( (D3DCUBEMAP_FACES)face, mipLevel, &mTargets[slot] ),
      "GFXD3DTextureTarget::attachTexture - could not get surface level for the passed texture!");

   // Update surface size
   if(slot == Color0)
   {
      IDirect3DSurface9 *surface = mTargets[Color0];
      if ( surface )
      {
         D3DSURFACE_DESC sd;
         surface->GetDesc(&sd);
         mTargetSize = Point2I(sd.Width, sd.Height);

         S32 format = sd.Format;
         GFXREVERSE_LOOKUP( GFXD3D9TextureFormat, GFXFormat, format );
         mTargetFormat = (GFXFormat)format;
      }
   }
}

void GFXPCD3D9TextureTarget::activate()
{
   const U32 &numSimultaneousRTs = mDevice->getNumRenderTargets();

   LPDIRECT3DDEVICE9 d3dDevice = mDevice->getDevice();

   // Clear the state indicator.
   stateApplied();

   // Set all the surfaces into the appropriate slots.
   for(U32 i = 0; i < (Color4 - Color0); i++)
   {
      if ( i + GFXTextureTarget::Color0 <= numSimultaneousRTs )
      {
         D3D9Assert(d3dDevice->SetRenderTarget(i, mTargets[GFXTextureTarget::Color0 + i]), 
            avar("GFXPCD3D9TextureTarget::activate() - failed to set slot %d for texture target!", i) );
      }
   }

   // TODO: This is often the same shared depth buffer used by most
   // render targets.  Are we getting performance hit from setting it
   // multiple times... aside from the function call?

   D3D9Assert(d3dDevice->SetDepthStencilSurface(mTargets[GFXTextureTarget::DepthStencil]), 
      "GFXPCD3D9TextureTarget::activate() - failed to set depthstencil target!" );
}

void GFXPCD3D9TextureTarget::deactivate()
{
   LPDIRECT3DDEVICE9 d3dDevice = mDevice->getDevice();

   const U32 &numSimultaneousRTs = mDevice->getNumRenderTargets();

   // Set NULL to all slots but Color0, start with 'i = 1'
   for(U32 i = 1; i < (Color4 - Color0); i++)
   {
      if ( i < numSimultaneousRTs )
      {
         D3D9Assert(d3dDevice->SetRenderTarget(i, NULL), 
            avar("GFXPCD3D9TextureTarget::activate() - failed to set slot %d for texture target!", i) );
      }
   }
}

void GFXPCD3D9TextureTarget::resolve()
{
   for (U32 i = 0; i < MaxRenderSlotId; i++)
   {
      // We use existance @ mResolveTargets as a flag that we need to copy
      // data from the rendertarget into the texture.
      if (mResolveTargets[i])
      {
         IDirect3DSurface9 *surf;         
         mResolveTargets[i]->get2DTex()->GetSurfaceLevel( 0, &surf );         
         AssertFatal(dynamic_cast<GFXD3D9Device*>(getOwningDevice()), "Incorrect device type!");
         GFXD3D9Device* d = static_cast<GFXD3D9Device*>(getOwningDevice());
         d->getDevice()->StretchRect(mTargets[i], NULL, surf, NULL, D3DTEXF_NONE );
         surf->Release();
      }
   }
}

void GFXPCD3D9TextureTarget::resolveTo( GFXTextureObject *tex )
{
   if ( mTargets[Color0] == NULL )
      return;

   IDirect3DSurface9 *surf;
   ((GFXD3D9TextureObject*)(tex))->get2DTex()->GetSurfaceLevel( 0, &surf );
   mDevice->getDevice()->StretchRect( mTargets[Color0], NULL, surf, NULL, D3DTEXF_NONE );

   surf->Release();     
}

void GFXPCD3D9TextureTarget::zombify()
{
   for(int i = 0; i < MaxRenderSlotId; i++)
      attachTexture(RenderSlot(i), NULL);
}

void GFXPCD3D9TextureTarget::resurrect()
{

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


GFXPCD3D9WindowTarget::GFXPCD3D9WindowTarget()
{
   mSwapChain    = NULL;
   mDepthStencil = NULL;
   mWindow       = NULL;
   mDevice       = NULL;
   mBackbuffer   = NULL;
   mImplicit     = true;
}

GFXPCD3D9WindowTarget::~GFXPCD3D9WindowTarget()
{
   SAFE_RELEASE(mSwapChain);
   SAFE_RELEASE(mDepthStencil);
   SAFE_RELEASE(mBackbuffer);
}

void GFXPCD3D9WindowTarget::initPresentationParams()
{
   // Get some video mode related info.
   GFXVideoMode vm = mWindow->getVideoMode();

   // Do some validation...
   if(vm.fullScreen == true && mImplicit == false)
   {
      AssertISV(false, 
         "GFXPCD3D9WindowTarget::initPresentationParams - Cannot go fullscreen with secondary window!");
   }

   Win32Window *win = dynamic_cast<Win32Window*>(mWindow);
   AssertISV(win, "GFXPCD3D9WindowTarget::initPresentationParams() - got a non Win32Window window passed in! Did DX go crossplatform?");

   HWND hwnd = win->getHWND();

   // At some point, this will become GFXPCD3D9WindowTarget like trunk has,
   // so this cast isn't as bad as it looks. ;) BTR
   GFXPCD3D9Device* pcdevice = dynamic_cast<GFXPCD3D9Device*>(mDevice);
   mPresentationParams = pcdevice->setupPresentParams(vm, hwnd);

   if (mImplicit)
   {
      pcdevice->mMultisampleType = mPresentationParams.MultiSampleType;
      pcdevice->mMultisampleLevel = mPresentationParams.MultiSampleQuality;
   }
}

const Point2I GFXPCD3D9WindowTarget::getSize()
{
   return mWindow->getVideoMode().resolution; 
}

GFXFormat GFXPCD3D9WindowTarget::getFormat()
{ 
   S32 format = mPresentationParams.BackBufferFormat;
   GFXREVERSE_LOOKUP( GFXD3D9TextureFormat, GFXFormat, format );
   return (GFXFormat)format;
}

bool GFXPCD3D9WindowTarget::present()
{
   AssertFatal(mSwapChain, "GFXPCD3D9WindowTarget::present - no swap chain present to present!");
   HRESULT res = mSwapChain->Present(NULL, NULL, NULL, NULL, NULL);

   return (res == S_OK);
}

void GFXPCD3D9WindowTarget::setImplicitSwapChain()
{
   AssertFatal(mImplicit, "Invalid swap chain type!  Additional swap chains are created as needed");
   // Reacquire our swapchain & DS
   if(!mSwapChain)
      mDevice->getDevice()->GetSwapChain(0, &mSwapChain);
   if(!mDepthStencil)
      mDevice->getDevice()->GetDepthStencilSurface(&mDepthStencil);
   if (!mBackbuffer)      
      mSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &mBackbuffer);
}

void GFXPCD3D9WindowTarget::createAdditionalSwapChain()
{
   AssertFatal(!mImplicit, "Invalid swap chain type!  Implicit swap chains use the device");

   // Since we're not going to do a device reset for an additional swap
   // chain, we can just release our resources and regrab them.
   SAFE_RELEASE(mSwapChain);
   SAFE_RELEASE(mDepthStencil);
   SAFE_RELEASE(mBackbuffer);

   // If there's a fullscreen window active, don't try to create these additional swap chains.
   // CodeReview, we need to store the window target with the implicit swap chain better, this line below 
   // could fail if the current render target isn't what we expect.
   GFXPCD3D9WindowTarget* currTarget = dynamic_cast<GFXPCD3D9WindowTarget*>(mDevice->getActiveRenderTarget());
   if (currTarget && currTarget->getWindow()->getVideoMode().fullScreen)
      return;

   // Setup our presentation params.
   initPresentationParams();

   // Create our resources!
   D3D9Assert(mDevice->getDevice()->CreateAdditionalSwapChain(&mPresentationParams, &mSwapChain),
      "GFXPCD3D9WindowTarget::createAdditionalSwapChain - couldn't reallocate additional swap chain!");
   D3D9Assert(mDevice->getDevice()->CreateDepthStencilSurface(mPresentationParams.BackBufferWidth, mPresentationParams.BackBufferHeight,
      D3DFMT_D24S8, mPresentationParams.MultiSampleType, mPresentationParams.MultiSampleQuality, false, &mDepthStencil, NULL), 
      "GFXPCD3D9WindowTarget::createAdditionalSwapChain: Unable to create stencil/depth surface");
   D3D9Assert(mSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &mBackbuffer),
      "GFXPCD3D9WindowTarget::createAdditionalSwapChain: Unable to get backbuffer!");   
}

void GFXPCD3D9WindowTarget::resetMode()
{
   mWindow->setSuppressReset(true);

   if (mSwapChain)
   {
      D3DPRESENT_PARAMETERS pp;
      mSwapChain->GetPresentParameters(&pp);
      bool ppFullscreen = !pp.Windowed;
      Point2I backbufferSize(pp.BackBufferWidth, pp.BackBufferHeight);

      if ((backbufferSize == getSize()) && (ppFullscreen == mWindow->getVideoMode().fullScreen))
         return;   
   }

   // So, the video mode has changed - if we're an additional swap chain
   // just kill the swapchain and reallocate to match new vid mode.
   if(mImplicit == false)
   {
      createAdditionalSwapChain();
   }
   else
   {
      // Setup our presentation params.
      initPresentationParams();

      // Otherwise, we have to reset the device, if we're the implicit swapchain.
      mDevice->reset(mPresentationParams);
   }

   // Update our size, too.
   mSize = Point2I(mPresentationParams.BackBufferWidth, mPresentationParams.BackBufferHeight);      

   mWindow->setSuppressReset(false);
}

void GFXPCD3D9WindowTarget::zombify()
{
   // Release our resources
   SAFE_RELEASE(mSwapChain);
   SAFE_RELEASE(mDepthStencil);
   SAFE_RELEASE(mBackbuffer);
}

void GFXPCD3D9WindowTarget::resurrect()
{
   if(mImplicit)
   {
      setImplicitSwapChain();
   }
   else if(!mSwapChain)
   {
      createAdditionalSwapChain();
   }
}

void GFXPCD3D9WindowTarget::activate()
{
   LPDIRECT3DDEVICE9 d3dDevice = mDevice->getDevice();
   d3dDevice->SetRenderTarget(0, mBackbuffer);
   d3dDevice->SetDepthStencilSurface(mDepthStencil);

   D3DPRESENT_PARAMETERS pp;

   mSwapChain->GetPresentParameters(&pp);

   // Update our video mode here, too.
   GFXVideoMode vm;
   vm = mWindow->getVideoMode();
   vm.resolution.x = pp.BackBufferWidth;
   vm.resolution.y = pp.BackBufferHeight;
   vm.fullScreen = !pp.Windowed;

   mSize = vm.resolution;
}

void GFXPCD3D9WindowTarget::resolveTo( GFXTextureObject *tex )
{
   IDirect3DSurface9 *surf;
   ((GFXD3D9TextureObject*)(tex))->get2DTex()->GetSurfaceLevel( 0, &surf );
   mDevice->getDevice()->StretchRect( mBackbuffer, NULL, surf, NULL, D3DTEXF_NONE );

   surf->Release();    
}