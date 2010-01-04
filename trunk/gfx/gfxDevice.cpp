//-----------------------------------------------------------------------------
// Torque Shader Engine
// 
// Copyright (c) 2003 GarageGames.Com
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gfx/gfxDevice.h"

#include "gfx/gfxInit.h"
#include "gfx/gfxCubemap.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxDrawUtil.h"
#include "gfx/gfxFence.h"
#include "gfx/gfxFontRenderBatcher.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/gfxShader.h"
#include "gfx/gfxStateBlock.h"
#include "gfx/screenshot.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "gfx/gfxTextureManager.h"

#include "core/frameAllocator.h"
#include "core/stream/fileStream.h"
#include "core/strings/unicode.h"
#include "core/util/journal/process.h"
#include "core/util/safeDelete.h"
#include "console/consoleTypes.h"

GFXDevice * GFXDevice::smGFXDevice = NULL;
bool GFXDevice::smWireframe = false;
bool gDisassembleAllShaders = false;

void GFXDevice::initConsole()
{
   GFXStringEnumTranslate::init();

   Con::addVariable( "$gfx::wireframe", TypeBool, &GFXDevice::smWireframe );
   Con::addVariable( "$gfx::disassembleAllShaders", TypeBool, &gDisassembleAllShaders );
}
//-----------------------------------------------------------------------------

// Static method
GFXDevice::DeviceEventSignal& GFXDevice::getDeviceEventSignal()
{
   static DeviceEventSignal theSignal;
   return theSignal;
}

//-----------------------------------------------------------------------------

GFXDevice::GFXDevice() 
{    
   VECTOR_SET_ASSOCIATION( mVideoModes );
   VECTOR_SET_ASSOCIATION( mRTStack );

   mWorldMatrixDirty = false;
   mWorldStackSize = 0;
   mProjectionMatrixDirty = false;
   mViewMatrixDirty = false;
   mTextureMatrixCheckDirty = false;

   mViewMatrix.identity();
   mProjectionMatrix.identity();
   
   for( int i = 0; i < WORLD_STACK_MAX; i++ )
      mWorldMatrix[i].identity();
   
   AssertFatal(smGFXDevice == NULL, "Already a GFXDevice created! Bad!");
   smGFXDevice = this;
      
   // Vertex buffer cache
   mVertexBufferDirty = false;
   
   // Primitive buffer cache
   mPrimitiveBufferDirty = false;
   mTexturesDirty = false;
   
   // Use of TEXTURE_STAGE_COUNT in initialization is okay [7/2/2007 Pat]
   for(U32 i = 0; i < TEXTURE_STAGE_COUNT; i++)
   {
      mTextureDirty[i] = false;
      mCurrentTexture[i] = NULL;
      mNewTexture[i] = NULL;
      mCurrentCubemap[i] = NULL;
      mNewCubemap[i] = NULL;
      mTexType[i] = GFXTDT_Normal;

      mTextureMatrix[i].identity();
      mTextureMatrixDirty[i] = false;
   }

   mLightsDirty = false;
   for(U32 i = 0; i < LIGHT_STAGE_COUNT; i++)
   {
      mLightDirty[i] = false;
      mCurrentLightEnable[i] = false;
   }

   mGlobalAmbientColorDirty = false;
   mGlobalAmbientColor = ColorF(0.0f, 0.0f, 0.0f, 1.0f);

   mLightMaterialDirty = false;
   dMemset(&mCurrentLightMaterial, NULL, sizeof(GFXLightMaterial));

   // State block 
   mStateBlockDirty = false;
   mCurrentStateBlock = NULL;
   mNewStateBlock = NULL;

   mCurrentShaderConstBuffer = NULL;

   // misc
   mAllowRender = true;
   mCanCurrentlyRender = false;
   mInitialized = false;
   
   mRTDirty = false;
   mViewport = RectI::Zero;
   mViewportDirty = false;

   mCurrentFrontBufferIdx = 0;

   mDeviceSwizzle32 = NULL;
   mDeviceSwizzle24 = NULL;

   mResourceListHead = NULL;

   mCardProfiler = NULL;   

   // Initialize our drawing utility.
   mDrawer = NULL;

   // Add a few system wide shader macros.
   GFXShader::addGlobalMacro( "TORQUE", "1" );
   GFXShader::addGlobalMacro( "TORQUE_VERSION", String::ToString(getVersionNumber()) );
}

GFXDrawUtil* GFXDevice::getDrawUtil()
{
   if (!mDrawer)
   {
      mDrawer = new GFXDrawUtil(this);
   }
   return mDrawer;
}


//-----------------------------------------------------------------------------

void GFXDevice::deviceInited()
{
   getDeviceEventSignal().trigger(deInit);
   mDeviceStatistics.setPrefix("$GFXDeviceStatistics::");

   // Initialize the static helper textures.
   GBitmap temp( 2, 2, false, GFXFormatR8G8B8A8 );
   temp.fill( ColorI::ONE );
   GFXTexHandle::ONE.set( &temp, &GFXDefaultStaticDiffuseProfile, false, "GFXTexHandle::ONE" ); 
   temp.fill( ColorI::ZERO );
   GFXTexHandle::ZERO.set( &temp, &GFXDefaultStaticDiffuseProfile, false, "GFXTexHandle::ZERO" ); 
   temp.fill( ColorI( 128, 128, 255 ) );
   GFXTexHandle::ZUP.set( &temp, &GFXDefaultStaticNormalMapProfile, false, "GFXTexHandle::ZUP" ); 
}

