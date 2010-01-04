//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PXBOXPLAYER_H
#define _PXBOXPLAYER_H

#ifndef _PHYSX_H_
#include "T3D/physics/physX/px.h"
#endif
#ifndef _PXPLAYER_H
#include "T3D/physics/physX/pxPlayer.h"
#endif

class Player;
class NxBoxController;
class PxWorld;


class PxBoxPlayer : public PxPlayer
{

protected:
   
   // Points to the same address as PxPlayer::mController
   NxBoxController *mBoxController;

public:

	PxBoxPlayer( Player *player, PxWorld *world );
	//virtual ~PxBoxPlayer() {}   

   virtual bool testSpacials( const Point3F &nPos, const Point3F &nSize ) const;

   virtual void setSpacials( const Point3F &nPos, const Point3F &nSize );

   virtual void renderDebug( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );

protected:

   virtual void _findContact( SceneObject **contactObject, VectorF *contactNormal ) const;
};


#endif // _PXBOXPLAYER_H