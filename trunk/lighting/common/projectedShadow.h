//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PROJECTEDSHADOW_H_
#define _PROJECTEDSHADOW_H_

#include "collision/depthSortList.h"
#include "sceneGraph/sceneObject.h"
#include "ts/tsShapeInstance.h"
#include "lighting/common/shadowBase.h"

class ShapeBase;
class LightInfo;
class DecalData;
class DecalInstance;
class RenderPassManager;
class RenderMeshMgr;
class CustomMaterial;
class BaseMatInstance;

GFX_DeclareTextureProfile( BLProjectedShadowProfile );
GFX_DeclareTextureProfile( BLProjectedShadowZProfile );

class ProjectedShadow : public ShadowBase
{

protected:


   /// This parameter is used to
   /// adjust the far plane out for our
   /// orthographic render in order to 
   /// force our object towards one end of the
   /// the eye space depth range.
   static F32 smDepthAdjust;

   F32 mRadius;
   MatrixF mWorldToLight;
   U32 mLastRenderTime;

   F32 mShadowLength;

   F32 mScore;
   bool mUpdateTexture;

   Point3F mLastObjectScale;
   Point3F mLastObjectPosition;
   VectorF mLastLightDir;

   DecalData *mDecalData;
   DecalInstance *mDecalInstance;

   SceneObject *mParentObject;
   ShapeBase *mShapeBase;

   MaterialParameterHandle *mCasterPositionSC;
   MaterialParameterHandle *mShadowLengthSC;

   static SimObjectPtr<RenderPassManager> smRenderPass;

   static RenderPassManager* _getRenderPass();

   GFXTexHandle mShadowTexture;
   GFXTextureTargetRef mRenderTarget;

   GFXTextureObject* _getDepthTarget( U32 width, U32 height );
   void _renderToTexture( F32 camDist, const TSRenderState &rdata );

   bool _updateDecal( const SceneState *sceneState );

   void _calcScore( const SceneState *state );
 
public:

   ProjectedShadow( SceneObject *object );
   virtual ~ProjectedShadow();

   bool shouldRender( const SceneState *state );

   void update( const SceneState *state );
   void render( F32 camDist, const TSRenderState &rdata );
   U32 getLastRenderTime() const { return mLastRenderTime; }
   const F32 getScore() const { return mScore; }

};

#endif // _PROJECTEDSHADOW_H_