#include "gfx/gfxTarget.h"
#include "console/console.h"

GFXTextureObject *GFXTextureTarget::sDefaultDepthStencil = reinterpret_cast<GFXTextureObject *>( 0x1 );

const String GFXTarget::describeSelf() const
{
   return String();
}