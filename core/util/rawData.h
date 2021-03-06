//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _RAWDATA_H_
#define _RAWDATA_H_

#ifndef _PLATFORM_H_
#  include "platform/platform.h"
#endif
#ifndef _TYPETRAITS_H_
#  include "platform/typetraits.h"
#endif


template< typename T >
class RawDataT
{
   public:

      typedef void Parent;
      typedef RawDataT< T > ThisType;

      /// Type of elements in the buffer.
      typedef T ValueType;

      /// If true, the structure own the data buffer and will
      /// delete[] it on destruction.
      bool ownMemory;

      /// The data buffer.
      T *data;

      /// Number of elements in the buffer.
      U32 size;

      RawDataT()
         : ownMemory(false), data(NULL), size(0)
      {
      }

      RawDataT( T* data, U32 size, bool ownMemory = false )
         : data( data ), size( size ), ownMemory( ownMemory ) {}

      RawDataT(const ThisType& rd)
      {
         data = rd.data;
         size = rd.size;
         ownMemory = false;
      }

      ~RawDataT() 
      {
         reset();
      }

      void reset()
      {
         if (ownMemory)
            delete [] data;

         data      = NULL;
         ownMemory = false;
         size      = 0;
      }

      void alloc(const U32 newSize)
      {
         reset();

         ownMemory = true;
         size = newSize;
         data = new ValueType[newSize];
      }

      void operator =(const ThisType& rd)
      {
         data = rd.data;
         size = rd.size;
         ownMemory = false;
      }

      /// Allocate a RawDataT instance inline with its data elements.
      ///
      /// @param Self RawDataT instance type; this is a type parameter so this
      ///   can work with types derived from RawDataT.
      template< class Self >
      static Self* allocInline( U32 numElements TORQUE_TMM_ARGS_DECL )
      {
         const char* file = __FILE__;
         U32 line = __LINE__;
#ifndef TORQUE_DISABLE_MEMORY_MANAGER
         file = fileName;
         line = lineNum;
#endif
         Self* inst = ( Self* ) dMalloc_r( sizeof( Self ) + numElements * sizeof( ValueType ), file, line );
         ValueType* data = ( ValueType* ) ( inst + 1 );
         constructArray< ValueType >( data, numElements );
         return constructInPlace< Self >( inst, data, numElements );
      }
};

template< typename T >
struct TypeTraits< RawDataT< T >* > : public TypeTraits< typename RawDataT< T >::Parent* >
{
   struct Construct
   {
      template< typename R >
      static R* single( U32 size )
      {
         typedef typename TypeTraits< R >::BaseType Type;
         return Type::template allocInline< Type >( size TORQUE_TMM_LOC );
      }
   };
   struct Destruct
   {
      template< typename R >
      static void single( R* ptr )
      {
         destructInPlace( ptr );
         dFree( ptr );
      }
   };
};

/// Raw byte buffer.
/// This isn't a typedef to allow forward declarations.
class RawData : public RawDataT< S8 >
{
   public:

      typedef RawDataT< S8 > Parent;

      RawData() {}
      RawData( S8* data, U32 size, bool ownMemory = false )
         : Parent( data, size, ownMemory ) {}
};

#endif // _RAWDATA_H_
