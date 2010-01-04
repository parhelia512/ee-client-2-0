//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/arrayObject.h"
#include "console/consoleTypes.h"
#include "math/mMathFn.h"


bool ArrayObject::smIncreasing = false;

S32 QSORT_CALLBACK ArrayObject::_valueCompare( const void* a, const void* b )
{
   ArrayObject::Element *ea = (ArrayObject::Element *) (a);
   ArrayObject::Element *eb = (ArrayObject::Element *) (b);
   S32 result = dStricmp(ea->value, eb->value);
   return ( smIncreasing ? -result : result );
}

S32 QSORT_CALLBACK ArrayObject::_valueNumCompare( const void* a, const void* b )
{
   ArrayObject::Element *ea = (ArrayObject::Element *) (a);
   ArrayObject::Element *eb = (ArrayObject::Element *) (b);
   F32 aCol = dAtof(ea->value);
   F32 bCol = dAtof(eb->value);
   F32 result = aCol - bCol;
   S32 res = result < 0 ? -1 : (result > 0 ? 1 : 0);
   return ( smIncreasing ? res : -res );
}

S32 QSORT_CALLBACK ArrayObject::_keyCompare( const void* a, const void* b )
{
   ArrayObject::Element *ea = (ArrayObject::Element *) (a);
   ArrayObject::Element *eb = (ArrayObject::Element *) (b);
   S32 result = dStricmp(ea->key, eb->key);   
   return ( smIncreasing ? -result : result );
}

S32 QSORT_CALLBACK ArrayObject::_keyNumCompare( const void* a, const void* b )
{
   ArrayObject::Element *ea = (ArrayObject::Element *) (a);
   ArrayObject::Element *eb = (ArrayObject::Element *) (b);
   const char* aCol = ea->key;
   const char* bCol = eb->key;
   F32 result = dAtof(aCol) - dAtof(bCol);
   S32 res = result < 0 ? -1 : (result > 0 ? 1 : 0);
   return ( smIncreasing ? res : -res );
}


IMPLEMENT_CONOBJECT(ArrayObject);

ArrayObject::ArrayObject()
   : mCurrentIndex( NULL ),
     mCaseSensitive( false )
{
}

void ArrayObject::initPersistFields()
{
   addField( "caseSensitive",    TypeBool,   Offset( mCaseSensitive, ArrayObject ) );

   Parent::initPersistFields();
}

bool ArrayObject::onAdd()
{
   return Parent::onAdd();
}

void ArrayObject::onRemove()
{
   mArray.empty();
   Parent::onRemove();
}

U32 ArrayObject::getIndexFromValue( const String &value ) const
{
   U32 foundIndex = -1;
   for ( U32 i = mCurrentIndex; i < mArray.size(); i++ )
   {
      if ( mArray[i].value.equal( value ) )
      {
         foundIndex = i;
         break;
      }
   }

   if( foundIndex < 0 )
   {
      for ( U32 i = 0; i < mCurrentIndex; i++ )
      {
         if ( mArray[i].value.equal( value ) )
         {
            foundIndex = i;
            break;
         }
      }
   }
   
   return foundIndex;
}

U32 ArrayObject::getIndexFromKey( const String &key ) const
{
   U32 foundIndex = -1;
   for ( U32 i = mCurrentIndex; i < mArray.size(); i++ )
   {
      if ( mArray[i].key.equal( key ) )
      {
         foundIndex = i;
         break;
      }
   }

   if( foundIndex < 0 )
   {
      for ( U32 i = 0; i < mCurrentIndex; i++ )
      {
         if ( mArray[i].key.equal( key ) )
         {
            foundIndex = i;
            break;
         }
      }
   }

   return foundIndex;
}

U32 ArrayObject::getIndexFromKeyValue( const String &key, const String &value ) const
{
   U32 foundIndex = -1;
   for ( U32 i = mCurrentIndex; i < mArray.size(); i++ )
   {
      if ( mArray[i].key == key && mArray[i].value.equal( value ) )
      {
         foundIndex = i;
         break;
      }
   }

   if ( foundIndex < 0 )
   {
      for ( U32 i = 0; i < mCurrentIndex; i++ )
      {
         if ( mArray[i].key.equal( key ) && mArray[i].value.equal( value ) )
         {
            foundIndex = i;
            break;
         }
      }
   }

   return foundIndex;
}


