//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/D3D9/gfxD3D9Device.h"
#include "gfx/D3D9/gfxD3D9OcclusionQuery.h"


GFXD3D9OcclusionQuery::GFXD3D9OcclusionQuery( GFXDevice *device )
 : GFXOcclusionQuery( device ), 
   mQuery( NULL )
{
}

GFXD3D9OcclusionQuery::~GFXD3D9OcclusionQuery()
{
   SAFE_RELEASE( mQuery );
}

bool GFXD3D9OcclusionQuery::begin()
{
   if ( mQuery == NULL )
   {
#ifdef TORQUE_OS_XENON
      HRESULT hRes = static_cast<GFXD3D9Device*>( mDevice )->getDevice()->CreateQueryTiled( D3DQUERYTYPE_OCCLUSION, 2, &mQuery );
#else
      HRESULT hRes = static_cast<GFXD3D9Device*>( mDevice )->getDevice()->CreateQuery( D3DQUERYTYPE_OCCLUSION, &mQuery );
#endif

      AssertFatal( hRes != D3DERR_NOTAVAILABLE, "GFXD3D9OcclusionQuery::resurrect - Hardware does not support D3D9 Occlusion-Queries, this should be caught before this type is created" );
      AssertISV( hRes != E_OUTOFMEMORY, "GFXD3D9OcclusionQuery::resurrect - Out of memory" );
   }

   // Add a begin marker to the command buffer queue.
   mQuery->Issue( D3DISSUE_BEGIN );

   return true;
}

void GFXD3D9OcclusionQuery::end()
{
   // Add an end marker to the command buffer queue.
   mQuery->Issue( D3DISSUE_END );
}

GFXD3D9OcclusionQuery::OcclusionQueryStatus GFXD3D9OcclusionQuery::getStatus( bool block, U32 *data )
{
   // If this ever shows up near the top of a profile 
   // then your system is GPU bound.
   PROFILE_SCOPE(GFXD3D9OcclusionQuery_getStatus);

   if ( mQuery == NULL )
      return Unset;

   HRESULT hRes;
   DWORD dwOccluded = 0;

   if ( block )
   {      
      while( ( hRes = mQuery->GetData( &dwOccluded, sizeof(DWORD), D3DGETDATA_FLUSH ) ) == S_FALSE )
         ;
   }
   else
   {
      hRes = mQuery->GetData( &dwOccluded, sizeof(DWORD), 0 );
   }

   if ( hRes == S_OK )   
   {
      if ( data != NULL )
         *data = dwOccluded;

      return dwOccluded > 0 ? NotOccluded : Occluded;   
   }

   if ( hRes == S_FALSE )
      return Waiting;

   return Error;   
}

void GFXD3D9OcclusionQuery::zombify()
{
   // Release our query
   SAFE_RELEASE( mQuery );
}

void GFXD3D9OcclusionQuery::resurrect()
{
   // Recreate the query
   if ( mQuery == NULL )
   {
      HRESULT hRes = static_cast<GFXD3D9Device*>( mDevice )->getDevice()->CreateQuery( D3DQUERYTYPE_OCCLUSION, &mQuery );

      AssertFatal( hRes != D3DERR_NOTAVAILABLE, "GFXD3D9OcclusionQuery::resurrect - Hardware does not support D3D9 Occlusion-Queries, this should be caught before this type is created" );
      AssertISV( hRes != E_OUTOFMEMORY, "GFXD3D9OcclusionQuery::resurrect - Out of memory" );
   }
}

const String GFXD3D9OcclusionQuery::describeSelf() const
{
   // We've got nothing
   return String();
}