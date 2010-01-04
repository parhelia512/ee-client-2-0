//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _LIGHTBASE_H_
#define _LIGHTBASE_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
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
#ifndef _LIGHTANIMDATA_H_
#include "T3D/lightAnimData.h"
#endif

class LightAnimData;

class LightBase : public SceneObject, public ISceneLight, public virtual ITickable
{
   typedef SceneObject Parent;
   friend class LightAnimData;
   friend class LightFlareData;

protected:

   bool mIsEnabled;

   ColorF mColor;

   F32 mBrightness;

   bool mCastShadows;

   F32 mPriority;

   LightInfo *mLight;

   LightAnimData *mAnimationData; 
   LightAnimState mAnimState;
   F32 mAnimationPeriod;
   F32 mAnimationPhase;

   LightFlareData *mFlareData;
   LightFlareState mFlareState;   
   F32 mFlareScale;

   static bool smRenderViz;

   virtual void _conformLights() {}

   void _onRenderViz(   ObjectRenderInst *ri, 
                        SceneState *state, 
                        BaseMatInstance *overrideMat );

   virtual void _renderViz( SceneState *state ) {}

   enum LightMasks
   {
      InitialUpdateMask = Parent::NextFreeMask,
      EnabledMask       = Parent::NextFreeMask << 1,
      TransformMask     = Parent::NextFreeMask << 2,
      UpdateMask        = Parent::NextFreeMask << 3,
      DatablockMask     = Parent::NextFreeMask << 4,
      MountedMask       = Parent::NextFreeMask << 5,
      NextFreeMask      = Parent::NextFreeMask << 6
   };

public:

   LightBase();
   virtual ~LightBase();

   // SimObject
   virtual bool onAdd();
   virtual void onRemove();
   virtual void onDeleteNotify(SimObject *object);

   // ConsoleObject
   void inspectPostApply();
   static void initPersistFields();
   DECLARE_CONOBJECT(LightBase);

   // NetObject
   U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   void unpackUpdate( NetConnection *conn, BitStream *stream );  

   // ISceneLight
   virtual void submitLights( LightManager *lm, bool staticLighting );
   virtual LightInfo* getLight() { return mLight; }

   // SceneObject
   virtual void setTransform( const MatrixF &mat );
   virtual bool prepRenderImage( SceneState *state, const U32 stateKey, const U32, const bool );
   virtual void onMount( SceneObject *obj, S32 node );
   virtual void onUnmount( SceneObject *obj, S32 node );
   virtual void unmount();

   // ITickable
   virtual void interpolateTick( F32 delta );
   virtual void processTick();
   virtual void advanceTime( F32 timeDelta );

   /// Toggles the light on and off.
   void setLightEnabled( bool enabled );
   bool getLightEnabled() { return mIsEnabled; };

   /// Animate the light.
   virtual void pauseAnimation( void );
   virtual void playAnimation( void );
   virtual void playAnimation( LightAnimData *animData );
};

#endif // _LIGHTBASE_H_
