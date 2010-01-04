
#ifndef _LIGHTANIMDATA_H_
#define _LIGHTANIMDATA_H_

#ifndef _SIMDATABLOCK_H_
#include "console/simDatablock.h"
#endif
#ifndef _CONSOLETYPES_H_
#include "console/consoleTypes.h"
#endif

class LightInfo;

struct LightAnimState
{  
   /// Object calling LightAnimData::animate fills these in!   
   bool active;
   F32 fullBrightness;
   F32 animationPhase;
   F32 animationPeriod;   
   LightInfo *lightInfo;
   
   /// Used internally by LightAnimData!
   F32 lastTime;
   
   void clear() 
   {
      fullBrightness = 1.0f;
      animationPhase = 1.0f;
      animationPeriod = 1.0f;            
      lightInfo = NULL;
      lastTime = 0.0;
   }
};

class LightAnimData : public SimDataBlock
{
   typedef SimDataBlock Parent;
public:

   LightAnimData();
   virtual ~LightAnimData();

   DECLARE_CONOBJECT( LightAnimData );

   static void initPersistFields();

   // SimDataBlock
   virtual bool preload( bool server, String &errorStr );
   virtual void packData( BitStream *stream );
   virtual void unpackData( BitStream *stream );

   /// Animates parameters on the passed Light's LightInfo object.
   virtual void animate( LightAnimState *state );

   // Fields...

   bool mAnimEnabled;

   bool mFlicker;
   F32 mChanceTurnOn;
   F32 mChanceTurnOff;

   F32 mMinBrightness;
   F32 mMaxBrightness;
};

DECLARE_CONSOLETYPE(LightAnimData)

#endif // _LIGHTANIMDATA_H_