//-----------------------------------------------------------------------------
// Torque Game Engine Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "environment/timeOfDay.h"

#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "T3D/gameConnection.h"
#include "environment/sun.h"

//#include "sceneGraph/sceneGraph.h"
//#include "math/mathUtils.h"

TimeOfDayUpdateSignal TimeOfDay::smTimeOfDayUpdateSignal;

IMPLEMENT_CO_NETOBJECT_V1(TimeOfDay);

TimeOfDay::TimeOfDay() 
   :  mElevation( 0.0f ),
      mAzimuth( 0.0f ),
      mAxisTilt( 23.44f ),       // 35 degree tilt
      mDayLen( 120.0f ),         // 2 minutes
      mStartTimeOfDay( 0.5f ),   // High noon
      mTimeOfDay( 0.0f ),        // initialized to StartTimeOfDay in onAdd
      mPlay( true ),
      mDayScale( 1.0f ),
      mNightScale( 1.5f )
{
   mNetFlags.set( Ghostable | ScopeAlways );
   mTypeMask = EnvironmentObjectType;

   // Sets the sun vector directly overhead for lightmap generation
   // The value of mSunVector is grabbed by the terrain lighting stuff.
   /*
   F32 ele, azi;
   ele = azi = TORADIANS(90);
   MathUtils::getVectorFromAngles(mSunVector, azi, ele);
   */
   mPrevElevation = 0;
   mNextElevation = 0;
   mAzimuthOverride = 1.0f;

   _initColors();
}

TimeOfDay::~TimeOfDay()
{
}

bool TimeOfDay::setTimeOfDay( void *obj, const char *data )
{
   TimeOfDay *tod = static_cast<TimeOfDay*>(obj);
   tod->setTimeOfDay( dAtof( data ) );

   return false;
}

bool TimeOfDay::setPlay( void *obj, const char *data )
{
   TimeOfDay *tod = static_cast<TimeOfDay*>(obj);
   tod->setPlay( dAtob( data ) );

   return false;
}

bool TimeOfDay::setDayLength( void *obj, const char *data )
{
   TimeOfDay *tod = static_cast<TimeOfDay*>(obj);
   F32 length = dAtof( data );
   if( length != 0 )
      tod->setDayLength( length );

   return false;

}

void TimeOfDay::initPersistFields()
{
	  addGroup( "TimeOfDay" );

      addField( "axisTilt", TypeF32, Offset( mAxisTilt, TimeOfDay ),
            "The angle in degrees between global equator and tropic." );

      addProtectedField( "dayLength", TypeF32, Offset( mDayLen, TimeOfDay ), &setDayLength, &defaultProtectedGetFn,
            "The length of a virtual day in real world seconds." );

      addField( "startTime", TypeF32, Offset( mStartTimeOfDay, TimeOfDay ),
         "" );

      addProtectedField( "time", TypeF32, Offset( mTimeOfDay, TimeOfDay ), &setTimeOfDay, &defaultProtectedGetFn, "Current time of day." );

      addProtectedField( "play", TypeBool, Offset( mPlay, TimeOfDay ), &setPlay, &defaultProtectedGetFn, "True when the TimeOfDay object is operating." );

      addField( "azimuthOverride", TypeF32, Offset( mAzimuthOverride, TimeOfDay ), "" );

      addField( "dayScale", TypeF32, Offset( mDayScale, TimeOfDay ), "Scalar applied to time that elapses while the sun is up." );

      addField( "nightScale", TypeF32, Offset( mNightScale, TimeOfDay ), "Scalar applied to time that elapses while the sun is down." );

   endGroup( "TimeOfDay" );

	Parent::initPersistFields();
}

void TimeOfDay::consoleInit()
{
   Parent::consoleInit();

   //addVariable( "$TimeOfDay::currentTime", &TimeOfDay::smCurrentTime );
   //addVariable( "$TimeOfDay::timeScale", TypeF32, &TimeOfDay::smTimeScale );
}

void TimeOfDay::inspectPostApply()
{
   _updatePosition();
   setMaskBits( OrbitMask );
}

void TimeOfDay::_onGhostAlwaysDone()
{
}

