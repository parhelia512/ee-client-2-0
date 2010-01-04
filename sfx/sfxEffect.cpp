//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sfx/sfxEffect.h"


//-----------------------------------------------------------------------------
//    SFXOneShotEffect.
//-----------------------------------------------------------------------------

SFXOneShotEffect::SFXOneShotEffect( SFXSource* source, U32 triggerPos, bool removeWhenDone )
   : Parent( source, removeWhenDone ),
     mTriggerPos( triggerPos )
{
}

bool SFXOneShotEffect::update()
{
   if( mSource->getPosition() >= mTriggerPos )
   {
      _onTrigger();
      return mRemoveWhenDone;
   }
   else
      return true;
}

//-----------------------------------------------------------------------------
//    SFXRangeEffect.
//-----------------------------------------------------------------------------

SFXRangeEffect::SFXRangeEffect( SFXSource* source, U32 startTime, U32 endTime, bool removeWhenDone )
   : Parent( source, removeWhenDone ),
     mStartTime( startTime ),
     mEndTime( endTime ),
     mIsActive( false )
{
}

bool SFXRangeEffect::update()
{
   if( !isActive() )
   {
      SFXStatus status = mSource->getStatus();
      if( ( status == SFXStatusPlaying || status == SFXStatusBlocked )
          && mSource->getPosition() >= mStartTime )
      {
         mIsActive = true;
         _onStart();
      }
   }
   
   if( isActive() )
      _onUpdate();
      
   if( isActive() )
   {
      SFXStatus status = mSource->getStatus();
      if( ( status == SFXStatusPlaying || status == SFXStatusBlocked )
          && mSource->getPosition() > mEndTime )
      {
         _onEnd();
         mIsActive = false;
         
         return mRemoveWhenDone;
      }
   }
   
   return true;
}

//-----------------------------------------------------------------------------
//    SFXFadeEffect.
//-----------------------------------------------------------------------------

SFXFadeEffect::SFXFadeEffect( SFXSource* source, F32 time, F32 endVolume, U32 startTime, EOnEnd onEndDo, bool removeWhenDone )
   : Parent( source, startTime, startTime + U32( time * 1000.0f ), removeWhenDone ),
     mEndVolume( endVolume ),
     mOnEnd( onEndDo )
{
   
}

SFXFadeEffect::~SFXFadeEffect()
{
   // If the fade is still ongoing, restore the source's volume.
   // For fade-in, set to end volume.  For fade-out, set to start volume.
   
   if( isActive() )
   {
      if( mStartVolume > mEndVolume )
         mSource->setVolume( mStartVolume );
      else
         mSource->setVolume( mEndVolume );
   }
}

void SFXFadeEffect::_onStart()
{
   mStartVolume = mSource->getVolume();
   mCurrentVolume = mStartVolume;
}

void SFXFadeEffect::_onUpdate()
{
   F32 multiplier = F32( mSource->getPosition() - mStartTime ) / F32( mEndTime - mStartTime );

   F32 newVolume;
   if( mStartVolume > mEndVolume )
      newVolume = mStartVolume - ( ( mStartVolume - mEndVolume ) * multiplier );
   else
      newVolume = mStartVolume + ( ( mEndVolume - mStartVolume ) * multiplier );
      
   if( newVolume != mCurrentVolume )
   {
      mCurrentVolume = newVolume;
      mSource->setVolume( mCurrentVolume );
   }
}

void SFXFadeEffect::_onEnd()
{
   mSource->setVolume( mEndVolume );
   
   switch( mOnEnd )
   {
      case ON_END_Pause:
         mSource->pause( 0.0f ); // Pause without fade.
         break;
         
      case ON_END_Stop:
         mSource->stop( 0.0f ); // Stop without fade.
         break;
         
      case ON_END_Nop: ;
   }
}

//-----------------------------------------------------------------------------
//    SFXMarkerEffect.
//-----------------------------------------------------------------------------

SFXMarkerEffect::SFXMarkerEffect( SFXSource* source, const String& name, U32 pos, bool removeWhenDone )
   : Parent( source, pos, removeWhenDone ),
     mMarkerName( name )
{
}

void SFXMarkerEffect::_onTrigger()
{
   Con::executef( mSource, "onMarkerPassed", mMarkerName.c_str() );
}