const String& ArrayObject::getKeyFromIndex( U32 index ) const
{
   if ( index > mArray.size() - 1 || index < 0 || mArray.size() == 0 )
      return String::EmptyString;

   return mArray[index].key;
}

const String& ArrayObject::getValueFromIndex( U32 index ) const
{
   if( index > mArray.size() - 1 || index < 0 || mArray.size() == 0 )
      return String::EmptyString;

   return mArray[index].value;
}

S32 ArrayObject::countValue( const String &value ) const
{
   S32 count = 0;
   for ( U32 i = 0; i < mArray.size(); i++ )
   {
      if ( mArray[i].value.equal( value ) )
         count++;
   }

   return count;
}

S32 ArrayObject::countKey( const String &key) const 
{
   S32 count = 0;
   for ( U32 i = 0; i < mArray.size(); i++ )
   {
      if ( mArray[i].key.equal( key ) )
         count++;
   }

   return count;
}

void ArrayObject::push_back( const String &key, const String &value )
{
   Element temp;
   temp.value = value;
   temp.key = key;
   mArray.push_back(temp);
}

void ArrayObject::push_front( const String &key, const String &value )
{
   Element temp;
   temp.value = value;
   temp.key = key;
   mArray.push_front(temp);
}

void ArrayObject::insert( const String &key, const String &value, U32 index )
{
   index = mClampF( index, 0, mArray.size() - 1 );

   U32 size = mArray.size() - 1;
   for( U32 i = size; i >= index; i-- )
      moveIndex( i, i + 1 );

   Element temp;
   temp.value = value;
   temp.key = key;
   mArray[index] = temp;
}

void ArrayObject::pop_back()
{
   if(mArray.size() <= 0)
      return;
   mArray.pop_back();
   if( mCurrentIndex >= mArray.size() )
      mCurrentIndex = mArray.size() - 1;
}

void ArrayObject::pop_front()
{
   if( mArray.size() <= 0 )
      return;

   mArray.pop_front();
   
   if( mCurrentIndex >= mArray.size() )
      mCurrentIndex = mArray.size() - 1;
}

void ArrayObject::erase( U32 index )
{
   if(index < 0 || index >= mArray.size())
      return;

   mArray.erase( index );
}

void ArrayObject::empty()
{
   S32 size = mArray.size();
   for(U32 i=0; i<size; i++)
      mArray.pop_front();
   mCurrentIndex = 0;
}

void ArrayObject::moveIndex(U32 prev, U32 index)
{
   if(index >= mArray.size())
      push_back(mArray[prev].key, mArray[prev].value);
   else
      mArray[index] = mArray[prev];
   mArray[index].value = String::EmptyString;
   mArray[index].key = String::EmptyString;
}

void ArrayObject::uniqueValue()
{
   for(U32 i=0; i<mArray.size(); i++)
   {
      for(U32 j=i+1; j<mArray.size(); j++)
      {
         if(mArray[i].value == mArray[j].value)
         {
            erase(j);
            j--;
         }
      }
   }
}

void ArrayObject::uniqueKey()
{
   for(U32 i=0; i<mArray.size(); i++)
   {
      for(U32 j=i+1; j<mArray.size(); j++)
      {
         if(mArray[i].key == mArray[j].key)
         {
            erase(j);
            j--;
         }
      }
   }
}

void ArrayObject::duplicate(ArrayObject* obj)
{
   empty();
   for(U32 i=0; i<obj->count(); i++)
   {
      StringTableEntry tempval = obj->getValueFromIndex(i);
      StringTableEntry tempkey = obj->getKeyFromIndex(i);
      push_back(tempkey, tempval);
   }
   mCurrentIndex = obj->getCurrent();
}

void ArrayObject::crop( ArrayObject *obj )
{
   for( U32 i = 0; i < obj->count(); i++ )
   {
      const String &tempkey = obj->getKeyFromIndex( i );
      for( U32 j = 0; j < mArray.size(); j++ )
      {
         if( mArray[j].key.equal( tempkey ) )
         {
            mArray.erase( j );
            j--;
         }
      }
  	}
}

