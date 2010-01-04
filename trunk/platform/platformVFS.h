//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _PLATFORMVFS_H_
#define _PLATFORMVFS_H_

namespace Zip
{
   class ZipArchive;
}

extern Zip::ZipArchive *openEmbeddedVFSArchive();
extern void closeEmbeddedVFSArchive();

#endif // _PLATFORMVFS_H_
