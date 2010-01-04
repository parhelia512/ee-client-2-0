//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include <stdarg.h>
#include <stdio.h>

#include "platform/platform.h"

// Sigh... guess what compiler needs this...
namespace DictHash { U32 hash( String::StringData* ); }
namespace KeyCmp
{
   template< typename Key > bool equals( const Key&, const Key& );
   template<> bool equals<>( String::StringData* const&, String::StringData* const& );
}

#include "core/util/str.h"
#include "core/util/tDictionary.h"
#include "core/strings/stringFunctions.h"
#include "core/strings/unicode.h"
#include "core/util/hashFunction.h"
#include "core/util/autoPtr.h"
#include "core/util/tVector.h"
#include "core/dataChunker.h"
#include "console/console.h"

#include "math/mMathFn.h"

#include "platform/platform.h"
#include "platform/profiler.h"
#include "platform/platformIntrinsics.h"
#include "platform/threads/mutex.h"

#ifndef TORQUE_DISABLE_MEMORY_MANAGER
#  undef new
#else
#  define _new new
#endif

const String::SizeType String::NPos = U32(~0);
const String String::EmptyString;

/// A delete policy for the AutoPtr class
struct DeleteString
{
   template<class T>
   static void destroy(T *ptr) { dFree(ptr); }
};


//-----------------------------------------------------------------------------

/// Search for a character.
/// Search for the position of the needle in the haystack.
/// Default mode is StrCase | StrLeft, mode also accepts StrNoCase and StrRight.
/// If pos is non-zero, then in mode StrLeft the search starts at (hay + pos) and
/// in mode StrRight the search starts at (hay + pos - 1)
/// @return Returns a pointer to the location of the character in the haystack or 0
static const char* StrFind(const char* hay, char needle, S32 pos, U32 mode)
{
   if (mode & String::Right)
   {
      // Go to the end first, then search backwards
      const char  *he = hay;

      if (pos)
      {
         he += pos - 1;
      }
      else
      {
         while (*he)
            he++;
      }

      if (mode & String::NoCase)
      {
         needle = dTolower(needle);

         for (; he >= hay; he--)
         {
            if (dTolower(*he) == needle)
               return he;
         }
      }
      else
      {
         for (; he >= hay; he--)
         {
            if (*he == needle)
               return he;
         }
      }
      return 0;
   }
   else
   {
      if (mode & String::NoCase)
      {
         needle = dTolower(needle);
         for (hay += pos; *hay && dTolower(*hay) != needle;)
            hay++;
      }
      else
      {
         for (hay += pos; *hay && *hay != needle;)
            hay++;
      }

      return *hay ? hay : 0;
   }
}

/// Search for a StringData.
/// Search for the position of the needle in the haystack.
/// Default mode is StrCase | StrLeft, mode also accepts StrNoCase and StrRight.
/// If pos is non-zero, then in mode StrLeft the search starts at (hay + pos) and
/// in mode StrRight the search starts at (hay + pos - 1)
/// @return Returns a pointer to the StringData in the haystack or 0
static const char* StrFind(const char* hay, const char* needle, S32 pos, U32 mode)
{
   if (mode & String::Right)
   {
      const char  *he = hay;

      if (pos)
      {
         he += pos - 1;
      }
      else
      {
         while (*he)
            he++;
      }

      if (mode & String::NoCase)
      {
         AutoPtr<char,DeleteString> ln(dStrlwr(dStrdup(needle)));
         for (; he >= hay; he--)
         {
            if (dTolower(*he) == *ln)
            {
               U32 i = 0;
               while (ln[i] && ln[i] == dTolower(he[i]))
                  i++;
               if (!ln[i])
                  return he;
               if (!hay[i])
                  return 0;
            }
         }
      }
      else
      {
         for (; he >= hay; he--)
         {
            if (*he == *needle)
            {
               U32 i = 0;
               while (needle[i] && needle[i] == he[i])
                  i++;
               if (!needle[i])
                  return he;
               if (!hay[i])
                  return 0;
            }
         }
      }
      return 0;
   }
   else
   {
      if (mode & String::NoCase)
      {
         AutoPtr<char,DeleteString> ln(dStrlwr(dStrdup(needle)));
         for (hay += pos; *hay; hay++)
         {
            if (dTolower(*hay) == *ln)
            {
               U32 i = 0;
               while (ln[i] && ln[i] == dTolower(hay[i]))
                  i++;
               if (!ln[i])
                  return hay;
               if (!hay[i])
                  return 0;
            }
         }
      }
      else
      {
         for (hay += pos; *hay; hay++)
         {
            if (*hay == *needle)
            {
               U32 i = 0;
               while (needle[i] && needle[i] == hay[i])
                  i++;
               if (!needle[i])
                  return hay;
               if (!hay[i])
                  return 0;
            }
         }
      }
   }

   return 0;
}

