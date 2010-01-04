//-----------------------------------------------------------------------------
// GarageGames Library
// Copyright (c) GarageGames, All Rights Reserved
//-----------------------------------------------------------------------------

#include "platform/types.h"

#if defined(TORQUE_OS_WIN32) || defined(TORQUE_OS_XBOX) || defined(TORQUE_OS_XENON)
#include <sys/utime.h>
#else
#include <sys/time.h>
#endif

#include "platform/platformVolume.h"
#include "core/util/zip/zipVolume.h"

using namespace Torque;
using namespace Torque::FS;

namespace Platform
{
namespace FS
{

bool  MountDefaults()
{
   String  path = getAssetDir();

   bool  mounted = Mount( "game", createNativeFS( path ));

   if ( !mounted )
      return false;

   // [8/31/2009 tomb] Disabling this for T3D 1.0 due to issues with script namespace corruption when running from zips
   // Note that the VirtualMountSystem must be enabled in volume.cpp for zip support to work.
   //return MountZips("game");
   return true;
}

bool MountZips(const String &root)
{
   Path basePath;
   basePath.setRoot(root);
   Vector<String> outList;

   S32 num = FindByPattern(basePath, "*.zip", true, outList);
   if(num == 0)
      return true; // not an error

   S32 mounted = 0;
   for(S32 i = 0;i < outList.size();++i)
   {
      String &zipfile = outList[i];
      mounted += (S32)Mount(root, new ZipFileSystem(zipfile, true));
   }

   return mounted == outList.size();
}

//-----------------------------------------------------------------------------

bool  Touch( const Path &path )
{
#if defined(TORQUE_OS_WIN32) || defined(TORQUE_OS_XBOX) || defined(TORQUE_OS_XENON)
   return( utime( path.getFullPath(), 0 ) != -1 );
#else
   return( utimes( path.getFullPath(), NULL) == 0 ); // utimes returns 0 on success.
#endif
}

} // namespace FS
} // namespace Platform
