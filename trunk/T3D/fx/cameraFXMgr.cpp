//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/fx/cameraFXMgr.h"

#include "math/mRandom.h"
#include "math/mMatrix.h"

// global cam fx
CameraFXManager gCamFXMgr;


//**************************************************************************
// Camera effect
//**************************************************************************
CameraFX::CameraFX()
{
   mElapsedTime = 0.0;
   mDuration = 1.0;
}

//--------------------------------------------------------------------------
// Update
//--------------------------------------------------------------------------
void CameraFX::update( F32 dt )
{
   mElapsedTime += dt;
}





//**************************************************************************
// Camera shake effect
//**************************************************************************
CameraShake::CameraShake()
{
   mFreq.zero();
   mAmp.zero();
   mStartAmp.zero();
   mTimeOffset.zero();
   mCamFXTrans.identity();
   mFalloff = 10.0;
}

//--------------------------------------------------------------------------
// Update
//--------------------------------------------------------------------------
void CameraShake::update( F32 dt )
{
   Parent::update( dt );

   fadeAmplitude();

   VectorF camOffset;
   camOffset.x = mAmp.x * sin( M_2PI * (mTimeOffset.x + mElapsedTime) * mFreq.x );
   camOffset.y = mAmp.y * sin( M_2PI * (mTimeOffset.y + mElapsedTime) * mFreq.y );
   camOffset.z = mAmp.z * sin( M_2PI * (mTimeOffset.z + mElapsedTime) * mFreq.z );

   VectorF rotAngles;
   rotAngles.x = camOffset.x * 10.0 * M_PI/180.0;
   rotAngles.y = camOffset.y * 10.0 * M_PI/180.0;
   rotAngles.z = camOffset.z * 10.0 * M_PI/180.0;
   MatrixF rotMatrix( EulerF( rotAngles.x, rotAngles.y, rotAngles.z ) );

   mCamFXTrans = rotMatrix;
   mCamFXTrans.setPosition( camOffset );
}

//--------------------------------------------------------------------------
// Fade out the amplitude over time
//--------------------------------------------------------------------------
void CameraShake::fadeAmplitude()
{
   F32 percentDone = (mElapsedTime / mDuration);
   if( percentDone > 1.0 ) percentDone = 1.0;

   F32 time = 1 + percentDone * mFalloff;
   time = 1 / (time * time);

   mAmp = mStartAmp * time;
}

//--------------------------------------------------------------------------
// Initialize
//--------------------------------------------------------------------------
void CameraShake::init()
{
   mTimeOffset.x = 0.0;
   mTimeOffset.y = gRandGen.randF();
   mTimeOffset.z = gRandGen.randF();
}

//**************************************************************************
// CameraFXManager
//**************************************************************************
CameraFXManager::CameraFXManager()
{
   mCamFXTrans.identity();
}

//--------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------
CameraFXManager::~CameraFXManager()
{
   clear();
}

//--------------------------------------------------------------------------
// Add new effect to currently running list
//--------------------------------------------------------------------------
void CameraFXManager::addFX( CameraFX *newFX )
{
   mFXList.pushFront( newFX );
}

//--------------------------------------------------------------------------
// Clear all currently running camera effects
//--------------------------------------------------------------------------
void CameraFXManager::clear()
{
   for(CamFXList::Iterator i = mFXList.begin(); i != mFXList.end(); ++i)
   {
      delete *i;
   }

   mFXList.clear();
}

//--------------------------------------------------------------------------
// Update camera effects
//--------------------------------------------------------------------------
void CameraFXManager::update( F32 dt )
{
   mCamFXTrans.identity();

   CamFXList::Iterator cur;
   for(CamFXList::Iterator i = mFXList.begin(); i != mFXList.end(); /*Trickiness*/)
   {
      // Store previous iterator and increment while iterator is still valid.
      cur = i;
      ++i;
      CameraFX * curFX = *cur;
      curFX->update( dt );
      MatrixF fxTrans = curFX->getTrans();

      mCamFXTrans.mul( fxTrans );

      if( curFX->isExpired() )
      {
         delete curFX;
         mFXList.erase( cur );
      }
   }
}
