//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "sceneGraph/reflectionManager.h"

#include "platform/profiler.h"
#include "platform/platformTimer.h"
#include "console/consoleTypes.h"
#include "core/tAlgorithm.h"
#include "math/mMathFn.h"
#include "T3D/gameConnection.h"
#include "ts/tsShapeInstance.h"
#include "gui/3d/guiTSControl.h"
#include "sceneGraph/sceneGraph.h"
#include "gfx/gfxDebugEvent.h"
#include "gfx/gfxStringEnumTranslate.h"


GFX_ImplementTextureProfile( ReflectRenderTargetProfile, 
                             GFXTextureProfile::DiffuseMap, 
                             GFXTextureProfile::PreserveSize | GFXTextureProfile::NoMipmap | GFXTextureProfile::RenderTarget | GFXTextureProfile::Pooled, 
                             GFXTextureProfile::None );

GFX_ImplementTextureProfile( RefractTextureProfile,
                             GFXTextureProfile::DiffuseMap,
                             GFXTextureProfile::PreserveSize | 
                             GFXTextureProfile::RenderTarget |
                             GFXTextureProfile::Pooled,
                             GFXTextureProfile::None );

static S32 QSORT_CALLBACK compareReflectors( const void *a, const void *b )
{
   const ReflectorBase *A = *((ReflectorBase**)a);
   const ReflectorBase *B = *((ReflectorBase**)b);     
   
   F32 dif = B->score - A->score;
   return (S32)mFloor( dif );
}


U32 ReflectionManager::smFrameReflectionMS = 10;

ReflectionManager::ReflectionManager()
 : mRefractTexScale( 0.5f ),
   mUpdateRefract( true ),
   mReflectFormat( GFXFormatR8G8B8A8 )
{
   mTimer = PlatformTimer::create();

#if defined(TORQUE_OS_XENON)
   // On the Xbox360, it needs to do a resolveTo from the active target, so this
   // may as well be the full size of the active target
   mRefractTexScale = 1.0f;
#endif

   GFXDevice::getDeviceEventSignal().notify( this, &ReflectionManager::_handleDeviceEvent );
}

ReflectionManager::~ReflectionManager()
{
   SAFE_DELETE( mTimer );
   AssertFatal( mReflectors.size() == 0, "ReflectionManager, some reflectors were left nregistered!" );

   GFXDevice::getDeviceEventSignal().remove( this, &ReflectionManager::_handleDeviceEvent );
}

void ReflectionManager::registerReflector( ReflectorBase *reflector )
{
   mReflectors.push_back_unique( reflector );
}

void ReflectionManager::unregisterReflector( ReflectorBase *reflector )
{
   mReflectors.remove( reflector );   
}