//-----------------------------------------------------------------------------

///
class String::StringData
{
   protected:

#ifdef TORQUE_DEBUG
      StringChar*       mString;       ///< so we can inspect data in a debugger
#endif

      U32               mRefCount;     ///< String reference count; string is not refcounted if this is U32_MAX (necessary for thread-safety of interned strings and the empty string).
      U32               mLength;       ///< String length in bytes excluding null.
      mutable U32       mNumChars;     ///< Character count; varies from byte count for strings with multi-bytes characters.
      mutable U32       mHashCase;     ///< case-sensitive hash
      mutable U32       mHashNoCase;   ///< case-insensitive hash
      mutable UTF16*    mUTF16;
      bool              mIsInterned;   ///< If true, this string is interned in the string table.
      StringChar        mData[1];      ///< Start of string data

      /// This constructor is only used for constructing the empty string.
      StringData() : mRefCount(U32_MAX), mLength(0), mNumChars(0), 
         mHashCase(U32_MAX), mHashNoCase(U32_MAX), mUTF16(new UTF16[1]),
         mData(), mIsInterned( false )
      {
         // operator new() will never call this constructor which is why we
         // initialized mLength in the initializer list.
         mData[0] = '\0';
         mUTF16[ 0 ] = '\0';
         
#ifdef TORQUE_DEBUG
         mString = &mData[0];
#endif
      }

   public:

      enum { MAX_HASH_LENGTH = 64 };

      ///
      StringData( const StringChar* data, bool interned = false ) : mRefCount(1), mNumChars(U32_MAX),
         mHashCase(U32_MAX), mHashNoCase(U32_MAX), mUTF16(NULL), mIsInterned( interned )
      {
         // mLength is initialized by operator new()

         if( data )
         {
            dMemcpy( mData, data, sizeof( StringChar ) * mLength );
            mData[ mLength ] = '\0';
         }
         
#ifdef TORQUE_DEBUG
         mString = &mData[0];
#endif
         if( mIsInterned )
            mRefCount = U32_MAX;
      }

      ~StringData()
      {
         if( mUTF16 )
            delete [] mUTF16;
      }

      void* operator new(size_t size, U32 len);
      void* operator new( size_t size, U32 len, DataChunker& chunker );
      void operator delete(void *);

      bool isShared() const
      {
         return ( mRefCount > 1 );
      }
      void addRef()
      {
         if( mRefCount != U32_MAX )
            mRefCount ++;
      }
      void release()
      {
         if( mRefCount != U32_MAX )
         {
            -- mRefCount;
            if( !mRefCount )
               delete this;
         }
      }
      U32 getLength() const
      {
         return mLength;
      }
      U32 getDataSize() const
      {
         return ( mLength + 1 );
      }
      U32 getDataSizeUTF16() const
      {
         return ( mLength * sizeof( UTF16 ) );
      }
      UTF8 operator []( U32 index ) const
      {
         AssertFatal( index < mLength, "String::StringData::operator []() - index out of range" );
         return mData[ index ];
      }
      UTF8* utf8()
      {
         return mData;
      }
      const UTF8* utf8() const
      {
         return mData;
      }
      UTF16* utf16() const
      {
         // IMPORTANT: if interned strings are added, setting mUTF16 must
         //   be done atomatically and the converted string freed if the CAS fails.

         if( !mUTF16 )
            mUTF16 = convertUTF8toUTF16( mData );
         return mUTF16;
      }
      U32 getHashCase() const
      {
         return mHashCase;
      }
      U32 getOrCreateHashCase() const
      {
         if( mHashCase == U32_MAX )
            mHashCase = Torque::hash((const U8 *)(mData), getMin( mLength, ( U32 ) MAX_HASH_LENGTH ), 0);
         return mHashCase;
      }
      U32 getHashNoCase() const
      {
         return mHashNoCase;
      }
      U32 getOrCreateHashNoCase() const
      {
         if( mHashNoCase == U32_MAX)
         {
            PROFILE_SCOPE(String_getHashCaseInsensitive);

            StringChar sLowerBuffer[ MAX_HASH_LENGTH ];
            U32 len = getMin( mLength, ( U32 ) MAX_HASH_LENGTH - 1 );

            dMemcpy(sLowerBuffer, utf8(), len );
            sLowerBuffer[ len ] = '\0';
            dStrlwr(sLowerBuffer);

            mHashNoCase = Torque::hash((const U8 *)(sLowerBuffer), len, 0);
         }

         return mHashNoCase;
      }
      U32 getNumChars() const
      {
         //TODO
         AssertFatal( false, "TODO" );

         return mNumChars;
      }
      bool isInterned() const
      {
         return mIsInterned;
      }
      static StringData* Empty()
      {
         static StringData empty;
         return &empty;
      }
};