void ArrayObject::append(ArrayObject* obj)
{
   for(U32 i=0; i<obj->count(); i++)
   {
      StringTableEntry tempval = obj->getValueFromIndex(i);
      StringTableEntry tempkey = obj->getKeyFromIndex(i);
      push_back(tempkey, tempval);
  	}
}

void ArrayObject::setKey( const String &key, U32 index )
{
   if ( index >= mArray.size() )
      return;

   mArray[index].key = key;
}

void ArrayObject::setValue( const String &value, U32 index )
{
   if ( index >= mArray.size() )
      return;
   
   mArray[index].value = value;
}

void ArrayObject::sort( bool valsort, bool desc, bool numeric )
{
   if ( mArray.size() <= 1 )
      return;

   smIncreasing = desc ? false : true;

   if ( numeric )
   {
      if ( valsort )
         dQsort( (void *)&(mArray[0]), mArray.size(), sizeof(Element), _valueNumCompare) ;
      else
         dQsort( (void *)&(mArray[0]), mArray.size(), sizeof(Element), _keyNumCompare );
   }
   else
   {
      if( valsort )
         dQsort( (void *)&(mArray[0]), mArray.size(), sizeof(Element), _valueCompare );
      else
         dQsort( (void *)&(mArray[0]), mArray.size(), sizeof(Element), _keyCompare );
   }
}

U32 ArrayObject::moveFirst()
{
   mCurrentIndex = 0;
   return mCurrentIndex;
}

U32 ArrayObject::moveLast()
{
   mCurrentIndex = mArray.size() - 1;
   return mCurrentIndex;
}

U32 ArrayObject::moveNext()
{
   if ( mCurrentIndex >= mArray.size() - 1 )
      return -1;
   
   mCurrentIndex++;
   
   return mCurrentIndex;
}

U32 ArrayObject::movePrev()
{
   if ( mCurrentIndex <= 0 )
      return -1;

   mCurrentIndex--;
   
   return mCurrentIndex;
}

void ArrayObject::setCurrent( S32 idx )
{
   if ( idx < 0 || idx >= mArray.size() )
   {
      Con::errorf( "ArrayObject::setCurrent( %d ) is out of the array bounds!", idx );
      return;
   }

   mCurrentIndex = idx;
}

void ArrayObject::echo()
{
   Con::printf( "ArrayObject Listing:" );
   Con::printf( "Index   Key       Value" );
   for ( U32 i = 0; i < mArray.size(); i++ )
   {
      StringTableEntry key = mArray[i].key;
      StringTableEntry val = mArray[i].value;
      Con::printf( "%d    [%s]    =>    %s", (S32)i, key, val );
   }
}

ConsoleMethod( ArrayObject, getIndexFromValue, S32, 3, 3, "(string value)"
             "Search array from current position for the first matching value" )
{
   StringTableEntry value = StringTable->insert( argv[2], object->isCaseSensitive() );
   return (S32)object->getIndexFromValue( value );
}

ConsoleMethod( ArrayObject, getIndexFromKey, S32, 3, 3, "(string key)"
             "Search array from current position for the first matching key" )
{
   StringTableEntry value = StringTable->insert( argv[2], object->isCaseSensitive() );
   return (S32)object->getIndexFromKey( value );
}

ConsoleMethod( ArrayObject, getValue, const char*, 3, 3, "(int index)"
             "Get the value of the array element at the submitted index" )
{
   StringTableEntry key = object->getValueFromIndex( dAtoi( argv[2] ) );
   char *buff = Con::getReturnBuffer( 512 );
   dSprintf( buff, 512, "%s", key );
   return buff;
}

ConsoleMethod( ArrayObject, getKey, const char*, 3, 3, "(int index)"
             "Get the key of the array element at the submitted index" )
{
   StringTableEntry key = object->getKeyFromIndex( dAtoi( argv[2] ) );
   char *buff = Con::getReturnBuffer( 512 );
   dSprintf( buff, 512, "%s", key );
   return buff;
}

