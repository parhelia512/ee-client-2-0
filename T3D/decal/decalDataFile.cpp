//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "decalDataFile.h"

#include "math/mathIO.h"
#include "core/tAlgorithm.h"
#include "core/stream/fileStream.h"

#include "T3D/decal/decalManager.h"
#include "T3D/decal/decalData.h"


void DecalSphere::updateBounds()
{
   // This will be the distance
   // from the sphere center
   // of the farthest item out.
   F32 distFromCenter = 0;

   // Used to hold the 
   // computed item distance.
   F32 itemDist = 0;

   F32 largestItemSize = 0;

   // Current item.
   DecalInstance *inst = NULL;

   for ( U32 i = 0; i < mItems.size(); i++ )
   {
      inst = mItems[i];
      
      itemDist = (mWorldSphere.center - inst->mPosition).len();
      if ( itemDist > distFromCenter )
         distFromCenter = itemDist;

      if ( inst->mSize > largestItemSize )
         largestItemSize = inst->mSize;
   }
   
   mWorldSphere.radius = distFromCenter + largestItemSize + largestItemSize + 0.5f;
}

template<>
void* Resource<DecalDataFile>::create( const Torque::Path &path )
{
   FileStream stream;
   stream.open( path.getFullPath(), Torque::FS::File::Read );
   if ( stream.getStatus() != Stream::Ok )
      return NULL;

   DecalDataFile *file = new DecalDataFile();
   if ( !file->read( stream ) )
   {
      delete file;
      return NULL;
   }

   return file;
}

template<> 
ResourceBase::Signature Resource<DecalDataFile>::signature()
{
   return MakeFourCC('d','e','c','f');
}

DecalDataFile::DecalDataFile()
   : mIsDirty( false )
{
   mSphereList.setSize( 0 );
}

DecalDataFile::~DecalDataFile()
{
   clear();
}

void DecalDataFile::clear()
{
   for ( U32 i=0; i < mSphereList.size(); i++ )
      delete mSphereList[i];

   mSphereList.clear();
   mChunker.freeBlocks();

   mIsDirty = true;
}

bool DecalDataFile::write( const char *path )
{
   // Open the stream.
   FileStream stream;
   if ( !stream.open( path, Torque::FS::File::Write ) )
   {
      Con::errorf( "DecalDataFile::write() - Failed opening stream!" );
      return false;
   }

   // Write our identifier... so we have a better
   // idea if we're reading pure garbage.
   // This identifier stands for "Torque Decal Data File".
   stream.write( 4, "TDDF" );

   // Now the version number.
   stream.write( (U8)FILE_VERSION );

   Vector<DecalInstance*> allDecals;

   // Gather all DecalInstances that should be saved.
   for ( U32 i = 0; i < mSphereList.size(); i++ )  
   {      
      Vector<DecalInstance*>::const_iterator item = mSphereList[i]->mItems.begin();
      for ( ; item != mSphereList[i]->mItems.end(); item++ )
      {
         if ( (*item)->mFlags & SaveDecal )
            allDecals.push_back( (*item) );
      }
   }

   // Gather all the DecalData datablocks used.
   Vector<const DecalData*> allDatablocks;
   for ( U32 i = 0; i < allDecals.size(); i++ )
      allDatablocks.push_back_unique( allDecals[i]->mDataBlock );

   // Write out datablock lookupNames.
   U32 count = allDatablocks.size();
   stream.write( count );
   for ( U32 i = 0; i < count; i++ )
      stream.write( allDatablocks[i]->lookupName );

   Vector<const DecalData*>::iterator dataIter;

   // Write out the DecalInstance list.
   count = allDecals.size();
   stream.write( count );
   for ( U32 i = 0; i < count; i++ )
   {
      DecalInstance *inst = allDecals[i];

      dataIter = find( allDatablocks.begin(), allDatablocks.end(), inst->mDataBlock );
      U8 dataIndex = dataIter - allDatablocks.begin();
      
      stream.write( dataIndex );
      mathWrite( stream, inst->mPosition );
      mathWrite( stream, inst->mNormal );
      mathWrite( stream, inst->mTangent );
		stream.write( inst->mTextureRectIdx );   
      stream.write( inst->mSize );
      stream.write( inst->mRenderPriority );
   }

   // Clear the dirty flag.
   mIsDirty = false;

   return true;
}