//-----------------------------------------------------------------------------

namespace DictHash
{
   inline U32 hash( String::StringData* data )
   {
      return data->getOrCreateHashCase();
   }
}
namespace KeyCmp
{
   template<>
   inline bool equals<>( String::StringData* const& d1, String::StringData* const& d2 )
   {
      return ( dStrcmp( d1->utf8(), d2->utf8() ) == 0 );
   }
}

/// Type for the intern string table.  We don't want String instances directly
/// on the table so that destructors don't run when the table is destroyed.  This
/// is because we really shouldn't depend on dtor ordering within this file and thus
/// we can't tell whether the intern string memory is freed before or after the
/// table is destroyed.
typedef HashTable< String::StringData*, String::StringData* > StringTable;

static Mutex sStringTableLock;
static DataChunker sStringTableChunker;
static StringTable sStringTable;

//-----------------------------------------------------------------------------

#ifdef TORQUE_DEBUG

/// Tracks the number of bytes allocated for strings.
/// @bug This currently does not include UTF16 allocations.
static U32 sgStringMemBytes;

/// Tracks the number of Strings which are currently instantiated.
static U32 sgStringInstances;

ConsoleFunction( dumpStringMemStats, void, 1, 1, "() - Dumps information about String memory usage" )
{
   Con::printf( "String Data: %i instances, %i bytes", sgStringInstances, sgStringMemBytes );
}

#endif

//-----------------------------------------------------------------------------

void* String::StringData::operator new( size_t size, U32 len )
{
   AssertFatal( len != 0, "String::StringData::operator new() - string must not be empty" );
   StringData *str = reinterpret_cast<StringData*>( dMalloc( size + len * sizeof(StringChar) ) );

   str->mLength      = len;

#ifdef TORQUE_DEBUG
   dFetchAndAdd( sgStringMemBytes, size + len * sizeof(StringChar) );
   dFetchAndAdd( sgStringInstances, 1 );
#endif

   return str;
}

void String::StringData::operator delete(void *ptr)
{
   StringData* sub = static_cast<StringData *>(ptr);
   AssertFatal( sub->mRefCount == 0, "StringData::delete() - invalid refcount" );

#ifdef TORQUE_DEBUG
   dFetchAndAdd( sgStringMemBytes, U32( -( S32( sizeof( StringData ) + sub->mLength * sizeof(StringChar) ) ) ) );
   dFetchAndAdd( sgStringInstances, U32( -1 ) );
#endif

   dFree( ptr );
}

void* String::StringData::operator new( size_t size, U32 len, DataChunker& chunker )
{
   AssertFatal( len != 0, "String::StringData::operator new() - string must not be empty" );
   StringData *str = reinterpret_cast<StringData*>( chunker.alloc( size + len * sizeof(StringChar) ) );

   str->mLength      = len;

#ifdef TORQUE_DEBUG
   dFetchAndAdd( sgStringMemBytes, size + len * sizeof(StringChar) );
   dFetchAndAdd( sgStringInstances, 1 );
#endif

   return str;
}

//-----------------------------------------------------------------------------

String::String()
{
   PROFILE_SCOPE(String_default_constructor);
   _string = StringData::Empty();
}

String::String(const String &str)
{
   PROFILE_SCOPE(String_String_constructor);
   _string = str._string;
   _string->addRef();
}

String::String(const StringChar *str)
{
   PROFILE_SCOPE(String_char_constructor);
   if( str && *str )
   {
      U32 len = dStrlen(str);
      _string = new ( len ) StringData( str );
   }
   else
      _string = StringData::Empty();
}

String::String(const StringChar *str, SizeType len)
{
   PROFILE_SCOPE(String_char_len_constructor);
   if (str && *str && len!=0)
   {
      AssertFatal(len<=dStrlen(str), "String::String: string too short");
      _string = new ( len ) StringData( str );
   }
   else
      _string = StringData::Empty();
}

