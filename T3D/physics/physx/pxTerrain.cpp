//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxTerrain.h"

#include "terrain/terrData.h"

#include "T3D/physics/physX/px.h"
#include "T3D/physics/physX/pxWorld.h"
#include "T3D/physics/physX/pxMaterial.h"
#include "T3D/physics/physX/pxUserData.h"


PxTerrain::PxTerrain()
   :  mTerrain( NULL ),
      mWorld( NULL ),
      mActor( NULL ),
      mHeightField( NULL )
{
}

PxTerrain::~PxTerrain()
{
   if ( mWorld )
      mWorld->unscheduleUpdate( this );
   _releaseActor();
}

NxHeightField* PxTerrain::_createHeightField(   TerrainBlock *terrain
                                                /*const Vector<S32> &materialIds*/ )
{
   // Since we're creating SDK level data we
   // have to have access to all active worlds.
   PxWorld::releaseWriteLocks();

   const U32 blockSize = terrain->getBlockSize() + 1;
   const TerrainFile *file = terrain->getFile();

   // Init the heightfield description.
   NxHeightFieldDesc heightFieldDesc;    
   heightFieldDesc.nbColumns           = blockSize;   
   heightFieldDesc.nbRows              = blockSize; 
   heightFieldDesc.thickness           = -10.0f;
   heightFieldDesc.convexEdgeThreshold = 0;

   // Allocate the samples.
   heightFieldDesc.samples       = new NxU32[ blockSize * blockSize ];
   heightFieldDesc.sampleStride  = sizeof(NxU32);   
   NxU8 *currentByte = (NxU8*)heightFieldDesc.samples;

   for ( U32 row = 0; row < blockSize; row++ )        
   {  
      const U32 tess = ( row + 1 ) % 2;

      for ( U32 column = 0; column < blockSize; column++ )            
      {
         NxHeightFieldSample *currentSample = (NxHeightFieldSample*)currentByte;

         currentSample->height = file->getHeight( blockSize - row - 1, column );

         currentSample->materialIndex0 = 1; //materialIds[0];
         currentSample->materialIndex1 = 1; //materialIds[0];

         currentSample->tessFlag = ( column + tess ) % 2;    

         currentByte += heightFieldDesc.sampleStride;     
      }   
   } 

   // Build it.
   NxHeightField* heightField = gPhysicsSDK->createHeightField( heightFieldDesc );

   // Destroy the temp sample array.
   delete [] heightFieldDesc.samples;   

   return heightField;
}

void PxTerrain::_releaseActor()
{
   if ( mWorld && mActor && mHeightField )
   {      
      mWorld->releaseActor( *mActor );
      mActor = NULL;
      mWorld->releaseHeightField( *mHeightField );      
      mHeightField = NULL;
   }
}

PxTerrain* PxTerrain::create( TerrainBlock *terrain, PxWorld *world
                             /*, const Vector<S32> &materialIds */ )
{
   PROFILE_SCOPE( PxTerrain_create );

   AssertFatal( world, "PxTerrain::create() - No world found!" );

   /*
   PhysXWorld *world = PhysXWorld::getWorld( isServerObject() );
   if ( world && mPhysXMaterials[0] )
   {
      // Gather the material ids.
      Vector<S32> materialIds;
      for ( U32 i=0; i < MAX_PHYSX_MATERIALS; i++ )
      {
         if ( !mPhysXMaterials[i] )
            break;

         materialIds.push_back( mPhysXMaterials[i]->getMaterialId() );
      }

      mPhysXActor = new PhysXTerrainComponent();
      mPhysXActor->init( world, this, materialIds );
   }
   */

   // This doesn't fail... so create the object.
   PxTerrain *pxTerrain = new PxTerrain();
   pxTerrain->mTerrain = terrain;
   pxTerrain->mWorld = world;

   // Create the actor, heightfield, all that stuff.
   pxTerrain->_createActor();   

   return pxTerrain;
}

void PxTerrain::_makeTransform(  NxMat34 *outPose, 
                                 const MatrixF &xfm, 
                                 TerrainBlock *terrain )
{
   NxMat34 rot;
   {
      NxMat33 rotX;
      rotX.rotX( Float_Pi / 2.0f );
      NxMat33 rotZ;
      rotZ.rotZ( Float_Pi );
      rot.M.multiply( rotZ, rotX );

      F32 terrSize = terrain->getWorldBlockSize();
      rot.t.set( terrSize, 0, 0 );   
   }

   NxMat34 mat;
   mat.setRowMajor44( xfm );

   outPose->multiply( mat, rot );
}

void PxTerrain::_createActor()
{
   // Setup the shape description.
   NxHeightFieldShapeDesc desc;  

   mHeightField = desc.heightField = _createHeightField( mTerrain /*, materialIds*/ );

   // TerrainBlock uses a 11.5 fixed point height format
   // giving it a maximum height range of 0 to 2048.
   desc.heightScale = 0.03125f;

   desc.rowScale = mTerrain->getSquareSize();  
   desc.columnScale = mTerrain->getSquareSize();  
   desc.materialIndexHighBits = 0;   

   NxActorDesc actorDesc;
   actorDesc.shapes.pushBack( &desc );
   actorDesc.body = NULL;
   actorDesc.name = mTerrain->getName();

   mUserData.setObject( mTerrain );
   actorDesc.userData = &mUserData;

   _makeTransform( &actorDesc.globalPose, mTerrain->getTransform(), mTerrain );

   mActor = mWorld->getScene()->createActor( actorDesc );
}

void PxTerrain::setTransform( const MatrixF &xfm )
{
   if ( !mActor )
      return;

   mWorld->releaseWriteLock();

   NxMat34 pose;
   _makeTransform( &pose, xfm, mTerrain );

   mActor->setGlobalPose( pose );
}

void PxTerrain::setScale( const Point3F &scale )
{
   // Terrain does not scale!
}

void PxTerrain::update()
{  
   PROFILE_SCOPE( PxTerrain_update );

   // NOTE: NxHeightField saveToDesc and loadFromDesc does not work properly.
   // Currently the only way to change an NxHeightFieldShape is to recreate it.
   // Therefore is method is NOT appropriate for frequent calls or very large terrains.

   mWorld->scheduleUpdate( this );   
}

void PxTerrain::_scheduledUpdate()
{
   _releaseActor();
   _createActor();

   smDeleteSignal.trigger();
}