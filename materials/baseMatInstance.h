//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _BASEMATINSTANCE_H_
#define _BASEMATINSTANCE_H_

#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif
#ifndef _BASEMATERIALDEFINITION_H_
#include "materials/baseMaterialDefinition.h"
#endif
#ifndef _MATERIALPARAMETERS_H_
#include "materials/materialParameters.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif
#ifndef _GFXENUMS_H_
#include "gfx/gfxEnums.h"
#endif
#ifndef _GFXSHADER_H_
#include "gfx/gfxShader.h"
#endif
#ifndef _MATERIALFEATUREDATA_H_
#include "materials/materialFeatureData.h"
#endif
#ifndef _MATINSTANCEHOOK_H_
#include "materials/matInstanceHook.h"
#endif

struct RenderPassData;
class GFXVertexBufferHandleBase;
class GFXPrimitiveBufferHandle;
struct SceneGraphData;
class SceneState;
struct GFXStateBlockDesc;
class GFXVertexFormat;
class MatrixSet;

///
class BaseMatInstance
{
protected:

   /// The array of active material hooks indexed 
   /// by a MatInstanceHookType.
   Vector<MatInstanceHook*> mHooks;

   ///
   MatFeaturesDelegate mFeaturesDelegate;

   ///
   String mMatNameStr;

   /// Should be true if init has been called and it succeeded.
   /// It is up to the derived class to set this variable appropriately.
   bool mIsValid;

public:

   virtual ~BaseMatInstance();

   /// @param features The features you want to allow for this material.  
   ///
   /// @param vertexFormat The vertex format on which this material will be rendered.
   ///
   /// @see GFXVertexFormat
   /// @see FeatureSet
   virtual bool init(   const FeatureSet &features, 
                        const GFXVertexFormat *vertexFormat ) = 0;

   /// Reinitializes the material using the previous
   /// initialization parameters.
   /// @see init
   virtual bool reInit() = 0;

   /// Returns true if init has been successfully called.
   /// It is up to the derived class to set this value properly.
   bool isValid() { return mIsValid; }

   /// Adds this stateblock to the base state block 
   /// used during initialization.
   /// @see init
   virtual void addStateBlockDesc(const GFXStateBlockDesc& desc) = 0;

   /// Adds a shader macro which will be passed to the shader
   /// during initialization.
   /// @see init
   virtual void addShaderMacro( const String &name, const String &value ) = 0;

   /// Get a MaterialParameters block for this BaseMatInstance, 
   /// caller is responsible for freeing it.
   virtual MaterialParameters* allocMaterialParameters() = 0;

   /// Set the current parameters for this BaseMatInstance
   virtual void setMaterialParameters(MaterialParameters* param) = 0;

   /// Get the current parameters for this BaseMatInstance (BaseMatInstances are created with a default active
   /// MaterialParameters which is managed by BaseMatInstance.
   virtual MaterialParameters* getMaterialParameters() = 0;

   /// Returns a MaterialParameterHandle for name.
   virtual MaterialParameterHandle* getMaterialParameterHandle(const String& name) = 0;

   /// Sets up the next rendering pass for this material.  It is
   /// typically called like so...
   ///
   ///@code
   ///   while( mat->setupPass( state, sgData ) )
   ///   {
   ///      mat->setTransforms(...);
   ///      mat->setSceneInfo(...);
   ///      ...
   ///      GFX->drawPrimitive();
   ///   }
   ///@endcode
   ///
   virtual bool setupPass( SceneState *state, const SceneGraphData &sgData ) = 0;
   
   /// This initializes the material transforms and should be 
   /// called after setupPass() within the pass loop.
   /// @see setupPass
   virtual void setTransforms( const MatrixSet &matrixSet, SceneState *state ) = 0;

   /// This initializes various material scene state settings and
   /// should be called after setupPass() within the pass loop.
   /// @see setupPass
   virtual void setSceneInfo( SceneState *state, const SceneGraphData &sgData ) = 0;

   /// This is normally called from within setupPass() automatically, so its
   /// unnessasary to do so manually unless a texture stage has changed.  If
   /// so it should be called after setupPass() within the pass loop.
   /// @see setupPass
   virtual void setTextureStages(SceneState *, const SceneGraphData &sgData ) = 0;

   virtual void setBuffers( GFXVertexBufferHandleBase *vertBuffer, GFXPrimitiveBufferHandle *primBuffer ) = 0;

   /// Returns the material this instance is based on.
   virtual BaseMaterialDefinition* getMaterial() = 0;

   // BTRTODO: This stuff below should probably not be in BaseMatInstance
   virtual bool hasGlow() = 0;
   
   virtual U32 getCurPass() = 0;

   virtual U32 getCurStageNum() = 0;

   virtual RenderPassData *getPass(U32 pass) = 0;

   /// Returns the active features in use by this material.
   /// @see getRequestedFeatures
   virtual const FeatureSet& getFeatures() const = 0;

   /// Returns the features that were requested at material
   /// creation time which may differ from the active features.
   /// @see getFeatures
   virtual const FeatureSet& getRequestedFeatures() const = 0;

   virtual const GFXVertexFormat* getVertexFormat() const = 0;

   virtual void dumpShaderInfo() const = 0;

   ///
   MatFeaturesDelegate& getFeaturesDelegate() { return mFeaturesDelegate; }

   /// @name Material Hook functions
   /// @{

   ///
   void addHook( MatInstanceHook *hook );

   /// Helper function for getting a hook.
   /// @see getHook
   template <class HOOK>
   inline HOOK* getHook() { return (HOOK*)getHook( HOOK::Type ); }

   ///
   MatInstanceHook* getHook( const MatInstanceHookType &type ) const;

   ///
   void deleteHook( const MatInstanceHookType &type );

   ///
   U32 deleteAllHooks();

   /// @}

};

#endif /// _BASEMATINSTANCE_H_