bool TimeOfDay::onAdd()
{
   if ( !Parent::onAdd() )
      return false;
   
   // The server initializes to the specified starting values.
   // The client initializes itself to the server time from
   // unpackUpdate.
   if ( isServerObject() )
   {
      mTimeOfDay = mStartTimeOfDay;
   }

   // We don't use a bounds.
   setGlobalBounds();
   resetWorldBox();
   addToScene();

   // Lets receive ghost events so we can resolve
   // the sun object.
   if ( isClientObject() )
      NetConnection::smGhostAlwaysDone.notify( this, &TimeOfDay::_onGhostAlwaysDone );

   if ( isServerObject() )   
      Con::executef( this, "onAdd" );   

   return true;
}

void TimeOfDay::onRemove()
{
   if ( isClientObject() )
      NetConnection::smGhostAlwaysDone.remove( this, &TimeOfDay::_onGhostAlwaysDone );

   removeFromScene();
   Parent::onRemove();
}

U32 TimeOfDay::packUpdate(NetConnection *conn, U32 mask, BitStream *stream )
{
   Parent::packUpdate( conn, mask, stream );

   if ( stream->writeFlag( mask & OrbitMask ) )
   {
      stream->write( mStartTimeOfDay );
      stream->write( mDayLen );
      stream->write( mTimeOfDay );
      stream->write( mAxisTilt );
      stream->write( mAzimuthOverride );

      stream->write( mDayScale );
      stream->write( mNightScale );

      stream->writeFlag( mPlay );
   }

   return 0;
}

void TimeOfDay::unpackUpdate( NetConnection *conn, BitStream *stream )
{
   Parent::unpackUpdate( conn, stream );

   if ( stream->readFlag() ) // OrbitMask
   {
      stream->read( &mStartTimeOfDay );
      stream->read( &mDayLen );
      stream->read( &mTimeOfDay );
      stream->read( &mAxisTilt );
      stream->read( &mAzimuthOverride );

      stream->read( &mDayScale );
      stream->read( &mNightScale );

      mPlay = stream->readFlag();

      _updatePosition();
   }
}

void TimeOfDay::advanceTime( F32 timeDelta )
{
   if ( !mPlay )
      return;
   
   F32 elevation = mRadToDeg( mElevation );
   bool daytime = false;

   if ( elevation > 350.0f || ( 0.0f <= elevation && elevation < 190.0f ) )
   {
      timeDelta *= mDayScale;
      daytime = true;
   }
   else
   {
      timeDelta *= mNightScale;
      daytime = false;
   }

   //Con::printf( "elevation ( %f ), ( %s )", elevation, ( daytime ) ? "day" : "night" );

   // do time updates   
   mTimeOfDay += timeDelta / mDayLen;

   // It could be possible for more than a full day to 
   // pass in a single advance time, so I put this inside a loop
   // but timeEvents will not actually be called for the
   // skipped day.
   while ( mTimeOfDay > 1.0f )
      mTimeOfDay -= 1.0f;

   _updatePosition();

   if ( isServerObject() )
      _updateTimeEvents();
}

void TimeOfDay::_updatePosition()
{
   //// Full azimuth/elevation calculation.
   //// calculate sun decline and meridian angle (in radians)
   //F32 sunDecline = mSin( M_2PI * mTimeOfYear ) * mDegToRad( mAxisTilt );
   //F32 meridianAngle = mTimeOfDay * M_2PI - mDegToRad( mLongitude );

   //// calculate the elevation and azimuth (in radians)
   //mElevation = _calcElevation( mDegToRad( mLatitude ), sunDecline, meridianAngle );
   //mAzimuth = _calcAzimuth( mDegToRad( mLatitude ), sunDecline, meridianAngle );

   // Simplified azimuth/elevation calculation.
   // calculate sun decline and meridian angle (in radians)
   F32 sunDecline = mDegToRad( mAxisTilt );
   F32 meridianAngle = mTimeOfDay * M_2PI;

   mPrevElevation = mNextElevation;

   // calculate the elevation and azimuth (in radians)
   mElevation = _calcElevation( 0.0f, sunDecline, meridianAngle );
   mAzimuth = _calcAzimuth( 0.0f, sunDecline, meridianAngle );

   if ( mFabs( mAzimuthOverride ) )
   {
      mElevation = mDegToRad( mTimeOfDay * 360.0f );
      mAzimuth = mAzimuthOverride;
   }

   mNextElevation = mElevation;

   // Only the client updates the sun position!
   if ( isClientObject() )
      smTimeOfDayUpdateSignal.trigger( this, mTimeOfDay );
}