String::String(const UTF16 *str)
{
   PROFILE_SCOPE(String_UTF16_constructor);

   if( str && str[ 0 ] )
   {
      UTF8* utf8 = convertUTF16toUTF8( str );
      U32 len = dStrlen( utf8 );
      _string = new ( len ) StringData( utf8 );
      delete [] utf8;
   }
   else
      _string = StringData::Empty();
}

String::~String()
{
   _string->release();
}

//-----------------------------------------------------------------------------

String String::intern() const
{
   if( isInterned() )
      return *this;
      
   // Lock the string table.
   
   MutexHandle mutex;
   mutex.lock( &sStringTableLock );
   
   // Lookup.
   
   StringTable::Iterator iter = sStringTable.find( _string );
   if( iter != sStringTable.end() )
      return ( *iter ).value;
      
   // Create new.
   
   StringData* data = new ( length(), sStringTableChunker ) StringData( c_str(), true );
   iter = sStringTable.insertUnique( data, data );
   
   return ( *iter ).value;
}

//-----------------------------------------------------------------------------

const StringChar* String::c_str() const
{
   return _string->utf8();
}

const UTF16 *String::utf16() const
{
   return _string->utf16();
}

String::SizeType String::length() const
{
   return _string->getLength();
}

String::SizeType String::size() const
{
   return _string->getDataSize();
}

String::SizeType String::numChars() const
{
   return _string->getNumChars();
}

bool String::isEmpty() const
{
   return ( _string == StringData::Empty() );
}

bool String::isShared() const
{
   return _string->isShared();
}

bool String::isSame( const String& str ) const
{
   return ( _string == str._string );
}

bool String::isInterned() const
{
   return ( _string->isInterned() );
}

U32 String::getHashCaseSensitive() const
{
   return _string->getOrCreateHashCase();
}

U32 String::getHashCaseInsensitive() const
{
   return _string->getOrCreateHashNoCase();
}

//-----------------------------------------------------------------------------

String::SizeType String::find(const String &str, SizeType pos, U32 mode) const
{
   return find(str._string->utf8(), pos, mode);
}

String& String::insert(SizeType pos, const String &str)
{
   return insert(pos, str._string->utf8());
}

String& String::replace(SizeType pos, SizeType len, const String &str)
{
   return replace(pos, len, str._string->utf8());
}

//-----------------------------------------------------------------------------

String& String::operator=(StringChar c)
{
   _string->release();

   _string = new ( 2 ) StringData( 0 );
   _string->utf8()[ 0 ] = c;
   _string->utf8()[ 1 ] = '\0';

   return *this;
}

String& String::operator+=(StringChar c)
{
   // Append the given string into a new string
   U32 len = _string->getLength();
   StringData* sub = new ( len + 1 ) StringData( NULL );

   copy( sub->utf8(), _string->utf8(), len );
   sub->utf8()[len] = c;
   sub->utf8()[len+1] = 0;

   _string->release();
   _string = sub;

   return *this;
}

//-----------------------------------------------------------------------------

String& String::operator=(const StringChar *str)
{
   // Protect against self assignment which is not only a
   // waste of time, but can also lead to the string being
   // freed before it can be reassigned.
   if ( _string->utf8() == str )
      return *this;

   _string->release();

   if (str && *str)
   {
      U32 len = dStrlen(str);
      _string = new ( len ) StringData( str );
   }
   else
      _string = StringData::Empty();

   return *this;
}

String& String::operator=(const String &src)
{
   // Inc src first to avoid assignment to self problems.
   src._string->addRef();

   _string->release();
   _string = src._string;

   return *this;
}

String& String::operator+=(const StringChar *src)
{
   if( src == NULL && !*src )
      return *this;

   // Append the given string into a new string
   U32 lena = _string->getLength();
   U32 lenb = dStrlen(src);
   U32 newlen = lena + lenb;

   StringData* sub;
   if( !newlen )
      sub = StringData::Empty();
   else
   {
      sub = new ( newlen ) StringData( NULL );

      copy(sub->utf8(),_string->utf8(),lena);
      copy(sub->utf8() + lena,src,lenb + 1);
   }

   _string->release();
   _string = sub;

   return *this;
}

String& String::operator+=(const String &src)
{
   if( src.isEmpty() )
      return *this;

   // Append the given string into a new string
   U32 lena = _string->getLength();
   U32 lenb = src._string->getLength();
   U32 newlen = lena + lenb;

   StringData* sub;
   if( !newlen )
      sub = StringData::Empty();
   else
   {
      sub = new ( newlen ) StringData( NULL );

      copy(sub->utf8(),_string->utf8(),lena);
      copy(sub->utf8() + lena,src._string->utf8(),lenb + 1);
   }

   _string->release();
   _string = sub;

   return *this;
}