//-----------------------------------------------------------------------------

// Static method
bool GFXDevice::destroy()
{
   // Cleanup the static helper textures.
   GFXTexHandle::ONE.free();
   GFXTexHandle::ZERO.free();
   GFXTexHandle::ZUP.free();

   // Make this release its buffer.
   PrimBuild::shutdown();

   // Let people know we are shutting down
   getDeviceEventSignal().trigger(deDestroy);

   if(smGFXDevice)
      smGFXDevice->preDestroy();
   SAFE_DELETE(smGFXDevice);

   return true;
}

//-----------------------------------------------------------------------------
void GFXDevice::preDestroy()
{
   // Delete draw util
   SAFE_DELETE( mDrawer );
}

//-----------------------------------------------------------------------------

GFXDevice::~GFXDevice()
{ 
   smGFXDevice = NULL;

   // Clean up our current PB, if any.
   mCurrentPrimitiveBuffer = NULL;
   mCurrentVertexBuffer = NULL;

   // Clear out our current texture references
   for (U32 i = 0; i < TEXTURE_STAGE_COUNT; i++)
   {
      mCurrentTexture[i] = NULL;
      mNewTexture[i] = NULL;
      mCurrentCubemap[i] = NULL;
      mNewCubemap[i] = NULL;
   }

   // Check for resource leaks
#ifdef TORQUE_DEBUG
   AssertFatal( GFXTextureObject::dumpActiveTOs() == 0, "There is a texture object leak, check the log for more details." );
   GFXPrimitiveBuffer::dumpActivePBs();
#endif

   SAFE_DELETE( mTextureManager );

   // Clear out our state block references
   mCurrentStateBlocks.clear();
   mNewStateBlock = NULL;
   mCurrentStateBlock = NULL;

   mCurrentShaderConstBuffer = NULL;
   /// End Block above BTR

   // -- Clear out resource list
   // Note: our derived class destructor will have already released resources.
   // Clearing this list saves us from having our resources (which are not deleted
   // just released) turn around and try to remove themselves from this list.
   while (mResourceListHead)
   {
      GFXResource * head = mResourceListHead;
      mResourceListHead = head->mNextResource;
      
      head->mPrevResource = NULL;
      head->mNextResource = NULL;
      head->mOwningDevice = NULL;
   }
}

//-----------------------------------------------------------------------------

F32 GFXDevice::formatByteSize(GFXFormat format)
{
   if(format < GFXFormat_16BIT)
      return 1.0f;// 8 bit...
   else if(format < GFXFormat_24BIT)
      return 2.0f;// 16 bit...
   else if(format < GFXFormat_32BIT)
      return 3.0f;// 24 bit...
   else if(format < GFXFormat_64BIT)
      return 4.0f;// 32 bit...
   else if(format < GFXFormat_128BIT)
      return 8.0f;// 64 bit...
   else if(format < GFXFormat_UNKNOWNSIZE)
      return 16.0f;// 128 bit...
   return 4.0f;// default...
}

GFXStateBlockRef GFXDevice::createStateBlock(const GFXStateBlockDesc& desc)
{
   PROFILE_SCOPE( GFXDevice_CreateStateBlock );

   U32 hashValue = desc.getHashValue();
   if (mCurrentStateBlocks[hashValue])
      return mCurrentStateBlocks[hashValue];

   GFXStateBlockRef result = createStateBlockInternal(desc);
   result->registerResourceWithDevice(this);   
   mCurrentStateBlocks[hashValue] = result;
   return result;
}

void GFXDevice::setStateBlock(GFXStateBlock* block)
{
   AssertFatal(block, "NULL state block!");
   AssertFatal(block->getOwningDevice() == this, "This state doesn't apply to this device!");

   if (block != mCurrentStateBlock)
   {
      mStateDirty = true;
      mStateBlockDirty = true;
      mNewStateBlock = block;
   } else {
      mStateBlockDirty = false;
      mNewStateBlock = mCurrentStateBlock;
   }
}

void GFXDevice::setStateBlockByDesc( const GFXStateBlockDesc &desc )
{
   PROFILE_SCOPE( GFXDevice_SetStateBlockByDesc );
   GFXStateBlock *block = createStateBlock( desc );
   setStateBlock( block );
}

void GFXDevice::setShaderConstBuffer(GFXShaderConstBuffer* buffer)
{
   mCurrentShaderConstBuffer = buffer;
}


//-----------------------------------------------------------------------------

