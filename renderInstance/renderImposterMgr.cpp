//-----------------------------------------------------------------------------
// Torque Forest Kit
// Copyright (C) Sickhead Games, LLC
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "renderInstance/renderImposterMgr.h"

#include "sceneGraph/sceneGraph.h"
#include "T3D/gameConnection.h"
#include "ts/tsLastDetail.h"
#include "materials/shaderData.h"
#include "lighting/lightManager.h"
#include "lighting/lightInfo.h"
#include "sceneGraph/sceneState.h"
#include "gfx/gfxDebugEvent.h"
#include "renderInstance/renderPrePassMgr.h"
#include "gfx/gfxTransformSaver.h"
#include "console/consoleTypes.h"
#include "gfx/util/screenspace.h"

GFXImplementVertexFormat( ImposterVertex )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float4 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float3, 0 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float4, 1 );
};

const RenderInstType RenderImposterMgr::RIT_Imposter("Imposter");


U32 RenderImposterMgr::smRendered = 0.0f;
U32 RenderImposterMgr::smBatches = 0.0f;
U32 RenderImposterMgr::smDrawCalls = 0.0f;
U32 RenderImposterMgr::smPolyCount = 0.0f;
U32 RenderImposterMgr::smRTChanges = 0.0f;


IMPLEMENT_CONOBJECT(RenderImposterMgr);

RenderImposterMgr::RenderImposterMgr()
   :  RenderBinManager( RenderImposterMgr::RIT_Imposter, 1.0f, 1.0f ),
      mImposterBatchSize( 250 )
{
   RenderPrePassMgr::getRenderSignal().notify( this, &RenderImposterMgr::_renderPrePass );
}

RenderImposterMgr::RenderImposterMgr( F32 renderOrder, F32 processAddOrder )
   :  RenderBinManager( RenderImposterMgr::RIT_Imposter, renderOrder, processAddOrder ),
      mImposterBatchSize( 250 )
{
   RenderPrePassMgr::getRenderSignal().notify( this, &RenderImposterMgr::_renderPrePass );
}

void RenderImposterMgr::initPersistFields()
{
   Con::addVariable( "$ImposterStats::rendered", TypeS32, &smRendered );
   Con::addVariable( "$ImposterStats::batches", TypeS32, &smBatches );
   Con::addVariable( "$ImposterStats::drawCalls", TypeS32, &smDrawCalls );
   Con::addVariable( "$ImposterStats::polyCount", TypeS32, &smPolyCount );
   Con::addVariable( "$ImposterStats::rtChanges", TypeS32, &smRTChanges );

   Parent::initPersistFields();
}

RenderImposterMgr::~RenderImposterMgr()
{
   RenderPrePassMgr::getRenderSignal().remove( this, &RenderImposterMgr::_renderPrePass );

   mVB = NULL;
   mIB = NULL;
}

RenderImposterMgr::ShaderState::ShaderState()
   :  mShader( NULL )
{
   LightManager::smActivateSignal.notify( this, &ShaderState::_onLMActivate );
}

RenderImposterMgr::ShaderState::~ShaderState()
{
   LightManager::smActivateSignal.remove( this, &ShaderState::_onLMActivate );
}

bool RenderImposterMgr::ShaderState::init(   const String &shaderName,
                                             const GFXStateBlockDesc *desc  )
{
   ShaderData *shaderData;
   if ( !Sim::findObject( shaderName, shaderData ) )
   {
      Con::warnf( "TSImposterRenderMgr - failed to locate shader '%s'!", shaderName.c_str() );
      return false;
   }

   // We're adding both the lightinfo uncondition and the
   // prepass conditioner to the shader...  we usually only
   // use one of them, but the extra macros doesn't hurt.

   Vector<GFXShaderMacro> macros;
   mLightTarget = MatTextureTarget::findTargetByName( "lightinfo" );
   if ( mLightTarget )
      mLightTarget->getTargetShaderMacros( &macros );

   MatTextureTargetRef prepassTarget = MatTextureTarget::findTargetByName( "prepass" );
   if ( prepassTarget )
      prepassTarget->getTargetShaderMacros( &macros );

   // Get the shader.
   mShader = shaderData->getShader( macros );
   if ( !mShader )
      return false;

   mConsts = mShader->allocConstBuffer();

   mWorldViewProjectSC = mShader->getShaderConstHandle( "$modelViewProj" );
   mCamPosSC = mShader->getShaderConstHandle( "$camPos" );
   mCamRightSC = mShader->getShaderConstHandle( "$camRight" );
   mCamUpSC = mShader->getShaderConstHandle( "$camUp" );
   mSunDirSC = mShader->getShaderConstHandle( "$sunDir" );
   mFogDataSC = mShader->getShaderConstHandle( "$fogData" );
   mParamsSC = mShader->getShaderConstHandle( "$params" );
   mUVsSC = mShader->getShaderConstHandle( "$uvs" );
   mLightColorSC = mShader->getShaderConstHandle( "$lightColor" );
   mAmbientSC = mShader->getShaderConstHandle( "$ambient" );

   mLightTexRT = mShader->getShaderConstHandle( "$lightTexRT" );

   GFXStateBlockDesc d;
   d.cullDefined = true;
   d.cullMode = GFXCullNone;
   d.samplersDefined = true;
   d.samplers[0] = GFXSamplerStateDesc::getClampLinear();
   d.samplers[1] = GFXSamplerStateDesc::getClampLinear();
   d.samplers[2] = GFXSamplerStateDesc::getClampLinear();

   // We clip in the shader!
   //d.alphaDefined = true;
   //d.alphaTestEnable = true;
   //d.alphaTestRef = 84;
   //d.alphaTestFunc = GFXCmpGreater;

   d.zDefined = true;
   d.zEnable = true;
   d.zWriteEnable = true;

   if ( desc )
      d.addDesc( *desc );

   mSB = GFX->createStateBlock(d);
   return true;
}

