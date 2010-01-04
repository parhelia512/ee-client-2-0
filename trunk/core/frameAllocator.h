//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _FRAMEALLOCATOR_H_
#define _FRAMEALLOCATOR_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif

/// Temporary memory pool for per-frame allocations.
///
/// In the course of rendering a frame, it is often necessary to allocate
/// many small chunks of memory, then free them all in a batch. For instance,
/// say we're allocating storage for some vertex calculations:
///
/// @code
///   // Get FrameAllocator memory...
///   U32 waterMark = FrameAllocator::getWaterMark();
///   F32 * ptr = (F32*)FrameAllocator::alloc(sizeof(F32)*2*targetMesh->vertsPerFrame);
///
///   ... calculations ...
///
///   // Free frameAllocator memory
///   FrameAllocator::setWaterMark(waterMark);
/// @endcode
class FrameAllocator
{
   static U8*   smBuffer;
   static U32   smHighWaterMark;
   static U32   smWaterMark;

#ifdef TORQUE_DEBUG
   static U32 smMaxFrameAllocation;
#endif

  public:
   inline static void init(const U32 frameSize);
   inline static void destroy();

   inline static void* alloc(const U32 allocSize);

   inline static void setWaterMark(const U32);
   inline static U32  getWaterMark();
   inline static U32  getHighWaterMark();

#ifdef TORQUE_DEBUG
   static U32 getMaxFrameAllocation() { return smMaxFrameAllocation; }
#endif
};

void FrameAllocator::init(const U32 frameSize)
{
#ifdef FRAMEALLOCATOR_DEBUG_GUARD
   AssertISV( false, "FRAMEALLOCATOR_DEBUG_GUARD has been removed because it allows non-contiguous memory allocation by the FrameAllocator, and this is *not* ok." );
#endif

   AssertFatal(smBuffer == NULL, "Error, already initialized");
   smBuffer = new U8[frameSize];
   smWaterMark = 0;
   smHighWaterMark = frameSize;
}

void FrameAllocator::destroy()
{
   AssertFatal(smBuffer != NULL, "Error, not initialized");

   delete [] smBuffer;
   smBuffer = NULL;
   smWaterMark = 0;
   smHighWaterMark = 0;
}


void* FrameAllocator::alloc(const U32 allocSize)
{
   U32 _allocSize = allocSize;

   AssertFatal(smBuffer != NULL, "Error, no buffer!");
   AssertFatal(smWaterMark + _allocSize <= smHighWaterMark, "Error alloc too large, increase frame size!");

   // Keep all frame allocator allocations aligned to DWORD boundries on the 360
   // Add 3, mask out the lower 3 bits.
   smWaterMark = ( smWaterMark + ( TORQUE_BYTE_ALIGNMENT - 1 ) ) & (~( TORQUE_BYTE_ALIGNMENT - 1 ));

   // Sanity check.
   AssertFatal( !( smWaterMark & ( TORQUE_BYTE_ALIGNMENT - 1 ) ), "Frame allocation is not on a 4-byte boundry." );

   U8* p = &smBuffer[smWaterMark];
   smWaterMark += _allocSize;

#ifdef TORQUE_DEBUG
   if (smWaterMark > smMaxFrameAllocation)
      smMaxFrameAllocation = smWaterMark;
#endif

   return p;
}


void FrameAllocator::setWaterMark(const U32 waterMark)
{
   AssertFatal(waterMark < smHighWaterMark, "Error, invalid waterMark");
   smWaterMark = waterMark;
}

U32 FrameAllocator::getWaterMark()
{
   return smWaterMark;
}

U32 FrameAllocator::getHighWaterMark()
{
   return smHighWaterMark;
}

/// Helper class to deal with FrameAllocator usage.
///
/// The purpose of this class is to make it simpler and more reliable to use the
/// FrameAllocator. Simply use it like this:
///
/// @code
/// FrameAllocatorMarker mem;
///
/// char *buff = (char*)mem.alloc(100);
/// @endcode
///
/// When you leave the scope you defined the FrameAllocatorMarker in, it will
/// automatically restore the watermark on the FrameAllocator. In situations
/// with complex branches, this can be a significant headache remover, as you
/// don't have to remember to reset the FrameAllocator on every posssible branch.
class FrameAllocatorMarker
{
   U32 mMarker;

public:
   FrameAllocatorMarker()
   {
      mMarker = FrameAllocator::getWaterMark();
   }

   ~FrameAllocatorMarker()
   {
      FrameAllocator::setWaterMark(mMarker);
   }

   void* alloc(const U32 allocSize) const
   {
      return FrameAllocator::alloc(allocSize);
   }
};

