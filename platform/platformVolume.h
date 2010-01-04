//-----------------------------------------------------------------------------
// GarageGames Library
// Copyright (c) GarageGames, All Rights Reserved
//-----------------------------------------------------------------------------

#ifndef _PLATFORMVOLUME_H_
#define _PLATFORMVOLUME_H_

#include "core/volume.h"


namespace Platform
{
namespace FS
{
   using namespace Torque;
   using namespace Torque::FS;

   FileSystemRef  createNativeFS( const String &volume );

   String   getAssetDir();

   /// Mount default OS file systems.
   /// On POSIX environment this means mounting a root FileSystem "/", mounting
   /// the $HOME environment variable as the "home:/" file system and setting the
   /// current working directory to the current OS working directory.
   bool InstallFileSystems();

   bool MountDefaults();
   bool MountZips(const String &root);
   
   bool Touch( const Path &path );

} // Namespace FS
} // Namespace Platform

#endif

