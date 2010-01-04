
#ifndef _LIGHTFLAREDATA_H_
#define _LIGHTFLAREDATA_H_

#ifndef _SIMDATABLOCK_H_
#include "console/simDatablock.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _GFXPRIMITIVEBUFFER_H_
#include "gfx/gfxPrimitiveBuffer.h"
#endif
#ifndef _CONSOLETYPES_H_
#include "console/consoleTypes.h"
#endif
#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif
#ifndef _GFXSTATEBLOCK_H_
#include "gfx/gfxStateBlock.h"
#endif

class LightInfo;
struct ObjectRenderInst;
class SceneState;
class BaseMatInstance;
class GFXOcclusionQuery;

struct LightFlareState
{  
   ~LightFlareState();
   void clear();

   /// Object calling LightFlareData::prepRender fills these in!
   F32 scale;              
   F32 fullBrightness;
   MatrixF lightMat;
   LightInfo *lightInfo;
   F32 worldRadius;

   /// Used internally by LightFlareData!
   U32 visChangedTime;
   bool visible;   
   F32 occlusion;
   GFXVertexBufferHandle<GFXVertexPCT> vertBuffer;   
   GFXOcclusionQuery *occlusionQuery;
   GFXOcclusionQuery *fullPixelQuery;   
};

class LightFlareData : public SimDataBlock
{
   typedef SimDataBlock Parent;
   
   #define MAX_ELEMENTS 10

public:

   LightFlareData();
   virtual ~LightFlareData();

   DECLARE_CONOBJECT( LightFlareData );

   static void initPersistFields();
   virtual void inspectPostApply();

   // SimDataBlock
   virtual bool preload( bool server, String &errorStr );
   virtual void packData( BitStream *stream );
   virtual void unpackData( BitStream *stream );

   /// Submits render instances for corona and flare effects.
   void prepRender( SceneState *state, LightFlareState *flareState );

protected:

   bool _preload( bool server, String &errorStr );
   void _makePrimBuffer( GFXPrimitiveBufferHandle *pb, U32 count );
   void _renderCorona( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat );

protected:

   // Fields...

   F32 mScale;
   bool mFlareEnabled;
   String mFlareTextureName;
   GFXTexHandle mFlareTexture;
   F32 mOcclusionRadius;

   RectF mElementRect[MAX_ELEMENTS];
   F32 mElementDist[MAX_ELEMENTS];
   F32 mElementScale[MAX_ELEMENTS];
   ColorF mElementTint[MAX_ELEMENTS];
   bool mElementRotate[MAX_ELEMENTS];
   bool mElementUseLightColor[MAX_ELEMENTS];   

protected:

   U32 mElementCount;   
   GFXPrimitiveBufferHandle mFlarePrimBuffer;   
};

DECLARE_CONSOLETYPE(LightFlareData)

#endif // _LIGHTFLAREDATA_H_