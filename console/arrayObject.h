//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _ARRAYOBJECT_H_
#define _ARRAYOBJECT_H_

#ifndef _SIMBASE_H_
#include "console/simBase.h"
#endif

// This class is based on original code by community
// member Daniel Neilsen:
//
// http://www.garagegames.com/community/resources/view/4711


///
class ArrayObject : public SimObject
{
   typedef SimObject Parent;

protected:

   static bool smIncreasing;

   struct Element
   {
      String key;
      String value;
   };

   static S32 QSORT_CALLBACK _valueCompare( const void *a, const void *b );
   static S32 QSORT_CALLBACK _valueNumCompare( const void *a, const void *b );
   static S32 QSORT_CALLBACK _keyCompare( const void *a, const void *b );
   static S32 QSORT_CALLBACK _keyNumCompare( const void *a, const void *b );

public:
  
   ArrayObject();
   virtual ~ArrayObject() {}

   // SimObject
   bool onAdd();
   void onRemove();
   
   DECLARE_CONOBJECT( ArrayObject );
   static void initPersistFields();

   /// @name Data Query 
   /// @{
   
   /// Returns true if string handly by the array is case-sensitive.
   bool isCaseSensitive() const { return mCaseSensitive; }

   /// Searches the array for the first matching value from the 
   /// current array position.  It will return -1 if no matching
   /// index is found.
   U32 getIndexFromValue( const String &value ) const;

   /// Searches the array for the first matching key from the current
   /// array position.  It will return -1 if no matching index found.
   U32 getIndexFromKey( const String &key ) const;
   
   /// Returns the key for a given index.
   /// Will return a null value for an invalid index
   const String& getKeyFromIndex( U32 index ) const;

   /// Returns the value for a given index.
   /// Will return a null value for an invalid index
   const String&	getValueFromIndex( U32 index ) const;
   
   ///
   U32 getIndexFromKeyValue( const String &key, const String &value ) const;

   /// Counts the number of elements in the array
   S32 count() const { return mArray.size(); }

   /// Counts the number of instances of a particular value in the array
   S32 countValue( const String &value ) const;

   /// Counts the number of instances of a particular key in the array
   S32 countKey( const String &key ) const;

   /// @}

   /// @name Data Alteration
   /// @{

   /// Adds a new array item to the end of the array
   void push_back( const String &key, const String &value );

   /// Adds a new array item to the front of the array
   void push_front( const String &key, const String &value );

   /// Adds a new array item to a particular index of the array
   void insert( const String &key, const String &value, U32 index );

   /// Removes an array item from the end of the array
   void pop_back();

   /// Removes an array item from the end of the array
   void pop_front();

   /// Removes an array item from a particular index of the array
   void erase( U32 index );

   /// Clears an array
   void empty();

   /// Moves a key and value from one index location to another.
   void moveIndex( U32 prev, U32 index );

   /// @}

   /// @name Complex Data Alteration
   /// @{

   /// Removes any duplicate values from the array
   /// (keeps the first instance only)
   void uniqueValue();

   /// Removes any duplicate keys from the array
   /// (keeps the first instance only)
   void uniqueKey();

   /// Makes this array an exact duplicate of another array
   void duplicate( ArrayObject *obj );

   /// Crops the keys that exists in the target array from our current array
   void crop( ArrayObject *obj );

   /// Appends the target array to our current array
   void append( ArrayObject *obj );

   /// Sets the key at the given index
   void setKey( const String &key, U32 index );

   /// Sets the key at the given index
   void setValue( const String &value, U32 index );

   /// This sorts the array.
   /// @param valtest  Determines whether sorting by value or key.
   /// @param desc     Determines if sorting ascending or descending.
   /// @param numeric  Determines if sorting alpha or numeric search.
   void sort( bool valtest, bool desc, bool numeric );

   /// @}

   /// @name Pointer Manipulation
   /// @{

   /// Moves pointer to arrays first position
   U32 moveFirst();

   /// Moves pointer to arrays last position
   U32 moveLast();

   /// Moves pointer to arrays next position
   /// If last position it returns -1 and no move occurs;
   U32 moveNext();

   /// Moves pointer to arrays prev position
   /// If first position it returns -1 and no move occurs;
   U32 movePrev();

   /// Returns current pointer index.
   U32 getCurrent() const { return mCurrentIndex; }

   ///
   void setCurrent( S32 idx );

   /// @}


   /// @name Data Listing
   /// @{

   /// Echos the content of the array to the console.
   void echo();

   /// @}

protected:

   bool mCaseSensitive;
   U32 mCurrentIndex;
   Vector<Element> mArray;
};

#endif // _ARRAYOBJECT_H_