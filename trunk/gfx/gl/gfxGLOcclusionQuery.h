//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFX_GL_OCCLUSIONQUERY_H_
#define _GFX_GL_OCCLUSIONQUERY_H_

#ifndef _GFXOCCLUSIONQUERY_H_
#include "gfx/gfxOcclusionQuery.h"
#endif

class GFXGLOcclusionQuery : public GFXOcclusionQuery
{
public:
   GFXGLOcclusionQuery( GFXDevice *device );
   virtual ~GFXGLOcclusionQuery();

   virtual bool begin();
   virtual void end();
   virtual OcclusionQueryStatus getStatus( bool block, U32 *data = NULL );

   // GFXResource
   virtual void zombify(); 
   virtual void resurrect();
   virtual const String describeSelf() const;
   
private:
   U32 mQuery;
};

#endif // _GFX_GL_OCCLUSIONQUERY_H_