void GFXDevice::updateStates(bool forceSetAll /*=false*/)
{
   PROFILE_SCOPE(GFXDevice_updateStates);

   if(forceSetAll)
   {
      bool rememberToEndScene = false;
      if(!canCurrentlyRender())
      {
         if (!beginScene())
         {
            AssertFatal(false, "GFXDevice::updateStates:  Unable to beginScene!");
         }
         rememberToEndScene = true;
      }

      setMatrix( GFXMatrixProjection, mProjectionMatrix );
      setMatrix( GFXMatrixWorld, mWorldMatrix[mWorldStackSize] );
      setMatrix( GFXMatrixView, mViewMatrix );

      if(mCurrentVertexBuffer.isValid())
         mCurrentVertexBuffer->prepare();

      if( mCurrentPrimitiveBuffer.isValid() ) // This could be NULL when the device is initalizing
         mCurrentPrimitiveBuffer->prepare();

      /// Stateblocks
      if ( mNewStateBlock )
         setStateBlockInternal(mNewStateBlock, true);
      mCurrentStateBlock = mNewStateBlock;

      for(U32 i = 0; i < getNumSamplers(); i++)
      {
         switch (mTexType[i])
         {
            case GFXTDT_Normal :
               {
                  mCurrentTexture[i] = mNewTexture[i];
                  setTextureInternal(i, mCurrentTexture[i]);
               }  
               break;
            case GFXTDT_Cube :
               {
                  mCurrentCubemap[i] = mNewCubemap[i];
                  if (mCurrentCubemap[i])
                     mCurrentCubemap[i]->setToTexUnit(i);
                  else
                     setTextureInternal(i, NULL);
               }
               break;
            default:
               AssertFatal(false, "Unknown texture type!");
               break;
         }
      }

      // Set our material
      setLightMaterialInternal(mCurrentLightMaterial);

      // Set our lights
      for(U32 i = 0; i < LIGHT_STAGE_COUNT; i++)
      {
         setLightInternal(i, mCurrentLight[i], mCurrentLightEnable[i]);
      }

       _updateRenderTargets();

      if(rememberToEndScene)
         endScene();

      return;
   }

   if (!mStateDirty)
      return;

   // Normal update logic begins here.
   mStateDirty = false;

   // Update Projection Matrix
   if( mProjectionMatrixDirty )
   {
      setMatrix( GFXMatrixProjection, mProjectionMatrix );
      mProjectionMatrixDirty = false;
   }
   
   // Update World Matrix
   if( mWorldMatrixDirty )
   {
      setMatrix( GFXMatrixWorld, mWorldMatrix[mWorldStackSize] );
      mWorldMatrixDirty = false;
   }
   
   // Update View Matrix
   if( mViewMatrixDirty )
   {
      setMatrix( GFXMatrixView, mViewMatrix );
      mViewMatrixDirty = false;
   }


   if( mTextureMatrixCheckDirty )
   {
      for( int i = 0; i < getNumSamplers(); i++ )
      {
         if( mTextureMatrixDirty[i] )
         {
            mTextureMatrixDirty[i] = false;
            setMatrix( (GFXMatrixType)(GFXMatrixTexture + i), mTextureMatrix[i] );
         }
      }

      mTextureMatrixCheckDirty = false;
   }
   
   // Update vertex buffer
   if( mVertexBufferDirty )
   {
      if(mCurrentVertexBuffer.isValid())
         mCurrentVertexBuffer->prepare();
      mVertexBufferDirty = false;
   }
   
   // Update primitive buffer
   //
   // NOTE: It is very important to set the primitive buffer AFTER the vertex buffer
   // because in order to draw indexed primitives in DX8, the call to SetIndicies
   // needs to include the base vertex offset, and the DX8 GFXDevice relies on
   // having mCurrentVB properly assigned before the call to setIndices -patw
   if( mPrimitiveBufferDirty )
   {
      if( mCurrentPrimitiveBuffer.isValid() ) // This could be NULL when the device is initalizing
         mCurrentPrimitiveBuffer->prepare();
      mPrimitiveBufferDirty = false;
   }

   // NOTE: With state blocks, it's now important to update state before setting textures
   // some devices (e.g. OpenGL) set states on the texture and we need that information before
   // the texture is activated.
   if (mStateBlockDirty)
   {
      setStateBlockInternal(mNewStateBlock, false);
      mCurrentStateBlock = mNewStateBlock;
      mStateBlockDirty = false;
   }

   if( mTexturesDirty )
   {
      mTexturesDirty = false;
      for(U32 i = 0; i < getNumSamplers(); i++)
      {
         if(!mTextureDirty[i])
            continue;
         mTextureDirty[i] = false;

         switch (mTexType[i])
         {
         case GFXTDT_Normal :
            {
               mCurrentTexture[i] = mNewTexture[i];
               setTextureInternal(i, mCurrentTexture[i]);
            }  
            break;
         case GFXTDT_Cube :
            {
               mCurrentCubemap[i] = mNewCubemap[i];
               if (mCurrentCubemap[i])
                  mCurrentCubemap[i]->setToTexUnit(i);
               else
                  setTextureInternal(i, NULL);
            }
            break;
         default:
            AssertFatal(false, "Unknown texture type!");
            break;
         }
      }
   }
   
   // Set light material
   if(mLightMaterialDirty)
   {
      setLightMaterialInternal(mCurrentLightMaterial);
      mLightMaterialDirty = false;
   }

   // Set our lights
   if(mLightsDirty)
   {
      mLightsDirty = false;
      for(U32 i = 0; i < LIGHT_STAGE_COUNT; i++)
      {
         if(!mLightDirty[i])
            continue;

         mLightDirty[i] = false;
         setLightInternal(i, mCurrentLight[i], mCurrentLightEnable[i]);
      }
   }

   _updateRenderTargets();

#ifdef TORQUE_DEBUG_RENDER
   doParanoidStateCheck();
#endif
}

