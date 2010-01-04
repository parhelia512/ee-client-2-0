//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TSLASTDETAIL_H_
#define _TSLASTDETAIL_H_

#ifndef _MATHTYPES_H_
#include "math/mathTypes.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef __RESOURCE_H__
#include "core/resource.h"
#endif
#ifndef _TSRENDERDATA_H_
#include "ts/tsRenderState.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif


class TSShape;
class TSShapeInstance;
class TSRenderState;
class GBitmap;
class TextureHandle;
class SceneState;



/// The texture profile used for TSLastDetail imposter 
/// diffuse textures.
///
/// Use the 'metrics(texture)' console command to track
/// the memory usage of this profile.
///
GFX_DeclareTextureProfile(TSImposterDiffuseTexProfile);

/// The texture profile used for TSLastDetail imposter 
/// normal map textures.
///
/// Use the 'metrics(texture)' console command to track
/// the memory usage of this profile.
///
GFX_DeclareTextureProfile(TSImposterNormalMapTexProfile);


/// This neat little class renders the object to a texture so that when the object
/// is far away, it can be drawn as a billboard instead of a mesh.  This happens
/// when the model is first loaded as to keep the realtime render as fast as possible.
/// It also renders the model from a few different perspectives so that it would actually
/// pass as a model instead of a silly old billboard.  In other words, this is an imposter.
class TSLastDetail
{
protected:

   /// The shape which we're impostering.
   TSShape *mShape;

   /// This is the path of the object, which is
   /// where we'll be storing our cache for rendered imposters.
   String mCachePath;

   /// The shape detail level to capture into
   /// the imposters.
   S32 mDl;

   /// The bounding radius of the shape 
   /// used to size the billboard.
   F32 mRadius;

   /// The square dimensions of each 
   /// captured imposter image.
   S32 mDim;

   /// The number steps around the equator of
   /// the globe at which we capture an imposter.
   U32 mNumEquatorSteps;

   /// The number of steps to go from equator to
   /// each polar region (0 means equator only) at
   /// which we capture an imposter.
   U32 mNumPolarSteps;   

   /// The angle in radians of sub-polar regions.
   F32 mPolarAngle;

   /// If true we captures polar images in the 
   /// imposter texture.
   bool mIncludePoles;

   /// The texture handle for the imposters. All the images
   /// are stored in one texture, if you have specified a 
   /// resolution or quantity that won't fit in a single 
   /// texture the imposter won't render.
   GFXTexHandle mTexture;

   /// The normal map generated for the imposter images
   /// which has the same uv layout as the texture.
   GFXTexHandle mNormalMap;

   /// The vector of texture UVs used for rendering
   /// imposters from the composite textures.
   Vector<RectF> mTextureUVs;

   /// This is a global list of all the TSLastDetail
   /// objects in the system.
   static Vector<TSLastDetail*> smLastDetails;

public:

   TSLastDetail(  TSShape *shape, 
                  const String &cachePath,
                  U32 numEquatorSteps, 
                  U32 numPolarSteps, 
                  F32 polarAngle, 
                  bool includePoles, 
                  S32 dl, 
                  S32 dim );

   ~TSLastDetail();

   /// Calls update on all TSLastDetail objects in the system.
   /// @see update()
   static void updateImposterImages( bool forceUpdate = false );

   /// Updates the imposter images by reading them from the disk
   /// or generating them if the TSShape is more recient than the
   /// cached imposter textures.
   ///
   /// This should not be called from within any rendering code.
   ///
   /// @param forceUpdate  If true the disk cache is invalidated and
   ///                     new imposter images are rendered.
   ///
   void update( bool forceUpdate = false );

   /// Internal function called from TSShapeInstance to 
   /// submit an imposter render instance.
   void render( const TSRenderState &rdata, F32 alpha );

   /// Returns the composite imposter diffuse texture.
   /// @see mTexture
   GFXTextureObject* getTextureMap() { return mTexture; }

   /// Returns the composite imposter normal map texture.
   /// @see mNormalMap
   GFXTextureObject* getNormalMap() { return mNormalMap; }

   /// Returns the vector of texture UVs.
   /// @see mTextureUVs
   const Vector<RectF>& getTextureUVs() const { return mTextureUVs; }

   /// Returns the polar step count.
   /// @see mNumPolarSteps
   U32 getNumPolarSteps() const { return mNumPolarSteps; }

   /// Returns the equator step count.
   /// @see mNumEquatorSteps
   U32 getNumEquatorSteps() const { return mNumEquatorSteps; }

   /// Returns the polar angle in degrees.
   /// @see mPolarAngle
   F32 getPolarAngle() const { return mPolarAngle; }

   /// Is true if the imposter texture includes images at the poles.
   bool getIncludePoles() const { return mIncludePoles; }

   /// Returns the radius.
   /// @see mRadius
   F32 getRadius() const { return mRadius; }
};


#endif // _TSLASTDETAIL_H_


