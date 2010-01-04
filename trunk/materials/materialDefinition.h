//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _MATERIALDEFINITION_H_
#define _MATERIALDEFINITION_H_

#ifndef _BASEMATERIALDEFINITION_H_
#include "materials/baseMaterialDefinition.h"
#endif
#ifndef _MATERIALFEATUREDATA_H_
#include "materials/materialFeatureData.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _GFXSTRUCTS_H_
#include "gfx/gfxStructs.h"
#endif
#ifndef _GFXCUBEMAP_H_
#include "gfx/gfxCubemap.h"
#endif

class CubemapData;
class SFXProfile;
struct SceneGraphData;

class MaterialSoundProfile;
class MaterialPhysicsProfile;

//**************************************************************************
// Basic Material
//**************************************************************************
class Material : public BaseMaterialDefinition
{
   typedef BaseMaterialDefinition Parent;
public:
   static GFXCubemap *GetNormalizeCube();

   //-----------------------------------------------------------------------
   // Enums
   //-----------------------------------------------------------------------
   enum Constants
   {
      MAX_TEX_PER_PASS = 8,         ///< Number of textures per pass
      MAX_STAGES = 4,
      NUM_EFFECT_COLOR_STAGES = 2,  ///< Number of effect color definitions for transitioning effects.
   };

   enum TexType
   {
      NoTexture = 0,
      Standard = 1,
      Detail,
      Bump,
      Env,
      Cube,
      SGCube,  // scene graph cube - probably dynamic
      Lightmap,
      ToneMapTex,
      Mask,
      BackBuff,
      ReflectBuff,
      Misc,
      DynamicLight,
      DynamicLight2,
      DynamicLight3,
      DynamicLight4,
      DynamicLightSecondary,
      DynamicLightMask,
      NormalizeCube,
      TexTarget,
   };

   enum BlendOp
   {
      None = 0,
      Mul,
      Add,
      AddAlpha,      // add modulated with alpha channel
      Sub,
      LerpAlpha,     // linear interpolation modulated with alpha channel
      ToneMap,
      NumBlendTypes
   };

   enum AnimType
   {
      Scroll = 1,
      Rotate = 2,
      Wave   = 4,
      Scale  = 8,
      Sequence = 16,
   };

   enum WaveType
   {
      Sin = 0,
      Triangle,
      Square,
   };

   class StageData
   {
   protected:

      ///
      typedef HashTable<const FeatureType*,GFXTexHandle> TextureTable;

      /// The sparse table of textures by feature index.
      /// @see getTex
      /// @see setTex
      TextureTable mTextures;

      /// The cubemap for this stage.
      GFXCubemap *mCubemap;

   public:

      StageData()
         : mCubemap( NULL )
      {
      }

      /// Returns the texture object or NULL if there is no
      /// texture entry for that feature type in the table.
      inline GFXTextureObject* getTex( const FeatureType &type ) const
      {
         TextureTable::ConstIterator iter = mTextures.find( &type );
         if ( iter == mTextures.end() )
            return NULL;

         return iter->value.getPointer();
      }

      /// Assigns a texture object by feature type.
      inline void setTex( const FeatureType &type, GFXTextureObject *tex )
      {
         if ( !tex )
         {
            TextureTable::Iterator iter = mTextures.find( &type );
            if ( iter != mTextures.end() )
               mTextures.erase( iter );

            return;
         }

         TextureTable::Iterator iter = mTextures.findOrInsert( &type );
         iter->value = tex;
      }

      /// Returns true if we have a valid texture assigned to
      /// any feature in the texture table.
      inline bool hasValidTex() const
      {
         TextureTable::ConstIterator iter = mTextures.begin();
         for ( ; iter != mTextures.end(); iter++ )
         {
            if ( iter->value.isValid() )
               return true;
         }

         return false;
      }

      /// Returns the active texture features.
      inline FeatureSet getFeatureSet() const
      {
         FeatureSet result;

         TextureTable::ConstIterator iter = mTextures.begin();
         for ( ; iter != mTextures.end(); iter++ )
         {
            if ( iter->value.isValid() )
               result.addFeature( *iter->key );
         }

         return result;
      }

      /// Returns the stage cubemap.
      GFXCubemap* getCubemap() const { return mCubemap; }

      /// Set the stage cubemap.
      void setCubemap( GFXCubemap *cubemap ) { mCubemap = cubemap; }

   };

public:

   //-----------------------------------------------------------------------
   // Data
   //-----------------------------------------------------------------------
   FileName mDiffuseMapFilename[MAX_STAGES];
   FileName mBaseTexFilename[MAX_STAGES];

   FileName mOverlayMapFilename[MAX_STAGES];
   FileName mOverlayTexFilename[MAX_STAGES];

   FileName mLightMapFilename[MAX_STAGES];

   FileName mToneMapFilename[MAX_STAGES];
   
   FileName mDetailMapFilename[MAX_STAGES];
   FileName mDetailTexFilename[MAX_STAGES];

   FileName mNormalMapFilename[MAX_STAGES];
   FileName mBumpTexFilename[MAX_STAGES];
   FileName mSpecularMapFilename[MAX_STAGES];

   FileName mEnvMapFilename[MAX_STAGES];
   FileName mEnvTexFilename[MAX_STAGES];

   StageData mStages[MAX_STAGES];   
   
