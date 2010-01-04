
#ifndef _PXFLUID_H_
#define _PXFLUID_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif

#ifndef _PHYSX_H_
#include "T3D/physics/physx/px.h"
#endif

class PxWorld;
class NxScene;

class PxFluid : public SceneObject
{
   typedef SceneObject Parent;

protected:
   
   enum NetMasks
   {
      UpdateMask     = Parent::NextFreeMask,
      ResetMask      = Parent::NextFreeMask << 1,
      NextFreeMask   = Parent::NextFreeMask << 2
   };

   struct FluidParticle
   {
      NxVec3   position;
      NxVec3   velocity;
      NxReal	density;
      NxReal   lifetime;
      NxU32	   id;
      NxVec3	collisionNormal;
   };

   #define MAX_PARTICLES 100

public:

   PxFluid();
   virtual ~PxFluid();

   DECLARE_CONOBJECT( PxFluid );      

   // SimObject
   virtual bool onAdd();
   virtual void onRemove();
   static void initPersistFields();
   virtual void inspectPostApply();

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

   void resetParticles();
   void setRate( F32 rate );

protected:

   void renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );

   void _createFluid();
   void _destroyFluid();

protected:

   PxWorld *mWorld;
   NxScene *mScene;

   FluidParticle *mParticles;
   //NxParticleData *mParticleData;
   NxFluid *mFluid;
   U32 mParticleCount;
   NxFluidEmitter *mEmitter;
};

#endif // _PXFLUID_H_