//-----------------------------------------------------------------------------
// Torque Shader Engine
// 
// Copyright (c) 2003 GarageGames.Com
//-----------------------------------------------------------------------------

#ifndef _GFXADAPTER_H_
#define _GFXADAPTER_H_

#ifndef _GFXSTRUCTS_H_
#include "gfx/gfxStructs.h"
#endif

#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif

#ifndef _UTIL_DELEGATE_H_
#include "core/util/delegate.h"
#endif

struct GFXAdapter 
{
public:
   typedef Delegate<GFXDevice* (U32 adapterIndex)> CreateDeviceInstanceDelegate;     

   enum
   {
      MaxAdapterNameLen = 512,
   };

   char mName[MaxAdapterNameLen];

   /// List of available full-screen modes. Windows can be any size,
   /// so we do not enumerate them here.
   Vector<GFXVideoMode> mAvailableModes;

   /// Supported shader model. 0.f means none supported.
   F32 mShaderModel;

   const char * getName() const { return mName; }
   GFXAdapterType mType;
   U32            mIndex;
   CreateDeviceInstanceDelegate mCreateDeviceInstanceDelegate;

   GFXAdapter()
   {
      VECTOR_SET_ASSOCIATION( mAvailableModes );

      mName[0] = 0;
      mShaderModel = 0.f;
      mIndex = 0;
   }

   ~GFXAdapter()
   {
      mAvailableModes.clear();
   }
private:
   // Disallow copying to prevent mucking with our data above.
   GFXAdapter(const GFXAdapter&);
};

#endif // _GFXADAPTER_H_
