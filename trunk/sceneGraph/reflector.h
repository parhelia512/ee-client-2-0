#ifndef _REFLECTOR_H_
#define _REFLECTOR_H_

#ifndef _GFXCUBEMAP_H_
#include "gfx/gfxCubemap.h"
#endif
#ifndef _GFXTARGET_H_
#include "gfx/gfxTarget.h"
#endif
#ifndef _SIMDATABLOCK_H_
#include "console/simDatablock.h"
#endif
#ifndef _MMATH_H_
#include "math/mMath.h"
#endif

struct CameraQuery;
class Point2I;
class Frustum;
class SceneGraph;
class SceneObject;
class GFXOcclusionQuery;


struct ReflectParams
{
   const CameraQuery *query;
   Point2I viewportExtent;
   Frustum culler;
   U32 startOfUpdateMs;
};


class ReflectorDesc : public SimDataBlock
{
   typedef SimDataBlock Parent;

public:

   ReflectorDesc();
   virtual ~ReflectorDesc();

   DECLARE_CONOBJECT( ReflectorDesc );

   static void initPersistFields();

   virtual void packData( BitStream *stream );
   virtual void unpackData( BitStream* stream );   
   virtual bool preload( bool server, String &errorStr );

   U32 texSize;   
   F32 nearDist;
   F32 farDist;
   U32 objectTypeMask;
   F32 detailAdjust;
   F32 priority;
   U32 maxRateMs;
   bool useOcclusionQuery;
   //U32 lastLodSize;
};


class ReflectorBase
{
public:

   ReflectorBase();
   virtual ~ReflectorBase();

   bool isEnabled() const { return mEnabled; }

   virtual void unregisterReflector();
   virtual F32 calcScore( const ReflectParams &params );
   virtual void updateReflection( const ReflectParams &params ) {}

   GFXOcclusionQuery* getOcclusionQuery() const { return mOcclusionQuery; }

   bool isOccluded() const { return mOccluded; }

   /// Returns true if this reflector is in the process of rendering.
   bool isRendering() const { return mIsRendering; }

protected:

   bool mEnabled;

   bool mIsRendering;

   GFXOcclusionQuery *mOcclusionQuery;

   bool mOccluded;

   SceneObject *mObject;

   ReflectorDesc *mDesc;

public:

   // These are public because some of them
   // are exposed as fields.

   F32 score;
   U32 lastUpdateMs;
   

};

typedef Vector<ReflectorBase*> ReflectorList;


class CubeReflector : public ReflectorBase
{
   typedef ReflectorBase Parent;

public:

   CubeReflector();
   virtual ~CubeReflector() {}

   void registerReflector( SceneObject *inObject,
                           ReflectorDesc *inDesc );

   virtual void unregisterReflector();
   virtual void updateReflection( const ReflectParams &params );   

   GFXCubemap* getCubemap() const { return cubemap; }

   void updateFace( const ReflectParams &params, U32 faceidx );
   F32 calcFaceScore( const ReflectParams &params, U32 faceidx );

protected:

   GFXTexHandle depthBuff;
   GFXTextureTargetRef renderTarget;   
   GFXCubemapHandle  cubemap;
   U32 mLastTexSize;

   class CubeFaceReflector : public ReflectorBase
   {
      typedef ReflectorBase Parent;
      friend class CubeReflector;

   public:
      U32 faceIdx;
      CubeReflector *cube;

      virtual void updateReflection( const ReflectParams &params ) { cube->updateFace( params, faceIdx ); } 
      virtual F32 calcScore( const ReflectParams &params );
   };

   CubeFaceReflector mFaces[6];
};


class PlaneReflector : public ReflectorBase
{
   typedef ReflectorBase Parent;

public:

   PlaneReflector() 
   {
      refplane.set( Point3F(0,0,0), Point3F(0,0,1) );
      objectSpace = false;
      mLastTexSize = 0;
   }

   virtual ~PlaneReflector() {}

   void registerReflector( SceneObject *inObject,
                           ReflectorDesc *inDesc );

   virtual F32 calcScore( const ReflectParams &params );
   virtual void updateReflection( const ReflectParams &params ); 

   /// Set up camera matrix for a reflection on the plane
   MatrixF getCameraReflection( MatrixF &camTrans );

   /// Oblique frustum clipping - use near plane of zbuffer as a clip plane
   MatrixF getFrustumClipProj( MatrixF &modelview );

protected:

   U32 mLastTexSize;

   // The camera position at the last update.
   Point3F mLastPos;

   // The camera direction at the last update.
   VectorF mLastDir;

public:

   GFXTextureTargetRef reflectTarget;
   GFXTexHandle reflectTex;   
   PlaneF refplane;
   bool objectSpace;
};

#endif // _REFLECTOR_H_