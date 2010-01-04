//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _CAMERAFXMGR_H_
#define _CAMERAFXMGR_H_

#ifndef _TORQUE_LIST_
#include "core/util/tList.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif

//**************************************************************************
// Abstract camera effect template
//**************************************************************************
class CameraFX
{
protected:
   MatrixF  mCamFXTrans;
   F32      mElapsedTime;
   F32      mDuration;

public:
   CameraFX();

   MatrixF &   getTrans(){ return mCamFXTrans; }
   bool        isExpired(){ return mElapsedTime >= mDuration; }
   void        setDuration( F32 duration ){ mDuration = duration; }

   virtual void update( F32 dt );
};

//--------------------------------------------------------------------------
// Camera shake effect
//--------------------------------------------------------------------------
class CameraShake : public CameraFX
{
   typedef CameraFX Parent;

   VectorF mFreq;  // these are vectors to represent these values in 3D
   VectorF mStartAmp;
   VectorF mAmp;
   VectorF mTimeOffset;
   F32     mFalloff;

public:
   CameraShake();

   void init();
   void fadeAmplitude();
   void setFalloff( F32 falloff ){ mFalloff = falloff; }
   void setFrequency( VectorF &freq ){ mFreq = freq; }
   void setAmplitude( VectorF &amp ){ mStartAmp = amp; }

   virtual void update( F32 dt );
};


//**************************************************************************
// CameraFXManager
//**************************************************************************
class CameraFXManager
{
   typedef CameraFX * CameraFXPtr;

   MatrixF              mCamFXTrans;
   typedef Torque::List<CameraFXPtr> CamFXList;
   CamFXList mFXList;

public:
   void        addFX( CameraFX *newFX );
   void        clear();
   MatrixF &   getTrans(){ return mCamFXTrans; }
   void        update( F32 dt );

   CameraFXManager();
   ~CameraFXManager();

};

extern CameraFXManager gCamFXMgr;


#endif