//-----------------------------------------------------------------------------

String operator+(const String &a, const String &b)
{
   if( a.isEmpty() )
      return b;
   else if( b.isEmpty() )
      return a;

   U32 lena = a.length();
   U32 lenb = b.length();

   String::StringData *sub = new ( lena + lenb ) String::StringData( NULL );

   String::copy(sub->utf8(),a._string->utf8(),lena);
   String::copy(sub->utf8() + lena,b._string->utf8(),lenb + 1);

   return String(sub);
}

String operator+(const String &a, StringChar c)
{
   U32 lena = a.length();
   String::StringData *sub = new ( lena + 1 ) String::StringData( NULL );

   String::copy(sub->utf8(),a._string->utf8(),lena);

   sub->utf8()[lena] = c;
   sub->utf8()[lena+1] = 0;

   return String(sub);
}

String operator+(StringChar c, const String &a)
{
   U32 lena = a.length();
   String::StringData *sub = new ( lena + 1 ) String::StringData( NULL );

   String::copy(sub->utf8() + 1,a._string->utf8(),lena + 1);
   sub->utf8()[0] = c;

   return String(sub);
}

String operator+(const String &a, const StringChar *b)
{
   AssertFatal(b,"String:: Invalid null ptr argument");

   if( a.isEmpty() )
      return String( b );

   U32 lena = a.length();
   U32 lenb = dStrlen(b);

   if( !lenb )
      return a;

   String::StringData *sub = new ( lena + lenb ) String::StringData( NULL );

   String::copy(sub->utf8(),a._string->utf8(),lena);
   String::copy(sub->utf8() + lena,b,lenb + 1);

   return String(sub);
}

String operator+(const StringChar *a, const String &b)
{
   AssertFatal(a,"String:: Invalid null ptr argument");

   if( b.isEmpty() )
      return String( a );

   U32 lena = dStrlen(a);
   if( !lena )
      return b;

   U32 lenb = b.length();

   String::StringData* sub = new ( lena + lenb ) String::StringData( NULL );

   String::copy(sub->utf8(),a,lena);
   String::copy(sub->utf8() + lena,b._string->utf8(),lenb + 1);

   return String(sub);
}

bool String::operator==(const String &str) const
{
   if( str._string == _string )
      return true;
   else if( str._string->isInterned() && _string->isInterned() )
      return false;
   else if( str.length() != length() )
      return false;
   else if( str._string->getHashCase() != U32_MAX
            && _string->getHashCase() != U32_MAX
            && str._string->getHashCase() != _string->getHashCase() )
      return false;
   else
      return ( dMemcmp( str._string->utf8(), _string->utf8(), _string->getLength() ) == 0 );
}

bool String::operator<(const String &str) const
{
   return ( dStrnatcmp( _string->utf8(), str._string->utf8() ) < 0 );
}

bool String::operator>(const String &str) const
{
   return ( dStrnatcmp( _string->utf8(), str._string->utf8() ) > 0 );
}

bool String::operator<=(const String &str) const
{
   return ( dStrnatcmp( _string->utf8(), str._string->utf8() ) <= 0 );
}

bool String::operator>=(const String &str) const
{
   return ( dStrnatcmp( _string->utf8(), str._string->utf8() ) >= 0 );
}

//-----------------------------------------------------------------------------
// Base functions for string comparison

S32 String::compare(const StringChar *str, SizeType len, U32 mode) const
{
   AssertFatal(str,"String:: Invalid null ptr argument");

   const StringChar  *p1 = _string->utf8();
   const StringChar  *p2 = str;

   if (p1 == p2)
      return 0;

   if( mode & String::Right )
   {
      U32 n = len;
      if( n > length() )
         n = length();

      p1 += length() - n;
      p2 += dStrlen( str ) - n;
   }

   if (mode & String::NoCase)
   {
      if (len)
      {
         for (;--len; p1++,p2++)
         {
            if (dTolower(*p1) != dTolower(*p2) || !*p1)
               break;
         }
      }
      else
      {
         while (dTolower(*p1) == dTolower(*p2) && *p1)
         {
            p1++;
            p2++;
         }
      }

      return dTolower(*p1) - dTolower(*p2);
   }

   if (len)
      return dMemcmp(p1,p2,len);

   while (*p1 == *p2 && *p1)
   {
      p1++;
      p2++;
   }

   return *p1 - *p2;
}