ConsoleMethod( ArrayObject, setKey, void, 4, 4, "(string key, int index)"
             "Set the key at the given index" )
{
   StringTableEntry key = StringTable->insert( argv[2], object->isCaseSensitive() );
   U32 index = dAtoi( argv[3] );
   object->setKey( key, index );
}

ConsoleMethod( ArrayObject, setValue, void, 4, 4, "(string value, int index)"
             "Set the value at the given index" )
{
   StringTableEntry value = StringTable->insert( argv[2], object->isCaseSensitive() );
   U32 index = dAtoi( argv[3] );
   object->setValue( value, index );
}

ConsoleMethod( ArrayObject, count, S32, 2, 2, "Get the number of elements in the array" )
{
   return (S32)object->count();
}

ConsoleMethod( ArrayObject, countValue, S32, 3, 3, "(string value)"
             "Get the number of times a particular value is found in the array" )
{
   StringTableEntry value = StringTable->insert( argv[2], object->isCaseSensitive() );
   return (S32)object->countValue( value );
}

ConsoleMethod( ArrayObject, countKey, S32, 3, 3, "(string key)"
             "Get the number of times a particular key is found in the array" )
{
   StringTableEntry value = StringTable->insert( argv[2], object->isCaseSensitive() );
   return (S32)object->countKey( value );
}

ConsoleMethod( ArrayObject, add, void, 4, 4, "(string key, string value)"
             "Adds a new element to the end of an array" )
{
   StringTableEntry key = StringTable->insert( argv[2], object->isCaseSensitive() );
   StringTableEntry value = StringTable->insert( argv[3], object->isCaseSensitive() );
   object->push_back( key, value );
}

ConsoleMethod( ArrayObject, push_back, void, 4, 4, "(string key, string value)"
             "Adds a new element to the end of an array" )
{
   StringTableEntry key = StringTable->insert( argv[2], object->isCaseSensitive() );
   StringTableEntry value = StringTable->insert( argv[3], object->isCaseSensitive() );
   object->push_back( key, value );
}

ConsoleMethod( ArrayObject, push_front, void, 4, 4, "(string key, string value)"
             "Adds a new element to the front of an array" )
{
   StringTableEntry key = StringTable->insert( argv[2], object->isCaseSensitive() );
   StringTableEntry value = StringTable->insert( argv[3], object->isCaseSensitive() );
   object->push_front( key, value );
}

ConsoleMethod( ArrayObject, insert, void, 5, 5, "(string key, string value, int index)"
             "Adds a new element to a specified position in the array" )
{
   StringTableEntry key = StringTable->insert( argv[2], object->isCaseSensitive() );
   StringTableEntry value = StringTable->insert( argv[3], object->isCaseSensitive() );
   object->insert( key, value, dAtoi( argv[4] ) );
}

ConsoleMethod( ArrayObject, pop_back, void, 2, 2, "Removes the last element from the array" )
{
   object->pop_back();
}

ConsoleMethod( ArrayObject, pop_front, void, 2, 2, "Removes the first element from the array" )
{
   object->pop_front();
}

ConsoleMethod( ArrayObject, erase, void, 3, 3, "(int index)"
             "Removes an element at a specific position from the array" )
{
   object->erase( dAtoi( argv[2] ) );
}

ConsoleMethod( ArrayObject, empty, void, 2, 2, "Emptys all elements from an array")
{
   object->empty();
}

ConsoleMethod( ArrayObject, uniqueValue, void, 2, 2, "Removes any elements that have duplicated values (leaving the first instance)")
{
   object->uniqueValue();
}

ConsoleMethod( ArrayObject, uniqueKey, void, 2, 2, "Removes any elements that have duplicated keys (leaving the first instance)")
{
   object->uniqueKey();
}

ConsoleMethod( ArrayObject, duplicate, bool, 3, 3, "(ArrayObject target)"
             "Alters array into an exact duplicate of the target array" )
{
   ArrayObject *target;
   
   if ( Sim::findObject( argv[2], target ) )
   {
      object->duplicate( target );
      return true;
   }

   return false;
}