F32 TimeOfDay::_calcElevation( F32 lat, F32 dec, F32 mer )
{
   return mAsin( mSin(lat) * mSin(dec) + mCos(lat) * mCos(dec) * mCos(mer) );
}

F32 TimeOfDay::_calcAzimuth( F32 lat, F32 dec, F32 mer )
{
   // Add PI to normalize this from the range of -PI/2 to PI/2 to 0 to 2 * PI;
	  return mAtan2( mSin(mer), mCos(mer) * mSin(lat) - mTan(dec) * mCos(lat) ) + M_PI_F;
}

void TimeOfDay::_getSunColor( ColorF *outColor ) const
{
	  const COLOR_TARGET *ct = NULL;

   F32 ele = mClampF( M_2PI_F - mElevation, 0.0f, M_PI_F );
	  F32 phase = -1.0f;
	  F32 div;

   if (!mColorTargets.size())
   {
      outColor->set(1.0f,1.0f,1.0f);
      return;
   }

   if (mColorTargets.size() == 1)
   {
      ct = &mColorTargets[0];
      outColor->set(ct->color.red, ct->color.green, ct->color.blue);
      return;
   }

   //simple check
   if ( mColorTargets[0].elevation != 0.0f )
   {
      AssertFatal(0, "TimeOfDay::GetColor() - First elevation must be 0.0 radians")
      outColor->set(1.0f, 1.0f, 1.0f);
      //mBandMod = 1.0f;
      //mCurrentBandColor = color;
      return;
   }

   if ( mColorTargets[mColorTargets.size()-1].elevation != M_PI_F )
   {
      AssertFatal(0, "Celestails::GetColor() - Last elevation must be PI")
      outColor->set(1.0f, 1.0f, 1.0f);
      //mBandMod = 1.0f;
      //mCurrentBandColor = color;
      return;
   }

   //we need to find the phase and interp... also loop back around
   U32 count=0;
   for (;count < mColorTargets.size() - 1; count++)
   {
      const COLOR_TARGET *one = &mColorTargets[count];
      const COLOR_TARGET *two = &mColorTargets[count+1];

      if (ele >= one->elevation && ele <= two->elevation)
      {
			      div = two->elevation - one->elevation;
			
         //catch bad input divide by zero
         if ( mFabs( div ) < 0.01f )
            div = 0.01f;
			
			      phase = (ele - one->elevation) / div;
			      outColor->interpolate( one->color, two->color, phase );

			      //mCurrentBandColor.interpolate(one->bandColor, two->bandColor, phase);
			      //mBandMod = one->bandMod * (1.0f - phase) + two->bandMod * phase;

			      return;
		    }
	  }

	  AssertFatal(0,"This isn't supposed to happen");
}

void TimeOfDay::_initColors()
{
   // NOTE: The elevation targets represent distances 
   // from PI/2 radians (strait up).

   ColorF c;
   ColorF bc;

   // e is for elevation
   F32 e = M_PI_F / 13.0f; // (semicircle in radians)/(number of color target entries);

   // Day
   c.set(1.0f,1.0f,1.0f);
   _addColorTarget(0, c, 1.0f, c); // High noon at equanox
   c.set(.9f,.9f,.9f);
   _addColorTarget(e * 1.0f, c, 1.0f, c);
   c.set(.9f,.9f,.9f);
   _addColorTarget(e * 2.0f, c, 1.0f, c);
   c.set(.8f,.75f,.75f);
   _addColorTarget(e * 3.0f, c, 1.0f, c);
   c.set(.7f,.65f,.65f);
   _addColorTarget(e * 4.0f, c, 1.0f, c);

   //Dawn and Dusk (3 entries)
   c.set(.7f,.65f,.65f);
   bc.set(.8f,.6f,.3f);
   _addColorTarget(e * 5.0f, c, 3.0f, bc);
   c.set(.65f,.54f,.4f);
   bc.set(.75f,.5f,.4f);
   _addColorTarget(e * 6.0f, c, 2.75f, bc);
   c.set(.55f,.45f,.25f);
   bc.set(.65f,.3f,.3f);
   _addColorTarget(e * 7.0f, c, 2.5f, bc);

   //NIGHT
   c.set(.3f,.3f,.3f);
   bc.set(.7f,.4f,.2f);
   _addColorTarget(e * 8.0f, c, 1.25f, bc);
   c.set(.25f,.25f,.3f);
   bc.set(.8f,.3f,.2f);
   _addColorTarget(e * 9.0f, c, 1.00f, bc);
   c.set(.25f,.25f,.4f);
   _addColorTarget(e * 10.0f, c, 1.0f, c);
   c.set(.2f,.2f,.35f);
   _addColorTarget(e * 11.0f, c, 1.0f, c);
   c.set(.15f,.15f,.2f);
   _addColorTarget(M_PI_F, c, 1.0f, c); // Midnight at equanox.
}

