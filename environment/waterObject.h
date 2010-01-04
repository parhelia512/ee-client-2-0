
#ifndef _WATEROBJECT_H_
#define _WATEROBJECT_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _GFXSTRUCTS_H_
#include "gfx/gfxStructs.h"
#endif
#ifndef _FOGSTRUCTS_H_
#include "sceneGraph/fogStructs.h"
#endif
#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif
#ifndef _REFLECTOR_H_
#include "sceneGraph/reflector.h"
#endif


GFXDeclareVertexFormat( GFXWaterVertex )
{
   Point3F point;
   Point3F normal;
   GFXVertexColor color;
   Point2F undulateData;
   Point4F horizonFactor;
};


class MaterialParameterHandle;
class BaseMatInstance;
class ShaderData;

struct WaterMatParams
{
   MaterialParameterHandle* mRippleDirSC;
   MaterialParameterHandle* mRippleTexScaleSC;
   MaterialParameterHandle* mRippleSpeedSC;
   MaterialParameterHandle* mRippleMagnitudeSC;
   MaterialParameterHandle* mWaveDirSC;
   MaterialParameterHandle* mWaveDataSC;   
   MaterialParameterHandle* mReflectTexSizeSC;
   MaterialParameterHandle* mBaseColorSC;
   MaterialParameterHandle* mMiscParamsSC;
   MaterialParameterHandle* mReflectParamsSC;
   MaterialParameterHandle* mReflectNormalSC;
   MaterialParameterHandle* mHorizonPositionSC;
   MaterialParameterHandle* mFogParamsSC;   
   MaterialParameterHandle* mMoreFogParamsSC;
   MaterialParameterHandle* mFarPlaneDistSC;
   MaterialParameterHandle* mWetnessParamsSC;
   MaterialParameterHandle* mDistortionParamsSC;
   MaterialParameterHandle* mUndulateMaxDistSC;
   MaterialParameterHandle* mAmbientColorSC;
   MaterialParameterHandle* mFoamParamsSC;
   MaterialParameterHandle* mFoamColorModulateSC;
   MaterialParameterHandle* mGridElementSizeSC;   
   MaterialParameterHandle* mElapsedTimeSC;
   MaterialParameterHandle* mModelMatSC;
   MaterialParameterHandle* mFoamSamplerSC;
   MaterialParameterHandle* mRippleSamplerSC;
   MaterialParameterHandle* mCubemapSamplerSC;

   void clear();
   void init(BaseMatInstance* matInst);
};


class GFXOcclusionQuery;
class PostEffect;
class CubemapData;

//-------------------------------------------------------------------------
// WaterObject Class
//-------------------------------------------------------------------------

class WaterObject : public SceneObject
{
   typedef SceneObject Parent;

protected:

   enum MaskBits {      
      UpdateMask     = Parent::NextFreeMask << 0,
      WaveMask       = Parent::NextFreeMask << 1,
      MaterialMask   = Parent::NextFreeMask << 2,
      TextureMask    = Parent::NextFreeMask << 3,
      NextFreeMask   = Parent::NextFreeMask << 4
   };

   enum consts {
      MAX_WAVES = 3,
      NUM_ANIM_FRAMES = 32,
   };

   enum MaterialType
   {
      WaterMat = 0,
      UnderWaterMat,      
      BasicWaterMat,
      BasicUnderWaterMat,      
      NumMatTypes
   };

public:

   WaterObject();
   virtual ~WaterObject();


   // ConsoleObject
   static void initPersistFields();

   // SimObject
   virtual bool onAdd();
   virtual void onRemove();
   virtual void inspectPostApply();

   // NetObject
   virtual U32  packUpdate( NetConnection * conn, U32 mask, BitStream *stream );
   virtual void unpackUpdate( NetConnection * conn, BitStream *stream );

   // SceneObject
   virtual bool prepRenderImage( SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseZoneState = false );
   virtual void renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );

   // WaterObject
   virtual F32 getViscosity() const { return mViscosity; }
   virtual F32 getDensity() const { return mDensity; }
   virtual F32 getSurfaceHeight( const Point2F &pos ) const { return 0.0f; };  
   virtual const char* getLiquidType() const { return mLiquidType; }
   virtual F32 getWaterCoverage( const Box3F &worldBox ) const { return 0.0f; }
   virtual VectorF getFlow( const Point3F &pos ) const { return Point3F::Zero; }
   virtual void updateUnderwaterEffect( SceneState *state );
   virtual bool isUnderwater( const Point3F &pnt ) { return false; }

