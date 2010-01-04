//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/simObjectList.h"

#include "console/console.h"
#include "console/sim.h"
#include "console/simObject.h"


String SimObjectList::smSortScriptCallbackFn;

void SimObjectList::pushBack(SimObject* obj)
{
   if (find(begin(),end(),obj) == end())
      push_back(obj);
}

void SimObjectList::pushBackForce(SimObject* obj)
{
   iterator itr = find(begin(),end(),obj);
   if (itr == end())
   {
      push_back(obj);
   }
   else
   {
      // Move to the back...
      //
      SimObject* pBack = *itr;
      removeStable(pBack);
      push_back(pBack);
   }
}

void SimObjectList::pushFront(SimObject* obj)
{
   if (find(begin(),end(),obj) == end())
      push_front(obj);
}

void SimObjectList::remove(SimObject* obj)
{
   iterator ptr = find(begin(),end(),obj);
   if (ptr != end())
      erase(ptr);
}

void SimObjectList::removeStable(SimObject* obj)
{
   iterator ptr = find(begin(),end(),obj);
   if (ptr != end())
      erase(ptr);
}

S32 QSORT_CALLBACK SimObjectList::_compareId(const void* a,const void* b)
{
   return (*reinterpret_cast<const SimObject* const*>(a))->getId() -
      (*reinterpret_cast<const SimObject* const*>(b))->getId();
}

void SimObjectList::sortId()
{
   dQsort(address(),size(),sizeof(value_type),_compareId);
}

S32 QSORT_CALLBACK SimObjectList::_callbackSort( const void *a, const void *b )
{
   const SimObject *objA = *reinterpret_cast<const SimObject* const*>( a );
   const SimObject *objB = *reinterpret_cast<const SimObject* const*>( b );   

   static char idA[64];
   dSprintf( idA, sizeof( idA ), "%d", objA->getId() );
   static char idB[64];
   dSprintf( idB, sizeof( idB ), "%d", objB->getId() );

   return dAtoi( Con::executef( smSortScriptCallbackFn, idA, idB ) );
}

void SimObjectList::scriptSort( const String &scriptCallback )
{
   AssertFatal( smSortScriptCallbackFn.isEmpty(), "SimObjectList::scriptSort() - The script sort is not reentrant!" );

   smSortScriptCallbackFn = scriptCallback;
   dQsort( address(), size(), sizeof(value_type), _callbackSort );
   smSortScriptCallbackFn = String::EmptyString;
}