void TimeOfDay::_addColorTarget( F32 ele, const ColorF &color, F32 bandMod, const ColorF &bandColor )
{
   COLOR_TARGET  newTarget;

   newTarget.elevation = ele;
   newTarget.color = color;
   newTarget.bandMod = bandMod;
   newTarget.bandColor = bandColor;

   mColorTargets.push_back(newTarget);
}

int QSORT_CALLBACK cmpTriggerElevation( const void *p1, const void *p2 )
{
   const TimeOfDayEvent *evnt1 = (const TimeOfDayEvent*)p1;
   const TimeOfDayEvent *evnt2 = (const TimeOfDayEvent*)p2;

   if ( evnt1->triggerElevation < evnt2->triggerElevation )
      return -1;
   else if ( evnt1->triggerElevation > evnt2->triggerElevation )
      return 1;
   else
      return 0;
}

void TimeOfDay::_updateTimeEvents()
{
   // Sort by trigger times here.
   //dQsort( mTimeEvents.address(), mTimeEvents.size(), sizeof( TimeOfDayEvent ), cmpTriggerElevation ); 

   // Get the prev, next elevation within 0-360 range.
   F32 prevElevation = mRadToDeg( mPrevElevation );
   F32 nextElevation = mRadToDeg( mNextElevation );

   // Walk the list, and fire any 
   // events whose trigger times 
   // are equal to the current time
   // or lie between the previous and
   // current times.
   for ( U32 i = 0; i < mTimeEvents.size(); i++ )
   {
      const TimeOfDayEvent &timeEvent = mTimeEvents[i];

      bool fire = false;

      // Elevation just rolled over form 360 to 0
      if ( nextElevation < prevElevation )
      {
         if ( nextElevation >= timeEvent.triggerElevation ||
              prevElevation < timeEvent.triggerElevation )
         {
            fire = true;
         }
      }
      // Normal progression, nextElevation is greater than previous
      else
      {
         if ( nextElevation >= timeEvent.triggerElevation &&
              prevElevation < timeEvent.triggerElevation )
         {
            fire = true;
         }
      }
            
      if ( fire )
      {
         // Call the time event callback.
         _onTimeEvent( timeEvent.identifier );
      }
   }
}

void TimeOfDay::addTimeEvent( F32 triggerElevation, const UTF8 *identifier )
{
   mTimeEvents.increment();
   mTimeEvents.last().triggerElevation = triggerElevation;
   mTimeEvents.last().identifier = identifier;
}

void TimeOfDay::_onTimeEvent( const String &identifier )
{
   String strCurrentTime = String::ToString( "%g", mTimeOfDay );

   F32 elevation = mRadToDeg( mElevation );
   while( elevation < 0 )
      elevation += 360.0f;
   while( elevation > 360.0f )
      elevation -= 360.0f;

   String strCurrentElevation = String::ToString( "%g", elevation );

   Con::executef( this, "onTimeEvent", identifier.c_str(), strCurrentTime.c_str(), strCurrentElevation.c_str() );
}

ConsoleMethod( TimeOfDay, addTimeOfDayEvent, void, 4, 4, "addTimeOfDayEvent( triggerElevation, identifierString )" )
{
   object->addTimeEvent( dAtof( argv[2] ), argv[3] );
}

ConsoleMethod( TimeOfDay, setTimeOfDay, void, 3, 3, "setTimeOfDay( time )" )
{
   object->setTimeOfDay( dAtof( argv[2] ) );
}

ConsoleMethod( TimeOfDay, setPlay, void, 3, 3, "setPlay( bool )" )
{
   object->setPlay( dAtob( argv[2] ) );
}

ConsoleMethod( TimeOfDay, setDayLength, void, 3, 3, "setDayLength( time )" )
{
   F32 length = dAtof( argv[2] );
   if( length != 0 )
      object->setDayLength( length );
   else
      Con::warnf( "setDayLength( time ): time must not equal zero." );
}
