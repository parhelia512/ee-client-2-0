//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _BITVECTOR_H_
#define _BITVECTOR_H_


/// Manage a vector of bits of arbitrary size.
class BitVector
{
protected:

   /// The array of bytes that stores our bits.
   U8* mBits;

   /// The allocated size of the bit array.
   U32 mByteSize;

   /// The size of the vector in bits. 
   U32 mSize;

   /// Returns a size in bytes which is 32bit aligned
   /// and can hold all the requested bits.
   static U32 calcByteSize( const U32 numBits );

   /// Internal function which resizes the bit array.
   void _resize( U32 sizeInBits, bool copyBits );

public:

   /// Default constructor which creates an bit
   /// vector with a bit size of zero.
   BitVector();
   
   /// Constructs a bit vector with the desired size.
   BitVector( U32 sizeInBits );
   
   /// Destructor.
   ~BitVector();

   /// @name Size Management
   /// @{
   
   /// Resizes the bit vector.
   void setSize( U32 sizeInBits );

   /// Returns the size in bits.
   U32 getSize() const;

   /// Returns the 32bit aligned size in bytes.
   U32 getByteSize() const;

   /// Returns the allocated size in bytes
   /// which may be more than getByteSize().
   U32 getAllocatedByteSize() const { return mByteSize; }

   /// Retuns the bits.
   const U8* getBits() const { return mBits; }

   /// Returns the writeable bits.
   U8* getNCBits() { return mBits; }

   /// @}

   /// Copy the content of another bit vector.
   void copy( const BitVector &from );

   /// @name Mutators
   /// Note that bits are specified by index, unlike BitSet32.
   /// @{

   /// Clear all the bits.
   void clear();

   /// Set all the bits.
   void set();

   /// Set the specified bit.
   void set(U32 bit);
   
   /// Clear the specified bit.
   void clear(U32 bit);
   
   /// Test that the specified bit is set.
   bool test(U32 bit) const;

   /// Return true if all bits are set.
   bool testAll() const;
   
   /// Return true if all bits are clear.
   bool testAllClear() const;

   /// @}
};

inline BitVector::BitVector()
{
   mBits     = NULL;
   mByteSize = 0;
   mSize = 0;
}


inline BitVector::BitVector( U32 sizeInBits )
{
   mBits     = NULL;
   mByteSize = 0;
   mSize = 0;
   setSize( sizeInBits );
}

inline BitVector::~BitVector()
{
   delete [] mBits;
   mBits = NULL;
   mByteSize = 0;
   mSize = 0;
}

inline U32 BitVector::calcByteSize( U32 numBits )
{
   // Make sure that we are 32 bit aligned
   return (((numBits + 0x7) >> 3) + 0x3) & ~0x3;
}

inline void BitVector::setSize( const U32 sizeInBits )
{
   _resize( sizeInBits, true );
}

inline U32 BitVector::getSize() const
{
   return mSize;
}

inline U32 BitVector::getByteSize() const
{
   return calcByteSize(mSize);
}

inline void BitVector::clear()
{
   if (mSize != 0)
      dMemset( mBits, 0x00, getByteSize() );
}

inline void BitVector::copy( const BitVector &from )
{
   _resize( from.getSize(), false );
   if (mSize != 0)
      dMemcpy( mBits, from.getBits(), getByteSize() );
}

inline void BitVector::set()
{
   if (mSize != 0)
      dMemset(mBits, 0xFF, getByteSize() );
}

inline void BitVector::set(U32 bit)
{
   AssertFatal(bit < mSize, "BitVector::set - Error, out of range bit!");

   mBits[bit >> 3] |= U8(1 << (bit & 0x7));
}

inline void BitVector::clear(U32 bit)
{
   AssertFatal(bit < mSize, "BitVector::clear - Error, out of range bit!");
   mBits[bit >> 3] &= U8(~(1 << (bit & 0x7)));
}

inline bool BitVector::test(U32 bit) const
{
   AssertFatal(bit < mSize, "BitVector::test - Error, out of range bit!");
   return (mBits[bit >> 3] & U8(1 << (bit & 0x7))) != 0;
}

#endif //_BITVECTOR_H_