   /// This is the color used if there is no diffuse
   /// texture map and the alpha value is not zero.
   ColorF mDiffuse[MAX_STAGES];

   ColorF mSpecular[MAX_STAGES];
   
   /// This is not really a color multiplication.  This does a
   /// lerp between the diffuse color/tex and this color based
   /// on its alpha channel.
   ColorF mColorMultiply[MAX_STAGES];

   F32 mSpecularPower[MAX_STAGES];
   bool mPixelSpecular[MAX_STAGES];

   bool mVertLit[MAX_STAGES];

   F32 mParallaxScale[MAX_STAGES];   
  
   F32 mMinnaertConstant[MAX_STAGES];
   bool mSubSurface[MAX_STAGES];
   ColorF mSubSurfaceColor[MAX_STAGES];
   F32 mSubSurfaceRolloff[MAX_STAGES];

   /// The repetition scale of the detail texture
   /// over the base texture.
   Point2F mDetailScale[MAX_STAGES];

   // yes this should be U32 - we test for 2 or 4...
   U32 mExposure[MAX_STAGES];

   U32 mAnimFlags[MAX_STAGES];
   Point2F mScrollDir[MAX_STAGES];
   F32 mScrollSpeed[MAX_STAGES];
   Point2F mScrollOffset[MAX_STAGES];

   F32 mRotSpeed[MAX_STAGES];
   Point2F mRotPivotOffset[MAX_STAGES];
   F32 mRotPos[MAX_STAGES];
   
   F32 mWavePos[MAX_STAGES];
   F32 mWaveFreq[MAX_STAGES];
   F32 mWaveAmp[MAX_STAGES];
   U32 mWaveType[MAX_STAGES];
   
   F32 mSeqFramePerSec[MAX_STAGES];
   F32 mSeqSegSize[MAX_STAGES];
   
   bool mGlow[MAX_STAGES];          // entire stage glows
   bool mEmissive[MAX_STAGES];

   bool mDoubleSided;

   String mCubemapName;
   CubemapData* mCubemapData;
   bool mDynamicCubemap;

   bool mTranslucent;   
   BlendOp mTranslucentBlendOp;
   bool mTranslucentZWrite;

   /// A generic setting which tells the system to skip 
   /// generation of shadows from this material.
   bool mCastShadows;

   bool mAlphaTest;
   U32 mAlphaRef;

   bool mPlanarReflection;

   bool mAutoGenerated;

   ///@{
   /// Behavioral properties.

   bool mShowFootprints;            ///< If true, show footprints when walking on surface with this material.  Defaults to false.
   bool mShowDust;                  ///< If true, show dust emitters (footpuffs, hover trails, etc) when on surface with this material.  Defaults to false.

   /// Color to use for particle effects and such when located on this material.
   ColorF mEffectColor[ NUM_EFFECT_COLOR_STAGES ];

   /// Footstep sound to play when walking on surface with this material.
   /// Numeric ID of footstep sound defined on player datablock (0 == soft,
   /// 1 == hard, 2 == metal, 3 == snow).
   /// Defaults to -1 which deactivates default sound.
   /// @see mFootstepSoundCustom
   S32 mFootstepSoundId;
   S32 mImpactSoundId;

   /// Sound effect to play when walking on surface with this material.
   /// If defined, overrides mFootstepSoundId.
   /// @see mFootstepSoundCustom
   SFXProfile* mFootstepSoundCustom;
   SFXProfile* mImpactSoundCustom;

   F32 mFriction;                   ///< Friction coefficient when moving along surface.

   ///@}
   
   String mMapTo; // map Material to this texture name
  
   ///
   /// Material interface
   ///
   Material();

   /// Allocates and returns a BaseMatInstance for this material.  Caller is responsible
   /// for freeing the instance
   virtual BaseMatInstance* createMatInstance();      
   virtual bool isIFL() const { return mIsIFL; }      
   void setIsIFL(bool isIFL) { mIsIFL = isIFL; }
   virtual bool isTranslucent() const { return mTranslucent && mTranslucentBlendOp != Material::None; }   
   virtual bool isDoubleSided() const { return mDoubleSided; }
   virtual bool isAutoGenerated() const { return mAutoGenerated; }
   virtual void setAutoGenerated(bool isAutoGenerated) { mAutoGenerated = isAutoGenerated; }
   virtual bool isLightmapped() const;
   virtual bool castsShadows() const { return mCastShadows; }
   const String &getPath() const { return mPath; }

   void flush();

   /// Re-initializes all the material instances 
   /// that use this material.
   void reload();

   /// Called to update time based parameters for a material.  Ensures 
   /// that it only happens once per tick.
   void updateTimeBasedParams();

   ///
   /// SimObject interface
   ///
   virtual bool onAdd();
   virtual void onRemove();
   virtual void inspectPostApply();

   //
   // ConsoleObject interface
   //
   static void initPersistFields();

   DECLARE_CONOBJECT(Material);
protected:

   // Per material animation parameters
   U32 mLastUpdateTime;

   bool mIsIFL;
   String mPath;

   static EnumTable mAnimFlagTable;
   static EnumTable mBlendOpTable;
   static EnumTable mWaveTypeTable;

   /// Map this material to the texture specified
   /// in the "mapTo" data variable.
   virtual void _mapMaterial();

private:
   static GFXCubemapHandle smNormalizeCube;
};

#endif // _MATERIALDEFINITION_H_
