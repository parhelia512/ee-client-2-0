//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PXPLUGIN_H_
#define _T3D_PHYSICS_PXPLUGIN_H_

#ifndef _T3D_PHYSICS_PHYSICSPLUGIN_H_
#include "T3D/physics/physicsPlugin.h"
#endif


class PxPlugin : public PhysicsPlugin
{

public:

   PxPlugin();
   ~PxPlugin();

   // PhysicsPlugin
   virtual PhysicsStatic* createStatic( NetObject *object );
   virtual PhysicsPlayer* createPlayer( Player *player );

   virtual bool isSimulationEnabled() const;
   virtual void enableSimulation( const String &worldName, bool enable );

   virtual void setTimeScale( const F32 timeScale );
   virtual const F32 getTimeScale() const;

   virtual bool createWorld( const String &worldName );
   virtual void destroyWorld( const String &worldName );

   virtual PhysicsWorld* getWorld( const String &worldName ) const;
   virtual PhysicsWorld* getWorld() const;
   virtual U32 getWorldCount() const;
};

#endif // _T3D_PHYSICS_PXPLUGIN_H_