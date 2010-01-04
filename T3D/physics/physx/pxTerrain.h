//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _T3D_PHYSICS_PXTERRAIN_H_
#define _T3D_PHYSICS_PXTERRAIN_H_

#ifndef _T3D_PHYSICS_PHYSICSSTATIC_H_
#include "T3D/physics/physicsStatic.h"
#endif
#ifndef _PXUSERDATA_H_
#include "T3D/physics/physx/pxUserData.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif

class TerrainBlock;
class PxWorld;
class SceneObject;
class PhysXMaterial;
class TerrainFile;
class MatrixF;

class NxActor;
class NxHeightField;
class NxMat34;


class PxTerrain : public PhysicsStatic
{
protected:

   TerrainBlock *mTerrain;   

   NxActor *mActor;

   PxWorld *mWorld;

   NxHeightField *mHeightField;

   /// The userdata object assigned to 
   /// the terrain actor.
   PxUserData mUserData;
 
   void _init();

   void _releaseActor();

   void _createActor();

   ///
   static NxHeightField* _createHeightField( TerrainBlock *terrBlock /*,
                                             const Vector<S32> &materialIds*/ );

	PxTerrain();

   static void _makeTransform(   NxMat34 *outPose, 
                                 const MatrixF &xfm, 
                                 TerrainBlock *terrain );

public:

	virtual ~PxTerrain();

   static PxTerrain* create( TerrainBlock *terrain, PxWorld *world /*, const Vector<S32> &materialIds */ );

   // PhysicsStatic
   void setTransform( const MatrixF &xfm );
   void setScale( const Point3F &scale );
   virtual void update();

   void _scheduledUpdate();
};

#endif // _T3D_PHYSICS_PXTERRAIN_H_