ConsoleMethod( ArrayObject, crop, bool, 3, 3, "(ArrayObject target)"
             "Removes elements with matching keys from array" )
{
   ArrayObject *target;
   if ( Sim::findObject( argv[2], target ) )
   {
      object->crop( target );
      return true;
   }

   return false;
}

ConsoleMethod( ArrayObject, append, bool, 3, 3, "(ArrayObject target)"
             "Appends the target array to the array object" )
{
   ArrayObject *target;
   if ( Sim::findObject( argv[2], target ) )
   {
      object->append( target );
      return true;
   }

   return false;
}

ConsoleMethod( ArrayObject, sort, void, 2, 3, "(bool desc)"
             "Alpha sorts the array by value (default ascending sort)" )
{
   bool descending = argc == 3 ? dAtob( argv[2] ) : false;
   object->sort( true, descending, false );
}

ConsoleMethod( ArrayObject, sorta, void, 2, 2, "Alpha sorts the array by value in ascending order" )
{
   object->sort( true, false, false );
}

ConsoleMethod( ArrayObject, sortd, void, 2, 2, "Alpha sorts the array by value in descending order" )
{
   object->sort( true, true, false );
}

ConsoleMethod( ArrayObject, sortk, void, 2, 3, "(bool desc)"
             "Alpha sorts the array by key (default ascending sort)" )
{
   bool descending = argc == 3 ? dAtob( argv[2] ) : false;
   object->sort( false, descending, false );
}

ConsoleMethod( ArrayObject, sortka, void, 2, 2, "Alpha sorts the array by key in ascending order" )
{
   object->sort( false, false, false );
}

ConsoleMethod( ArrayObject, sortkd, void, 2, 2, "Alpha sorts the array by key in descending order" )
{
   object->sort( false, true, false );
}

ConsoleMethod( ArrayObject, sortn, void, 2, 3, "(bool desc)"
             "Numerically sorts the array by value (default ascending sort)" )
{
   bool descending = argc == 3 ? dAtob( argv[2] ) : false;
   object->sort( true, descending, true );
}

ConsoleMethod( ArrayObject, sortna, void, 2, 2, "Numerically sorts the array by value in ascending order" ) 
{
   object->sort( true, false, true );
}

ConsoleMethod( ArrayObject, sortnd, void, 2, 2, "Numerically sorts the array by value in descending order" )
{
   object->sort( true, true, true );
}

ConsoleMethod( ArrayObject, sortnk, void, 2, 3, "(bool desc)"
             "Numerically sorts the array by key (default ascending sort)" )
{
   bool descending = argc == 3 ? dAtob( argv[2] ) : false;
   object->sort( false, descending, true );
}

ConsoleMethod( ArrayObject, sortnka, void, 2, 2, "Numerical sorts the array by key in ascending order" )
{
   object->sort( false, false, true );
}

ConsoleMethod( ArrayObject, sortnkd, void, 2, 2, "Numerical sorts the array by key in descending order" )
{
   object->sort( false, true, true );
}

ConsoleMethod( ArrayObject, moveFirst, S32, 2, 2, "Moves array pointer to start of array" )
{
   return object->moveFirst();
}

ConsoleMethod( ArrayObject, moveLast, S32, 2, 2, "Moves array pointer to end of array" )
{
   return object->moveLast();
}

ConsoleMethod( ArrayObject, moveNext, S32, 2, 2, "Moves array pointer to next position (returns -1 if cannot move)" )
{
   return object->moveNext();
}

ConsoleMethod( ArrayObject, movePrev, S32, 2, 2, "Moves array pointer to prev position (returns -1 if cannot move)" )
{
   return object->movePrev();
}

ConsoleMethod( ArrayObject, getCurrent, S32, 2, 2, "Gets the current pointer index" )
{
   return object->getCurrent();
}

ConsoleMethod( ArrayObject, setCurrent, void, 3, 3, "Sets the current pointer index" )
{
   object->setCurrent( dAtoi(argv[2]) );
}

ConsoleMethod( ArrayObject, echo, void, 2, 2, "Echos the array in the console" )
{
   object->echo();
}