S32 String::compare(const String &str, SizeType len, U32 mode) const
{
   if ( str._string == _string )
      return 0;

   return compare( str.c_str(), len, mode );
}

bool String::equal(const String &str, U32 mode) const
{
   if( !mode )
      return ( *this == str );
   else
   {
      if( _string == str._string )
         return true;
      else if( _string->isInterned() && str._string->isInterned() )
         return false;
      else if( length() != str.length() )
         return false;
      else if( _string->getHashNoCase() != U32_MAX
               && str._string->getHashNoCase() != U32_MAX
               && _string->getHashNoCase() != str._string->getHashNoCase() )
         return false;
      else
         return ( compare( str.c_str(), length(), mode ) == 0 );
   }
}

//-----------------------------------------------------------------------------

String::SizeType String::find(StringChar c, SizeType pos, U32 mode) const
{
   const StringChar* ptr = StrFind(_string->utf8(),c,pos,mode);

   return ptr? SizeType(ptr - _string->utf8()): NPos;
}

String::SizeType String::find(const StringChar *str, SizeType pos, U32 mode)  const
{
   AssertFatal(str,"String:: Invalid null ptr argument");

   const StringChar* ptr = StrFind(_string->utf8(),str,pos,mode);

   return ptr? SizeType(ptr - _string->utf8()): NPos;
}


//-----------------------------------------------------------------------------

String& String::insert(SizeType pos, const StringChar *str)
{
   AssertFatal(str,"String:: Invalid null ptr argument");

   return insert(pos,str,dStrlen(str));
}

///@todo review for error checking
String& String::insert(SizeType pos, const StringChar *str, SizeType len)
{
   if( !len )
      return *this;

   AssertFatal( str, "String:: Invalid null ptr argument" );
         
   SizeType lena = length();
   AssertFatal((pos <= lena),"Calling String::insert with position greater than length");
   U32 newlen = lena + len;

   StringData *sub;
   if( !newlen )
      sub = StringData::Empty();
   else
   {
      sub = new ( newlen ) StringData( NULL );

      String::copy(sub->utf8(),_string->utf8(),pos);
      String::copy(sub->utf8() + pos,str,len);
      String::copy(sub->utf8() + pos + len,_string->utf8() + pos,lena - pos + 1);
   }

   _string->release();
   _string = sub;

   return *this;
}

String& String::erase(SizeType pos, SizeType len)
{
   AssertFatal( len != 0, "String::erase() - Calling String::erase with 0 length" );
   AssertFatal( ( pos + len ) <= length(), "String::erase() - Invalid string region" );

   if( !len )
      return *this;

   SizeType slen = length();
   U32 newlen = slen - len;

   StringData *sub;
   if( !newlen )
      sub = StringData::Empty();
   else
   {
      sub = new ( newlen ) StringData( NULL );

      if (pos > 0)
         String::copy(sub->utf8(),_string->utf8(),pos);

      String::copy(sub->utf8() + pos, _string->utf8() + pos + len, slen - (pos + len) + 1);
   }

   _string->release();
   _string = sub;

   return *this;
}

///@todo review for error checking
String& String::replace(SizeType pos, SizeType len, const StringChar *str)
{
   AssertFatal( str, "String::replace() - Invalid null ptr argument" );
   AssertFatal( len != 0, "String::replace() - Zero length" );
   AssertFatal( ( pos + len ) <= length(), "String::replace() - Invalid string region" );

   SizeType slen = length();
   SizeType rlen = dStrlen(str);

   U32 newlen = slen - len + rlen;
   StringData *sub;
   if( !newlen )
      sub = StringData::Empty();
   else
   {
      sub = new ( newlen ) StringData( NULL );

      String::copy(sub->utf8(),_string->utf8(), pos);
      String::copy(sub->utf8() + pos,str,rlen);
      String::copy(sub->utf8() + pos + rlen,_string->utf8() + pos + len,slen - pos - len + 1);
   }

   _string->release();
   _string = sub;

   return *this;
}

String &String::replace( StringChar c1, StringChar c2 )
{
   if( isEmpty() )
      return *this;

   StringData *sub = new ( length() ) StringData( _string->utf8() );

   StringChar  *c = sub->utf8();

   while ( *c )
   {
      if ( *c == c1 )
         *c = c2;

      c++;
   }
 
   _string->release();
   _string = sub;

   return *this;
}

