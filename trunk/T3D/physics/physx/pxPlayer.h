//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PXPLAYER_H
#define _PXPLAYER_H

#ifndef _PHYSX_H_
#include "T3D/physics/physX/px.h"
#endif
#ifndef _T3D_PHYSICS_PHYSICSPLAYER_H_
#include "T3D/physics/physicsPlayer.h"
#endif
#ifndef _PXUSERDATA_H_
#include "T3D/physics/physx/pxUserData.h"
#endif
#ifndef NX_PHYSICS_NXCONTROLLER
#include <NxController.h>
#endif

class Player;
class PxWorld;


class PxPlayer : public PhysicsPlayer, public NxUserControllerHitReport
{

protected:

   NxController *mController;

   F32 mSkinWidth;

   Point3F mSize;

   PxWorld *mWorld;

   /// The userdata object assigned to the controller.
   PxUserData mUserData;

   /// Helper flag for singleplayer hack.
   bool mDummyMove;

   // NxUserControllerHitReport
   virtual NxControllerAction onShapeHit( const NxControllerShapeHit& hit );
   virtual NxControllerAction onControllerHit( const NxControllersHit& hit );

public:
   
	PxPlayer( Player *player, PxWorld *world );
	virtual ~PxPlayer();	

   static PxPlayer* create( Player *player, PxWorld *world );

   virtual Point3F move( const VectorF &displacement, Collision *outCol );

   virtual void setPosition( const MatrixF &mat ); 

   virtual void findContact( SceneObject **contactObject, VectorF *contactNormal ) const;
   
   virtual bool testSpacials( const Point3F &nPos, const Point3F &nSize ) const { return true; }

   virtual void setSpacials( const Point3F &nPos, const Point3F &nSize ) {}

   virtual void renderDebug( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );	

   inline PxPlayer* getServerObj() const;
   inline PxPlayer* getClientObj() const;

   virtual void enableCollision();
   virtual void disableCollision();

protected:

   virtual Point3F _move( const VectorF &displacement, Collision *outCol );
   virtual void _findContact( SceneObject **contactObject, VectorF *contactNormal ) const {}

   //void _onPreTick();
   //void _onPostTick( U32 elapsed );
   void _savePosition();
   void _restorePosition();
   
   // Because of the way NxController works we must notify it when
   // static objects in the scene are deleted, otherwise we can get a crash!   
   void _onStaticDeleted();

private:

   NxExtendedVec3 mSavedServerPosition;
   bool mPositionSaved;
};


#endif // _PXPLAYER_H