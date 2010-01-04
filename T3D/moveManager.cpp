//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "math/mConstants.h"
#include "T3D/moveManager.h"

bool MoveManager::mDeviceIsKeyboardMouse = false;
F32 MoveManager::mForwardAction = 0;
F32 MoveManager::mBackwardAction = 0;
F32 MoveManager::mUpAction = 0;
F32 MoveManager::mDownAction = 0;
F32 MoveManager::mLeftAction = 0;
F32 MoveManager::mRightAction = 0;

bool MoveManager::mFreeLook = false;
F32 MoveManager::mPitch = 0;
F32 MoveManager::mYaw = 0;
F32 MoveManager::mRoll = 0;

F32 MoveManager::mPitchUpSpeed = 0;
F32 MoveManager::mPitchDownSpeed = 0;
F32 MoveManager::mYawLeftSpeed = 0;
F32 MoveManager::mYawRightSpeed = 0;
F32 MoveManager::mRollLeftSpeed = 0;
F32 MoveManager::mRollRightSpeed = 0;

F32 MoveManager::mXAxis_L = 0;
F32 MoveManager::mYAxis_L = 0;
F32 MoveManager::mXAxis_R = 0;
F32 MoveManager::mYAxis_R = 0;

U32 MoveManager::mTriggerCount[MaxTriggerKeys] = { 0, };
U32 MoveManager::mPrevTriggerCount[MaxTriggerKeys] = { 0, };

F32 MoveManager::mPitchCam = 0;
F32 MoveManager::mYawCam = 0;
F32 MoveManager::mKeyYawCam = 0;
F32 MoveManager::mDistanceCam = 3;

const Move NullMove =
{
   /*px=*/16, /*py=*/16, /*pz=*/16,
   /*pyaw=*/0, /*ppitch=*/0, /*proll=*/0,
   /*x=*/0, /*y=*/0,/*z=*/0,
   /*yaw=*/0, /*pitch=*/0, /*roll=*/0,
   /*id=*/0, 
   /*sendCount=*/0,

   /*checksum=*/false,
   /*deviceIsKeyboardMouse=*/false, 
   /*freeLook=*/false,
   /*triggers=*/{false,false,false,false,false,false}
};

void MoveManager::init()
{
	Con::addVariable("mvPitchCam", TypeF32, &mPitchCam);
	Con::addVariable("mvYawCam", TypeF32, &mYawCam);
	Con::addVariable("mvDistanceCam", TypeF32, &mDistanceCam);
	Con::addVariable("mvKeyYawCam", TypeF32, &mKeyYawCam);

   Con::addVariable("mvForwardAction", TypeF32, &mForwardAction);
   Con::addVariable("mvBackwardAction", TypeF32, &mBackwardAction);
   Con::addVariable("mvUpAction", TypeF32, &mUpAction);
   Con::addVariable("mvDownAction", TypeF32, &mDownAction);
   Con::addVariable("mvLeftAction", TypeF32, &mLeftAction);
   Con::addVariable("mvRightAction", TypeF32, &mRightAction);

   Con::addVariable("mvFreeLook", TypeBool, &mFreeLook);
   Con::addVariable("mvDeviceIsKeyboardMouse", TypeBool, &mDeviceIsKeyboardMouse);
   Con::addVariable("mvPitch", TypeF32, &mPitch);
   Con::addVariable("mvYaw", TypeF32, &mYaw);
   Con::addVariable("mvRoll", TypeF32, &mRoll);
   Con::addVariable("mvPitchUpSpeed", TypeF32, &mPitchUpSpeed);
   Con::addVariable("mvPitchDownSpeed", TypeF32, &mPitchDownSpeed);
   Con::addVariable("mvYawLeftSpeed", TypeF32, &mYawLeftSpeed);
   Con::addVariable("mvYawRightSpeed", TypeF32, &mYawRightSpeed);
   Con::addVariable("mvRollLeftSpeed", TypeF32, &mRollLeftSpeed);
   Con::addVariable("mvRollRightSpeed", TypeF32, &mRollRightSpeed);

   // Dual-analog
   Con::addVariable( "mvXAxis_L", TypeF32, &mXAxis_L );
   Con::addVariable( "mvYAxis_L", TypeF32, &mYAxis_L );

   Con::addVariable( "mvXAxis_R", TypeF32, &mXAxis_R );
   Con::addVariable( "mvYAxis_R", TypeF32, &mYAxis_R );

   for(U32 i = 0; i < MaxTriggerKeys; i++)
   {
      char varName[256];
      dSprintf(varName, sizeof(varName), "mvTriggerCount%d", i);
      Con::addVariable(varName, TypeS32, &mTriggerCount[i]);
   }
}

static inline F32 clampFloatWrap(F32 val)
{
   return val - F32(S32(val));
}

