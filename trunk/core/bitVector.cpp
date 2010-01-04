//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "core/bitVector.h"


void BitVector::_resize( U32 sizeInBits, bool copyBits )
{
   if ( sizeInBits != 0 ) 
   {
      U32 newSize = calcByteSize( sizeInBits );
      if ( mByteSize < newSize ) 
      {
         U8 *newBits = new U8[newSize];
         if ( copyBits )
            dMemcpy( newBits, mBits, mByteSize );
         delete [] mBits;
         mBits = newBits;
         mByteSize = newSize;
      }
   } 
   else 
   {
      delete [] mBits;
      mBits     = NULL;
      mByteSize = 0;
   }

   mSize = sizeInBits;
}

bool BitVector::testAll() const
{
   const U32 remaider = mSize % 8;
   const U32 testBytes = mSize / 8;

   for ( U32 i=0; i < testBytes; i++ )
      if ( mBits[i] != 0xFF )
         return false;

   if ( remaider == 0 )
      return true;

   const U8 mask = (U8)0xFF >> ( 8 - remaider );
   return ( mBits[testBytes] & mask ) == mask; 
}

bool BitVector::testAllClear() const
{
   const U32 remaider = mSize % 8;
   const U32 testBytes = mSize / 8;

   for ( U32 i=0; i < testBytes; i++ )
      if ( mBits[i] != 0 )
         return false;

   if ( remaider == 0 )
      return true;

   const U8 mask = (U8)0xFF >> ( 8 - remaider );
   return ( mBits[testBytes] & mask ) == 0;
}
