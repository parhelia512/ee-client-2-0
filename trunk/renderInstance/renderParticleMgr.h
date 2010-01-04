//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _RENDER_PARTICLE_MGR_H_
#define _RENDER_PARTICLE_MGR_H_

#ifndef _TEXTARGETBIN_MGR_H_
#include "renderInstance/renderTexTargetBinManager.h"
#endif

#ifndef _GFXPRIMITIVEBUFFER_H_
#include "gfx/gfxPrimitiveBuffer.h"
#endif

GFXDeclareVertexFormat( CompositeQuadVert )
{
   GFXVertexColor uvCoord;
};

class RenderParticleMgr : public RenderTexTargetBinManager
{
   typedef RenderTexTargetBinManager Parent;
   friend class RenderTranslucentMgr;

public:
   // Generic PrePass Render Instance Type
   static const RenderInstType RIT_Particles;

   RenderParticleMgr();
   RenderParticleMgr( F32 renderOrder, F32 processAddOrder );
   virtual ~RenderParticleMgr();

   // RenderBinManager
   void render(SceneState * state);
   void sort();
   void clear();
   AddInstResult addElement( RenderInst *inst );

   // ConsoleObject
   DECLARE_CONOBJECT(RenderParticleMgr);

   const static U8 HighResStencilRef = 0x80;
   const static U8 ParticleSystemStencilMask = 0x80; // We are using the top bit
   const static U32 OffscreenPoolSize = 5;

   virtual void setTargetChainLength(const U32 chainLength);

protected:

   // Override
   virtual bool _handleGFXEvent(GFXDevice::GFXDeviceEventType event);

   bool _initShader();
   void _initGFXResources();
   void _onLMActivate( const char*, bool activate );


   // Not only a helper method, but a method for the RenderTranslucentMgr to
   // request a particle system draw
   void renderInstance(ParticleRenderInst *ri, SceneState *state);

   bool mOffscreenRenderEnabled;

   /// The prepass render target used for the
   /// soft particle shader effect.
   MatTextureTargetRef mPrepassTarget;

   /// The shader used for particle rendering.
   GFXShaderRef mParticleShader;

   GFXShaderRef mParticleCompositeShader;
   MatTextureTargetRef mEdgeTarget;

   struct OffscreenSystemEntry
   {
      S32 targetChainIdx;
      MatrixF clipMatrix;
      RectF screenRect; 
      bool drawnThisFrame;
      Vector<ParticleRenderInst *> pInstances;
   };
   Vector<OffscreenSystemEntry> mOffscreenSystems;

   struct ShaderConsts
   {
      GFXShaderConstBufferRef mShaderConsts;
      GFXShaderConstHandle *mModelViewProjSC;
      GFXShaderConstHandle *mFSModelViewProjSC;
      GFXShaderConstHandle *mOneOverFarSC;
      GFXShaderConstHandle *mOneOverSoftnessSC;
      GFXShaderConstHandle *mPrePassTargetParamsSC;
      GFXShaderConstHandle *mAlphaFactorSC;
      GFXShaderConstHandle *mAlphaScaleSC;

   } mParticleShaderConsts;

   struct CompositeShaderConsts
   {
      GFXShaderConstBufferRef mShaderConsts;
      GFXShaderConstHandle *mSystemDepth;
      GFXShaderConstHandle *mScreenRect;
      GFXShaderConstHandle *mEdgeTargetParamsSC;
      GFXShaderConstHandle *mOffscreenTargetParamsSC;
   } mParticleCompositeShaderConsts;

   GFXVertexBufferHandle<CompositeQuadVert> mScreenQuadVertBuff;
   GFXPrimitiveBufferHandle mScreenQuadPrimBuff;

   GFXStateBlockRef mStencilClearSB;
   GFXStateBlockRef mHighResBlocks[ParticleRenderInst::BlendStyle_COUNT];
   GFXStateBlockRef mOffscreenBlocks[ParticleRenderInst::BlendStyle_COUNT];
   GFXStateBlockRef mBackbufferBlocks[ParticleRenderInst::BlendStyle_COUNT];
   GFXStateBlockRef mMixedResBlocks[ParticleRenderInst::BlendStyle_COUNT];
   
   GFXStateBlockRef _getHighResStateBlock(ParticleRenderInst *ri);
   GFXStateBlockRef _getMixedResStateBlock(ParticleRenderInst *ri);
   GFXStateBlockRef _getOffscreenStateBlock(ParticleRenderInst *ri);
   GFXStateBlockRef _getCompositeStateBlock(ParticleRenderInst *ri);
};


#endif // _RENDER_TRANSLUCENT_MGR_H_
