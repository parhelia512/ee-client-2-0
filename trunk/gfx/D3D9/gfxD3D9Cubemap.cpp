//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/D3D9/gfxD3D9Cubemap.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/D3D9/gfxD3D9EnumTranslate.h"

_D3DCUBEMAP_FACES GFXD3D9Cubemap::faceList[6] = 
{ 
   D3DCUBEMAP_FACE_POSITIVE_X, D3DCUBEMAP_FACE_NEGATIVE_X,
   D3DCUBEMAP_FACE_POSITIVE_Y, D3DCUBEMAP_FACE_NEGATIVE_Y,
   D3DCUBEMAP_FACE_POSITIVE_Z, D3DCUBEMAP_FACE_NEGATIVE_Z
};

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
GFXD3D9Cubemap::GFXD3D9Cubemap()
{
   mCubeTex = NULL;
   mDynamic = false;
   mFaceFormat = GFXFormatR8G8B8A8;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
GFXD3D9Cubemap::~GFXD3D9Cubemap()
{
   releaseSurfaces();

   if ( mDynamic )
      GFXTextureManager::removeEventDelegate( this, &GFXD3D9Cubemap::_onTextureEvent );
}

//-----------------------------------------------------------------------------
// Release D3D surfaces
//-----------------------------------------------------------------------------
void GFXD3D9Cubemap::releaseSurfaces()
{
   if ( !mCubeTex )
      return;

   mCubeTex->Release();
   mCubeTex = NULL;
}

void GFXD3D9Cubemap::_onTextureEvent( GFXTexCallbackCode code )
{
   // Can this happen?
   if ( !mDynamic ) 
      return;
   
   if ( code == GFXZombify )
      releaseSurfaces();
   else if ( code == GFXResurrect )
      initDynamic( mTexSize );
}

//-----------------------------------------------------------------------------
// Init Static
//-----------------------------------------------------------------------------
void GFXD3D9Cubemap::initStatic( GFXTexHandle *faces )
{
   //if( mCubeTex )
   //   return;

   if( faces )
   {
      AssertFatal( faces[0], "empty texture passed to CubeMap::create" );


      LPDIRECT3DDEVICE9 D3D9Device = static_cast<GFXD3D9Device *>(GFX)->getDevice();     
      
      // NOTE - check tex sizes on all faces - they MUST be all same size
      mTexSize = faces[0].getWidth();
      mFaceFormat = faces[0].getFormat();

      D3D9Assert( D3D9Device->CreateCubeTexture( mTexSize, 1, 0, GFXD3D9TextureFormat[mFaceFormat],
                 D3DPOOL_MANAGED, &mCubeTex, NULL ), NULL );

      fillCubeTextures( faces, D3D9Device );
//      mCubeTex->GenerateMipSubLevels();
   }
}


//-----------------------------------------------------------------------------
// Init Dynamic
//-----------------------------------------------------------------------------
void GFXD3D9Cubemap::initDynamic( U32 texSize, GFXFormat faceFormat )
{
   if ( mCubeTex )
      return;

   if ( !mDynamic )
      GFXTextureManager::addEventDelegate( this, &GFXD3D9Cubemap::_onTextureEvent );

   mDynamic = true;
   mTexSize = texSize;
   mFaceFormat = faceFormat;
   
   LPDIRECT3DDEVICE9 D3D9Device = reinterpret_cast<GFXD3D9Device *>(GFX)->getDevice();

   // might want to try this as a 16 bit texture...
   D3D9Assert( D3D9Device->CreateCubeTexture( texSize,
                                            1, 
#ifdef TORQUE_OS_XENON
                                            0,
#else
                                            D3DUSAGE_RENDERTARGET, 
#endif
                                            GFXD3D9TextureFormat[faceFormat],
                                            D3DPOOL_DEFAULT, 
                                            &mCubeTex, 
                                            NULL ), NULL );
}

//-----------------------------------------------------------------------------
// Fills in face textures of cube map from existing textures
//-----------------------------------------------------------------------------
void GFXD3D9Cubemap::fillCubeTextures( GFXTexHandle *faces, LPDIRECT3DDEVICE9 D3DDevice )
{
   for( U32 i=0; i<6; i++ )
   {
      // get cube face surface
      IDirect3DSurface9 *cubeSurf = NULL;
      D3D9Assert( mCubeTex->GetCubeMapSurface( faceList[i], 0, &cubeSurf ), NULL );

      // get incoming texture surface
      GFXD3D9TextureObject *texObj = dynamic_cast<GFXD3D9TextureObject*>( (GFXTextureObject*)faces[i] );
      IDirect3DSurface9 *inSurf;
      D3D9Assert( texObj->get2DTex()->GetSurfaceLevel( 0, &inSurf ), NULL );
      
      // copy incoming texture into cube face
      D3D9Assert( GFXD3DX.D3DXLoadSurfaceFromSurface( cubeSurf, NULL, NULL, inSurf, NULL, 
                                  NULL, D3DX_FILTER_NONE, 0 ), NULL );
      cubeSurf->Release();
      inSurf->Release();
   }
}

//-----------------------------------------------------------------------------
// Set the cubemap to the specified texture unit num
//-----------------------------------------------------------------------------
void GFXD3D9Cubemap::setToTexUnit( U32 tuNum )
{
   static_cast<GFXD3D9Device *>(GFX)->getDevice()->SetTexture( tuNum, mCubeTex );
}

void GFXD3D9Cubemap::zombify()
{
   // Static cubemaps are handled by D3D
   if( mDynamic )
      releaseSurfaces();
}

void GFXD3D9Cubemap::resurrect()
{
   // Static cubemaps are handled by D3D
   if( mDynamic )
      initDynamic( mTexSize, mFaceFormat );
}