bool DecalDataFile::read( Stream &stream )
{
   // NOTE: we are shortcutting by just saving out the DecalInst and 
   // using regular addDecal methods to add them, which will end up
   // generating the DecalSphere(s) in the process.
   // It would be more efficient however to just save out all the data
   // and read it all in with no calculation required.

   // Read our identifier... so we know we're 
   // not reading in pure garbage.
   char id[4] = { 0 };
   stream.read( 4, id );
   if ( dMemcmp( id, "TDDF", 4 ) != 0 )
   {
      Con::errorf( "DecalDataFile::read() - This is not a Decal file!" );
      return false;
   }

   // Empty ourselves before we really begin reading.
   clear();

   // Now the version number.
   U8 version;
   stream.read( &version );
   if ( version != (U8)FILE_VERSION )
   {
      Con::errorf( "DecalDataFile::read() - file versions do not match!" );
      Con::errorf( "You must manually delete the old .decals file before continuing!" );
      return false;
   }

   // Read in the lookupNames of the DecalData datablocks and recover the datablock.   
   Vector<DecalData*> allDatablocks;
   U32 count;
   stream.read( &count );
   allDatablocks.setSize( count );
   for ( U32 i = 0; i < count; i++ )
   {      
      String lookupName;      
      stream.read( &lookupName );
      
      DecalData *data = DecalData::findDatablock( lookupName );
      
      if ( data )
         allDatablocks[ i ] = data;
      else
      {
         allDatablocks[ i ] = NULL;
         Con::errorf( "DecalDataFile::read() - DecalData %s does not exist!", lookupName.c_str() );
      }
   }

   U8 dataIndex;   
   DecalData *data;

   // Now read all the DecalInstance(s).
   stream.read( &count );
   for ( U32 i = 0; i < count; i++ )
   {           
      DecalInstance *inst = _allocateInstance();

      stream.read( &dataIndex );
      mathRead( stream, &inst->mPosition );
      mathRead( stream, &inst->mNormal );
      mathRead( stream, &inst->mTangent );
		stream.read( &inst->mTextureRectIdx );
      stream.read( &inst->mSize );
      stream.read( &inst->mRenderPriority );

      inst->mVisibility = 1.0f;
      inst->mFlags = PermanentDecal | SaveDecal | ClipDecal;
      inst->mCreateTime = Sim::getCurrentTime();
      inst->mVerts = NULL;
      inst->mIndices = NULL;
      inst->mVertCount = 0;
      inst->mIndxCount = 0;

      data = allDatablocks[ dataIndex ];

      if ( data )          
      {         
         inst->mDataBlock = data;

         addDecal( inst );
			
			// onload set instances should get added to the appropriate vec
			inst->mId = gDecalManager->mDecalInstanceVec.size();
			gDecalManager->mDecalInstanceVec.push_back(inst);
      }
      else
      {
         _freeInstance( inst );
         Con::errorf( "DecalDataFile::read - cannot find DecalData for DecalInstance read from disk." );
      }
   }

   // Clear the dirty flag.
   mIsDirty = false;

   return true;
}

DecalInstance* DecalDataFile::_allocateInstance()
{
   DecalInstance *decal = mChunker.alloc();
   decal->mRenderPriority = 0;
   decal->mCustomTex = NULL;
   decal->mId = -1;

   // TODO: Add a real constructor!

   return decal;
}

