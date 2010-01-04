//-----------------------------------------------------------------------------
// Torque Shader Engine
// 
// Copyright (c) 2003 GarageGames.Com
//-----------------------------------------------------------------------------
#include "gfx/D3D9/gfxD3D9Device.h"
#include "gfx/D3D9/gfxD3D9EnumTranslate.h"
#include "gfx/D3D9/gfxD3D9PrimitiveBuffer.h"
#include "core/util/safeRelease.h"

void GFXD3D9PrimitiveBuffer::prepare()
{
	static_cast<GFXD3D9Device *>( mDevice )->_setPrimitiveBuffer(this);
}

void GFXD3D9PrimitiveBuffer::unlock()
{
   ib->Unlock();
   mLocked = false;
   mIsFirstLock = false;
}

GFXD3D9PrimitiveBuffer::~GFXD3D9PrimitiveBuffer() 
{
   if( mBufferType != GFXBufferTypeVolatile )
         SAFE_RELEASE( ib );
}

void GFXD3D9PrimitiveBuffer::zombify()
{
   if(mBufferType != GFXBufferTypeDynamic)
      return;
   AssertFatal(!mLocked, "GFXD3D9PrimitiveBuffer::zombify - Cannot zombify a locked buffer!");
   SAFE_RELEASE(ib);
}

void GFXD3D9PrimitiveBuffer::resurrect()
{
   if ( mBufferType != GFXBufferTypeDynamic )
      return;
      
   U32 usage = D3DUSAGE_WRITEONLY;

#ifndef TORQUE_OS_XENON
   usage |= D3DUSAGE_DYNAMIC;
#endif

   D3DPOOL pool = D3DPOOL_DEFAULT;

   D3D9Assert(static_cast<GFXD3D9Device*>(mDevice)->mD3DDevice->CreateIndexBuffer( sizeof(U16) * mIndexCount , 
        usage , GFXD3D9IndexFormat[GFXIndexFormat16], pool, &ib, 0),
        "GFXD3D9PrimitiveBuffer::resurrect - Failed to allocate an index buffer.");
}
