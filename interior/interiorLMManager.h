//-----------------------------------------------------------------------------
// Torque 3D Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _INTERIORLMMANAGER_H_
#define _INTERIORLMMANAGER_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif

#include "gfx/gfxTextureHandle.h"

class GBitmap;
class Interior;
class InteriorInstance;

typedef U32 LM_HANDLE;

class InteriorLMManager
{
   private:

      struct InstanceLMInfo {
         InteriorInstance *         mInstance;
         LM_HANDLE *                mHandlePtr;
         Vector<GFXTexHandle>       mLightmapHandles;
      };

      struct InteriorLMInfo {
         Interior *                 mInterior;
         LM_HANDLE *                mHandlePtr;
         U32                        mNumLightmaps;
         LM_HANDLE                  mBaseInstanceHandle;
         Vector<InstanceLMInfo*>    mInstances;
      };

      Vector<InteriorLMInfo*>       mInteriors;

   public:

      InteriorLMManager();
      ~InteriorLMManager();

      void destroyBitmaps();
      void destroyTextures();

      void downloadGLTextures();
      void downloadGLTextures(LM_HANDLE interiorHandle);
      bool loadBaseLightmaps(LM_HANDLE interiorHandle, LM_HANDLE instanceHandle);

      void addInterior(LM_HANDLE & interiorHandle, U32 numLightmaps, Interior * interior);
      void removeInterior(LM_HANDLE interiorHandle);

      void addInstance(LM_HANDLE interiorHandle, LM_HANDLE & instanceHandle, InteriorInstance * instance);
      void removeInstance(LM_HANDLE interiorHandle, LM_HANDLE instanceHandle);
      void useBaseTextures(LM_HANDLE interiorHandle, LM_HANDLE instanceHandle);

      void clearLightmaps(LM_HANDLE interiorHandle, LM_HANDLE instanceHandle);

      GFXTexHandle     &getHandle(LM_HANDLE interiorHandle, LM_HANDLE instanceHandle, U32 index);
      Vector<GFXTexHandle> & getHandles(LM_HANDLE interiorHandle, LM_HANDLE instanceHandle);

      // helper's
      GFXTexHandle &duplicateBaseLightmap(LM_HANDLE interiorHandle, LM_HANDLE instanceHandle, U32 index);
      GBitmap * getBitmap(LM_HANDLE interiorHandle, LM_HANDLE instanceHandle, U32 index);
};

extern InteriorLMManager         gInteriorLMManager;

#endif