void DecalDataFile::addDecal( DecalInstance *inst )
{
   DecalSphere *closestSphere = NULL;
   F32 closestDist = F32_MAX;

   // Might want to expose these.
   const F32 distanceTol = 10.0f;
   const F32 maxRadius = 10.0f;
	
	// Validate the decal instance.
   //inst->mTextureRectIdx = getMin(  inst->mTextureRectIdx, 
   //                                 (U32)DecalData::MAX_TEXCOORD_COUNT );

   // First find the closest existing sphere to this
   // item that is within our tolerance. 
   for ( U32 i = 0; i < mSphereList.size(); i++ )
   {
      DecalSphere *sphere = mSphereList[i];
      const SphereF &bounds = sphere->mWorldSphere;

      F32 dist = bounds.distanceTo( inst->mPosition );

      if (  dist > distanceTol )
         continue;
      else if ( dist < 0.0f )
      {
         // This point is inside the
         // sphere, so we can just add this item.
         closestDist = distanceTol;
         closestSphere = sphere;
         break;
      }

      // If the new radius is greater
      // than the max radius, break,
      // and add a new sphere.
      F32 newRadius = dist + bounds.radius + 0.5f + inst->mSize;
      SphereF newBounds( bounds.center, newRadius );

      if ( newRadius > maxRadius )
         continue;     

      if ( distanceTol < closestDist )
      {
         closestDist = distanceTol;
         closestSphere = sphere;
         closestSphere->mWorldSphere = newBounds;
      }
   }

   // If we didn't find an existing cell... create one.
   if ( !closestSphere )
   {
      F32 itemSize = inst->mSize;
      F32 radius = itemSize * itemSize + 0.5f;
      const Point3F &itemPos = inst->mPosition;
      Point3F offsetPos( itemPos );
      offsetPos.y += itemSize;
      SphereF newSphere( offsetPos, radius );

      radius += mFabs( newSphere.distanceTo( itemPos ) );

      closestSphere = new DecalSphere( itemPos, radius );
      mSphereList.push_back( closestSphere );
   }

   // And add the DecalInst to the sphere we either just created
   // or found earlier.
   closestSphere->mItems.push_back( inst );
   closestSphere->updateBounds();   
}

void DecalDataFile::removeDecal( DecalInstance *inst )
{     
   DecalSphere *pSphere = NULL;

   for ( U32 i = 0; i < mSphereList.size(); i++ )
   {
      pSphere = mSphereList[i];
      
      Vector<DecalInstance*> &items = pSphere->mItems;

      //Vector<DecalInstance*>::iterator found = find( items.begin(), items.last(), inst );
      bool found = items.remove( inst );

      if ( found )
      {         
         if ( items.empty() )
         {
            delete pSphere;      
            mSphereList.erase( i );
         }
         else
            pSphere->updateBounds();

         _freeInstance( inst );

         return;
      }
   }

   Con::errorf( "DecalDataFile did not contain a DecalInstance passed to removeData!" );
}

void DecalDataFile::notifyDecalModified( DecalInstance *inst )
{
   // Delete buffers, need to be regenerated the next time 
   // this instance is rendered.

   // Currently the decal editor itself handles reclipping decals
   // that are modified, since it needs the edge verts anyway,
   // but might want to do that here in the future.

   /*
   if ( inst->mVerts )
   {
      delete inst->mVerts;
      inst->mVerts = NULL;

      delete inst->mIndices;
      inst->mIndices = NULL;
   }

   inst->mFlags |= ClipDecal;
   */

   // Find the DecalSphere containing this DecalInst,
   // Remove it from that sphere,
   // Delete the sphere if that was the last DecalInst,
   // Re-add the DecalInst to the DecalDataFile.

   DecalSphere *pSphere = NULL;

   for ( U32 i = 0; i < mSphereList.size(); i++ )
   {
      pSphere = mSphereList[i];

      Vector<DecalInstance*> &items = pSphere->mItems;

      bool found = items.remove( inst );

      if ( found )
      {       
         if ( items.empty() )
         {
            delete pSphere;      
            mSphereList.erase( i );
         }
         else
            pSphere->updateBounds();

         addDecal( inst );

         return;
      }
   }

   Con::errorf( "DecalDataFile did not contain a DecalInstance passed to notifyDecalModified!" );
}