void ReflectionManager::update(  F32 timeSlice, 
                                 const Point2I &resolution, 
                                 const CameraQuery &query )
{
   GFXDEBUGEVENT_SCOPE( UpdateReflections, ColorI::WHITE );

   if ( mReflectors.empty() )
      return;

   PROFILE_SCOPE( ReflectionManager_Update );

   // Calculate our target time from the slice.
   U32 targetMs = timeSlice * smFrameReflectionMS;

   // Setup a culler for testing the 
   // visibility of reflectors.
   Frustum culler;
   culler.set( false,
               query.fov,
               (F32)resolution.x / (F32)resolution.y,
               query.nearPlane, 
               query.farPlane,
               query.cameraMatrix );

   // We use the frame time and not real time 
   // here as this may be called multiple times 
   // within a frame.
   U32 startOfUpdateMs = Platform::getVirtualMilliseconds();

   ReflectParams refparams;
   refparams.query = &query;
   refparams.viewportExtent = resolution;
   refparams.culler = culler;
   refparams.startOfUpdateMs = startOfUpdateMs;

   // Update the reflection score.
   ReflectorList::iterator reflectorIter = mReflectors.begin();
   for ( ; reflectorIter != mReflectors.end(); reflectorIter++ )
      (*reflectorIter)->calcScore( refparams );

   // Sort them by the score.
   dQsort( mReflectors.address(), mReflectors.size(), sizeof(ReflectorBase*), compareReflectors );
   
   // Update as many reflections as we can 
   // within the target time limit.
   mTimer->getElapsedMs();
   mTimer->reset();
   U32 numUpdated = 0;
   reflectorIter = mReflectors.begin();
   for ( ; reflectorIter != mReflectors.end(); reflectorIter++ )
   {      
      // We're sorted by score... so once we reach 
      // a zero score we have nothing more to update.
      if ( (*reflectorIter)->score <= 0.0f )
         break;

      (*reflectorIter)->updateReflection( refparams );
      (*reflectorIter)->lastUpdateMs = startOfUpdateMs;
      numUpdated++;

      // If we run out of update time then stop.
      if ( mTimer->getElapsedMs() > targetMs )
         break;
   }

   U32 totalElapsed = mTimer->getElapsedMs();

   // Set metric/debug related script variables...

   U32 numEnabled = mReflectors.size();   
   U32 numVisible = 0;
   U32 numOccluded = 0;

   reflectorIter = mReflectors.begin();
   for ( ; reflectorIter != mReflectors.end(); reflectorIter++ )
   {      
      ReflectorBase *pReflector = (*reflectorIter);
      if ( pReflector->isOccluded() )
         numOccluded++;
   }

   const GFXTextureProfileStats &stats = ReflectRenderTargetProfile.getStats();
   
   F32 mb = ( stats.activeBytes / 1024.0f ) / 1024.0f;
   char temp[256];

   dSprintf( temp, 256, "%s %d %0.2f\n", 
      ReflectRenderTargetProfile.getName().c_str(),
      stats.activeCount,
      mb );   

   Con::setVariable( "$Reflect::textureStats", temp );
   Con::setIntVariable( "$Reflect::renderTargetsAllocated", stats.allocatedTextures );
   Con::setIntVariable( "$Reflect::poolSize", stats.activeCount );
   Con::setIntVariable( "$Reflect::numObjects", numEnabled );   
   Con::setIntVariable( "$Reflect::numVisible", numVisible ); 
   Con::setIntVariable( "$Reflect::numOccluded", numOccluded );
   Con::setIntVariable( "$Reflect::numUpdated", numUpdated );
   Con::setIntVariable( "$Reflect::elapsed", totalElapsed );
}

GFXTexHandle ReflectionManager::allocRenderTarget( const Point2I &size )
{
   return GFXTexHandle( size.x, size.y, mReflectFormat, 
                        &ReflectRenderTargetProfile, 
                        avar("%s() - mReflectTex (line %d)", __FUNCTION__, __LINE__) );
}

GFXTextureObject* ReflectionManager::getRefractTex()
{
   GFXTarget *target = GFX->getActiveRenderTarget();
   GFXFormat targetFormat = target->getFormat();
   const Point2I &targetSize = target->getSize();
   const U32 desWidth = mFloor( (F32)targetSize.x * mRefractTexScale );
   const U32 desHeight = mFloor( ( F32)targetSize.y * mRefractTexScale );

   if ( mRefractTex.isNull() || 
        mRefractTex->getWidth() != desWidth ||
        mRefractTex->getHeight() != desHeight ||
        mRefractTex->getFormat() != targetFormat )
   {
      mRefractTex.set( desWidth, desHeight, targetFormat, &RefractTextureProfile, "mRefractTex" );    
      mUpdateRefract = true;
   }

   if ( mUpdateRefract )
   {
      target->resolveTo( mRefractTex );   
      mUpdateRefract = false;
   }

   return mRefractTex;
}

bool ReflectionManager::_handleDeviceEvent( GFXDevice::GFXDeviceEventType evt )
{
   switch( evt )
   {
   case GFXDevice::deStartOfFrame:

      mUpdateRefract = true;
      break;

   case GFXDevice::deDestroy:
      
      mRefractTex = NULL;      
      break;  
      
   default:
      break;
   }

   return true;
}

ConsoleFunction( setReflectFormat, void, 2, 2, "")
{
   GFXFormat format;
   Con::setData( TypeEnum, &format, 0, 1, argv+1, &gTextureFormatEnumTable );
   REFLECTMGR->setReflectFormat( format );
}

