//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXD3D9DEVICE_H_
#define _GFXD3D9DEVICE_H_

#ifdef TORQUE_OS_XENON
#  include "platformXbox/platformXbox.h"
#else
#  include <d3dx9.h>
#  include "platformWin32/platformWin32.h"
#endif
#ifndef _GFXD3D9STATEBLOCK_H_
#include "gfx/D3D9/gfxD3D9StateBlock.h"
#endif
#ifndef _GFXD3DTEXTUREMANAGER_H_
#include "gfx/D3D9/gfxD3D9TextureManager.h"
#endif
#ifndef _GFXD3D9CUBEMAP_H_
#include "gfx/D3D9/gfxD3D9Cubemap.h"
#endif
#ifndef _GFXD3D9PRIMITIVEBUFFER_H_
#include "gfx/D3D9/gfxD3D9PrimitiveBuffer.h"
#endif
#ifndef _GFXINIT_H_
#include "gfx/gfxInit.h"
#endif
#ifndef _PLATFORMDLIBRARY_H
#include "platform/platformDlibrary.h"
#endif

#include "DxErr.h"

inline void D3D9Assert( HRESULT hr, const char *info ) 
{
#if defined( TORQUE_DEBUG )
   if( FAILED( hr ) ) 
   {
      char buf[256];
      dSprintf( buf, 256, "%s\n%s\n%s", DXGetErrorStringA( hr ), DXGetErrorDescriptionA( hr ), info );
      AssertFatal( false, buf ); 
      //      DXTrace( __FILE__, __LINE__, hr, info, true );
   }
#endif
}


