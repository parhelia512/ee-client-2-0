//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/matInstance.h"

#include "materials/materialManager.h"
#include "materials/customMaterialDefinition.h"
#include "materials/processedMaterial.h"
#include "materials/processedFFMaterial.h"
#include "materials/processedShaderMaterial.h"
#include "materials/processedCustomMaterial.h"
#include "shaderGen/featureMgr.h"
#include "gfx/gfxDevice.h"
#include "gfx/sim/cubemapData.h"
#include "gfx/gfxCubemap.h"
#include "core/util/safeDelete.h"

class MatInstParameters;

class MatInstanceParameterHandle : public MaterialParameterHandle
{
public:
   virtual ~MatInstanceParameterHandle() {}
   MatInstanceParameterHandle(const String& name);

   void loadHandle(ProcessedMaterial* pmat);
   
   // MaterialParameterHandle interface
   const String& getName() const { return mName; }
   virtual bool isValid() const; 
   virtual S32 getSamplerRegister( U32 pass ) const;
private:   
   friend class MatInstParameters;
   String mName;
   MaterialParameterHandle* mProcessedHandle;
};

MatInstanceParameterHandle::MatInstanceParameterHandle(const String& name)
{
   mName = name;
   mProcessedHandle = NULL;
}

bool MatInstanceParameterHandle::isValid() const
{
   return mProcessedHandle && mProcessedHandle->isValid();
}

S32 MatInstanceParameterHandle::getSamplerRegister( U32 pass ) const
{ 
   if ( !mProcessedHandle )
      return -1;
   return mProcessedHandle->getSamplerRegister( pass );
}

void MatInstanceParameterHandle::loadHandle(ProcessedMaterial* pmat)                                         
{
   mProcessedHandle = pmat->getMaterialParameterHandle(mName);
}

MatInstParameters::MatInstParameters() 
{ 
   mOwnParameters = false; 
   mParameters = NULL;
}

MatInstParameters::MatInstParameters(MaterialParameters* matParams)
{
   mOwnParameters = false; 
   mParameters = matParams;
}

void MatInstParameters::loadParameters(ProcessedMaterial* pmat)
{
   mOwnParameters = true; 
   mParameters = pmat->allocMaterialParameters();
}

MatInstParameters::~MatInstParameters()
{
   if (mOwnParameters)
      SAFE_DELETE(mParameters);
}

const Vector<GFXShaderConstDesc>& MatInstParameters::getShaderConstDesc() const
{ 
   return mParameters->getShaderConstDesc(); 
}

U32 MatInstParameters::getAlignmentValue(const GFXShaderConstType constType)
{
   return mParameters->getAlignmentValue(constType);
}

#define MATINSTPARAMSET(handle, f) \
   if (!mParameters) \
      return; \
   AssertFatal(dynamic_cast<MatInstanceParameterHandle*>(handle), "Invalid handle type!"); \
   MatInstanceParameterHandle* mph = static_cast<MatInstanceParameterHandle*>(handle); \
   mParameters->set(mph->mProcessedHandle, f); \

void MatInstParameters::set(MaterialParameterHandle* handle, const F32 f)
{
   MATINSTPARAMSET(handle, f);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const Point2F& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const Point3F& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const Point4F& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const ColorF& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const S32 f)
{
   MATINSTPARAMSET(handle, f);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const Point2I& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const Point3I& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const Point4I& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const AlignedArray<F32>& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const AlignedArray<Point2F>& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const AlignedArray<Point3F>& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const AlignedArray<Point4F>& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const AlignedArray<S32>& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const AlignedArray<Point2I>& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const AlignedArray<Point3I>& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const AlignedArray<Point4I>& fv)
{
   MATINSTPARAMSET(handle, fv);
}

void MatInstParameters::set(MaterialParameterHandle* handle, const MatrixF& mat, const GFXShaderConstType matrixType)
{
   AssertFatal(dynamic_cast<MatInstanceParameterHandle*>(handle), "Invalid handle type!"); 
   MatInstanceParameterHandle* mph = static_cast<MatInstanceParameterHandle*>(handle); 
   mParameters->set(mph->mProcessedHandle, mat, matrixType); 
}

void MatInstParameters::set(MaterialParameterHandle* handle, const MatrixF* mat, const U32 arraySize, const GFXShaderConstType matrixType)
{
   AssertFatal(dynamic_cast<MatInstanceParameterHandle*>(handle), "Invalid handle type!"); 
   MatInstanceParameterHandle* mph = static_cast<MatInstanceParameterHandle*>(handle); 
   mParameters->set(mph->mProcessedHandle, mat, arraySize, matrixType); 
}
#undef MATINSTPARAMSET

