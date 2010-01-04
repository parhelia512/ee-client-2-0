
#ifndef _DECALINSTANCE_H_
#define _DECALINSTANCE_H_

#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif

#ifndef _DECALDATA_H_
#include "T3D/decal/decalData.h"
#endif

struct DecalVertex;
class SceneState;

/// DecalInstance represents a rendering decal in the scene.
/// You should not allocate this yourself, add new decals to the scene
/// via the DecalManager.
/// All data is public, change it if you wish, but be sure to inform
/// the DecalManager.
class DecalInstance
{
public:

   DecalData *mDataBlock;

   Point3F mPosition;
   Point3F mNormal;
   Point3F mTangent;
   F32 mRotAroundNormal;   
   F32 mSize;

   U32 mCreateTime;
   F32 mVisibility;

   U32 mTextureRectIdx;      

   DecalVertex *mVerts;
   U16 *mIndices;

   U32 mVertCount;
   U32 mIndxCount;

   U8 mFlags;

   U8 mRenderPriority;

   S32 mId;

   GFXTexHandle *mCustomTex;

public:   
	///可以在外界控制的一些接口
	void						setPosition(const Point3F & pos);
	const Point3F &	getPosition();
	void						setTangent(const Point3F & tangent);
	const Point3F &	getTangent();
	
   ///
   void getWorldMatrix( MatrixF *outMat, bool flip = false );

   ///
   U8 getRenderPriority() const;

   /// Calculates the screen pixel radius of the decal, used for LOD.
   F32 calcPixelRadius( const SceneState *state ) const;

   /// Calculates the "real" end pixel radius of the decal based on
   /// its size and the setting for endPixRadius in the DecalData.
   F32 calcEndPixRadius( const Point2I &viewportExtent ) const;
	
	/// The constructor ss
	DecalInstance() : mId(-1) {}   
};

inline U8 DecalInstance::getRenderPriority() const
{
   return mRenderPriority == 0 ? mDataBlock->renderPriority : mRenderPriority;
}

#endif // _DECALINSTANCE_H_
