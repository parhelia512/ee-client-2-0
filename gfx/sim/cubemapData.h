#ifndef _CUBEMAPDATA_H_
#define _CUBEMAPDATA_H_

#ifndef _SIMOBJECT_H_
#include "console/simObject.h"
#endif

#ifndef _GFXCUBEMAP_H_
#include "gfx/gfxCubemap.h"
#endif

#ifndef _GFXTARGET_H_
#include "gfx/gfxTarget.h"
#endif

#ifndef _SCENEGRAPH_H_
#include "sceneGraph/sceneGraph.h"
#endif

class SceneGraph;

//**************************************************************************
// Cubemap data
//**************************************************************************
class CubemapData : public SimObject
{
   typedef SimObject Parent;   

public:   

   GFXCubemapHandle  mCubemap;

   CubemapData();
   ~CubemapData();

   bool onAdd();
   static void initPersistFields();

   DECLARE_CONOBJECT(CubemapData);

   // Force creation of cubemap
   void createMap();   

   // Update a dynamic cubemap @ pos
   void updateDynamic(SceneGraph* sm, const Point3F& pos);
	void updateFaces();
   
   // Dynamic cube map support
   bool mDynamic;
   U32 mDynamicSize;   
   F32 mDynamicNearDist;
   F32 mDynamicFarDist;
   U32 mDynamicObjectTypeMask;

protected:

   FileName mCubeFaceFile[6];
   GFXTexHandle mCubeFace[6];

   GFXTexHandle mDepthBuff;
   GFXTextureTargetRef mRenderTarget;
#ifdef INIT_HACK
   bool mInit;
#endif
};

#endif // CUBEMAPDATA