String &String::replace(const String &s1, const String &s2)
{
   // Find number of occurrences of s1 and
   // Calculate length of the new string...

   const U32 &s1len = s1.length();
   const U32 &s2len = s2.length();

   U32 pos = 0;
   Vector<U32> indices;
   StringChar *walk = _string->utf8();

   while ( walk )
   {
      // Casting away the const... was there a better way?
      walk = (StringChar*)StrFind( _string->utf8(), s1.c_str(), pos, Case|Left );
      if ( walk )
      {
         pos = SizeType(walk - _string->utf8());
         indices.push_back( pos );
         pos += s1len;
      }
   }

   // Early-out, no StringDatas found.
   if ( indices.size() == 0 )
      return *this;

   U32 newSize = size() - ( indices.size() * s1len ) + ( indices.size() * s2len );
   StringData *sub;
   if( newSize == 1 )
      sub = StringData::Empty();
   else
   {
      sub = new (newSize - 1 ) StringData( NULL );

      // Now assemble the new string from the pieces of the old...

      // Index into the old string
      pos = 0;
      // Index into the new string
      U32 newPos = 0;
      // Used to store a character count to be memcpy'd
      U32 copyCharCount = 0;

      for ( U32 i = 0; i < indices.size(); i++ )
      {
         const U32 &index = indices[i];

         // Number of chars (if any) before the next indexed StringData
         copyCharCount = index - pos;

         // Copy chars before the StringData if we have any.
         if ( copyCharCount > 0 )
         {
            dMemcpy( sub->utf8() + newPos, _string->utf8() + pos, copyCharCount * sizeof(StringChar) );
            newPos += copyCharCount;
         }

         // Copy over the replacement string.
         if ( s2len > 0 )
            dMemcpy( sub->utf8() + newPos, s2._string->utf8(), s2len * sizeof(StringChar) );

         newPos += s2len;
         pos = index + s1len;
      }

      // There could be characters left in the original string after the last
      // StringData occurrence, which we need to copy now - outside the loop.
      copyCharCount = length() - indices.last() - s1len;
      if ( copyCharCount != 0 )
         dMemcpy( sub->utf8() + newPos, _string->utf8() + pos, copyCharCount * sizeof(StringChar) );

      // Null terminate it!
      sub->utf8()[newSize-1] = 0;
   }

   _string->release();
   _string = sub;

   return *this;
}

//-----------------------------------------------------------------------------

String String::substr(SizeType pos, SizeType len) const
{
   AssertFatal( pos <= length(), "String::substr - Invalid position!" );

   if ( len == -1 )
      len = length() - pos;

   AssertFatal( len + pos <= length(), "String::substr - Invalid length!" );

   StringData* sub;
   if( !len )
      sub = StringData::Empty();
   else
      sub = new ( len ) StringData( _string->utf8() + pos );

   return sub;
}

//-----------------------------------------------------------------------------

String String::trim() const
{
   if( isEmpty() )
      return *this;
   
   const StringChar* start = _string->utf8();
   while( *start && dIsspace( *start ) )
      start ++;
   
   const StringChar* end = _string->utf8() + length() - 1;
   while( end > start && dIsspace( *end ) )
      end --;
   end ++;
   
   const U32 len = end - start;
   if( len == length() )
      return *this;
   
   StringData* sub;
   if( !len )
      sub = StringData::Empty();
   else
      sub = new ( len ) StringData( start );

   return sub;
}

//-----------------------------------------------------------------------------

void String::copy(StringChar* dst, const StringChar *src, U32 len)
{
   dMemcpy(dst, src, len * sizeof(StringChar));
}

//-----------------------------------------------------------------------------

#if defined(TORQUE_OS_WIN32) || defined(TORQUE_OS_XBOX) || defined(TORQUE_OS_XENON)
// This standard function is not defined when compiling with VC7...
#define vsnprintf	_vsnprintf
#endif

String::StrFormat::~StrFormat()
{
   if( _dynamicBuffer )
      dFree( _dynamicBuffer );
}

S32 String::StrFormat::format( const char *format, void *args )
{
   _len=0;
   return formatAppend(format,args);
}

