
#include "platform/platform.h"
#include "lightAnimData.h"

#include "console/consoleTypes.h"
#include "T3D/lightBase.h"
#include "math/mRandom.h"
#include "sim/processList.h"
#include "core/stream/bitStream.h"

LightAnimData::LightAnimData()
: mFlicker( false ),
  mChanceTurnOn( 0.2f ),
  mChanceTurnOff( 0.2f ),
  mMinBrightness( 0.0f ),
  mMaxBrightness( 1.0f ),
  mAnimEnabled( true )
{
}

LightAnimData::~LightAnimData()
{
}

IMPLEMENT_CO_DATABLOCK_V1( LightAnimData );

void LightAnimData::initPersistFields()
{
   addField( "animEnabled", TypeBool, Offset( mAnimEnabled, LightAnimData ) );
   addField( "flicker", TypeBool, Offset( mFlicker, LightAnimData ) );
   addField( "chanceTurnOn", TypeF32, Offset( mChanceTurnOn, LightAnimData ) );
   addField( "chanceTurnOff", TypeF32, Offset( mChanceTurnOff, LightAnimData ) );
   addField( "minBrightness", TypeF32, Offset( mMinBrightness, LightAnimData ) );
   addField( "maxBrightness", TypeF32, Offset( mMaxBrightness, LightAnimData ) );

   Parent::initPersistFields();
}

bool LightAnimData::preload( bool server, String &errorStr )
{
   if ( !Parent::preload( server, errorStr ) )
      return false;

   return true;
}

void LightAnimData::packData( BitStream *stream )
{
   Parent::packData( stream );

   stream->writeFlag( mAnimEnabled );
   stream->writeFlag( mFlicker );
   stream->write( mChanceTurnOn );
   stream->write( mChanceTurnOff );
   stream->write( mMinBrightness );
   stream->write( mMaxBrightness );
}

void LightAnimData::unpackData( BitStream *stream )
{
   Parent::unpackData( stream );

   mAnimEnabled = stream->readFlag();
   mFlicker = stream->readFlag();
   stream->read( &mChanceTurnOn );
   stream->read( &mChanceTurnOff );
   stream->read( &mMinBrightness );
   stream->read( &mMaxBrightness );
}

void LightAnimData::animate( LightAnimState *state )
{   
   LightInfo *lightInfo = state->lightInfo;

   if ( !mAnimEnabled )
   {
      lightInfo->setBrightness( state->fullBrightness );
      return;
   }

   F32 timeSec = (F32)Sim::getCurrentTime() / 1000.0f;

   if ( mFlicker )
   {
      F32 delta = timeSec - state->lastTime;
      delta = getMax( getMin( delta, 10.0f ), 0.0f );   

      bool isOn = lightInfo->getBrightness() > 0.0f;
      F32 chance = isOn ? mChanceTurnOff : mChanceTurnOn;
      F32 toggledBrightness = isOn ? 0.0f : state->fullBrightness;

      while ( delta > TickSec )
      {            
         if ( mRandF() < chance )
         {
            lightInfo->setBrightness( toggledBrightness );      
            delta = 0;
            break;
         }

         delta -= TickSec;
      }

      // We have to save the remainder of delta we did not use.
      state->lastTime = timeSec - delta;
   }
   else
   {
      F32 t = ( timeSec + state->animationPhase ) / state->animationPeriod;    
      t = mSin( t * 2.0f ) * 0.5f + 0.5f;
      
      F32 brightness = mLerp( mMinBrightness, mMaxBrightness, t );
      lightInfo->setBrightness( state->fullBrightness * brightness );   
   }                 
}

IMPLEMENT_CONSOLETYPE( LightAnimData )
IMPLEMENT_GETDATATYPE( LightAnimData )
IMPLEMENT_SETDATATYPE( LightAnimData )