void RenderImposterMgr::render( SceneState *state )
{
   PROFILE_SCOPE( RenderImposterMgr_Render );

   if (  !mElementList.size() ||
         (  !mDiffuseShaderState.mShader && 
            !mDiffuseShaderState.init( "TSImposterShaderData", NULL ) ) )
      return;

   GFXDEBUGEVENT_SCOPE( RenderImposterMgr_Render, ColorI::RED );

   _innerRender( state, mDiffuseShaderState );
}

void RenderImposterMgr::sort()
{
   Parent::sort();

   // Sort is called before rendering, so this is a
   // better place to clear stats than clear().
   smRendered = 0.0f;
   smBatches = 0.0f;
   smDrawCalls = 0.0f;
   smPolyCount = 0.0f;
   smRTChanges = 0.0f;
}

void RenderImposterMgr::_renderPrePass( const SceneState *state, RenderPrePassMgr *prePassBin, bool startPrePass )
{
   PROFILE_SCOPE( RenderImposterMgr_RenderPrePass );

   if (  !mElementList.size() || !startPrePass ||
         (  !mPrePassShaderState.mShader && 
            !mPrePassShaderState.init( "TSImposterPrePassShaderData", 
               &prePassBin->getOpaqueStenciWriteDesc() ) ) )
      return;

   GFXDEBUGEVENT_SCOPE( RenderImposterMgr_RenderPrePass, ColorI::RED );

   _innerRender( state, mPrePassShaderState );
}

