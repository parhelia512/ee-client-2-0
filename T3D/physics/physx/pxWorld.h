//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PHYSX_WORLD_H_
#define _PHYSX_WORLD_H_

#ifndef _T3D_PHYSICS_PHYSICSWORLD_H_
#include "T3D/physics/physicsWorld.h"
#endif
#ifndef _MMATH_H_
#include "math/mMath.h"
#endif
#ifndef _PHYSX_H_
#include "T3D/physics/physX/px.h"
#endif
#ifndef _PHYSX_CASTS_H_
#include "T3D/physics/physX/pxCasts.h"
#endif
#ifndef _SIGNAL_H_
#include "core/util/tSignal.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif

class PxContactReporter;
class PxUserNotify;
class NxController;
class NxControllerDesc;
class ShapeBase;
class TSStatic;
class SceneObject;
class ProcessList;
class GameBase;
class CharacterControllerManager;
class PxTerrain;


class PxWorld : public PhysicsWorld
{
protected:

   F32 mEditorTimeScale;

   Vector<NxCloth*> mReleaseClothQueue;
   Vector<NxJoint*> mReleaseJointQueue;
   Vector<NxActor*> mReleaseActorQueue;
   Vector<NxHeightField*> mReleaseHeightFieldQueue;

   Vector<PxTerrain*> mTerrainUpdateQueue;

   Vector<NxActor*> mCatchupQueue;

   PxContactReporter *mConactReporter;

   PxUserNotify *mUserNotify;

   NxScene *mScene;

	CharacterControllerManager *mControllerManager;
	
   /// The hardware accelerated compartment used
   /// for high performance dynamic rigid bodies.
   NxCompartment *mRigidCompartment;

   static const VectorF smDefaultGravity;

   bool  mErrorReport;

   bool	mIsEnabled;

   bool mIsSimulating;

   U32 mTickCount;

   ProcessList *mProcessList;

   bool _init( bool isServer, ProcessList *processList );

   void _destroy();

   void _releaseQueues();

   void _updateTerrain();

public:

   /// @name Overloaded PhysicWorld Methods
   /// @{

   virtual bool initWorld( bool isServer, ProcessList *processList );

   virtual void destroyWorld();

   virtual bool castRay( const Point3F &startPnt, const Point3F &endPnt, RayInfo *ri, const Point3F &impulse );

   virtual void explosion( const Point3F &pos, F32 radius, F32 forceMagnitude );   

   /// @}

   /// @name Static Methods
   /// @{

   static bool restartSDK( bool destroyOnly = false, PxWorld *clientWorld = NULL, PxWorld *serverWorld = NULL );

   static void releaseWriteLocks();

   /// @}

   PxWorld();
   virtual ~PxWorld();

public:

   NxScene* getScene() { return mScene; }

   NxCompartment* getRigidCompartment() { return mRigidCompartment; }

   U32 getTick() { return mTickCount; }

   void tickPhysics( U32 elapsedMs );

   void getPhysicsResults();

   //void enableCatchupMode( GameBase *obj );

   bool isWritable() const { return !mIsSimulating; /* mScene->isWritable(); */ }

   void releaseWriteLock();

   void setEnabled( bool enabled );
   bool getEnabled() const { return mIsEnabled; }

   NxMaterial* createMaterial( NxMaterialDesc &material );

   ///
   /// @see releaseController
   NxController* createController( NxControllerDesc &desc ); 

   //U16 setMaterial(NxMaterialDesc &material, U16 id);

   void releaseActor( NxActor &actor );

   void releaseJoint( NxJoint &joint );
   
   void releaseCloth( NxCloth &cloth );
   
   void releaseClothMesh( NxClothMesh &clothMesh );

   void releaseController( NxController &controller );

   void releaseHeightField( NxHeightField &heightfield );

   /// Returns the contact reporter for this scene.
   PxContactReporter* getContactReporter() { return mConactReporter; }

   void setEditorTimeScale( F32 timeScale ) { mEditorTimeScale = timeScale; }
   const F32 getEditorTimeScale() const { return mEditorTimeScale; }

   void scheduleUpdate( PxTerrain *terrain );
   void unscheduleUpdate( PxTerrain *terrain );

};

#endif // _PHYSX_WORLD_H_
