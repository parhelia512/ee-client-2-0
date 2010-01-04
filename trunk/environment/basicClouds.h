
#ifndef _BASICCLOUDS_H_
#define _BASICCLOUDS_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif
#ifndef _GFXPRIMITIVEBUFFER_H_
#include "gfx/gfxPrimitiveBuffer.h"
#endif
#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif


class BasicClouds : public SceneObject
{
   typedef SceneObject Parent;

   enum 
   { 
      BasicCloudsMask = Parent::NextFreeMask,      
      NextFreeMask = Parent::NextFreeMask << 1,
   };  

   #define TEX_COUNT 3

public:

   BasicClouds();
   virtual ~BasicClouds() {}

   DECLARE_CONOBJECT( BasicClouds );

   // ConsoleObject
   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();
   virtual void inspectPostApply();   

   // NetObject
   virtual U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   virtual void unpackUpdate( NetConnection *conn, BitStream *stream );

   // SceneObject
   bool prepRenderImage( SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseZoneState=false);   
   void renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *mi );

protected:

   void _initTexture();
   void _initBuffers();
   void _initBuffer( F32 height, GFXVertexBufferHandle<GFXVertexPT> *vb, GFXPrimitiveBufferHandle *pb );

protected: 

   static U32 smVertStride;
   static U32 smStrideMinusOne;
   static U32 smVertCount;
   static U32 smTriangleCount;

   GFXTexHandle mTexture[TEX_COUNT];

   GFXStateBlockRef mStateblock;

   GFXShaderRef mShader;

   GFXShaderConstBufferRef mShaderConsts;
   GFXShaderConstHandle *mTimeSC; 
   GFXShaderConstHandle *mModelViewProjSC; 
   GFXShaderConstHandle *mTexScaleSC;
   GFXShaderConstHandle *mTexDirectionSC;
   GFXShaderConstHandle *mTexOffsetSC;

   GFXVertexBufferHandle<GFXVertexPT> mVB[TEX_COUNT];
   GFXPrimitiveBufferHandle mPB;    

   // Fields...

   bool mLayerEnabled[TEX_COUNT];
   String mTexName[TEX_COUNT];
   F32 mTexScale[TEX_COUNT];
   Point2F mTexDirection[TEX_COUNT];
   F32 mTexSpeed[TEX_COUNT];   
   Point2F mTexOffset[TEX_COUNT];
   F32 mHeight[TEX_COUNT];
};


#endif // _BASICCLOUDS_H_