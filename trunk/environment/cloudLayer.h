
#ifndef _CLOUDLAYER_H_
#define _CLOUDLAYER_H_

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
#ifndef _MATINSTANCE_H_
#include "materials/matInstance.h"
#endif

GFXDeclareVertexFormat( GFXCloudVertex )
{
   Point3F point;
   Point3F normal;
   Point3F binormal;
   Point3F tangent; 
   Point2F texCoord;
};

class CloudLayer : public SceneObject
{
   typedef SceneObject Parent;

   enum 
   { 
      CloudLayerMask    = Parent::NextFreeMask,      
      NextFreeMask      = Parent::NextFreeMask << 1,
   };  

   #define TEX_COUNT 3

public:

   CloudLayer();
   virtual ~CloudLayer() {}

   DECLARE_CONOBJECT( CloudLayer );

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

protected:

   static U32 smVertStride;
   static U32 smStrideMinusOne;
   static U32 smVertCount;
   static U32 smTriangleCount;

   GFXTexHandle mTexture;

   GFXShaderRef mShader;

   GFXStateBlockRef mStateblock;

   GFXShaderConstBufferRef mShaderConsts;
   GFXShaderConstHandle *mModelViewProjSC; 
   GFXShaderConstHandle *mAmbientColorSC;
   GFXShaderConstHandle *mSunColorSC;
   GFXShaderConstHandle *mSunVecSC;
   GFXShaderConstHandle *mTexOffsetSC[3];
   GFXShaderConstHandle *mTexScaleSC;
   GFXShaderConstHandle *mBaseColorSC;    
   GFXShaderConstHandle *mCoverageSC;  
   GFXShaderConstHandle *mEyePosWorldSC;

   GFXVertexBufferHandle<GFXCloudVertex> mVB;
   GFXPrimitiveBufferHandle mPB;

   Point2F mTexOffset[3];
   U32 mLastTime;

   // Fields...

   String mTextureName;
   F32 mTexScale[TEX_COUNT];
   Point2F mTexDirection[TEX_COUNT];
   F32 mTexSpeed[TEX_COUNT];   
   
   ColorF mBaseColor;
   F32 mCoverage;
   F32 mWindSpeed;   
   F32 mHeight;
};


#endif // _CLOUDLAYER_H_