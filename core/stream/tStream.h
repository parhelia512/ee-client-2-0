//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TSTREAM_H_
#define _TSTREAM_H_

#ifndef _PLATFORM_H_
#  include "platform/platform.h"
#endif
#ifndef _TYPETRAITS_H_
#  include "platform/typetraits.h"
#endif

//RDTODO: error handling


/// @file
/// Definitions for lightweight componentized streaming.
///
/// This file is an assembly of lightweight classes/interfaces that
/// describe various aspects of streaming classes.  The advantage
/// over using Torque's Stream class is that very little requirements
/// are placed on implementations, that specific abilities can be
/// mixed and matched very selectively, and that complex stream processing
/// chains can be hidden behind very simple stream interfaces.


///

struct StreamIOException
{
   //TODO
};

enum EAsyncIOStatus
{
   ASYNC_IO_Pending,
   ASYNC_IO_Complete,
   ASYNC_IO_Error
};

//-----------------------------------------------------------------------------
//    Several component interface.
//-----------------------------------------------------------------------------

/// Interface for streams with an explicit position property.

template< typename P = U32 >
class IPositionable
{
   public:
   
      typedef void Parent;

      /// The type used to indicate positions.
      typedef P PositionType;

      /// @return the current position.
      virtual PositionType getPosition() const = 0;
      
      /// Set the current position to be "pos".
      /// @param pos The new position.
      virtual void setPosition( PositionType pos ) = 0;
};

/// Interface for structures that allow their state to be reset.

class IResettable
{
   public:
   
      typedef void Parent;
   
      ///
      virtual void reset() = 0;
};

/// Interface for structures of finite size.

template< typename T = U32 >
class ISizeable
{
   public:
   
      typedef void Parent;

      /// The type used to indicate the structure's size.
      typedef T SizeType;

      /// @return the size of the structure in number of elements.
      virtual SizeType getSize() const = 0;
};

/// Interface for structures that represent processes.

class IProcess
{
   public:
   
      typedef void Parent;

      ///
      virtual void start() = 0;

      ///
      virtual void stop() = 0;

      ///
      virtual void pause() = 0;
};

///
class IPolled
{
   public:

      typedef void Parent;
   
      ///
      virtual bool update() = 0;
};


//-----------------------------------------------------------------------------
//    IInputStream.
//-----------------------------------------------------------------------------

/// An input stream delivers a sequence of elements of type "T".

template< typename T >
class IInputStream
{
   public:

      typedef void Parent;

      /// The element type of this input stream.
      typedef T ElementType;
      
      /// Read the next "num" elements into "buffer".
      ///
      /// @param buffer The buffer into which the elements are stored.
      /// @param num Number of elements to read.
      /// @return the number of elements actually read; this may be less than
      ///   "num" or even zero if no elements are available or reading failed.
      virtual U32 read( ElementType* buffer, U32 num ) = 0;
};

/// An input stream over elements of type "T" that reads from
/// user-specified explicit offsets.

template< typename T, typename Offset = U32 >
class IOffsetInputStream
{
   public:

      typedef void Parent;
      typedef Offset OffsetType;
      typedef T ElementType;

      ///
      virtual U32 readAt( OffsetType offset, T* buffer, U32 num ) = 0;
};

/// An input stream over elements of type "T" that works in
/// the background.

template< typename T, typename Offset = U32 >
class IAsyncInputStream
{
   public:

      typedef void Parent;
      typedef Offset OffsetType;
      typedef T ElementType;

      ///
      virtual void* issueReadAt( OffsetType offset, T* buffer, U32 num ) = 0;

      ///
      virtual EAsyncIOStatus tryCompleteReadAt( void* handle, U32& outNumRead, bool wait = false ) = 0;

      ///
      virtual void cancelReadAt( void* handle ) = 0;
};

//-----------------------------------------------------------------------------
//    IOutputStream.
//-----------------------------------------------------------------------------

/// An output stream that writes elements of type "T".

template< typename T >
class IOutputStream
{
   public:

      typedef void Parent;      
      typedef T ElementType;

      ///
      virtual void write( const ElementType* buffer, U32 num ) = 0;
};

/// An output stream that writes elements of type "T" to a
/// user-specified explicit offset.

template< typename T, typename Offset = U32 >
class IOffsetOutputStream
{
   public:

      typedef void Parent;
      typedef Offset OffsetType;
      typedef T ElementType;

      ///
      virtual void writeAt( OffsetType offset, const ElementType* buffer, U32 num ) = 0;
};

/// An output stream that writes elements of type "T" in the background.

template< typename T, typename Offset = U32 >
class IAsyncOutputStream
{
   public:

      typedef void Parent;
      typedef Offset OffsetType;
      typedef T ElementType;

      ///
      virtual void* issueWriteAt( OffsetType offset, const ElementType* buffer, U32 num ) = 0;

      ///
      virtual EAsyncIOStatus tryCompleteWriteAt( void* handle, bool wait = false ) = 0;

      ///
      virtual void cancelWriteAt( void* handle ) = 0;
};

//-----------------------------------------------------------------------------
//    IInputStreamFilter.
//-----------------------------------------------------------------------------

/// An input stream filter takes an input stream "Stream" and processes it
/// into an input stream over type "To".

template< typename To, typename Stream >
class IInputStreamFilter : public IInputStream< To >
{
   public:

      typedef IInputStream< To > Parent;

      ///
      typedef typename TypeTraits< Stream >::BaseType SourceStreamType;

      /// The element type of the source stream.
      typedef typename SourceStreamType::ElementType SourceElementType;

      ///
      IInputStreamFilter( const Stream& stream )
         : mSourceStream( stream ) {}

      ///
      const Stream& getSourceStream() const { return mSourceStream; }
      Stream& getSourceStream() { return mSourceStream; }

   private:

      Stream mSourceStream;
};

//-----------------------------------------------------------------------------
//    IOutputStreamFilter.
//-----------------------------------------------------------------------------

/// An output stream filter takes an output stream "Stream" and processes it
/// into an output stream over type "To".

template< typename To, class Stream >
class IOutputStreamFilter : public IOutputStream< To >
{
   public:

      typedef IOutputStream< To > Parent;

      ///
      typedef typename TypeTraits< Stream >::BaseType TargetStreamType;

      /// The element type of the target stream.
      typedef typename TargetStreamType::ElementType TargetElementType;

      ///
      IOutputStreamFilter( const Stream& stream )
         : mTargetStream( stream ) {}

      ///
      const Stream& getTargetStream() const { return mTargetStream; }
      Stream& getTargetStream() { return mTargetStream; }

   private:

      Stream mTargetStream;
};

#endif // _TSTREAM_H_
