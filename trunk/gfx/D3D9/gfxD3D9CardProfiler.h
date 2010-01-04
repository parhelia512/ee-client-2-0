//-----------------------------------------------------------------------------
// Torque Shader Engine 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXD3D9CARDPROFILER_H_
#define _GFXD3D9CARDPROFILER_H_

#ifndef _D3D9_H_
#include <d3d9.h>
#endif

#include "gfx/gfxCardProfile.h"


class GFXD3D9CardProfiler : public GFXCardProfiler
{
private:
   typedef GFXCardProfiler Parent;

   LPDIRECT3DDEVICE9 mD3DDevice;
   UINT mAdapterOrdinal;

public:
   GFXD3D9CardProfiler();
   ~GFXD3D9CardProfiler();
   void init();

protected:
   const String &getRendererString() const { static String sRS("D3D9"); return sRS; }

   void setupCardCapabilities();
   bool _queryCardCap(const String &query, U32 &foundResult);
   bool _queryFormat(const GFXFormat fmt, const GFXTextureProfile *profile, bool &inOutAutogenMips);
};

#endif
