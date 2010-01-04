//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _DDSFILE_H_
#define _DDSFILE_H_

#ifndef _GFXSTRUCTS_H_
#include "gfx/gfxStructs.h"
#endif
#ifndef _BITSET_H_
#include "core/bitSet.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef __RESOURCE_H__
#include "core/resource.h"
#endif

class Stream;
class GBitmap;


struct DDSFile
{
   enum DDSFlags
   {
      ComplexFlag = BIT(0), ///< Indicates this includes a mipchain, cubemap, or
                            ///  volume texture, ie, isn't a plain old bitmap.
      MipMapsFlag = BIT(1), ///< Indicates we have a mipmap chain in the file.
      CubeMapFlag = BIT(2), ///< Indicates we are a cubemap. Requires all six faces.
      VolumeFlag  = BIT(3), ///< Indicates we are a volume texture.

      PitchSizeFlag = BIT(4),  ///< Cue as to how to interpret our pitchlinear value.
      LinearSizeFlag = BIT(5), ///< Cue as to how to interpret our pitchlinear value.

      RGBData        = BIT(6), ///< Indicates that this is straight out RGBA data.
      CompressedData = BIT(7), ///< Indicates that this is compressed or otherwise
                               ///  exotic data.
   };

   BitSet32    mFlags;
   U32         mHeight;
   U32         mWidth;
   U32         mDepth;
   U32         mPitchOrLinearSize;
   U32         mMipMapCount;

   GFXFormat   mFormat;
   U32         mBytesPerPixel; ///< Ignored if we're a compressed texture.
   U32         mFourCC;
   String      mCacheString;
   Torque::Path mSourcePath;

   bool        mHasTransparency;

   struct SurfaceData
   {
      SurfaceData()
      {
         VECTOR_SET_ASSOCIATION( mMips );
      }

      ~SurfaceData()
      {
         // Free our mips!
         for(S32 i=0; i<mMips.size(); i++)
            delete[] mMips[i];
      }

      Vector<U8*> mMips;

      // Helper function to read in a mipchain.
      bool readMipChain();

      void dumpImage(DDSFile *dds, U32 mip, const char *file);
      
      /// Helper for reading a mip level.
      void readNextMip(DDSFile *dds, Stream &s, U32 height, U32 width, U32 mipLevel);
      
      /// Helper for writing a mip level.
      void writeNextMip(DDSFile *dds, Stream &s, U32 height, U32 width, U32 mipLevel);
   };
   
   Vector<SurfaceData*> mSurfaces;

   /// Clear all our information; used before reading.
   void clear();

   /// Reads a DDS file from the stream.
   bool read(Stream &s);

   /// Called from read() to read in the DDS header.
   bool readHeader(Stream &s);

   /// Writes this DDS file to the stream.
   bool write(Stream &s);

   /// Called from write() to write the DDS header.
   bool writeHeader(Stream &s);

   /// For our current format etc., what is the size of a surface with the
   /// given dimensions?
   U32 getSurfaceSize( U32 mipLevel = 0 ) const { return getSurfaceSize( mHeight, mWidth, mipLevel ); }
   U32 getSurfaceSize( U32 height, U32 width, U32 mipLevel = 0 ) const;

   /// Returns the total video memory size of the texture
   /// including all mipmaps and compression settings.
   U32 getSizeInBytes() const;

   U32 getWidth( U32 mipLevel = 0 ) const { return getMax( U32(1), mWidth >> mipLevel ); }
   U32 getHeight( U32 mipLevel = 0 ) const { return getMax(U32(1), mHeight >> mipLevel); }
   U32 getDepth( U32 mipLevel = 0 ) const { return getMax(U32(1), mDepth >> mipLevel); }

   bool getHasTransparency() const { return mHasTransparency; }

   U32 getPitch( U32 mipLevel = 0 ) const;

   const Torque::Path &getSourcePath() const { return mSourcePath; }
   const String &getTextureCacheString() const { return mCacheString; }

   static Resource<DDSFile> load( const Torque::Path &path );

   // For debugging fun!
   static S32 smActiveCopies;

   DDSFile()
   {
      VECTOR_SET_ASSOCIATION( mSurfaces );
      smActiveCopies++;

      mHasTransparency = false;
   }

   DDSFile( const DDSFile &dds );

   ~DDSFile()
   {
      smActiveCopies--;

      // Free our surfaces!
      for(S32 i=0; i<mSurfaces.size(); i++)
         delete mSurfaces[i];

      mSurfaces.clear();
   }

   static DDSFile *createDDSFileFromGBitmap( const GBitmap *gbmp );
};

#endif // _DDSFILE_H_