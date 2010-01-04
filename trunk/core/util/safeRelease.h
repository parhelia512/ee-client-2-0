//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef SAFE_RELEASE
#  define SAFE_RELEASE(x) if( x != NULL ) { x->Release(); x = NULL; }
#endif