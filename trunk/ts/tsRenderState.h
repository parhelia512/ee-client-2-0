//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TSRENDERDATA_H_
#define _TSRENDERDATA_H_

#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif

class SceneState;
class GFXCubemap;
class Frustum;


/// A simple class for passing render state through the pre-render pipeline.
///
/// @section TSRenderState_intro Introduction
///
/// TSRenderState holds on to certain pieces of data that may be
/// set at the preparation stage of rendering (prepRengerImage etc.)
/// which are needed further along in the process of submitting
/// a render instance for later rendering by the RenderManager.
///
/// It was created to clean up and refactor the DTS rendering
/// from having a large number of static data that would be used
/// in varying places.  These statics were confusing and would often
/// cause problems when not properly cleaned up by various objects after
/// submitting their RenderInstances.
///
/// @section TSRenderState_functionality What Does TSRenderState Do?
///
/// TSRenderState is a simple class that performs the function of passing along
/// (from the prep function(s) to the actual submission) the data 
/// needed for the desired state of rendering.
///
/// @section TSRenderState_example Usage Example
///
/// TSRenderState is very easy to use.  Merely create a TSRenderState object (in prepRenderImage usually)
/// and set any of the desired data members (SceneState, camera transform etc.), and pass the address of
/// your TSRenderState to your render function.
///

class TSRenderState
{
protected:
   
   SceneState *mState;

   GFXCubemap *mCubemap;

   /// Used to override the normal
   /// fade value of an object.
   /// This is multiplied by the current
   /// fade value of the instance
   /// to gain the resulting visibility fade (see TSMesh::render()).
   F32 mFadeOverride;

   /// These are used in some places
   /// TSShapeInstance::render, however,
   /// it appears they are never set to anything
   /// other than false.  We provide methods
   /// for setting them regardless.
   bool mNoRenderTranslucent;
   bool mNoRenderNonTranslucent;

   /// A generic hint value passed from the game
   /// code down to the material for use by shader 
   /// features.
   void *mMaterialHint;

   /// An optional object space frustum used to cull
   /// subobjects within the shape.
   Frustum *mCuller;

public:

   TSRenderState();
   TSRenderState( const TSRenderState &state );

   /// @name Get/Set methods.
   /// @{
   SceneState* getSceneState() const { return mState; }
   void setSceneState( SceneState *state ) { mState = state; }

   GFXCubemap* getCubemap() const { return mCubemap; }
   void setCubemap( GFXCubemap *cubemap ) { mCubemap = cubemap; }

   F32 getFadeOverride() const { return mFadeOverride; }
   void setFadeOverride( F32 fade ) { mFadeOverride = fade; }

   bool isNoRenderTranslucent() const { return mNoRenderTranslucent; }
   void setNoRenderTranslucent( bool noRenderTrans ) { mNoRenderTranslucent = noRenderTrans; }

   bool isNoRenderNonTranslucent() const { return mNoRenderNonTranslucent; }
   void setNoRenderNonTranslucent( bool noRenderNonTrans ) { mNoRenderNonTranslucent = noRenderNonTrans; }

   void* getMaterialHint() const { return mMaterialHint; }
   void setMaterialHint( void *materialHint ) { mMaterialHint = materialHint; }

   Frustum* getCuller() const { return mCuller; }
   void setCuller( Frustum *culler ) { mCuller = culler; }

   /// @}
};

#endif // _TSRENDERDATA_H_