//****************************************************************************
// Material Instance
//****************************************************************************
MatInstance::MatInstance( Material &mat )
{
   VECTOR_SET_ASSOCIATION( mCurrentHandles );
   VECTOR_SET_ASSOCIATION( mCurrentParameters );

   mMaterial = &mat;

   construct();
}

//----------------------------------------------------------------------------
// Construct
//----------------------------------------------------------------------------
void MatInstance::construct()
{
   mCurPass = -1;
   mProcessedMaterial = false;
   mVertexFormat = NULL;
   mMaxStages = 1;
   mMatNameStr = "Unknown";
   mActiveParameters = NULL;
   mDefaultParameters = NULL;

   MATMGR->_track(this);
}

//----------------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------------
MatInstance::~MatInstance()
{
   SAFE_DELETE(mProcessedMaterial);
   SAFE_DELETE(mDefaultParameters);
   for (U32 i = 0; i < mCurrentHandles.size(); i++)
      SAFE_DELETE(mCurrentHandles[i]);   

   MATMGR->_untrack(this);
}

//----------------------------------------------------------------------------
// Init
//----------------------------------------------------------------------------
bool MatInstance::init( const FeatureSet &features, 
                        const GFXVertexFormat *vertexFormat ) 
{
   mFeatureList = features;
   mVertexFormat = vertexFormat;
   
   SAFE_DELETE(mProcessedMaterial);   
   mIsValid = processMaterial();         

   return mIsValid;
}


//----------------------------------------------------------------------------
// reInitialize
//----------------------------------------------------------------------------
bool MatInstance::reInit()
{
   SAFE_DELETE(mProcessedMaterial);   
   mIsValid = processMaterial();

   if ( mIsValid )
   {
      for (U32 i = 0; i < mCurrentHandles.size(); i++)
         mCurrentHandles[i]->loadHandle(mProcessedMaterial);

      for (U32 i = 0; i < mCurrentParameters.size(); i++)
         mCurrentParameters[i]->loadParameters(mProcessedMaterial);
   }

   return mIsValid;
}

//----------------------------------------------------------------------------
// Process stages
//----------------------------------------------------------------------------
bool MatInstance::processMaterial()
{
   AssertFatal( mMaterial, "Material is not valid!" );
   if ( !mMaterial )   
      return false;   

   SAFE_DELETE(mDefaultParameters);

   if( dynamic_cast<CustomMaterial*>(mMaterial) )
   {
      F32 pixVersion = GFX->getPixelShaderVersion();
      CustomMaterial* custMat = static_cast<CustomMaterial*>(mMaterial);
      if ((custMat->mVersion > pixVersion) || (custMat->mVersion == 0.0))
      {
         if(custMat->mFallback)
         {
            mMaterial = custMat->mFallback;
            return processMaterial();            
         }
         else
         {            
            AssertWarn(custMat->mVersion == 0.0f, avar("Can't load CustomMaterial %s for %s, using generic FF fallback", 
               String(mMaterial->getName()).isEmpty() ? "Unknown" : mMaterial->getName(), custMat->mMapTo.c_str()));
            mProcessedMaterial = new ProcessedFFMaterial(*mMaterial);
         }
      }
      else 
         mProcessedMaterial = new ProcessedCustomMaterial(*mMaterial);
   }
   else if(GFX->getPixelShaderVersion() > 0.001)
      mProcessedMaterial = getShaderMaterial();
   else
      mProcessedMaterial = new ProcessedFFMaterial(*mMaterial);

   if (mProcessedMaterial)
   {
      mProcessedMaterial->addStateBlockDesc( mUserDefinedState );
      mProcessedMaterial->setShaderMacros( mUserMacros );

      FeatureSet features( mFeatureList );
      features.exclude( MATMGR->getExclusionFeatures() );
      
      if( !mProcessedMaterial->init(features, mVertexFormat, mFeaturesDelegate) )
      {
         Con::errorf( "Failed to initialize material '%s'", getMaterial()->getName() );
         SAFE_DELETE( mProcessedMaterial );
         return false;
      }

      mDefaultParameters = new MatInstParameters(mProcessedMaterial->getDefaultMaterialParameters());
      mActiveParameters = mDefaultParameters;

      return true;
   }
   
   return false;
}

ProcessedMaterial* MatInstance::getShaderMaterial()
{
   return new ProcessedShaderMaterial(*mMaterial);
}

