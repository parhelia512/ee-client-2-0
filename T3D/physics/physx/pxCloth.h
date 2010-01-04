//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PXCLOTH_H_
#define _PXCLOTH_H_


#ifndef _GAMEBASE_H_
#include "T3D/gameBase.h"
#endif
#ifndef _GFXPRIMITIVEBUFFER_H_
#include "gfx/gfxPrimitiveBuffer.h"
#endif
#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif

#ifndef _PHYSX_H_
#include "T3D/physics/physx/px.h"
#endif
#ifndef _T3D_PHYSICS_PHYSICSPLUGIN_H_
#include "T3D/physics/physicsPlugin.h"
#endif

class Material;

class PxWorld;

class NxScene;
class NxClothMesh;
class NxCloth;

class PxCloth : public GameBase
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
      UpdateMask        = Parent::NextFreeMask << 6,
      MountedMask       = Parent::NextFreeMask << 7,
      NextFreeMask      = Parent::NextFreeMask << 8
   };  

public:

   PxCloth();
   virtual ~PxCloth();

   DECLARE_CONOBJECT( PxCloth );      

   // SimObject
   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();
   virtual void inspectPostApply();
   void onPhysicsReset( PhysicsResetEvent reset );

   // NetObject
   virtual U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   virtual void unpackUpdate( NetConnection *conn, BitStream *stream );

   // SceneObject
   virtual void setTransform( const MatrixF &mat );
   virtual void setScale( const VectorF &scale );
   bool prepRenderImage( SceneState *state,
                         const U32 stateKey,
                         const U32 startZone, 
                         const bool modifyBaseState );

   // GameBase
   virtual bool onNewDataBlock( GameBaseData *dptr );
   virtual void processTick( const Move *move );
   virtual void interpolateTick( F32 delta );

protected:

   PxWorld *mWorld;
   NxScene *mScene;

   NxClothMesh *mClothMesh;
   NxCloth *mCloth;

   NxMeshData mReceiveBuffers;
   NxClothDesc mClothDesc; 
   NxClothMeshDesc mClothMeshDesc;

   bool mBendingEnabled;
   bool mDampingEnabled;
   bool mTriangleCollisionEnabled;
   bool mSelfCollisionEnabled;

   F32 mDensity;
   F32 mThickness;
   F32 mFriction;
   F32 mBendingStiffness;
   F32 mStretchingStiffness;
   F32 mDampingCoefficient;
   F32 mCollisionResponseCoefficient;
   F32 mAttachmentResponseCoefficient;

   U32 mAttachmentMask;

   static EnumTable mAttachmentFlagTable;

   String mMaterialName;
   SimObjectPtr<Material> mMaterial;
   BaseMatInstance *mMatInst;

   String lookupName;

   /// The output verts from the PhysX simulation.
   GFXVertexPNTT *mVertexRenderBuffer;

   /// The output indices from the PhysX simulation.
   U16 *mIndexRenderBuffer;

   U32 mMaxVertices;
   U32 mMaxIndices;
   U32 mNumParentIndices;

   /// The number of indices in the cloth which
   /// is updated by the PhysX simulation.
   U32 mNumIndices;

   /// The number of verts in the cloth which
   /// is updated by the PhysX simulation.
   U32 mNumVertices;

   U32 mMeshDirtyFlags;
   bool mTeared;
   bool mIsDummy;
   bool mIsVBDirty;
   bool mRecreatePending;

   GFXPrimitiveBufferHandle mPrimBuffer;
   GFXVertexBufferHandle<GFXVertexPNTT> mVB;

   Point2F mPatchSamples; 
   Point2F mPatchSize;

   MatrixF mResetXfm;

   void _updateMaterial();
   void _initMaterial();

   void _recreateCloth( const MatrixF &transform );
   void _setClothFromServer( PxCloth *serverObj );

   void _initClothMesh();
   void _createClothPatch( const MatrixF &transform );
   void _initReceiveBuffers( U32 numVertices, U32 numTriangles );

   void _setupAttachments();

   void _updateVBIB();
   void _alternateUpdateVBIB();
};

#endif // _PXCLOTH_H_