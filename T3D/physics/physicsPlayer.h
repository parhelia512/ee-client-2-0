//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PHYSICSPLAYER_H_
#define _T3D_PHYSICS_PHYSICSPLAYER_H_

#ifndef _T3D_PHYSICS_PHYSICSOBJECT_H_
#include "T3D/physics/physicsObject.h"
#endif
#ifndef _MMATH_H_
#include "math/mMath.h"
#endif

struct Collision;
struct ObjectRenderInst;
class BaseMatInstance;
class Player;
class SceneState;
class SceneObject;


///
class PhysicsPlayer : public PhysicsObject
{

protected: 

   Collision *mLastCollision;

   Player *mPlayer;

public:

   PhysicsPlayer( Player *player ) { mPlayer = player; }
   virtual ~PhysicsPlayer() {};

   virtual void findContact( SceneObject **contactObject, VectorF *contactNormal ) const = 0;

   virtual Point3F move( const VectorF &displacement, Collision *outCol ) = 0;

   virtual void setPosition( const MatrixF &mat ) = 0; 

   virtual bool testSpacials( const Point3F &nPos, const Point3F &nSize ) const = 0;

   virtual void setSpacials( const Point3F &nPos, const Point3F &nSize ) = 0;

   virtual void renderDebug( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat ) {};

   virtual void enableCollision() = 0;
   virtual void disableCollision() = 0;
};


#endif // _T3D_PHYSICS_PHYSICSPLAYER_H_