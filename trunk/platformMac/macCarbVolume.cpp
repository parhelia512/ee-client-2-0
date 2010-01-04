//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include <fcntl.h>
#include <unistd.h>


#include "platformMac/macCarbVolume.h"
#include "platform/platformVolume.h"
#include "console/console.h"


//#define DEBUG_SPEW


//-----------------------------------------------------------------------------
//    Change notifications.
//-----------------------------------------------------------------------------


// http://developer.apple.com/documentation/Darwin/Conceptual/FSEvents_ProgGuide/KernelQueues/KernelQueues.html


MacFileSystemChangeNotifier::MacFileSystemChangeNotifier( MacFileSystem* fs )
   : Parent( fs )
{
   VECTOR_SET_ASSOCIATION( mDirs );
   VECTOR_SET_ASSOCIATION( mEvents );
   
   mQueue = kqueue();
   if( mQueue < 0 )
      Con::errorf( "MacFileSystemChangeNotifier - could not create kqueue" );
}

MacFileSystemChangeNotifier::~MacFileSystemChangeNotifier()
{
   for( U32 i = 0, num = mEvents.size(); i < num; ++ i )
      close( mEvents[ i ].ident );
}

void MacFileSystemChangeNotifier::internalProcessOnce()
{
   if( mQueue < 0 )
      return;

   // Allocate temporary event data space.
   
   const U32 numEvents = mEvents.size();
   struct kevent* eventData = ( struct kevent* ) alloca( numEvents * sizeof( struct kevent ) );
   
   // Check for events.
   
   struct timespec timeout;
   dMemset( &timeout, 0, sizeof( struct timespec ) );
   
   const S32 numChanges = kevent( mQueue, mEvents.address(), numEvents, eventData, numEvents, &timeout );
   if( numChanges > 0 )
   {
      for( U32 i = 0; i < numChanges; ++ i )
      {
         U32 index = ( U32 ) eventData[ i ].udata;
               
         // Signal the change.
         
         #ifdef DEBUG_SPEW
         Platform::outputDebugString( "[MacFileSystemChangeNotifier] changed dir %i: '%s'", index, mDirs[ index ].getFullPath().c_str() );
         #endif
         
         internalNotifyDirChanged( mDirs[ index ] );
      }
   }
}

bool MacFileSystemChangeNotifier::internalAddNotification( const Torque::Path& dir )
{
   if( mQueue < 0 )
      return false;
      
   for( U32 i = 0, num = mDirs.size(); i < num; ++ i )
      if( mDirs[ i ] == dir )
         return false;
         
   // Map the path.
   
   Torque::Path fullFSPath = mFS->mapTo( dir );
   String osPath = PathToOS( fullFSPath );
   
   // Open the path.
   
   int handle = open( osPath.c_str(), O_EVTONLY );
   if( handle <= 0 )
      return false;
      
   // Create the event.
      
   const U32 index = mEvents.size();
   mDirs.push_back( dir );
   mEvents.increment();
   struct kevent& event = mEvents.last();
   
   EV_SET( &event, handle, EVFILT_VNODE, EV_ADD | EV_CLEAR, NOTE_WRITE | NOTE_RENAME | NOTE_ATTRIB, 0, ( void* ) index );
   
   #ifdef DEBUG_SPEW
   Platform::outputDebugString( "[MacFileSystemChangeNotifier] added change notification %i to '%s' (full path: %s)", index, dir.getFullPath().c_str(), osPath.c_str() );
   #endif
   
   return true;
}

bool MacFileSystemChangeNotifier::internalRemoveNotification( const Torque::Path& dir )
{
   if( mQueue < 0 )
      return false;
      
   for( U32 i = 0, num = mDirs.size(); i < num; ++ i )
      if( mDirs[ i ] == dir )
      {
         #ifdef DEBUG_SPEW
         Platform::outputDebugString( "[MacFileSystemChangeNotifier] removing change notification %i from '%s'", i, dir.getFullPath().c_str() );
         #endif
         
         close( mEvents[ i ].ident );
         
         // Adjust indices.
         
         for( U32 n = i + 1; n < mEvents.size(); ++ n )
            mEvents[ n ].udata = ( void* ) ( ( ( U32 ) mEvents[ n ].udata ) - 1 );
            
         // Erase notification.
         
         mEvents.erase( i );
         mDirs.erase( i );
         
         return true;
      }
      
   return false;
}

//-----------------------------------------------------------------------------
//    Platform API.
//-----------------------------------------------------------------------------

Torque::FS::FileSystemRef Platform::FS::createNativeFS( const String &volume )
{
   return new MacFileSystem( volume );
}

bool Torque::FS::VerifyWriteAccess(const Torque::Path &path)
{
   return true;
}