//-----------------------------------------------------------------------------

void GFXDevice::setPrimitiveBuffer( GFXPrimitiveBuffer *buffer )
{
   if( buffer == mCurrentPrimitiveBuffer )
      return;
   
   mCurrentPrimitiveBuffer = buffer;
   mPrimitiveBufferDirty = true;
   mStateDirty = true;
}

//-----------------------------------------------------------------------------

void GFXDevice::drawPrimitive( U32 primitiveIndex )
{
   if( mStateDirty )
      updateStates();
      
   if (mCurrentShaderConstBuffer)
      setShaderConstBufferInternal(mCurrentShaderConstBuffer);
   
   AssertFatal( mCurrentPrimitiveBuffer.isValid(), "Trying to call drawPrimitive with no current primitive buffer, call setPrimitiveBuffer()" );
   AssertFatal( primitiveIndex < mCurrentPrimitiveBuffer->mPrimitiveCount, "Out of range primitive index.");
   drawPrimitive( mCurrentPrimitiveBuffer->mPrimitiveArray[primitiveIndex] );
}

void GFXDevice::drawPrimitive( const GFXPrimitive &prim )
{
   // Do NOT add index buffer offset to this call, it will be added by drawIndexedPrimitive
   drawIndexedPrimitive(   prim.type, 
                           prim.startVertex,
                           prim.minIndex, 
                           prim.numVertices, 
                           prim.startIndex, 
                           prim.numPrimitives );
}

//-----------------------------------------------------------------------------

void GFXDevice::drawPrimitives()
{
   if( mStateDirty )
      updateStates();

   if (mCurrentShaderConstBuffer)
      setShaderConstBufferInternal(mCurrentShaderConstBuffer);
   
   AssertFatal( mCurrentPrimitiveBuffer.isValid(), "Trying to call drawPrimitive with no current primitive buffer, call setPrimitiveBuffer()" );

   GFXPrimitive *info = NULL;
   
   for( U32 i = 0; i < mCurrentPrimitiveBuffer->mPrimitiveCount; i++ ) {
      info = &mCurrentPrimitiveBuffer->mPrimitiveArray[i];

      // Do NOT add index buffer offset to this call, it will be added by drawIndexedPrimitive
      drawIndexedPrimitive(   info->type, 
                              info->startVertex,
                              info->minIndex, 
                              info->numVertices, 
                              info->startIndex, 
                              info->numPrimitives );
   }
}

//-------------------------------------------------------------
// Console functions
//-------------------------------------------------------------
ConsoleFunction( getDisplayDeviceList, const char*, 1, 1, "Returns a tab-seperated string of the detected devices.")
{
   Vector<GFXAdapter*> adapters;
   GFXInit::getAdapters(&adapters);

   StringBuilder str;
   for (S32 i=0; i<adapters.size(); i++)
   {
      if (i)
         str.append( '\t' );
      str.append(adapters[i]->mName);
   }
   String temp = str.end();
   U32 tempSize = temp.size();

   char* retBuffer = Con::getReturnBuffer( tempSize );
   dMemcpy( retBuffer, temp, tempSize );
   return retBuffer;
}

//-----------------------------------------------------------------------------
// Set projection frustum
//-----------------------------------------------------------------------------
void GFXDevice::setFrustum(F32 left, 
                           F32 right, 
                           F32 bottom, 
                           F32 top, 
                           F32 nearPlane, 
                           F32 farPlane,
                           bool bRotate)
{
   // store values
   mFrustLeft = left;
   mFrustRight = right;
   mFrustBottom = bottom;
   mFrustTop = top;
   mFrustNear = nearPlane;
   mFrustFar = farPlane;
   mFrustOrtho = false;

   // compute matrix
   MatrixF projection;

   Point4F row;
   row.x = 2.0*nearPlane / (right-left);
   row.y = 0.0;
   row.z = 0.0;
   row.w = 0.0;
   projection.setRow( 0, row );

   row.x = 0.0;
   row.y = 2.0 * nearPlane / (top-bottom);
   row.z = 0.0;
   row.w = 0.0;
   projection.setRow( 1, row );

   row.x = (left+right) / (right-left);
   row.y = (top+bottom) / (top-bottom);
   row.z = farPlane / (nearPlane-farPlane);
   row.w = -1.0;
   projection.setRow( 2, row );

   row.x = 0.0;
   row.y = 0.0;
   row.z = nearPlane * farPlane / (nearPlane-farPlane);
   row.w = 0.0;
   projection.setRow( 3, row );

   projection.transpose();

   if (bRotate)
   {
      static MatrixF rotMat(EulerF( (M_PI_F / 2.0f), 0.0f, 0.0f));
      projection.mul( rotMat );
   }
   
   setProjectionMatrix( projection );
}


//-----------------------------------------------------------------------------
// Get projection frustum
//-----------------------------------------------------------------------------
void GFXDevice::getFrustum(F32 *left, F32 *right, F32 *bottom, F32 *top, F32 *nearPlane, F32 *farPlane, bool *isOrtho )
{
   if ( left )       *left       = mFrustLeft;
   if ( right )      *right      = mFrustRight;
   if ( bottom )     *bottom     = mFrustBottom;
   if ( top )        *top        = mFrustTop;
   if ( nearPlane )  *nearPlane  = mFrustNear;
   if ( farPlane )   *farPlane   = mFrustFar;
   if ( isOrtho )    *isOrtho    = mFrustOrtho;
}

