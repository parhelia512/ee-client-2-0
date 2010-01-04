//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXGLDEVICE_H_
#define _GFXGLDEVICE_H_

#include "platform/platform.h"

#include "gfx/gfxDevice.h"
#include "gfx/gfxInit.h"

#ifndef GL_GGL_H
#include "gfx/gl/ggl/ggl.h"
#endif

#include "windowManager/platformWindow.h"
#include "gfx/gfxFence.h"
#include "gfx/gfxResource.h"
#include "gfx/gl/gfxGLStateBlock.h"

class GFXGLVertexBuffer;
class GFXGLPrimitiveBuffer;
class GFXGLTextureTarget;
class GFXGLCubemap;

class GFXGLDevice : public GFXDevice
{
public:
   void zombify();
   void resurrect();
   GFXGLDevice(U32 adapterIndex);
   virtual ~GFXGLDevice();

   static void enumerateAdapters( Vector<GFXAdapter*> &adapterList );
   static GFXDevice *createInstance( U32 adapterIndex );

   virtual void init( const GFXVideoMode &mode, PlatformWindow *window = NULL );

   virtual void activate() { }
   virtual void deactivate() { }
   virtual GFXAdapterType getAdapterType() { return OpenGL; }

   virtual void enterDebugEvent(ColorI color, const char *name) { }
   virtual void leaveDebugEvent() { }
   virtual void setDebugMarker(ColorI color, const char *name) { }

   virtual void enumerateVideoModes();

   virtual U32 getTotalVideoMemory();

   virtual GFXCubemap * createCubemap();

   virtual F32 getFillConventionOffset() const { return 0.0f; }


   ///@}

   /// @name Render Target functions
   /// @{

   ///
   virtual GFXTextureTarget *allocRenderToTextureTarget();
   virtual GFXWindowTarget *allocWindowTarget(PlatformWindow *window);
   virtual void _updateRenderTargets();

   ///@}

   /// @name Shader functions
   /// @{
   virtual F32 getPixelShaderVersion() const { return mPixelShaderVersion; }
   virtual void  setPixelShaderVersion( F32 version ) { mPixelShaderVersion = version; }
   
   virtual void setShader(GFXShader* shd);
   virtual void disableShaders(); ///< Equivalent to setShader(NULL)
   
   /// @attention GL cannot check if the given format supports blending or filtering!
   virtual GFXFormat selectSupportedFormat(GFXTextureProfile *profile,
	   const Vector<GFXFormat> &formats, bool texture, bool mustblend, bool mustfilter);
      
   /// Returns the number of texture samplers that can be used in a shader rendering pass
   virtual U32 getNumSamplers() const;

   /// Returns the number of simultaneous render targets supported by the device.
   virtual U32 getNumRenderTargets() const;

   virtual GFXShader* createShader();
      
   virtual void clear( U32 flags, ColorI color, F32 z, U32 stencil );
   virtual bool beginSceneInternal();
   virtual void endSceneInternal();

   virtual void drawPrimitive( GFXPrimitiveType primType, U32 vertexStart, U32 primitiveCount );

   virtual void drawIndexedPrimitive(  GFXPrimitiveType primType, 
                                       U32 startVertex, 
                                       U32 minIndex, 
                                       U32 numVerts, 
                                       U32 startIndex, 
                                       U32 primitiveCount );

   virtual void setClipRect( const RectI &rect );
   virtual const RectI &getClipRect() const { return mClip; }

   virtual void preDestroy() { Parent::preDestroy(); }

   virtual U32 getMaxDynamicVerts() { return MAX_DYNAMIC_VERTS; }
   virtual U32 getMaxDynamicIndices() { return MAX_DYNAMIC_INDICES; }
   
   GFXFence *createFence();
   
   GFXOcclusionQuery* createOcclusionQuery();

   GFXGLStateBlockRef getCurrentStateBlock() { return mCurrentGLStateBlock; }
   
   virtual void setupGenericShaders( GenericShaderType type = GSColor );
   
   ///
   bool supportsAnisotropic() const { return mSupportsAnisotropic; }
   
