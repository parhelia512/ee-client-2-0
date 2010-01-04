//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SUN_H_
#define _SUN_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _LIGHTINFO_H_
#include "lighting/lightInfo.h"
#endif
#ifndef _ITICKABLE_H_
#include "core/iTickable.h"
#endif
#ifndef _LIGHTFLAREDATA_H_
#include "T3D/lightFlareData.h"
#endif

class TimeOfDay;

///
class Sun : public SceneObject, public ISceneLight, public virtual ITickable
{
   typedef SceneObject Parent;

protected:

   F32 mSunAzimuth;
   
   F32 mSunElevation;

   ColorF mLightColor;

   ColorF mLightAmbient;

   F32 mBrightness;

   bool mAnimateSun;
   F32  mTotalTime;
   F32  mCurrTime;
   F32  mStartAzimuth;
   F32  mEndAzimuth;
   F32  mStartElevation;
   F32  mEndElevation;

   bool mCastShadows;

   LightInfo *mLight;

   LightFlareData *mFlareData;
   LightFlareState mFlareState;
   F32 mFlareScale;

   bool mCoronaEnabled;
   String mCoronaTextureName;
   GFXTexHandle mCoronaTexture;
   F32 mCoronaScale;
   ColorF mCoronaTint;
   bool mCoronaUseLightColor;
 
   GFXStateBlockRef mCoronaSB;
   GFXStateBlockRef mCoronaWireframeSB;

   void _conformLights();
   void _initCorona();
   void _renderCorona( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );
   void _updateTimeOfDay( TimeOfDay *timeOfDay, F32 time );

   enum NetMaskBits 
   {
      UpdateMask = BIT(0)
   };

public:

   Sun();
   virtual ~Sun();

   // SimObject
   virtual bool onAdd();
   virtual void onRemove();

   // ConsoleObject
   DECLARE_CONOBJECT(Sun);   
   static void initPersistFields();
   void inspectPostApply();

   // NetObject
   U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   void unpackUpdate( NetConnection *conn, BitStream *stream ); 

   // ISceneLight
   virtual void submitLights( LightManager *lm, bool staticLighting );
   virtual LightInfo* getLight() { return mLight; }

   // ITickable
   virtual void interpolateTick( F32 delta ) {};
   virtual void processTick() {};
   virtual void advanceTime( F32 timeDelta );

   // SceneObject
   virtual bool prepRenderImage( SceneState *state, const U32 stateKey, const U32, const bool );

   ///
   void setAzimuth( F32 azimuth );

   ///
   void setElevation( F32 elevation );

   ///
   void setColor( const ColorF &color );

   ///
   void animate( F32 duration, F32 startAzimuth, F32 endAzimuth, F32 startElevation, F32 endElevation );
};

#endif // _SUN_H_
