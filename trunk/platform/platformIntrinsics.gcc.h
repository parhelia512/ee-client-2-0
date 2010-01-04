//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TORQUE_PLATFORM_PLATFORMINTRINSICS_GCC_H_
#define _TORQUE_PLATFORM_PLATFORMINTRINSICS_GCC_H_

/// @file
/// Compiler intrinsics for GCC.

#ifdef TORQUE_OS_MAC
#include <libkern/OSAtomic.h>
#elif defined(TORQUE_OS_PS3)
#include <cell/atomic.h>
#endif

// Fetch-And-Add

inline void dFetchAndAdd( volatile U32& ref, U32 val )
{
   #if defined(TORQUE_OS_PS3)
      cellAtomicAdd32( (std::uint32_t *)&ref, val );
   #elif !defined(TORQUE_OS_MAC)
      __sync_fetch_and_add( ( volatile long* ) &ref, val );
   #else
      OSAtomicAdd32( val, (int32_t* ) &ref);
   #endif
}
inline void dFetchAndAdd( volatile S32& ref, S32 val )
{
   #if defined(TORQUE_OS_PS3)
      cellAtomicAdd32( (std::uint32_t *)&ref, val );
   #elif !defined(TORQUE_OS_MAC)
      __sync_fetch_and_add( ( volatile long* ) &ref, val );
   #else
      OSAtomicAdd32( val, (int32_t* ) &ref);
   #endif
}

// Compare-And-Swap

inline bool dCompareAndSwap( volatile U32& ref, U32 oldVal, U32 newVal )
{
   // bool
   //OSAtomicCompareAndSwap32(int32_t oldValue, int32_t newValue, volatile int32_t *theValue);
   #if defined(TORQUE_OS_PS3)
      return ( cellAtomicCompareAndSwap32( (std::uint32_t *)&ref, newVal, oldVal ) == oldVal );
   #elif !defined(TORQUE_OS_MAC)
      return ( __sync_val_compare_and_swap( ( volatile long* ) &ref, oldVal, newVal ) == oldVal );
   #else
      return OSAtomicCompareAndSwap32(oldVal, newVal, (int32_t *) &ref);
   #endif
}
inline bool dCompareAndSwap( volatile U64& ref, U64 oldVal, U64 newVal )
{
   #if defined(TORQUE_OS_PS3)
      return ( cellAtomicCompareAndSwap32( (std::uint32_t *)&ref, newVal, oldVal ) == oldVal );
   #elif !defined(TORQUE_OS_MAC)
      return ( __sync_val_compare_and_swap( ( volatile long long* ) &ref, oldVal, newVal ) == oldVal );
   #else
      return OSAtomicCompareAndSwap64(oldVal, newVal, (int64_t *) &ref);
   #endif

}

#endif // _TORQUE_PLATFORM_PLATFORMINTRINSICS_GCC_H_