S32 String::StrFormat::formatAppend( const char *format, void *args )
{
   // Format into the fixed buffer first.
   S32 startLen = _len;
   if (_dynamicBuffer == NULL)
   {
      _len += vsnprintf(_fixedBuffer + _len, sizeof(_fixedBuffer) - _len, format, *(va_list*)args);
      if (_len >= 0 && _len < sizeof(_fixedBuffer))
         return _len;

      // Start off the dynamic buffer at twice fixed buffer size
      _len = startLen;
      _dynamicSize = sizeof(_fixedBuffer) * 2;
      _dynamicBuffer = (char*)dMalloc(_dynamicSize);
      dMemcpy(_dynamicBuffer, _fixedBuffer, _len + 1);
   }

   // Format into the dynamic buffer, if the buffer is not large enough, then
   // keep doubling it's size until it is.  The buffer is not reallocated
   // using reallocate() to avoid unnecessary buffer copying.
   _len += vsnprintf(_dynamicBuffer + _len, _dynamicSize - _len, format, *(va_list*)args);
   while (_len < 0 || _len >= _dynamicSize)
   {
      _len = startLen;
      char * oldBuffer = _dynamicBuffer;
      _dynamicBuffer = (char*)dRealloc(oldBuffer, _dynamicSize *= 2);
      dFree(oldBuffer);
      _len += vsnprintf(_dynamicBuffer + _len, _dynamicSize - _len, format, *(va_list*)args);
   }

   return _len;
}

S32 String::StrFormat::append(const char * str, S32 len)
{
   if (_dynamicBuffer == NULL)
   {
      if (_len+len >= 0 && _len+len < sizeof(_fixedBuffer))
      {
         dMemcpy(_fixedBuffer + _len, str, len);
         _len += len;
         _fixedBuffer[_len] = '\0';
         return _len;
      }

      _dynamicSize = sizeof(_fixedBuffer) * 2;
      _dynamicBuffer = (char*)dMalloc(_dynamicSize);
      dMemcpy(_dynamicBuffer, _fixedBuffer, _len + 1);
   }

   S32 newSize = _dynamicSize;
   while (newSize < _len+len)
      newSize *= 2;
   if (newSize != _dynamicSize)
      _dynamicBuffer = (char*) dRealloc(_dynamicBuffer, newSize);
   _dynamicSize = newSize;
   dMemcpy(_dynamicBuffer + _len, str, len);
   _len += len;
   _dynamicBuffer[_len] = '\0';
   return _len;
}

S32 String::StrFormat::append(const char * str)
{
   return append(str, dStrlen(str));
}

char*  String::StrFormat::copy( char *buffer )
{
   dMemcpy(buffer, _dynamicBuffer? _dynamicBuffer: _fixedBuffer, _len+1);
   return buffer;
}

//-----------------------------------------------------------------------------

String String::ToString(const char *str, ...)
{
   AssertFatal(str,"String:: Invalid null ptr argument");

   // Use the format object
   va_list args;
   va_start(args, str);
   String ret = VToString(str, args);
   va_end(args);
   return ret;
}

String String::VToString(const char* str, void* args)
{
   StrFormat format(str,&args);

   // Copy it into a string
   U32         len = format.length();
   StringData* sub;
   if( !len )
      sub = StringData::Empty();
   else
   {
      sub = new ( len ) StringData( NULL );

      format.copy( sub->utf8() );
      sub->utf8()[ len ] = 0;
   }

   return sub;
}

String   String::SpanToString(const char *start, const char *end)
{
   if ( end == start )
      return String();
   
   AssertFatal( end > start, "Invalid arguments to String::SpanToString - end is before start" );

   U32         len = U32(end - start);
   StringData* sub = new ( len ) StringData( start );

   return sub;
}

String String::ToLower(const String &string)
{
   if ( string.isEmpty() )
      return String();

   StringData* sub = new ( string.length() ) StringData( string );
   dStrlwr( sub->utf8() );

   return sub;
}

String String::ToUpper(const String &string)
{
   if ( string.isEmpty() )
      return String();

   StringData* sub = new ( string.length() ) StringData( string );
   dStrupr( sub->utf8() );

   return sub;
}

String String::GetTrailingNumber(const char* str, S32& number)
{
   // Check for trivial strings
   if (!str || !str[0])
      return String::EmptyString;

   // Find the number at the end of the string
   String base(str);
   const char* p = base.c_str() + base.length() - 1;

   // Ignore trailing whitespace
   while ((p != base.c_str()) && dIsspace(*p))
      p--;

   // Need at least one digit!
   if (!isdigit(*p))
      return base;

   // Back up to the first non-digit character
   while ((p != base.c_str()) && isdigit(*p))
      p--;

   // Convert number => allow negative numbers, treat '_' as '-' for Maya
   if ((*p == '-') || (*p == '_'))
      number = -dAtoi(p + 1);
   else
      number = ((p == base.c_str()) ? dAtoi(p) : dAtoi(++p));

   // Remove space between the name and the number
   while ((p > base.c_str()) && dIsspace(*(p-1)))
      p--;

   return base.substr(0, p - base.c_str());
}
