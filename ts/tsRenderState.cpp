//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "ts/tsRenderState.h"


TSRenderState::TSRenderState()
   :  mState( NULL ),
      mCubemap( NULL ),
      mFadeOverride( 1.0f ),
      mNoRenderTranslucent( false ),
      mNoRenderNonTranslucent( false ),
      mMaterialHint( NULL ),
      mCuller( NULL )
{
}

TSRenderState::TSRenderState( const TSRenderState &state )
   :  mState( state.mState ),
      mCubemap( state.mCubemap ),
      mFadeOverride( state.mFadeOverride ),
      mNoRenderTranslucent( state.mNoRenderTranslucent ),
      mNoRenderNonTranslucent( state.mNoRenderNonTranslucent ),
      mMaterialHint( state.mMaterialHint ),
      mCuller( state.mCuller )
{
}
