//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PXCAPSULEPLAYER_H
#define _PXCAPSULEPLAYER_H

#ifndef _PXPLAYER_H_
#include "T3D/physics/physX/pxPlayer.h"
#endif

class Player;
class PxWorld;
class NxCapsuleController;


class PxCapsulePlayer : public PxPlayer
{

protected:

   // Points to the same address as PxPlayer::mController
   NxCapsuleController *mCapsuleController;

public:

   PxCapsulePlayer( Player *player, PxWorld *world );
   //virtual ~PxCapsulePlayer() {}   

   virtual bool testSpacials( const Point3F &nPos, const Point3F &nSize ) const;

   virtual void setSpacials( const Point3F &nPos, const Point3F &nSize );

   virtual void renderDebug( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );

protected:

   virtual void _findContact( SceneObject **contactObject, VectorF *contactNormal ) const;
};


#endif // _PXCAPSULEPLAYER_H