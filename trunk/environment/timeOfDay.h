//-----------------------------------------------------------------------------
// Torque Game Engine Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------


#ifndef _TIMEOFDAY_H_
#define _TIMEOFDAY_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _ITICKABLE_H_
#include "core/iTickable.h"
#endif

class Sun;
class TimeOfDay;

struct COLOR_TARGET
{
	F32		elevation; // maximum target elevation
	ColorF	color; //normalized 0 = 1.0 ... 
	F32		bandMod;  //6 is max
	ColorF	bandColor;
};

typedef Vector<COLOR_TARGET> COLOR_TARGETS;

typedef Signal<void(TimeOfDay *timeOfDay, F32 time)> TimeOfDayUpdateSignal;


struct TimeOfDayEvent
{
   // The elevation at which
   // this event will fire.
   F32 triggerElevation;

   // User identifier for the event.
   String identifier;
};

class TimeOfDay : public SceneObject, public virtual ITickable
{
   typedef SceneObject Parent;

public:

   static S32 smCurrentTime;   
   static F32 smTimeScale; // To pause or resume time flow from outside this object, like in the editor.

	TimeOfDay();
   virtual ~TimeOfDay();
	
	// ConsoleObject
	static void initPersistFields();
   static void consoleInit();
   DECLARE_CONOBJECT( TimeOfDay );
   void inspectPostApply();

   // SimObject
   virtual bool onAdd();
   virtual void onRemove();

   // NetObject
   U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   void unpackUpdate( NetConnection *conn, BitStream *stream );  

   // ITickable
   virtual void interpolateTick( F32 delta ) {}
   virtual void processTick() {}
   virtual void advanceTime( F32 timeDelta );

   F32 getAzimuthRads() { return mAzimuth; }
   F32 getElevationRads() { return mElevation; }
   F32 getAzimuthDegrees() { return mRadToDeg(mAzimuth); }
   F32 getElevationDegrees() { return mRadToDeg(mElevation); }

   /*
	// Sun position stuff
	void UpdateSunPosition(void);
	// void UpdateSunPosition(fxSunLight *sunLight);


	// Scene lighting (Adapted from Joshua Ritter's Day/Night cycle code)
	// I changed references to pointers on the basis of principle. ;-)
	void EnableLighting(F32 emissiveScale = 1.0);
	void DisableLighting();
	F32 GetIntensity() 
	{ return (mCurrentColor.blue + mCurrentColor.green + mCurrentColor.red) / 3; }
   */
   static TimeOfDayUpdateSignal& getTimeOfDayUpdateSignal() { return smTimeOfDayUpdateSignal; } 
   void getSunColor( ColorF *outColor ) const { _getSunColor( outColor ); }

   void addTimeEvent( F32 triggerElevation, const UTF8 *identifier );

   void setTimeOfDay( F32 time ) { mTimeOfDay = time; setMaskBits( OrbitMask ); }
   void setPlay( bool play ) { mPlay = play; setMaskBits( OrbitMask ); }
   void setDayLength( F32 length ) { mDayLen = length; setMaskBits( OrbitMask ); }

protected:

   Vector<TimeOfDayEvent> mTimeEvents;

   void _updateTimeEvents();
   void _onTimeEvent( const String &identifier );

   static TimeOfDayUpdateSignal smTimeOfDayUpdateSignal;

   enum NetMaskBits 
   {
      OrbitMask = BIT(0),
   };

   void _updatePosition();

   void _onGhostAlwaysDone();

   F32 _calcElevation( F32 lat, F32 dec, F32 mer );

   F32 _calcAzimuth( F32 lat, F32 dec, F32 mer );

   /// Adds all of our target colors to our COLOR_TARGETS.
   void _initColors();

   /// Adds a color target to our set of targets.
   /// 
   /// @param ele [in] target sun elevation.
   /// @param color [in] target color.
   /// @param bandMod [in]
   /// @param bandColor [in]
   void _addColorTarget( F32 ele, const ColorF &color, F32 bandMod, const ColorF &bandColor );

	  // Grab our sun and sky colors based upon sun elevation.
   void _getSunColor( ColorF *outColor ) const;

   static bool setTimeOfDay( void *obj, const char *data );
   static bool setPlay( void *obj, const char *data );
   static bool setDayLength( void *obj, const char *data );

   /*
	// Get a pointer to the sun's light object
	Sun* GetSunObject();
	// return number between 0 and 1 representing color variance
	F32 getColorVariance();
   */

	// Date tracking stuff	
   F32 mStartTimeOfDay;    ///< The time of day this object begins at.
	F32 mDayLen;            ///< length of day in real world seconds.
   F32 mPrevElevation;      ///< The 0-360 elevation for the previous update.
   F32 mNextElevation;      ///< The 0-360 elevation for the next update.
   F32 mTimeOfDay;         ///< The zero to one time of day where zero is the start of a day and one is the end.	

   F32 mAzimuthOverride;  ///< Used to specify an azimuth that will stay constant throughout the day cycle.

	// Global positioning stuff
	F32 mAxisTilt; // angle between global equator and tropic

	F32 mAzimuth; // Angle from true north of celestial object in radians
	F32 mElevation; // Angle from horizon of celestial object in radians

   VectorF mZenithDirection; // The direction of celestial object at the zenith of its orbit.

   // Scalar applied to time that elapses while the sun is up.
   F32 mDayScale;
   // Scalar applied to time that elapses while the sun is down.
   F32 mNightScale;

	// color management
	COLOR_TARGETS mColorTargets;

   /*
	ColorF mCurrentColor;
	F32 mBandMod;
	ColorF mCurrentBandColor;

	// PersistFields preparation
	bool mConvertedToRads;	
   */

   // Debugging stuff that probably needs to be removed eventaully
   bool mPlay;
};


#endif // _TIMEOFDAY_H_