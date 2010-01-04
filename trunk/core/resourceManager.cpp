//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "core/resourceManager.h"

#include "core/volume.h"
#include "console/console.h"
#include "core/util/autoPtr.h"

static AutoPtr< ResourceManager > smInstance;

ResourceManager::ResourceManager()
:  mIterSigFilter( U32_MAX )
{
}

ResourceManager::~ResourceManager()
{
   // TODO: Dump resources that have not been released?
}

ResourceManager &ResourceManager::get()
{
   if ( smInstance.isNull() )
      smInstance = new ResourceManager;
   return *smInstance;
}

ResourceBase ResourceManager::load(const Torque::Path &path)
{
#ifdef TORQUE_DEBUG_RES_MANAGER
   Con::printf( "ResourceManager::load : [%s]", path.getFullPath().c_str() );
#endif

   ResourceHeaderMap::Iterator iter = mResourceHeaderMap.findOrInsert( path.getFullPath() );

   ResourceHeaderMap::Pair &pair = *iter;

   if ( pair.value == NULL )
   {
      pair.value = new ResourceBase::Header;
      FS::AddChangeNotification( path, this, &ResourceManager::notifiedFileChanged );
   }

   ResourceBase::Header *header = pair.value;

   if (header->getSignature() == 0)
     header->mPath = path;

   return ResourceBase( header );
}

ResourceBase ResourceManager::find(const Torque::Path &path)
{
#ifdef TORQUE_DEBUG_RES_MANAGER
   Con::printf( "ResourceManager::find : [%s]", path.getFullPath().c_str() );
#endif

   ResourceHeaderMap::Iterator iter = mResourceHeaderMap.find( path.getFullPath() );

   if ( iter == mResourceHeaderMap.end() )
      return ResourceBase();

   ResourceHeaderMap::Pair &pair = *iter;

   ResourceBase::Header	*header = pair.value;

   return ResourceBase(header);
}

#ifdef TORQUE_DEBUG
void ResourceManager::dumpToConsole()
{
   const U32   numResources = mResourceHeaderMap.size();

   if ( numResources == 0 )
   {
      Con::printf( "ResourceManager is not managing any resources" );
      return;
   }

   Con::printf( "ResourceManager is managing %d resources:", numResources );
   Con::printf( " [ref count/signature/path]" );

   ResourceHeaderMap::Iterator iter;

   for( iter = mResourceHeaderMap.begin(); iter != mResourceHeaderMap.end(); ++iter )
   {
      ResourceBase::Header	*header = (*iter).value;
            
      char fourCC[ 5 ];
      *( ( U32* ) fourCC ) = header->getSignature();
      fourCC[ 4 ] = 0;

      Con::printf( " %3d %s [%s] ", header->getRefCount(), fourCC, (*iter).key.c_str() );
   }
}
#endif

bool ResourceManager::remove( ResourceBase::Header* header )
{
   const Path &path = header->getPath();

#ifdef TORQUE_DEBUG_RES_MANAGER
   Con::printf( "ResourceManager::remove : [%s]", path.getFullPath().c_str() );
#endif

   ResourceHeaderMap::Iterator iter = mResourceHeaderMap.find( path.getFullPath() );
   if ( iter != mResourceHeaderMap.end() && iter->value == header )
   {
      AssertISV( header && (header->getRefCount() == 0), "ResourceManager error: trying to remove resource which is still in use." );
      mResourceHeaderMap.erase( iter );
   }
   else
   {
      iter = mPrevResourceHeaderMap.find( path.getFullPath() );
      if ( iter == mPrevResourceHeaderMap.end() || iter->value != header )
      {
         Con::errorf( "ResourceManager::remove : Trying to remove non-existent resource [%s]", path.getFullPath().c_str() );
         return false;
      }

      AssertISV( header && (header->getRefCount() == 0), "ResourceManager error: trying to remove resource which is still in use." );
      mPrevResourceHeaderMap.erase( iter );
   }

   FS::RemoveChangeNotification( path, this, &ResourceManager::notifiedFileChanged );

   return true;
}

void ResourceManager::notifiedFileChanged( const Torque::Path &path )
{   
   // First, find the resource.
   ResourceHeaderMap::Iterator iter = mResourceHeaderMap.find( path.getFullPath() );
   if ( iter == mResourceHeaderMap.end() )
   {
      // We aren't managing this resource, but it may be in the
      // previous resource list... either way we don't do a notification.
      return;
   }

   Con::warnf( "[ResourceManager::notifiedFileChanged] : File changed [%s]", path.getFullPath().c_str() );

   ResourceBase::Header	*header = (*iter).value;
   ResourceBase::Signature sig = header->getSignature();
   mResourceHeaderMap.erase( iter );

   // Move the resource into the previous resource map.
   iter = mPrevResourceHeaderMap.findOrInsert( path );
   iter->value = header;

   // Now notify users of the resource change so they 
   // can release and reload.
   mChangeSignal.trigger( sig, path );
}

ResourceBase ResourceManager::startResourceList( ResourceBase::Signature inSignature )
{
   mIter = mResourceHeaderMap.begin();

   mIterSigFilter = inSignature;

   return nextResource();
}

ResourceBase ResourceManager::nextResource()
{
   ResourceBase::Header	*header = NULL;

   while( mIter != mResourceHeaderMap.end() )
   {
      header = (*mIter).value;

      ++mIter;

      if ( mIterSigFilter == U32_MAX )
         return ResourceBase(header);

      if ( header->getSignature() == mIterSigFilter )
         return ResourceBase(header);
   }
   
   return ResourceBase();
}

#ifdef TORQUE_DEBUG
ConsoleFunctionGroupBegin(ResourceManagerFunctions, "Resource management functions.");

ConsoleFunction(resourceDump, void, 1, 1, "resourceDump() - list the currently managed resources")
{
   ResourceManager::get().dumpToConsole();
}

ConsoleFunctionGroupEnd( ResourceManagerFunctions );
#endif