//-----------------------------------------------------------------------------
// Set frustum using FOV (Field of view) in degrees along the horizontal axis
//-----------------------------------------------------------------------------
void GFXDevice::setFrustum( F32 FOVx, F32 aspectRatio, F32 nearPlane, F32 farPlane )
{
   // Figure our planes and pass it up.

   //b = a tan D
   F32 left    = -nearPlane * mTan( mDegToRad(FOVx) / 2.0 );
   F32 right   = -left;
   F32 bottom  = left / aspectRatio;
   F32 top     = -bottom;

   setFrustum(left, right, bottom, top, nearPlane, farPlane);

   return;
}

//-----------------------------------------------------------------------------
// Set projection matrix to ortho transform
//-----------------------------------------------------------------------------
void GFXDevice::setOrtho(F32 left, 
                         F32 right, 
                         F32 bottom, 
                         F32 top, 
                         F32 nearPlane, 
                         F32 farPlane,
                         bool doRotate)
{
   // store values
   mFrustLeft = left;
   mFrustRight = right;
   mFrustBottom = bottom;
   mFrustTop = top;
   mFrustNear = nearPlane;
   mFrustFar = farPlane;
   mFrustOrtho = true;

   // compute matrix
   MatrixF projection;

   Point4F row;

   row.x = 2.0f / (right - left);
   row.y = 0.0f;
   row.z = 0.0f;
   row.w = 0.0f;
   projection.setRow( 0, row );

   row.x = 0.0f;
   row.y = 2.0f / (top - bottom);
   row.z = 0.0f;
   row.w = 0.0f;
   projection.setRow( 1, row );

   row.x = 0.0f;
   row.y = 0.0f;
   // This may need be modified to work with OpenGL (d3d has 0..1 projection for z, vs -1..1 in OpenGL)
   row.z = 1.0f / (nearPlane - farPlane); 
   row.w = 0.0f;
   projection.setRow( 2, row );

   row.x = (left + right) / (left - right);
   row.y = (top + bottom) / (bottom - top);
   row.z = nearPlane / (nearPlane - farPlane);
   row.w = 1.0f;
   projection.setRow( 3, row );

   projection.transpose();

   static MatrixF sRotMat(EulerF( (M_PI_F / 2.0f), 0.0f, 0.0f));

   if( doRotate )
      projection.mul( sRotMat );
   
   setProjectionMatrix( projection );
}

Point2F GFXDevice::getWorldToScreenScale() const
{
   Point2F scale;

   const RectI &viewport = getViewport();

   if ( mFrustOrtho )
      scale.set(  viewport.extent.x / ( mFrustRight - mFrustLeft ),
                  viewport.extent.y / ( mFrustTop - mFrustBottom ) );
   else
      scale.set(  ( mFrustNear * viewport.extent.x ) / ( mFrustRight - mFrustLeft ),
                  ( mFrustNear * viewport.extent.y ) / ( mFrustTop - mFrustBottom ) );

   return scale;
}

//-----------------------------------------------------------------------------
// Set Light
//-----------------------------------------------------------------------------
void GFXDevice::setLight(U32 stage, GFXLightInfo* light)
{
   AssertFatal(stage < LIGHT_STAGE_COUNT, "GFXDevice::setLight - out of range stage!");

   if(!mLightDirty[stage])
   {
      mStateDirty = true;
      mLightsDirty = true;
      mLightDirty[stage] = true;
   }
   mCurrentLightEnable[stage] = (light != NULL);
   if(mCurrentLightEnable[stage])
      mCurrentLight[stage] = *light;
}

//-----------------------------------------------------------------------------
// Set Light Material
//-----------------------------------------------------------------------------
void GFXDevice::setLightMaterial(GFXLightMaterial mat)
{
   mCurrentLightMaterial = mat;
   mLightMaterialDirty = true;
   mStateDirty = true;
}

void GFXDevice::setGlobalAmbientColor(ColorF color)
{
   if(mGlobalAmbientColor != color)
   {
      mGlobalAmbientColor = color;
      mGlobalAmbientColorDirty = true;
   }
}

//-----------------------------------------------------------------------------
// Set texture
//-----------------------------------------------------------------------------
void GFXDevice::setTexture( U32 stage, GFXTextureObject *texture )
{
   AssertFatal(stage < getNumSamplers(), "GFXDevice::setTexture - out of range stage!");

   if (  mTexType[stage] == GFXTDT_Normal &&
         (  ( mTextureDirty[stage] && mNewTexture[stage].getPointer() == texture ) ||
            ( !mTextureDirty[stage] && mCurrentTexture[stage].getPointer() == texture ) ) )
      return;

   mStateDirty = true;
   mTexturesDirty = true;
   mTextureDirty[stage] = true;

   mNewTexture[stage] = texture;
   mTexType[stage] = GFXTDT_Normal;

   // Clear out the cubemaps
   mNewCubemap[stage] = NULL;
   mCurrentCubemap[stage] = NULL;
}

