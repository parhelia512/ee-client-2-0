//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _POST_EFFECT_H_
#define _POST_EFFECT_H_

#ifndef _SIMSET_H_
#include "console/simSet.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif
#ifndef _MPOINT2_H_
#include "math/mPoint2.h"
#endif
#ifndef _GFXSHADER_H_
#include "gfx/gfxShader.h"
#endif
#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _GFXTARGET_H_
#include "gfx/gfxTarget.h"
#endif
#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif
#ifndef _MATTEXTURETARGET_H_
#include "materials/matTextureTarget.h"
#endif
#ifndef _POSTEFFECTCOMMON_H_
#include "postFx/postEffectCommon.h"
#endif

class GFXStateBlockData;
class Frustum;
class SceneState;
class ConditionerFeature;

///
GFX_DeclareTextureProfile( PostFxTargetProfile );




///
class PostEffect : public SimGroup, public MatTextureTarget
{
   typedef SimGroup Parent;

   friend class PostEffectVis;

protected:

   enum
   {
      NumTextures = 4,
   };

   FileName mTexFilename[NumTextures];

   GFXTexHandle mTextures[NumTextures];

   GFXTextureObject *mActiveTextures[NumTextures];

   MatTextureTarget *mActiveNamedTarget[NumTextures];

   RectI mActiveTextureViewport[NumTextures];

   GFXStateBlockData *mStateBlockData;

   GFXStateBlockRef mStateBlock;

   String mShaderName;

   GFXShaderRef mShader;

   Vector<GFXShaderMacro> mShaderMacros;
   
   GFXShaderConstBufferRef mShaderConsts;

   GFXShaderConstHandle *mRTSizeSC;
   GFXShaderConstHandle *mOneOverRTSizeSC;

   GFXShaderConstHandle *mTexSizeSC[NumTextures];
   GFXShaderConstHandle *mRenderTargetParamsSC[NumTextures];

   GFXShaderConstHandle *mViewportOffsetSC;

   GFXShaderConstHandle *mFogDataSC;
   GFXShaderConstHandle *mFogColorSC;
   GFXShaderConstHandle *mEyePosSC;
   GFXShaderConstHandle *mMatWorldToScreenSC;
   GFXShaderConstHandle *mMatScreenToWorldSC;
   GFXShaderConstHandle *mMatPrevScreenToWorldSC;
   GFXShaderConstHandle *mNearFarSC;
   GFXShaderConstHandle *mInvNearFarSC;   
   GFXShaderConstHandle *mWorldToScreenScaleSC;
   GFXShaderConstHandle *mWaterColorSC;
   GFXShaderConstHandle *mWaterFogDataSC;     
   GFXShaderConstHandle *mAmbientColorSC;
   GFXShaderConstHandle *mWaterFogPlaneSC;
   GFXShaderConstHandle *mScreenSunPosSC;
   GFXShaderConstHandle *mLightDirectionSC;
   GFXShaderConstHandle *mCameraForwardSC;
   GFXShaderConstHandle *mAccumTimeSC;
   GFXShaderConstHandle *mDeltaTimeSC;
   GFXShaderConstHandle *mInvCameraMatSC;

   bool mAllowReflectPass;

   /// If true update the shader.
   bool mUpdateShader;   

   GFXTextureTargetRef mTarget;

   String mTargetName;

   GFXTexHandle mTargetTex;

   bool mIsNamedTarget;

   /// If mTargetSize is zero then this scale is
   /// used to make a relative texture size to the
   /// active render target.
   Point2F mTargetScale;

   /// If non-zero this is used as the absolute
   /// texture target size.
   /// @see mTargetScale
   Point2I mTargetSize;

   RectI mTargetRect;

   GFXFormat mTargetFormat;

   /// The color to prefill the named target when
   /// first created by the effect.
   ColorF mTargetClearColor;

   PFXRenderTime mRenderTime;
   PFXTargetClear mTargetClear;

   String mRenderBin;

   F32 mRenderPriority;

   U32 mPostEffectRequirements;
   bool mRequirementsMet;

   /// True if the effect has been enabled by the manager.
   bool mEnabled;
   
