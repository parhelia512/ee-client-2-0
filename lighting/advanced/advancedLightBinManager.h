//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _ADVANCEDLIGHTBINMANAGER_H_
#define _ADVANCEDLIGHTBINMANAGER_H_

#ifndef _TEXTARGETBIN_MGR_H_
#include "renderInstance/renderTexTargetBinManager.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _LIGHTINFO_H_
#include "lighting/lightInfo.h"
#endif
#ifndef _MATHUTIL_FRUSTUM_H_
#include "math/util/frustum.h"
#endif
#ifndef _MATINSTANCE_H_
#include "materials/matInstance.h"
#endif
#ifndef _SHADOW_COMMON_H_
#include "lighting/shadowMap/shadowCommon.h"
#endif


class AdvancedLightManager;
class ShadowMapManager;
class LightShadowMap;
class AdvancedLightBufferConditioner;
class LightMapParams;


class LightMatInstance : public MatInstance
{
   typedef MatInstance Parent;
protected:
   MaterialParameterHandle *mLightMapParamsSC;
   bool mInternalPass;

   enum
   {
      DynamicLight = 0,
      StaticLightNonLMGeometry,
      StaticLightLMGeometry,
      NUM_LIT_STATES
   };
   GFXStateBlockRef mLitState[NUM_LIT_STATES];

public:
   LightMatInstance(Material &mat) : Parent(mat), mLightMapParamsSC(NULL), mInternalPass(false) {}

   virtual bool init( const FeatureSet &features, const GFXVertexFormat *vertexFormat );
   virtual bool setupPass( SceneState *state, const SceneGraphData &sgData );
};


class AdvancedLightBinManager : public RenderTexTargetBinManager
{
   typedef RenderTexTargetBinManager Parent;
   friend class AdvancedLightProbeManager;

public:

   // Light info Render Inst Type
   static const RenderInstType RIT_LightInfo;
   
   // registered buffer name
   static const String smBufferName;

   /// The shadow filter mode to use on shadowed light materials.
   /// @see setShadowFilterMode
   static ShadowFilterMode smShadowFilterMode;

   // Used for console init
   AdvancedLightBinManager( AdvancedLightManager *lm = NULL, 
                            ShadowMapManager *sm = NULL,
                            GFXFormat lightBufferFormat = GFXFormatR8G8B8A8 );
   virtual ~AdvancedLightBinManager();

   // RenderBinManager interface
   virtual AddInstResult addElement(RenderInst *);
   virtual void render(SceneState *);
   virtual void clear();
   virtual void sort() {}

   // Expose a conditioner for light information
   virtual ConditionerFeature *getTargetConditioner() const;

   // Add a light to the bins
   virtual AddInstResult addLight( LightInfo *light );

   virtual bool setTargetSize(const Point2I &newTargetSize);

   // ConsoleObject interface
   DECLARE_CONOBJECT(AdvancedLightBinManager);

   /// Returns the constant specular power.
   /// @see smConstantSpecularPower
   static F32 getConstantSpecularPower() { return smConstantSpecularPower; } 
   
   /// Sets the constant specular power.
   /// @see smConstantSpecularPower   
   static void setConstantSpecularPower(F32 csp) { smConstantSpecularPower = csp; }

   bool MRTLightmapsDuringPrePass() const { return mMRTLightmapsDuringPrePass; }
   void MRTLightmapsDuringPrePass(bool val);

   /// Frees all the currently allocated light materials.
   void deleteLightMaterials();

protected:

   // Track a light material and associated data
   struct LightMaterialInfo
   {
      LightMatInstance *matInstance;

      // { zNear, zFar, 1/zNear, 1/zFar }
      MaterialParameterHandle *zNearFarInvNearFar;

      // Far frustum plane (World Space)
      MaterialParameterHandle *farPlane;

      // Far frustum plane (View Space)
      MaterialParameterHandle *vsFarPlane;

      // -dot( farPlane, eyePos )
      MaterialParameterHandle *negFarPlaneDotEye;

      // Light Parameters
      MaterialParameterHandle *lightPosition;
      MaterialParameterHandle *lightDirection;
      MaterialParameterHandle *lightColor;
      MaterialParameterHandle *lightBrightness;
      MaterialParameterHandle *lightAttenuation;
      MaterialParameterHandle *lightRange;
      MaterialParameterHandle *lightAmbient;
      MaterialParameterHandle *lightTrilight;
      MaterialParameterHandle *lightSpotParams;

      // Constant specular power
      MaterialParameterHandle *constantSpecularPower;

      LightMaterialInfo(   const String &matName, 
                           const GFXVertexFormat *vertexFormat,
                           const Vector<GFXShaderMacro> &macros = Vector<GFXShaderMacro>() );

      virtual ~LightMaterialInfo();


      void setViewParameters( const F32 zNear, 
                              const F32 zFar, 
                              const Point3F &eyePos, 
                              const PlaneF &farPlane,
                              const PlaneF &_vsFarPlane );

      void setLightParameters( const LightInfo *light, const MatrixF &worldViewOnly );
   };

protected:

   struct LightBinEntry
   {
      LightInfo* lightInfo;
      LightShadowMap* shadowMap;
      LightMaterialInfo* lightMaterial;
      GFXPrimitiveBuffer* primBuffer;
      GFXVertexBuffer* vertBuffer;
      U32 numPrims;
   };

   Vector<LightBinEntry> mLightBin;
   typedef Vector<LightBinEntry>::iterator LightBinIterator;

   bool mMRTLightmapsDuringPrePass;

   U32 mNumLightsCulled;
   AdvancedLightManager *mLightManager;
   ShadowMapManager *mShadowManager;
   Frustum mFrustum;
   Frustum mViewSpaceFrustum;

   static const String smLightMatNames[LightInfo::Count];

   static const String smShadowTypeMacro[ShadowType_Count];

   static const GFXVertexFormat* smLightMatVertex[LightInfo::Count];

   typedef CompoundKey<LightInfo::Type,ShadowType> LightMatKey;

   typedef HashTable<LightMatKey,LightMaterialInfo*> LightMatTable;

   /// The fixed table of light material info.
   LightMatTable mLightMaterials;

   LightMaterialInfo* _getLightMaterial( LightInfo::Type lightType, ShadowType shadowType );


   AdvancedLightBufferConditioner *mConditioner;

   /// This value is used as a constant power to raise specular values to, before
   /// storing them into the light info buffer. The per-material specular value is
   /// then computer by using the integer identity of exponentiation: (a^m)^n = a^(m*n)
   /// or: (specular^constSpecular)^(matSpecular/constSpecular) = specular^(matSpecular*constSpecular)   
   static F32 smConstantSpecularPower;

   typedef GFXVertexPNT FarFrustumQuadVert; 
   GFXVertexBufferHandle<FarFrustumQuadVert> mFarFrustumQuadVerts;


   //void _createMaterials();

   void _setupPerFrameParameters( const Point3F &eyePos );

   void setupSGData( SceneGraphData &data, LightInfo *light );
};

#endif // _ADVANCEDLIGHTBINMANAGER_H_
