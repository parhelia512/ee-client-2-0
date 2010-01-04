//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/matInstanceHook.h"


MatInstanceHookType::MatInstanceHookType( const char *type )
{
   TypeMap::Iterator iter = getTypeMap().find( type );
   if ( iter == getTypeMap().end() )
      iter = getTypeMap().insertUnique( type, getTypeMap().size() );

   mTypeIndex = iter->value;
}