void MatInstance::addStateBlockDesc(const GFXStateBlockDesc& desc)
{   
   mUserDefinedState = desc;
}

void MatInstance::addShaderMacro( const String &name, const String &value )
{   
   // Check to see if we already have this macro.
   Vector<GFXShaderMacro>::iterator iter = mUserMacros.begin();
   for ( ; iter != mUserMacros.end(); iter++ )
   {
      if ( iter->name == name )
      {
         iter->value = value;
         return;
      }
   }

   // Add a new macro.
   mUserMacros.increment();
   mUserMacros.last().name = name;
   mUserMacros.last().value = value;
}

//----------------------------------------------------------------------------
// Setup pass - needs scenegraph data because the lightmap will change across
//    several materials.
//----------------------------------------------------------------------------
bool MatInstance::setupPass(SceneState * state, const SceneGraphData &sgData )
{
   PROFILE_SCOPE( MatInstance_SetupPass );
   
   if( !mProcessedMaterial )
      return false;

   ++mCurPass;

   if ( !mProcessedMaterial->setupPass( state, sgData, mCurPass ) )
   {
      mProcessedMaterial->cleanup(mCurPass - 1);
      mCurPass = -1;
      return false;
   }
   return true;
}

void MatInstance::setTransforms(const MatrixSet &matrixSet, SceneState *state)
{
   PROFILE_SCOPE(MatInstance_setTransforms);
   mProcessedMaterial->setTransforms(matrixSet, state, getCurPass());
}

void MatInstance::setSceneInfo(SceneState * state, const SceneGraphData& sgData)
{
   mProcessedMaterial->setSceneInfo(state, sgData, getCurPass());
}

void MatInstance::setBuffers(GFXVertexBufferHandleBase* vertBuffer, GFXPrimitiveBufferHandle* primBuffer)
{
   mProcessedMaterial->setBuffers(vertBuffer, primBuffer);
}

void MatInstance::setTextureStages(SceneState * state, const SceneGraphData &sgData )
{
   mProcessedMaterial->setTextureStages(state, sgData, getCurPass());
}

U32 MatInstance::getCurStageNum()
{
   return mProcessedMaterial->getStageFromPass(getCurPass());
}

RenderPassData* MatInstance::getPass(U32 pass)
{
   return mProcessedMaterial->getPass(pass);
}

bool MatInstance::hasGlow() 
{ 
   if( mProcessedMaterial )
      return mProcessedMaterial->hasGlow(); 
   else
      return false;
}

const FeatureSet& MatInstance::getFeatures() const 
{
   return mProcessedMaterial->getFeatures(); 
}

MaterialParameterHandle* MatInstance::getMaterialParameterHandle(const String& name)
{
   AssertFatal(mProcessedMaterial, "Not init'ed!"); 
   for (U32 i = 0; i < mCurrentHandles.size(); i++)
   {
      if (mCurrentHandles[i]->getName().equal(name))
      {
         return mCurrentHandles[i];
      }
   }
   MatInstanceParameterHandle* mph = new MatInstanceParameterHandle(name);
   mph->loadHandle(mProcessedMaterial);
   mCurrentHandles.push_back(mph);
   return mph;
}

MaterialParameters* MatInstance::allocMaterialParameters() 
{  
   AssertFatal(mProcessedMaterial, "Not init'ed!"); 
   MatInstParameters* mip = new MatInstParameters();
   mip->loadParameters(mProcessedMaterial);
   mCurrentParameters.push_back(mip);
   return mip;   
}

void MatInstance::setMaterialParameters(MaterialParameters* param) 
{ 
   AssertFatal(mProcessedMaterial, "Not init'ed!"); 
   mProcessedMaterial->setMaterialParameters(param, mCurPass);
   AssertFatal(dynamic_cast<MatInstParameters*>(param), "Incorrect param type!");
   mActiveParameters = static_cast<MatInstParameters*>(param);
}

MaterialParameters* MatInstance::getMaterialParameters()
{ 
   AssertFatal(mProcessedMaterial, "Not init'ed!"); 
   return mActiveParameters;
}

void MatInstance::dumpShaderInfo() const
{
   if ( mMaterial == NULL )
   {
      Con::errorf( "Trying to get Material information on an invalid MatInstance" );
      return;
   }

   Con::printf( "Material Info for object %s - %s", mMaterial->getName(), mMaterial->mMapTo.c_str() );

   if ( mProcessedMaterial == NULL )
   {
      Con::printf( "  [no processed material!]" );
      return;
   }

   mProcessedMaterial->dumpMaterialInfo();
}
