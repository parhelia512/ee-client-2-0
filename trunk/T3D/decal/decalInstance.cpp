
#include "platform/platform.h"
#include "T3D/decal/decalInstance.h"
#include "sceneGraph/sceneState.h"

void DecalInstance::getWorldMatrix( MatrixF *outMat, bool flip )
{
   outMat->setPosition( mPosition );

   Point3F fvec;
   mCross( mNormal, mTangent, &fvec );

   outMat->setColumn( 0, mTangent );
   outMat->setColumn( 1, fvec );
   outMat->setColumn( 2, mNormal );
}

F32 DecalInstance::calcPixelRadius( const SceneState *state ) const
{   
   const Point2I &viewportExtent = state->getViewportExtent();
   const F32 pixelScale = viewportExtent.y / 300.0f;
   F32 decalSize = getMax( mSize, 0.001f );

   Point3F cameraOffset = state->getCameraPosition() - mPosition; 

   F32 dist = getMax( cameraOffset.len(), 0.01f );

   F32 invScale = 1.0f / decalSize;
   F32 scaledDistance = dist * invScale;

   return state->projectRadius( scaledDistance, decalSize ) * pixelScale;
}

F32 DecalInstance::calcEndPixRadius( const Point2I &viewportExtent ) const
{
   const F32 pixelScale = viewportExtent.y / 300.0f;
   F32 decalSize = getMax( mSize, 0.001f );

   return mDataBlock->endPixRadius * decalSize * pixelScale;
}

void DecalInstance::setPosition( const Point3F & pos )
{
	mPosition = pos;
}

void DecalInstance::setTangent( const Point3F & tangent )
{
	mTangent = tangent;
}

const Point3F & DecalInstance::getTangent()
{
	return mTangent;
}

const Point3F & DecalInstance::getPosition()
{
	return mPosition;
}