//-----------------------------------------------------------------------------
// Set cube texture
//-----------------------------------------------------------------------------
void GFXDevice::setCubeTexture( U32 stage, GFXCubemap *texture )
{
   AssertFatal(stage < getNumSamplers(), "GFXDevice::setTexture - out of range stage!");

   if (  mTexType[stage] == GFXTDT_Cube &&
         (  ( mTextureDirty[stage] && mNewCubemap[stage].getPointer() == texture ) ||
            ( !mTextureDirty[stage] && mCurrentCubemap[stage].getPointer() == texture ) ) )
      return;

   mStateDirty = true;
   mTexturesDirty = true;
   mTextureDirty[stage] = true;

   mNewCubemap[stage] = texture;
   mTexType[stage] = GFXTDT_Cube;

   // Clear out the normal textures
   mNewTexture[stage] = NULL;
   mCurrentTexture[stage] = NULL;
}

inline bool GFXDevice::beginScene()
{
   AssertFatal( mCanCurrentlyRender == false, "GFXDevice::beginScene() - The scene has already begun!" );

   mDeviceStatistics.clear();

   // Send the start of frame signal.
   getDeviceEventSignal().trigger( GFXDevice::deStartOfFrame );

   return beginSceneInternal();
}

//------------------------------------------------------------------------------

inline void GFXDevice::endScene()
{
   AssertFatal( mCanCurrentlyRender == true, "GFXDevice::endScene() - The scene has already ended!" );

   if( gScreenShot != NULL && gScreenShot->mPending )
      gScreenShot->captureStandard();

   // End frame signal
   getDeviceEventSignal().trigger( GFXDevice::deEndOfFrame );

   endSceneInternal();
   mDeviceStatistics.exportToConsole();
}

void GFXDevice::setViewport( const RectI &inRect ) 
{
   // Clip the rect against the renderable size.
   Point2I size = mCurrentRT->getSize();
   RectI maxRect(Point2I(0,0), size);
   RectI rect = inRect;
   rect.intersect(maxRect);

   if ( mViewport != rect )
   {
      mViewport = rect;
      mViewportDirty = true;
   }   
}

void GFXDevice::pushActiveRenderTarget()
{
   // Push the current target on to the stack.
   mRTStack.push_back( mCurrentRT );
}

void GFXDevice::popActiveRenderTarget()
{
   AssertFatal( mRTStack.size() > 0, "GFXDevice::popActiveRenderTarget() - stack is empty!" );

   // Restore the last item on the stack and pop.
   setActiveRenderTarget( mRTStack.last() );
   mRTStack.pop_back();
}

void GFXDevice::setActiveRenderTarget( GFXTarget *target )
{
   AssertFatal( target, 
      "GFXDevice::setActiveRenderTarget - must specify a render target!" );

   if ( target == mCurrentRT )
      return;
   
   // If we're not dirty then store the 
   // current RT for deactivation later.
   if ( !mRTDirty )
   {
      // Deactivate the target queued for deactivation
      if(mRTDeactivate)
         mRTDeactivate->deactivate();

      mRTDeactivate = mCurrentRT;
   }

   mRTDirty = true;
   mCurrentRT = target;

   // When a target changes we also change the viewport
   // to match it.  This causes problems when the viewport
   // has been modified for clipping to a GUI bounds.
   //
   // We should consider removing this and making it the
   // responsibility of the caller to set a proper viewport
   // when the target is changed.   
   setViewport( RectI( Point2I::Zero, mCurrentRT->getSize() ) );
}

/// Helper class for GFXDevice::describeResources.
class DescriptionOutputter
{
   /// Are we writing to a file?
   bool mWriteToFile;

   /// File if we are writing to a file
   FileStream mFile;
public:
   DescriptionOutputter(const char* file)
   {
      mWriteToFile = false;
      // If we've been given what could be a valid file path, open it.
      if(file && file[0] != '\0')
      {
         mWriteToFile = mFile.open(file, Torque::FS::File::Write);

         // Note that it is safe to retry.  If this is hit, we'll just write to the console instead of to the file.
         AssertFatal(mWriteToFile, avar("DescriptionOutputter::DescriptionOutputter - could not open file %s", file));
      }
   }

   ~DescriptionOutputter()
   {
      // Close the file
      if(mWriteToFile)
         mFile.close();
   }

   /// Writes line to the file or to the console, depending on what we want.
   void write(const char* line)
   {
      if(mWriteToFile)
         mFile.writeLine((const U8*)line);
      else
         Con::printf(line);
   }
};

#ifndef TORQUE_SHIPPING
void GFXDevice::dumpStates( const char *fileName ) const
{
   DescriptionOutputter output(fileName);

   output.write("Current state");
   if (!mCurrentStateBlock.isNull())
      output.write(mCurrentStateBlock->getDesc().describeSelf().c_str());
   else
      output.write("No state!");

   output.write("\nAll states:\n");
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      const GFXStateBlock* sb = dynamic_cast<const GFXStateBlock*>(walk);
      if (sb)
      {
         output.write(sb->getDesc().describeSelf().c_str());
      }
      walk = walk->getNextResource();
   }
}
#endif

