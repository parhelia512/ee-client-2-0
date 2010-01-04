//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MACCARBVOLUME_H_
#define _MACCARBVOLUME_H_

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>


#ifndef _POSIXVOLUME_H_
   #include "platformPOSIX/posixVolume.h"
#endif
#ifndef _TVECTOR_H_
   #include "core/util/tVector.h"
#endif


class MacFileSystem;


/// File system change notifications on Mac.
class MacFileSystemChangeNotifier : public Torque::FS::FileSystemChangeNotifier
{
   public:
   
      typedef Torque::FS::FileSystemChangeNotifier Parent;
      
   protected:
   
      /// The kqueue handle.
      int mQueue;
      
      ///
      Vector< struct kevent > mEvents;
   
      ///
      Vector< Torque::Path > mDirs;
   
      // FileSystemChangeNotifier.
      virtual void internalProcessOnce();
      virtual bool internalAddNotification( const Torque::Path& dir );
      virtual bool internalRemoveNotification( const Torque::Path& dir );
   
   public:
   
      MacFileSystemChangeNotifier( MacFileSystem* fs );
      
      virtual ~MacFileSystemChangeNotifier();
};

/// Mac filesystem.
class MacFileSystem : public Torque::Posix::PosixFileSystem
{
   public:
   
      typedef Torque::Posix::PosixFileSystem Parent;
      
      MacFileSystem( String volume )
         : Parent( volume )
      {
         mChangeNotifier = new MacFileSystemChangeNotifier( this );
      }
};

#endif // !_MACCARBVOLUME_H_