protected:
      
   virtual void setShaderXForms( BaseMatInstance *mat ) {};
   virtual void setupVBIB() {};
   virtual void innerRender( SceneState *state ) {};
   virtual void setCustomTextures( S32 matIdx, U32 pass, const WaterMatParams &paramHandles );
   void drawUnderwaterFilter( SceneState *state );

   virtual void setShaderParams( SceneState *state, BaseMatInstance *mat, const WaterMatParams &paramHandles );
   PostEffect* getUnderwaterEffect();

   bool initMaterial( S32 idx );   
   void cleanupMaterials();
   S32 getMaterialIndex( const Point3F &camPos );

   void initTextures();

   virtual void _getWaterPlane( const Point3F &camPos, PlaneF &outPlane, Point3F &outPos ) {}

protected:

   static bool _setFullReflect( void *obj, const char *data );

   // WaterObject
   F32 mViscosity;
   F32 mDensity;
   String mLiquidType;   
   F32 mFresnelBias;
   F32 mFresnelPower;

   // Reflection
   bool mFullReflect;
   PlaneReflector mPlaneReflector;
   ReflectorDesc mReflectorDesc;
   PlaneF mWaterPlane;
   Point3F mWaterPos;
   bool mReflectNormalUp;

   // Water Fogging
   WaterFogData mWaterFogData;   

   // Distortion
   F32 mDistortStartDist;
   F32 mDistortEndDist;
   F32 mDistortFullDepth;   

   // Ripples
   Point2F  mRippleDir[ MAX_WAVES ];
   F32      mRippleSpeed[ MAX_WAVES ];
   Point2F  mRippleTexScale[ MAX_WAVES ];
   F32      mRippleMagnitude[ MAX_WAVES ];

   F32 mOverallRippleMagnitude;

   // Waves   
   Point2F  mWaveDir[ MAX_WAVES ];
   F32      mWaveSpeed[ MAX_WAVES ];   
   F32      mWaveMagnitude[ MAX_WAVES ];  
   
   F32 mOverallWaveMagnitude;

   // Foam
   F32 mFoamScale;
   F32 mFoamMaxDepth;
   Point3F mFoamColorModulate;

   // Basic Lighting
   F32 mClarity;
   ColorI mUnderwaterColor;

   // Other textures
   String mRippleTexName;
   String mFoamTexName;
   String mCubemapName;

   // Not fields...

   /// Defined here and sent to the shader in WaterCommon::setShaderParams
   /// but needs to be initialized in child classes prior to that call.
   F32 mUndulateMaxDist;

   /// Defined in WaterCommon but set and used by child classes.
   /// If true will refuse to render a reflection even if called from 
   /// the ReflectionManager, is set true if occlusion query is enabled and 
   /// it determines it is occluded.
   //bool mSkipReflectUpdate;

   /// Derived classes can set this value prior to calling Parent::setShaderConst
   /// to pass it into the shader miscParam.w
   F32 mMiscParamW;   

   SimObjectPtr<PostEffect> mUnderwaterPostFx;

   /// A global for enabling wireframe rendering
   /// on all water objects.
   static bool smWireframe;

   // Rendering   
   bool mBasicLighting;
   //U32 mRenderUpdateCount;
   //U32 mReflectUpdateCount;
   bool mGenerateVB;
   String mSurfMatName[NumMatTypes];
   BaseMatInstance* mMatInstances[NumMatTypes];
   WaterMatParams mMatParamHandles[NumMatTypes];
   AlignedArray<Point2F> mConstArray;
   bool mUnderwater;
   GFXStateBlockRef mUnderwaterSB;
   GFXTexHandle mRippleTex;
   GFXTexHandle mFoamTex;
   CubemapData *mCubemap;
   MatrixSet *mMatrixSet;
};

#endif // _WATEROBJECT_H_