void GFXDevice::listResources(bool unflaggedOnly)
{
   U32 numTextures = 0, numShaders = 0, numRenderToTextureTargs = 0, numWindowTargs = 0;
   U32 numCubemaps = 0, numVertexBuffers = 0, numPrimitiveBuffers = 0, numFences = 0;
   U32 numStateBlocks = 0;

   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      if(unflaggedOnly && walk->isFlagged())
      {
         walk = walk->getNextResource();
         continue;
      }

      if(dynamic_cast<GFXTextureObject*>(walk))
         numTextures++;
      else if(dynamic_cast<GFXShader*>(walk))
         numShaders++;
      else if(dynamic_cast<GFXTextureTarget*>(walk))
         numRenderToTextureTargs++;
      else if(dynamic_cast<GFXWindowTarget*>(walk))
         numWindowTargs++;
      else if(dynamic_cast<GFXCubemap*>(walk))
         numCubemaps++;
      else if(dynamic_cast<GFXVertexBuffer*>(walk))
         numVertexBuffers++;
      else if(dynamic_cast<GFXPrimitiveBuffer*>(walk))
         numPrimitiveBuffers++;
      else if(dynamic_cast<GFXFence*>(walk))
         numFences++;
      else if (dynamic_cast<GFXStateBlock*>(walk))
         numStateBlocks++;
      else
         Con::warnf("Unknown resource: %x", walk);

      walk = walk->getNextResource();
   }
   const char* flag = unflaggedOnly ? "unflagged" : "allocated";

   Con::printf("GFX currently has:");
   Con::printf("   %i %s textures", numTextures, flag);
   Con::printf("   %i %s shaders", numShaders, flag);
   Con::printf("   %i %s texture targets", numRenderToTextureTargs, flag);
   Con::printf("   %i %s window targets", numWindowTargs, flag);
   Con::printf("   %i %s cubemaps", numCubemaps, flag);
   Con::printf("   %i %s vertex buffers", numVertexBuffers, flag);
   Con::printf("   %i %s primitive buffers", numPrimitiveBuffers, flag);
   Con::printf("   %i %s fences", numFences, flag);
   Con::printf("   %i %s state blocks", numStateBlocks, flag);
}

void GFXDevice::fillResourceVectors(const char* resNames, bool unflaggedOnly, Vector<GFXResource*> &textureObjects,
                                 Vector<GFXResource*> &textureTargets, Vector<GFXResource*> &windowTargets, Vector<GFXResource*> &vertexBuffers, 
                                 Vector<GFXResource*> &primitiveBuffers, Vector<GFXResource*> &fences, Vector<GFXResource*> &cubemaps, 
                                 Vector<GFXResource*> &shaders, Vector<GFXResource*> &stateblocks)
{
   bool describeTexture = true, describeTextureTarget = true, describeWindowTarget = true, describeVertexBuffer = true, 
      describePrimitiveBuffer = true, describeFence = true, describeCubemap = true, describeShader = true,
      describeStateBlock = true;

   // If we didn't specify a string of names, we'll print all of them
   if(resNames && resNames[0] != '\0')
   {
      // If we did specify a string of names, determine which names
      describeTexture =          (dStrstr(resNames, "GFXTextureObject")    != NULL);
      describeTextureTarget =    (dStrstr(resNames, "GFXTextureTarget")    != NULL);
      describeWindowTarget =     (dStrstr(resNames, "GFXWindowTarget")     != NULL);
      describeVertexBuffer =     (dStrstr(resNames, "GFXVertexBuffer")     != NULL);
      describePrimitiveBuffer =  (dStrstr(resNames, "GFXPrimitiveBuffer")   != NULL);
      describeFence =            (dStrstr(resNames, "GFXFence")            != NULL);
      describeCubemap =          (dStrstr(resNames, "GFXCubemap")          != NULL);
      describeShader =           (dStrstr(resNames, "GFXShader")           != NULL);
      describeStateBlock =       (dStrstr(resNames, "GFXStateBlock")           != NULL);
   }

   // Start going through the list
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      // If we only want unflagged resources, skip all flagged resources
      if(unflaggedOnly && walk->isFlagged())
      {
         walk = walk->getNextResource();
         continue;
      }

      // All of the following checks go through the same logic.
      // if(describingThisResource) 
      // {
      //    ResourceType* type = dynamic_cast<ResourceType*>(walk)
      //    if(type)
      //    {
      //       typeVector.push_back(type);
      //       walk = walk->getNextResource();
      //       continue;
      //    }
      // }

      if(describeTexture)
      {
         GFXTextureObject* tex = dynamic_cast<GFXTextureObject*>(walk);
         {
            if(tex)
            {
               textureObjects.push_back(tex);
               walk = walk->getNextResource();
               continue;
            }
         }
      }
      if(describeShader)
      {
         GFXShader* shd = dynamic_cast<GFXShader*>(walk);
         if(shd)
         {
            shaders.push_back(shd);
            walk = walk->getNextResource();
            continue;
         }
      }
      if(describeVertexBuffer)
      {
         GFXVertexBuffer* buf = dynamic_cast<GFXVertexBuffer*>(walk);
         if(buf)
         {
            vertexBuffers.push_back(buf);
            walk = walk->getNextResource();
            continue;
         }
      }
      if(describePrimitiveBuffer)
      {
         GFXPrimitiveBuffer* buf = dynamic_cast<GFXPrimitiveBuffer*>(walk);
         if(buf)
         {
            primitiveBuffers.push_back(buf);
            walk = walk->getNextResource();
            continue;
         }
      }
      if(describeTextureTarget)
      {
         GFXTextureTarget* targ = dynamic_cast<GFXTextureTarget*>(walk);
         if(targ)
         {
            textureTargets.push_back(targ);
            walk = walk->getNextResource();
            continue;
         }
      }
      if(describeWindowTarget)
      {
         GFXWindowTarget* targ = dynamic_cast<GFXWindowTarget*>(walk);
         if(targ)
         {
            windowTargets.push_back(targ);
            walk = walk->getNextResource();
            continue;
         }
      }
      if(describeCubemap)
      {
         GFXCubemap* cube = dynamic_cast<GFXCubemap*>(walk);
         if(cube)
         {
            cubemaps.push_back(cube);
            walk = walk->getNextResource();
            continue;
         }
      }
      if(describeFence)
      {
         GFXFence* fence = dynamic_cast<GFXFence*>(walk);
         if(fence)
         {
            fences.push_back(fence);
            walk = walk->getNextResource();
            continue;
         }
      }
      if (describeStateBlock)
      {
         GFXStateBlock* sb = dynamic_cast<GFXStateBlock*>(walk);
         if (sb)
         {
            stateblocks.push_back(sb);
            walk = walk->getNextResource();
            continue;
         }
      }
      // Wasn't something we were looking for
      walk = walk->getNextResource();
   }
}