   /// Skip processing of this PostEffect and its children even if its parent is enabled. 
   /// Parent and sibling PostEffects in the chain are still processed.
   /// This is intended for debugging purposes.
   bool mSkip;

   bool mOneFrameOnly;
   bool mOnThisFrame;  

   U32 mShaderReloadKey;

   class EffectConst
   {
   public:

      EffectConst( const String &name, const String &val )
         : mName( name ), 
           mHandle( NULL ),
           mDirty( true )
      {
         set( val );
      }

      void set( const String &newVal );

      void setToBuffer( GFXShaderConstBufferRef buff );

      String mName;

      GFXShaderConstHandle *mHandle;

      String mStringVal;

      bool mDirty;
   };

   typedef HashTable<StringCase,EffectConst*> EffectConstTable;

   EffectConstTable mEffectConsts;

   ///
   virtual void _updateScreenGeometry( const Frustum &frustum,
                                       GFXVertexBufferHandle<PFXVertex> *outVB );

   ///
   virtual void _setupStateBlock( const SceneState *state );

   /// 
   virtual void _setupConstants( const SceneState *state );

   ///
   virtual void _setupTransforms();

   ///
   virtual void _setupTarget( const SceneState *state, bool *outClearTarget );

   ///
   virtual void _setupTexture( U32 slot, GFXTexHandle &inputTex, const RectI *inTexViewport );

   /// Protected set method for toggling the enabled state.
   static bool _setIsEnabled( void* obj, const char* data );

   /// Called from the light manager activate signal.
   /// @see LightManager::addActivateCallback
   void _onLMActivate( const char*, bool activate )
   {
      if ( activate ) 
         mUpdateShader = true; 
   }

   /// We handle texture events to release named rendered targets.
   /// @see GFXTextureManager::addEventDelegate
   void _onTextureEvent( GFXTexCallbackCode code )
   {
      if ( code == GFXZombify && mIsNamedTarget )
         _cleanTargets();
   }

   ///
   void _updateConditioners();

   ///
   void _cleanTargets( bool recurse = false );

   GFXTextureObject* _texGen();

public:

   /// Constructor.
   PostEffect();

   /// Destructor.
   virtual ~PostEffect();

   virtual void process(   const SceneState *state, 
                           GFXTexHandle &inOutTex,
                           const RectI *inTexViewport = NULL );

   /// 
   void reload();

   /// 
   void enable();

   /// 
   void disable();

   /// 
   bool checkRequirements() const;

   /// Dump the shader disassembly to a temporary text file.
   /// Returns true and sets outFilename to the file if successful.
   bool dumpShaderDisassembly( String &outFilename ) const;

   ///
   bool isEnabled() const { return mEnabled; }

   PFXRenderTime getRenderTime() const { return mRenderTime; }

   const String& getRenderBin() const { return mRenderBin; }

   F32 getPriority() const { return mRenderPriority; }

   void setTexture( U32 i, GFXTextureObject *tex );

   void setShaderMacro( const String &name, const String &value = String::EmptyString );
   bool removeShaderMacro( const String &name );
   void clearShaderMacros();

   ///
   void setShaderConst( const String &name, const String &val );   

   void setOnThisFrame( bool enabled ) { mOnThisFrame = enabled; }
   bool isOnThisFrame() { return mOnThisFrame; }
   void setOneFrameOnly( bool enabled ) { mOneFrameOnly = enabled; }
   bool isOneFrameOnly() { return mOneFrameOnly; }

   // SimObject
   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();

   /// MatTextureTarget
   virtual GFXTextureObject* getTargetTexture( U32 index ) const;
   virtual const RectI& getTargetViewport() const { return mTargetRect; }
   virtual void setupSamplerState( GFXSamplerStateDesc *desc ) const {}
   virtual ConditionerFeature* getTargetConditioner() const { return NULL; }

   F32 getAspectRatio() const;

   DECLARE_CONOBJECT(PostEffect);

   enum PostEffectRequirements
   {
      RequiresDepth      = BIT(0),
      RequiresNormals    = BIT(1),
      RequiresLightInfo  = BIT(2),
   };
};

#endif // _POST_EFFECT_H_