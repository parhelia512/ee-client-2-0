//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFX_D3D9_OCCLUSIONQUERY_H_
#define _GFX_D3D9_OCCLUSIONQUERY_H_

#ifndef _GFXOCCLUSIONQUERY_H_
#include "gfx/gfxOcclusionQuery.h"
#endif

struct IDirect3DQuery9;


class GFXD3D9OcclusionQuery : public GFXOcclusionQuery
{
private:
   mutable IDirect3DQuery9 *mQuery;

public:
   GFXD3D9OcclusionQuery( GFXDevice *device );
   virtual ~GFXD3D9OcclusionQuery();

   virtual bool begin();
   virtual void end();   
   virtual OcclusionQueryStatus getStatus( bool block, U32 *data = NULL );

   // GFXResource
   virtual void zombify();   
   virtual void resurrect();
   virtual const String GFXD3D9OcclusionQuery::describeSelf() const;
};

#endif // _GFX_D3D9_OCCLUSIONQUERY_H_