void GFXDevice::describeResources(const char* resNames, const char* filePath, bool unflaggedOnly)
{
   const U32 numResourceTypes = 9;
   Vector<GFXResource*> resVectors[numResourceTypes];
   const char* reslabels[numResourceTypes] = { "texture", "texture target", "window target", "vertex buffers", "primitive buffers", "fences", "cubemaps", "shaders", "stateblocks" };   

   // Fill the vectors with the right resources
   fillResourceVectors(resNames, unflaggedOnly, resVectors[0], resVectors[1], resVectors[2], resVectors[3], 
      resVectors[4], resVectors[5], resVectors[6], resVectors[7], resVectors[8]);

   // Helper object
   DescriptionOutputter output(filePath);

   // Print the info to the file
   // Note that we check if we have any objects of that type.
   for (U32 i = 0; i < numResourceTypes; i++)
   {
      if (resVectors[i].size())
      {
         // Header
         String header = String::ToString("--------Dumping GFX %s descriptions...----------", reslabels[i]);
         output.write(header);
         // Data
         for (U32 j = 0; j < resVectors[i].size(); j++)
         {
            GFXResource* resource = resVectors[i][j];
            String dataline = String::ToString("Addr: %x %s", resource, resource->describeSelf().c_str());
            output.write(dataline.c_str());
         }
         // Footer
         output.write("--------------------Done---------------------");
         output.write("");
      }
   }
}

void GFXDevice::flagCurrentResources()
{
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      walk->setFlag();
      walk = walk->getNextResource();
   }
}

void GFXDevice::clearResourceFlags()
{
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      walk->clearFlag();
      walk = walk->getNextResource();
   }
}

ConsoleFunction(listGFXResources, void, 1, 2, "(bool unflaggedOnly = false)")
{
   bool unflaggedOnly = false;
   if(argc == 2)
      unflaggedOnly = dAtob(argv[1]);
   GFX->listResources(unflaggedOnly);
}

ConsoleFunction(flagCurrentGFXResources, void, 1, 1, "")
{
   GFX->flagCurrentResources();
}

ConsoleFunction(clearGFXResourceFlags, void, 1, 1, "")
{
   GFX->clearResourceFlags();
}

ConsoleFunction(describeGFXResources, void, 3, 4, "(string resourceNames, string filePath, bool unflaggedOnly = false)\n"
                                                  " If resourceNames is "", this function describes all resources.\n"
                                                  " If filePath is "", this function writes the resource descriptions to the console")
{
   bool unflaggedOnly = false;
   if(argc == 4)
      unflaggedOnly = dAtob(argv[3]);
   GFX->describeResources(argv[1], argv[2], unflaggedOnly);
}

ConsoleFunction(describeGFXStateBlocks, void, 2, 2, "(string filePath)\n"               
               " If filePath is "", this function writes the resource descriptions to the console")
{
   GFX->dumpStates(argv[1]);   
}

//-----------------------------------------------------------------------------
// Get pixel shader version - for script
//-----------------------------------------------------------------------------
ConsoleFunction( getPixelShaderVersion, F32, 1, 1, "Get pixel shader version.\n\n" )
{
   return GFX->getPixelShaderVersion();
}   

//-----------------------------------------------------------------------------
// Set pixel shader version - for script
//-----------------------------------------------------------------------------
ConsoleFunction( setPixelShaderVersion, void, 2, 2, "Set pixel shader version.\n\n" )
{
   GFX->setPixelShaderVersion( dAtof(argv[1]) );
}

ConsoleFunction( getDisplayDeviceInformation, const char*, 1, 1, "Get a string describing the current GFX device")
{
   if (!GFXDevice::devicePresent())
      return "(no device)";

   const GFXAdapter& adapter = GFX->getAdapter();
   return adapter.getName();
}
