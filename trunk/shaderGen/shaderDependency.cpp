//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "shaderGen/shaderDependency.h"

#include "core/stream/fileStream.h"
#include "core/frameAllocator.h"


ShaderIncludeDependency::ShaderIncludeDependency( const Torque::Path &pathToInclude ) 
   : mIncludePath( pathToInclude )
{
}

bool ShaderIncludeDependency::operator==( const ShaderDependency &cmpTo ) const
{
   return   this == &cmpTo ||
            (  dynamic_cast<const ShaderIncludeDependency*>( &cmpTo ) &&
               static_cast<const ShaderIncludeDependency*>( &cmpTo )->mIncludePath == mIncludePath );
}

void ShaderIncludeDependency::print( Stream &s ) const
{
   // Print the include... all shaders support #includes.
   String include = String::ToString( "#include \"%s\"\r\n", mIncludePath.getFullPath().c_str() );
   s.write( include.length(), include.c_str() );
}