   ///
   bool supportsMipLodBias() const { return mSupportsMipLodBias; }
   
protected:   
   /// Called by GFXDevice to create a device specific stateblock
   virtual GFXStateBlockRef createStateBlockInternal(const GFXStateBlockDesc& desc);
   /// Called by GFXDevice to actually set a stateblock.
   virtual void setStateBlockInternal(GFXStateBlock* block, bool force);   

   /// Called by base GFXDevice to actually set a const buffer
   virtual void setShaderConstBufferInternal(GFXShaderConstBuffer* buffer);

   virtual void setTextureInternal(U32 textureUnit, const GFXTextureObject*texture);
   virtual void setCubemapInternal(U32 cubemap, const GFXGLCubemap* texture);

   virtual void setLightInternal(U32 lightStage, const GFXLightInfo light, bool lightEnable);
   virtual void setLightMaterialInternal(const GFXLightMaterial mat);
   virtual void setGlobalAmbientInternal(ColorF color);

   /// @name State Initalization.
   /// @{

   /// State initalization. This MUST BE CALLED in setVideoMode after the device
   /// is created.
   virtual void initStates() { }

   virtual void setMatrix( GFXMatrixType mtype, const MatrixF &mat );

   virtual GFXVertexBuffer *allocVertexBuffer(  U32 numVerts, 
                                                const GFXVertexFormat *vertexFormat,
                                                U32 vertSize, 
                                                GFXBufferType bufferType );
   virtual GFXPrimitiveBuffer *allocPrimitiveBuffer( U32 numIndices, U32 numPrimitives, GFXBufferType bufferType );
   
private:
   typedef GFXDevice Parent;
   
   friend class GFXGLTextureObject;
   friend class GFXGLCubemap;
   friend class GFXGLWindowTarget;
   friend class GFXGLPrimitiveBuffer;
   friend class GFXGLVertexBuffer;

   static GFXAdapter::CreateDeviceInstanceDelegate mCreateDeviceInstance; 

   U32 mAdapterIndex;
   
   StrongRefPtr<GFXGLVertexBuffer> mCurrentVB;
   StrongRefPtr<GFXGLPrimitiveBuffer> mCurrentPB;
   
   /// Since GL does not have separate world and view matrices we need to track them
   MatrixF m_mCurrentWorld;
   MatrixF m_mCurrentView;

   void* mContext;
   void* mPixelFormat;

   F32 mPixelShaderVersion;
   
   bool mSupportsAnisotropic;
   bool mSupportsMipLodBias;
   
   U32 mMaxShaderTextures;
   U32 mMaxFFTextures;

   RectI mClip;
   
   GFXGLStateBlockRef mCurrentGLStateBlock;
   
   GLenum mActiveTextureType[TEXTURE_STAGE_COUNT];
   
   Vector< StrongRefPtr<GFXGLVertexBuffer> > mVolatileVBs; ///< Pool of existing volatile VBs so we can reuse previously created ones
   Vector< StrongRefPtr<GFXGLPrimitiveBuffer> > mVolatilePBs; ///< Pool of existing volatile PBs so we can reuse previously created ones

   GLsizei primCountToIndexCount(GFXPrimitiveType primType, U32 primitiveCount);
   void preDrawPrimitive();
   void postDrawPrimitive(U32 primitiveCount);  
   
   GFXVertexBuffer* findVolatileVBO(U32 numVerts, const GFXVertexFormat *vertexFormat, U32 vertSize); ///< Returns an existing volatile VB which has >= numVerts and the same vert flags/size, or creates a new VB if necessary
   GFXPrimitiveBuffer* findVolatilePBO(U32 numIndices, U32 numPrimitives); ///< Returns an existing volatile PB which has >= numIndices, or creates a new PB if necessary
   
   void initGLState(); ///< Guaranteed to be called after all extensions have been loaded, use to init card profiler, shader version, max samplers, etc.
   
   GFXFence* _createPlatformSpecificFence(); ///< If our platform (e.g. OS X) supports a fence extenstion (e.g. GL_APPLE_fence) this will create one, otherwise returns NULL
   
   void setVB(GFXGLVertexBuffer* vb); ///< Sets mCurrentVB
   void setPB(GFXGLPrimitiveBuffer* pb); ///< Sets mCurrentPB
};

#endif