static inline S32 clampRangeClamp(F32 val)
{
   if(val < -1)
      return 0;
   if(val > 1)
      return 32;
            
   // 0.5 / 16 = 0.03125 ... this forces a round up to
   // make the precision near zero equal in the negative
   // and positive directions.  See...
   //
   // http://www.garagegames.com/community/forums/viewthread/49714
   
   return (S32)((val + 1.03125) * 16);
}


#define FANG2IANG(x)   ((U32)((S16)((F32(0x10000) / M_2PI) * x)) & 0xFFFF)
#define IANG2FANG(x)   (F32)((M_2PI / F32(0x10000)) * (F32)((S16)x))

void Move::unclamp()
{
   yaw = IANG2FANG(pyaw);
   pitch = IANG2FANG(ppitch);
   roll = IANG2FANG(proll);

   x = (px - 16) / F32(16);
   y = (py - 16) / F32(16);
   z = (pz - 16) / F32(16);
}

static inline F32 clampAngleClamp( F32 angle )
{
  const F32 limit = ( M_PI_F / 180.0f ) * 179.999f;
  if ( angle < -limit )
     return -limit;
  if ( angle > limit )
     return limit;

  return angle;
}

void Move::clamp()
{
   // If yaw/pitch/roll goes equal or greater than -PI/+PI it 
   // flips the direction of the rotation... we protect against
   // that by clamping before the conversion.
            
   yaw   = clampAngleClamp( yaw );
   pitch = clampAngleClamp( pitch );
   roll  = clampAngleClamp( roll );

   // angles are all 16 bit.
   pyaw = FANG2IANG(yaw);
   ppitch = FANG2IANG(pitch);
   proll = FANG2IANG(roll);

   px = clampRangeClamp(x);
   py = clampRangeClamp(y);
   pz = clampRangeClamp(z);
   unclamp();
}

void Move::pack(BitStream *stream, const Move * basemove)
{
   bool alwaysWriteAll = basemove!=NULL;
   if (!basemove)
      basemove = &NullMove;

   S32 i;
   bool triggerDifferent = false;
   for (i=0; i < MaxTriggerKeys; i++)
      if (trigger[i] != basemove->trigger[i])
         triggerDifferent = true;
   bool somethingDifferent = (pyaw!=basemove->pyaw)     ||
                             (ppitch!=basemove->ppitch) ||
                             (proll!=basemove->proll)   ||
                             (px!=basemove->px)         ||
                             (py!=basemove->py)         ||
                             (pz!=basemove->pz)         ||
                             (deviceIsKeyboardMouse!=basemove->deviceIsKeyboardMouse) ||
                             (freeLook!=basemove->freeLook) ||
                             triggerDifferent;
   
   if (alwaysWriteAll || stream->writeFlag(somethingDifferent))
   {
      if(stream->writeFlag(pyaw != basemove->pyaw))
      stream->writeInt(pyaw, 16);
      if(stream->writeFlag(ppitch != basemove->ppitch))
      stream->writeInt(ppitch, 16);
      if(stream->writeFlag(proll != basemove->proll))
      stream->writeInt(proll, 16);

      if (stream->writeFlag(px != basemove->px))
         stream->writeInt(px, 6);
      if (stream->writeFlag(py != basemove->py))
         stream->writeInt(py, 6);
      if (stream->writeFlag(pz != basemove->pz))
         stream->writeInt(pz, 6);
      stream->writeFlag(freeLook);
      stream->writeFlag(deviceIsKeyboardMouse);

      if (stream->writeFlag(triggerDifferent))
         for(i = 0; i < MaxTriggerKeys; i++)
      stream->writeFlag(trigger[i]);
   }
}

void Move::unpack(BitStream *stream, const Move * basemove)
{
   bool alwaysReadAll = basemove!=NULL;
   if (!basemove)
      basemove=&NullMove;

   if (alwaysReadAll || stream->readFlag())
   {
      pyaw = stream->readFlag() ? stream->readInt(16) : basemove->pyaw;
      ppitch = stream->readFlag() ? stream->readInt(16) : basemove->ppitch;
      proll = stream->readFlag() ? stream->readInt(16) : basemove->proll;

      px = stream->readFlag() ? stream->readInt(6) : basemove->px;
      py = stream->readFlag() ? stream->readInt(6) : basemove->py;
      pz = stream->readFlag() ? stream->readInt(6) : basemove->pz;
      freeLook = stream->readFlag();
      deviceIsKeyboardMouse = stream->readFlag();

      bool triggersDiffer = stream->readFlag();
      for (S32 i = 0; i< MaxTriggerKeys; i++)
         trigger[i] = triggersDiffer ? stream->readFlag() : basemove->trigger[i];
      unclamp();
   }
   else
      *this = *basemove;
}
