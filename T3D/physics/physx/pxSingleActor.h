//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PXSINGLEACTOR_H
#define _PXSINGLEACTOR_H

#ifndef _GAMEBASE_H_
#include "T3D/gameBase.h"
#endif
#ifndef __RESOURCE_H__
#include "core/resource.h"
#endif
#ifndef _T3D_PHYSICS_PHYSICSPLUGIN_H_
#include "T3D/physics/physicsPlugin.h"
#endif
#ifndef _PXUSERDATA_H_
#include "T3D/physics/physx/pxUserData.h"
#endif


class TSShape;
class TSShapeInstance;
class PxSingleActor;
class PxWorld;
class NxScene;
class NxActor;
class NxMat34;
class NxVec3;

namespace NXU
{
   class NxuPhysicsCollection;
}


class PxSingleActorData : public GameBaseData
{
   typedef GameBaseData Parent;

public:

   PxSingleActorData();
   virtual ~PxSingleActorData();

   DECLARE_CONOBJECT(PxSingleActorData);
   
   static void initPersistFields();
   
   void packData(BitStream* stream);
   void unpackData(BitStream* stream);
   
   bool preload(bool server, String &errorBuffer );
   
   void allocPrimBuffer( S32 overrideSize = -1 );

   bool _loadCollection( const UTF8 *path, bool isBinary );

public:

   // Rendering
   StringTableEntry shapeName;
   Resource<TSShape> shape;

   /// Filename to load the physics actor from.
   StringTableEntry physXStream;

   F32 forceThreshold;

   /// Physics collection that holds the actor
   /// and all associated shapes and data.
   NXU::NxuPhysicsCollection *physicsCollection;

   // Angular and Linear Drag (dampening) is scaled by this when in water.
   F32 waterDragScale;

   // The density of this object (for purposes of buoyancy calculation only).
   F32 buoyancyDensity;

   /// If this flag is set to true,
   /// the physics actor will only be
   /// created on the client, and the server
   /// object is only responsible for ghosting.
   /// Objects with this flag set will never stop
   /// the physics player from moving through them.
   bool clientOnly;

   NxActor* createActor( NxScene *scene, const NxMat34 *nxMat, const Point3F& scale );
};

//--------------------------------------------------------------------------

class PxSingleActor : public GameBase //, public LightReceiver
{
   typedef GameBase Parent;

   enum MaskBits 
   {
      MoveMask          = Parent::NextFreeMask << 0,
      WarpMask          = Parent::NextFreeMask << 1,
      LightMask         = Parent::NextFreeMask << 2,
      SleepMask         = Parent::NextFreeMask << 3,
      ForceSleepMask    = Parent::NextFreeMask << 4,
      ImpulseMask       = Parent::NextFreeMask << 5,
      NextFreeMask      = Parent::NextFreeMask << 6
   };
   
// PhysX
protected:

   PxWorld *mWorld;

   NxActor *mActor;

   /// The userdata object assigned to our actor.
   PxUserData mUserData;

   MatrixF mResetPos;

   VectorF mBuildScale;
   F32 mBuildAngDrag;
   F32 mBuildLinDrag;

   VectorF mStartImpulse;

   bool mSleepingLastTick;

   void sweepTest( MatrixF *mat );
   void applyCorrection( const MatrixF& mat, const NxVec3& linVel, const NxVec3& angVel );
   void applyWarp( const MatrixF& mat, bool interpRender, bool sweep );

protected:

   PxSingleActorData *mDataBlock;

   void setScale( const VectorF& scale );
   void setTransform( const MatrixF& mat );

   bool onAdd();
   void onRemove();

   bool onNewDataBlock( GameBaseData *dptr );

protected:

   TSShapeInstance *mShapeInstance;

   // Interpolation
   Point3F mLastPos;
   Point3F mNextPos;

   QuatF mLastRot;
   QuatF mNextRot;

   bool prepRenderImage(   SceneState* state,
                           const U32 stateKey,
                           const U32 startZone, 
                           const bool modifyBaseState );
   void renderObject     ( SceneState *state);

   //void processMessage(const String & msgName, SimObjectMessageData *);

public:

   PxSingleActor();

   DECLARE_CONOBJECT(PxSingleActor);
   static void initPersistFields();

   U32  packUpdate  ( NetConnection *conn, U32 mask, BitStream *stream );
   void unpackUpdate( NetConnection *conn,           BitStream *stream );

   /// Processes a move event and updates object state once every 32 milliseconds.
   ///
   /// This takes place both on the client and server, every 32 milliseconds (1 tick).
   ///
   /// @see    ProcessList
   /// @param  move   Move event corresponding to this tick, or NULL.
   void processTick( const Move *move );

   /// Interpolates between tick events.  This takes place on the CLIENT ONLY.
   ///
   /// @param   delta   Time since last call to interpolate
   void interpolateTick( F32 delta );

   void applyImpulse( const VectorF& vec );
   
   void onPhysicsReset( PhysicsResetEvent reset );

   F32 getMass() const;
   Point3F getVelocity() const;

   /// @see GameBase
   void onCollision( GameBase *object, const VectorF &vec );

protected:

   void _onContact(  NxActor *ourActor, 
                     NxActor *hitActor, 
                     SceneObject *hitObject,
                     const Point3F &hitPoint,
                     const Point3F &normalForce );

   void _updateContainerForces();   
   void _createActor();
};

#endif // _PXSINGLEACTOR_H

