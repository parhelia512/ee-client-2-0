//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _DECALDATAFILE_H_
#define _DECALDATAFILE_H_

#ifndef _DATACHUNKER_H_
#include "core/dataChunker.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _MSPHERE_H_
#include "math/mSphere.h"
#endif


class DecalInstance;
class Stream;

/// A bounding sphere in world space and a list of DecalInstance(s)
/// contained by it. DecalInstance(s) are organized/binned in this fashion
/// as a lookup and culling optimization.
class DecalSphere
{
public:
   
   DecalSphere() {}   
   DecalSphere( const Point3F &position, F32 radius ) 
   {
      mWorldSphere.center = position;
      mWorldSphere.radius = radius;
   }

   void updateBounds();

   Vector<DecalInstance*> mItems;
   SphereF mWorldSphere;
};

/// This is the data file for decals.
/// Not intended to be used directly, do your work with decals
/// via the DecalManager.
class DecalDataFile
{
   friend class DecalManager;

public:

   DecalDataFile();
   virtual ~DecalDataFile();

   Vector<DecalSphere*>& getGrid() { return mSphereList; }
   const Vector<DecalSphere*>& getGrid() const { return mSphereList; }

   /// Deletes all the data and resets the 
   /// file to an empty state.
   void clear();

   /// 
   bool write( const char *path );

   ///
   bool read( Stream &stream );

   void addDecal( DecalInstance *inst );
   void removeDecal( DecalInstance *inst );
   void notifyDecalModified( DecalInstance *inst );

protected:

   DecalInstance* _allocateInstance();
   void _freeInstance( DecalInstance *decal );

protected:

   enum { FILE_VERSION = 5 };

   /// Set to true if the file is dirty and
   /// needs to be saved before being destroyed.
   bool mIsDirty;

   /// List of bounding sphere shapes that contain and organize
   /// DecalInstances for optimized culling and lookup.
   Vector<DecalSphere*> mSphereList;

   FreeListChunker<DecalInstance> mChunker;
};

inline void DecalDataFile::_freeInstance( DecalInstance *decal )
{
   mChunker.free( decal );
}

#endif // _DECALDATAFILE_H_