/// Class for temporary variables that you want to allocate easily using
/// the FrameAllocator. For example:
/// @code
/// FrameTemp<char> tempStr(32); // NOTE! This parameter is NOT THE SIZE IN BYTES. See constructor docs.
/// dStrcat( tempStr, SomeOtherString );
/// tempStr[2] = 'l';
/// Con::printf( tempStr );
/// Con::printf( "Foo: %s", ~tempStr );
/// @endcode
///
/// This will automatically handle getting and restoring the watermark of the
/// FrameAllocator when it goes out of scope. You should notice the strange
/// operator in front of tempStr on the printf call. This is normally a unary
/// operator for ones-complement, but in this class it will simply return the
/// memory of the allocation. It's the same as doing (const char *)tempStr
/// in the above case. The reason why it is necessary for the second printf
/// and not the first is because the second one is taking a variable arg
/// list and so it isn't getting the cast so that it's cast operator can
/// properly return the memory instead of the FrameTemp object itself.
///
/// @note It is important to note that this object is designed to just be a
/// temporary array of a dynamic size. Some wierdness may occur if you try
/// to perform crazy pointer stuff with it using regular operators on it.
template<class T>
class FrameTemp
{
protected:
   U32 mWaterMark;
   T *mMemory;
   U32 mNumObjectsInMemory;

public:
   /// Constructor will store the FrameAllocator watermark and allocate the memory off
   /// of the FrameAllocator.
   ///
   /// @note It is important to note that, unlike the FrameAllocatorMarker and the
   /// FrameAllocator itself, the argument to allocate is NOT the size in bytes,
   /// doing:
   /// @code
   /// FrameTemp<F64> f64s(5);
   /// @endcode
   /// Is the same as
   /// @code
   /// F64 *f64s = new F64[5];
   /// @endcode
   ///
   /// @param   count   The number of objects to allocate
   FrameTemp( const U32 count = 1 ) : mNumObjectsInMemory( count )
   {
      AssertFatal( count > 0, "Allocating a FrameTemp with less than one instance" );
      mWaterMark = FrameAllocator::getWaterMark();
      mMemory = reinterpret_cast<T *>( FrameAllocator::alloc( sizeof( T ) * count ) );

      for( int i = 0; i < mNumObjectsInMemory; i++ )
         constructInPlace<T>( &mMemory[i] );
   }

   /// Destructor restores the watermark
   ~FrameTemp()
   {
      // Call destructor
      for( int i = 0; i < mNumObjectsInMemory; i++ )
         destructInPlace<T>( &mMemory[i] );

      FrameAllocator::setWaterMark( mWaterMark );
   }

   /// NOTE: This will return the memory, NOT perform a ones-complement
   T* operator ~() { return mMemory; };
   /// NOTE: This will return the memory, NOT perform a ones-complement
   const T* operator ~() const { return mMemory; };

   /// NOTE: This will dereference the memory, NOT do standard unary plus behavior
   T& operator +() { return *mMemory; };
   /// NOTE: This will dereference the memory, NOT do standard unary plus behavior
   const T& operator +() const { return *mMemory; };

   T& operator *() { return *mMemory; };
   const T& operator *() const { return *mMemory; };

   T** operator &() { return &mMemory; };
   const T** operator &() const { return &mMemory; };

   operator T*() { return mMemory; }
   operator const T*() const { return mMemory; }

   operator T&() { return *mMemory; }
   operator const T&() const { return *mMemory; }

   operator T() { return *mMemory; }
   operator const T() const { return *mMemory; }
   
   T& operator []( U32 i ) { return mMemory[ i ]; }
   const T& operator []( U32 i ) const { return mMemory[ i ]; }

   T& operator []( S32 i ) { return mMemory[ i ]; }
   const T& operator []( S32 i ) const { return mMemory[ i ]; }

   /// @name Vector-like Interface
   /// @{
   T *address() const { return mMemory; }
   dsize_t size() const { return mNumObjectsInMemory; }
   /// @}
};

//-----------------------------------------------------------------------------
// FrameTemp specializations for types with no constructor/destructor
#define FRAME_TEMP_NC_SPEC(type) \
   template<> \
   inline FrameTemp<type>::FrameTemp( const U32 count ) \
   { \
      AssertFatal( count > 0, "Allocating a FrameTemp with less than one instance" ); \
      mWaterMark = FrameAllocator::getWaterMark(); \
      mMemory = reinterpret_cast<type *>( FrameAllocator::alloc( sizeof( type ) * count ) ); \
   } \
   template<>\
   inline FrameTemp<type>::~FrameTemp() \
   { \
      FrameAllocator::setWaterMark( mWaterMark ); \
   } \

FRAME_TEMP_NC_SPEC(char);
FRAME_TEMP_NC_SPEC(float);
FRAME_TEMP_NC_SPEC(double);
FRAME_TEMP_NC_SPEC(bool);
FRAME_TEMP_NC_SPEC(int);
FRAME_TEMP_NC_SPEC(short);

FRAME_TEMP_NC_SPEC(unsigned char);
FRAME_TEMP_NC_SPEC(unsigned int);
FRAME_TEMP_NC_SPEC(unsigned short);

#undef FRAME_TEMP_NC_SPEC

//-----------------------------------------------------------------------------

#endif  // _H_FRAMEALLOCATOR_
