#include "gfx/D3D9/pc/gfxPCD3D9Device.h"
#include "gfx/D3D9/gfxD3D9CardProfiler.h"
#include "gfx/D3D9/gfxD3D9Shader.h"
#include "gfx/D3D9/gfxD3D9VertexBuffer.h"
#include "gfx/D3D9/gfxD3D9EnumTranslate.h"
#include "core/strings/unicode.h"
#include "console/console.h"

//------------------------------------------------------------------------------
// D3DX Function binding
//------------------------------------------------------------------------------

bool d3dxBindFunction( DLibrary *dll, void *&fnAddress, const char *name )
{
   fnAddress = dll->bind( name );

   if (!fnAddress)
      Con::warnf( "D3DX Loader: DLL bind failed for %s", name );

   return fnAddress != 0;
}

void GFXD3D9Device::initD3DXFnTable()
{
   if ( smD3DX.isLoaded )
      return;

   // We only load the d3dx version that we compiled 
   // and linked against which should keep unexpected
   // problems from newer or older SDKs to a minimum.
   String d3dxVersion = String::ToString( "d3dx9_%d.dll", (S32)D3DX_SDK_VERSION );
   smD3DX.dllRef = OsLoadLibrary( d3dxVersion );

   // If the d3dx version we requested didn't load then we have
   // a corrupt or old install of DirectX.... prompt them to update.
   if ( !smD3DX.dllRef )
   {
      Con::errorf( "Unsupported DirectX version!" );
      Platform::messageBox(   Con::getVariable( "$appName" ),
                              "DirectX could not be started!\r\n"
                              "Please be sure you have the latest version of DirectX installed.",
                              MBOk, MIStop );
      Platform::forceShutdown( -1 );
   }

   smD3DX.isLoaded = true;

   #define D3DX_FUNCTION(fn_name, fn_return, fn_args) \
   smD3DX.isLoaded &= d3dxBindFunction(smD3DX.dllRef, *(void**)&smD3DX.fn_name, #fn_name);
   #     include "gfx/D3D9/d3dx9Functions.h"
   #undef D3DX_FUNCTION

   AssertISV( smD3DX.isLoaded, "D3DX Failed to load all functions." );
}
