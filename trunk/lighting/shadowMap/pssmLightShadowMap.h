//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _PSSMLIGHTSHADOWMAP_H_
#define _PSSMLIGHTSHADOWMAP_H_

#ifndef _LIGHTSHADOWMAP_H_
#include "lighting/shadowMap/lightShadowMap.h"
#endif
#ifndef _MATHUTIL_FRUSTUM_H_
#include "math/util/frustum.h"
#endif


class PSSMLightShadowMap : public LightShadowMap
{
   typedef LightShadowMap Parent;
public:
   PSSMLightShadowMap( LightInfo *light );

   // LightShadowMap
   virtual ShadowType getShadowType() const { return ShadowType_PSSM; }
   virtual void _render( SceneGraph *sceneManager, const SceneState *diffuseState );
   virtual void setShaderParameters(GFXShaderConstBuffer* params, LightingShaderConstants* lsc);

protected:

   void _setNumSplits( U32 numSplits, U32 texSize );
   void _calcSplitPos(const Frustum& currFrustum);
   Box3F _calcClipSpaceAABB(const Frustum& f, const MatrixF& transform, F32 farDist);
   void _roundProjection(const MatrixF& lightMat, const MatrixF& cropMatrix, Point3F &offset, U32 splitNum);

   static const int MAX_SPLITS = 4;
   U32 mNumSplits;
   F32 mSplitDist[MAX_SPLITS+1];   // +1 because we store a cap
   RectI mViewports[MAX_SPLITS];
   Point3F mScaleProj[MAX_SPLITS];
   Point3F mOffsetProj[MAX_SPLITS];
   Point4F mFarPlaneScalePSSM;
   F32 mLogWeight;
};

#endif