void RenderImposterMgr::_innerRender( const SceneState *state, ShaderState &shaderState )
{
   PROFILE_SCOPE( RenderImposterMgr_InnerRender );

   // Capture the GFX stats for this render.
   GFXDeviceStatistics stats;
   stats.start( GFX->getDeviceStatistics() );

   GFXTransformSaver saver;

   // Init the shader.
   GFX->setShader( shaderState.mShader );
   GFX->setShaderConstBuffer( shaderState.mConsts );
   GFX->setStateBlock( shaderState.mSB );

   // Set the projection and world transform info.
   MatrixF proj = GFX->getProjectionMatrix() * GFX->getWorldMatrix();
   shaderState.mConsts->set( shaderState.mWorldViewProjectSC, proj );

   if (  shaderState.mSunDirSC ||
         shaderState.mLightColorSC ||
         shaderState.mAmbientSC )
   {
      // Pass the lighting consts.
      const LightInfo *sunlight = state->getLightManager()->getSpecialLight( LightManager::slSunLightType );
      VectorF sunDir( sunlight->getDirection() );
      sunDir.normalize();

      shaderState.mConsts->set( shaderState.mSunDirSC, sunDir );
      shaderState.mConsts->set( shaderState.mLightColorSC, sunlight->getColor() );
      shaderState.mConsts->set( shaderState.mAmbientSC, sunlight->getAmbient() );
   }

   // Get the data we need from the camera matrix.
   const MatrixF &camMat = state->getCameraTransform();
   Point3F camPos; 
   VectorF camRight, camUp, camDir;
   camMat.getColumn( 0, &camRight );
   camMat.getColumn( 1, &camDir );
   camMat.getColumn( 2, &camUp );
   camMat.getColumn( 3, &camPos );
   shaderState.mConsts->set( shaderState.mCamPosSC, camPos );
   shaderState.mConsts->set( shaderState.mCamRightSC, camRight );
   shaderState.mConsts->set( shaderState.mCamUpSC, camUp );

   if ( shaderState.mLightTexRT && shaderState.mLightTarget )
   {
      GFXTextureObject *texObject = shaderState.mLightTarget->getTargetTexture( 0 );
      GFX->setTexture( 2, texObject );
   
      // TODO: This normally shouldn't be NULL, but can be on the
      // first render... not sure why... we should investigate and
      // fix that rather than protect against it.
      if ( texObject )
      {
         const Point3I &targetSz = texObject->getSize();
         const RectI &targetVp = shaderState.mLightTarget->getTargetViewport();
         Point4F rtParams;

         ScreenSpace::RenderTargetParameters(targetSz, targetVp, rtParams);

         shaderState.mConsts->set( shaderState.mLightTexRT, rtParams ); 
      }
   }

   // Setup a fairly large dynamic vb to hold a bunch of imposters.
   if ( !mVB.isValid() )
   {
      // Setup the vb to hold a bunch of imposters at once.
      mVB.set( GFX, mImposterBatchSize * 4, GFXBufferTypeDynamic );
      
      // Setup a static index buffer for rendering.
      mIB.set( GFX, mImposterBatchSize * 6, 0, GFXBufferTypeStatic );
      U16 *idxBuff;
      mIB.lock(&idxBuff, NULL, NULL, NULL);
      for ( U32 i=0; i < mImposterBatchSize; i++ )
      {
         //
         // The vertex pattern in the VB for each 
         // imposter is as follows...
         //
         //     0----1
         //     |\   |
         //     | \  |
         //     |  \ |
         //     |   \|
         //     3----2
         //
         // We setup the index order below to ensure
         // sequental, cache friendly, access.
         //
         U32 offset = i * 4;
         idxBuff[i*6+0] = 0 + offset;
         idxBuff[i*6+1] = 1 + offset;
         idxBuff[i*6+2] = 2 + offset;
         idxBuff[i*6+3] = 2 + offset;
         idxBuff[i*6+4] = 3 + offset;
         idxBuff[i*6+5] = 0 + offset;
      }
      mIB.unlock();
   }

   // Set the buffers here once.
   GFX->setPrimitiveBuffer( mIB );
   GFX->setVertexBuffer( mVB );

   // Batch up the imposters into the buffer.  These
   // are already sorted by texture, to minimize switches
   // so just batch them up and render as they come.

   ImposterVertex* vertPtr = NULL;
   U32 vertCount = 0;
   F32 halfSize, fade, scale;
   Point3F center;
   QuatF rotQuat;

   const U32 binSize = mElementList.size();
   for( U32 i=0; i < binSize; )
   {
      ImposterRenderInst *ri = static_cast<ImposterRenderInst*>( mElementList[i].inst );

      TSLastDetail* detail = ri->detail;

      // Setup the textures.
      GFX->setTexture( 0, detail->getTextureMap() );
      GFX->setTexture( 1, detail->getNormalMap() );

      // Setup the constants for this batch.
      Point4F params( (detail->getNumPolarSteps() * 2) + 1, detail->getNumEquatorSteps(), detail->getPolarAngle(), detail->getIncludePoles() );
      shaderState.mConsts->set( shaderState.mParamsSC, params );

      U32 uvCount = getMin( detail->getTextureUVs().size(), 64 );
      AlignedArray<Point4F> rectData( uvCount, sizeof( Point4F ), (U8*)detail->getTextureUVs().address(), false );
      shaderState.mConsts->set( shaderState.mUVsSC, rectData );

      vertPtr = mVB.lock();
      vertCount = 0;

      for ( ; i < binSize; i++ )
      {
         ri = static_cast<ImposterRenderInst*>( mElementList[i].inst );

         // Stop the loop if the detail changed.
         if ( ri->detail != detail )
            break;

         ++smRendered;

         // If we're out of vb space then draw what we got.
         if ( vertCount + 4 >= mVB->mNumVerts )
         {
            mVB.unlock();
            GFX->drawIndexedPrimitive( GFXTriangleList, 0, 0, vertCount, 0, vertCount / 2 );
            vertPtr = mVB.lock();
            vertCount = 0;
         }

         center   = ri->center;
         halfSize = ri->halfSize;
         fade     = ri->alpha;
         scale    = ri->scale;
         rotQuat  = ri->rotQuat;

         // Fill in the points for this instance.
         vertPtr->center = center;
         vertPtr->center.w = 0;
         vertPtr->miscParams.set( halfSize, fade, scale );
         vertPtr->rotQuat.set( rotQuat.x, rotQuat.y, rotQuat.z, rotQuat.w );
         vertPtr++;

         vertPtr->center = center;
         vertPtr->center.w = 1;
         vertPtr->miscParams.set( halfSize, fade, scale );
         vertPtr->rotQuat.set( rotQuat.x, rotQuat.y, rotQuat.z, rotQuat.w );
         vertPtr++;

         vertPtr->center = center;
         vertPtr->center.w = 2;
         vertPtr->miscParams.set( halfSize, fade, scale );
         vertPtr->rotQuat.set( rotQuat.x, rotQuat.y, rotQuat.z, rotQuat.w );
         vertPtr++;

         vertPtr->center = center;
         vertPtr->center.w = 3;
         vertPtr->miscParams.set( halfSize, fade, scale );
         vertPtr->rotQuat.set( rotQuat.x, rotQuat.y, rotQuat.z, rotQuat.w );
         vertPtr++;

         vertCount += 4;
      }

      // Any remainder to dump?
      if ( vertCount > 0 )
      {
         smBatches++;
         mVB.unlock();
         GFX->drawIndexedPrimitive( GFXTriangleList, 0, 0, vertCount, 0, vertCount / 2 );
      }
   }

   // Capture the GFX stats for this render.
   stats.end( GFX->getDeviceStatistics() );
   smDrawCalls += stats.mDrawCalls;
   smPolyCount += stats.mPolyCount;
   smRTChanges += stats.mRenderTargetChanges;
}
