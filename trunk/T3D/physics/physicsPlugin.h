//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PHYSICSPLUGIN_H_
#define _T3D_PHYSICS_PHYSICSPLUGIN_H_

#ifndef _SIMSET_H_
#include "console/simSet.h"
#endif
#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif
#ifndef _TORQUE_STRING_H_
#include "core/util/str.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif
#ifndef _UTIL_DELEGATE_H_
#include "core/util/delegate.h"
#endif



class NetObject;
class Player;
//class MatrixF;
//class Point3F;

class SceneObject;
class PhysicsObject;
class PhysicsStatic;
class PhysicsWorld;
class PhysicsPlayer;

enum PhysicsResetEvent
{
   PhysicsResetEvent_Store,
   PhysicsResetEvent_Restore
};

typedef Signal<void(PhysicsResetEvent reset)> PhysicsResetSignal;

typedef Delegate<PhysicsObject*( const SceneObject *)> CreatePhysicsObjectFn; 
typedef Map<StringNoCase, CreatePhysicsObjectFn> CreateFnMap;

///
class PhysicsPlugin
{

protected:

   static PhysicsResetSignal smPhysicsResetSignal;

   // Our map of Strings to PhysicsWorld pointers.
   Map<StringNoCase, PhysicsWorld*> mPhysicsWorldLookup;

   static String smServerWorldName;
   static String smClientWorldName;

   /// A SimSet of objects to delete before the
   /// physics reset/restore event occurs.
   SimObjectPtr<SimSet> mPhysicsCleanup;
   
   static bool smSinglePlayer;

   Map<StringNoCase, CreatePhysicsObjectFn> mCreateFnMap;

public:

   PhysicsPlugin();
   virtual ~PhysicsPlugin();

   /// Returns the physics cleanup set.
   SimSet* getPhysicsCleanup() const { return mPhysicsCleanup; }
   
   inline bool isSinglePlayer() const { return smSinglePlayer; }

   virtual PhysicsStatic* createStatic( NetObject *object ) = 0;
   virtual PhysicsPlayer* createPlayer( Player *player ) = 0;

   virtual bool isSimulationEnabled() const = 0;
   virtual void enableSimulation( const String &worldName, bool enable ) = 0;

   virtual void setTimeScale( const F32 timeScale ) = 0;
   virtual const F32 getTimeScale() const = 0;

   static PhysicsResetSignal& getPhysicsResetSignal() { return smPhysicsResetSignal; } 

   virtual bool createWorld( const String &worldName ) = 0;
   virtual void destroyWorld( const String &worldName ) = 0;

   virtual PhysicsWorld* getWorld( const String &worldName ) const = 0;

   void registerObjectType( AbstractClassRep *torqueType, CreatePhysicsObjectFn createFn );
   void unregisterObjectType( AbstractClassRep *torqueType );

   PhysicsObject* createPhysicsObject( SceneObject *torqueObj );
};

/// The global pointer to the compile time 
/// selected physics system.
extern PhysicsPlugin *gPhysicsPlugin;

#endif // _T3D_PHYSICS_PHYSICSPLUGIN_H_