// Typedefs
#define D3DX_FUNCTION(fn_name, fn_return, fn_args) \
   typedef fn_return (WINAPI *D3DXFNPTR##fn_name##)##fn_args##;
#include "gfx/D3D9/d3dx9Functions.h"
#undef D3DX_FUNCTION

// Function table
struct D3DXFNTable
{
   D3DXFNTable() : isLoaded( false ){};
   bool isLoaded;
   DLibraryRef dllRef;
#define D3DX_FUNCTION(fn_name, fn_return, fn_args) \
   D3DXFNPTR##fn_name fn_name;
#include "gfx/D3D9/d3dx9Functions.h"
#undef D3DX_FUNCTION
};

#define GFXD3DX static_cast<GFXD3D9Device *>(GFX)->smD3DX 

class GFXResource;
class GFXD3D9ShaderConstBuffer;

//------------------------------------------------------------------------------

class GFXD3D9Device : public GFXDevice
{
   friend class GFXResource;
   friend class GFXD3D9PrimitiveBuffer;
   friend class GFXD3D9VertexBuffer;
   friend class GFXD3D9TextureObject;
   friend class GFXPCD3D9TextureTarget;
   friend class GFXPCD3D9WindowTarget;

   typedef GFXDevice Parent;

protected:

   MatrixF mTempMatrix;    ///< Temporary matrix, no assurances on value at all
   RectI mClipRect;

   typedef StrongRefPtr<GFXD3D9VertexBuffer> RPGDVB;
   Vector<RPGDVB> mVolatileVBList;

   /// Used to lookup a vertex declaration for the vertex format.
   /// @see allocVertexDecl
   typedef Map<String,IDirect3DVertexDeclaration9*> VertexDeclMap;
   VertexDeclMap mVertexDecls;

   IDirect3DSurface9 *mDeviceBackbuffer;
   IDirect3DSurface9 *mDeviceDepthStencil;
   IDirect3DSurface9 *mDeviceColor;

   GFXD3D9VertexBuffer *mCurrentOpenAllocVB;
   GFXD3D9VertexBuffer *mCurrentVB;
   void *mCurrentOpenAllocVertexData;

   static void initD3DXFnTable();
   //-----------------------------------------------------------------------
   StrongRefPtr<GFXD3D9PrimitiveBuffer> mDynamicPB;                       ///< Dynamic index buffer
   GFXD3D9PrimitiveBuffer *mCurrentOpenAllocPB;
   GFXD3D9PrimitiveBuffer *mCurrentPB;

   IDirect3DVertexShader9 *mLastVertShader;
   IDirect3DPixelShader9 *mLastPixShader;

   S32 mCreateFenceType;

   LPDIRECT3D9       mD3D;        ///< D3D Handle
   LPDIRECT3DDEVICE9 mD3DDevice;  ///< Handle for D3DDevice

   U32  mAdapterIndex;            ///< Adapter index because D3D supports multiple adapters

   F32 mPixVersion;
   U32 mNumSamplers;               ///< Profiled (via caps)
   U32 mNumRenderTargets;          ///< Profiled (via caps)

   D3DMULTISAMPLE_TYPE mMultisampleType;
   DWORD mMultisampleLevel;

   bool mOcclusionQuerySupported;

   /// To manage creating and re-creating of these when device is aquired
   void reacquireDefaultPoolResources();

   /// To release all resources we control from D3DPOOL_DEFAULT
   void releaseDefaultPoolResources();

   /// This you will probably never, ever use, but it is used to generate the code for
   /// the initStates() function
   void regenStates();

   virtual GFXD3D9VertexBuffer* findVBPool( const GFXVertexFormat *vertexFormat, U32 numVertsNeeded );
   virtual GFXD3D9VertexBuffer* createVBPool( const GFXVertexFormat *vertexFormat, U32 vertSize );

#ifdef TORQUE_DEBUG
   /// @name Debug Vertex Buffer information/management
   /// @{

   ///
   U32 mNumAllocatedVertexBuffers; ///< To keep track of how many are allocated and freed
   GFXD3D9VertexBuffer *mVBListHead;
   void addVertexBuffer( GFXD3D9VertexBuffer *buffer );
   void removeVertexBuffer( GFXD3D9VertexBuffer *buffer );
   void logVertexBuffers();
   /// @}
#endif

   // State overrides
   // {

   ///
   virtual void setTextureInternal(U32 textureUnit, const GFXTextureObject* texture);

   /// Called by GFXDevice to create a device specific stateblock
   virtual GFXStateBlockRef createStateBlockInternal(const GFXStateBlockDesc& desc);
   /// Called by GFXDevice to actually set a stateblock.
   virtual void setStateBlockInternal(GFXStateBlock* block, bool force);

   /// Track the last const buffer we've used.  Used to notify new constant buffers that
   /// they should send all of their constants up
   StrongRefPtr<GFXD3D9ShaderConstBuffer> mCurrentConstBuffer;
   /// Called by base GFXDevice to actually set a const buffer
   virtual void setShaderConstBufferInternal(GFXShaderConstBuffer* buffer);

   // CodeReview - How exactly do we want to deal with this on the Xenon?
   // Right now it's just in an #ifndef in gfxD3D9Device.cpp - AlexS 4/11/07
   virtual void setLightInternal(U32 lightStage, const GFXLightInfo light, bool lightEnable);
   virtual void setLightMaterialInternal(const GFXLightMaterial mat);
   virtual void setGlobalAmbientInternal(ColorF color);

   virtual void initStates()=0;
   // }

   // Index buffer management
   // {
   virtual void _setPrimitiveBuffer( GFXPrimitiveBuffer *buffer );
   virtual void drawIndexedPrimitive(  GFXPrimitiveType primType, 
                                       U32 startVertex, 
                                       U32 minIndex, 
                                       U32 numVerts, 
                                       U32 startIndex, 
                                       U32 primitiveCount );
   // }

   virtual GFXShader* createShader();

   /// Device helper function
   virtual D3DPRESENT_PARAMETERS setupPresentParams( const GFXVideoMode &mode, const HWND &hwnd ) const = 0;
   
public:
   static D3DXFNTable smD3DX;

   static GFXDevice *createInstance( U32 adapterIndex );

   GFXTextureObject* createRenderSurface( U32 width, U32 height, GFXFormat format, U32 mipLevel );

   /// Constructor
   /// @param   d3d   Direct3D object to instantiate this device with
   /// @param   index   Adapter index since D3D can use multiple graphics adapters
   GFXD3D9Device( LPDIRECT3D9 d3d, U32 index );
   virtual ~GFXD3D9Device();

   // Activate/deactivate
   // {
   virtual void init( const GFXVideoMode &mode, PlatformWindow *window = NULL ) = 0;

   virtual void preDestroy() { Parent::preDestroy(); if(mTextureManager) mTextureManager->kill(); }

   GFXAdapterType getAdapterType(){ return Direct3D9; }

   virtual GFXCubemap *createCubemap();

   virtual F32  getPixelShaderVersion() const { return mPixVersion; }
   virtual void setPixelShaderVersion( F32 version ){ mPixVersion = version; }
   virtual void disableShaders();
   virtual void setShader( GFXShader *shader );
   virtual U32  getNumSamplers() const { return mNumSamplers; }
   virtual U32  getNumRenderTargets() const { return mNumRenderTargets; }
   // }

   // Misc rendering control
   // {
   virtual void clear( U32 flags, ColorI color, F32 z, U32 stencil );
   virtual bool beginSceneInternal();
   virtual void endSceneInternal();

   virtual void setClipRect( const RectI &rect );
   virtual const RectI& getClipRect() const { return mClipRect; }

   // }

   /// @name Render Targets
   /// @{
   virtual void _updateRenderTargets();
   /// @}

   // Vertex/Index buffer management
   // {
   void setVB( GFXVertexBuffer *buffer );

   virtual GFXVertexBuffer* allocVertexBuffer(  U32 numVerts, 
                                                const GFXVertexFormat *vertexFormat,
                                                U32 vertSize,
                                                GFXBufferType bufferType );
   virtual GFXPrimitiveBuffer *allocPrimitiveBuffer( U32 numIndices, U32 numPrimitives, GFXBufferType bufferType );
   virtual void deallocVertexBuffer( GFXD3D9VertexBuffer *vertBuff );

   void allocVertexDecl( GFXD3D9VertexBuffer *vertBuff );
   // }

   virtual U32 getMaxDynamicVerts() { return MAX_DYNAMIC_VERTS; }
   virtual U32 getMaxDynamicIndices() { return MAX_DYNAMIC_INDICES; }

   // Rendering
   // {
   virtual void drawPrimitive( GFXPrimitiveType primType, U32 vertexStart, U32 primitiveCount );
   // }

   virtual LPDIRECT3DDEVICE9 getDevice(){ return mD3DDevice; }
   virtual LPDIRECT3D9 getD3D() { return mD3D; }

   /// Reset
   virtual void reset( D3DPRESENT_PARAMETERS &d3dpp ) = 0;

   GFXShaderRef mGenericShader[GS_COUNT];

   virtual void setupGenericShaders( GenericShaderType type  = GSColor );

   // Function only really used on the, however a centralized function for
   // destroying resources is probably a good thing -patw
   virtual void destroyD3DResource( IDirect3DResource9 *d3dResource ) { SAFE_RELEASE( d3dResource ); }; 

   inline virtual F32 getFillConventionOffset() const { return 0.5f; }
   virtual void doParanoidStateCheck();

   GFXFence *createFence();

   GFXOcclusionQuery* createOcclusionQuery();   

   // Default multisample parameters
   D3DMULTISAMPLE_TYPE getMultisampleType() const { return mMultisampleType; }
   DWORD getMultisampleLevel() const { return mMultisampleLevel; } 
};


#endif // _GFXD3D9DEVICE_H_
