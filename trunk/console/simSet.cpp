//------------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//------------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/simSet.h"

#include "core/stringTable.h"
#include "console/console.h"
#include "core/stream/fileStream.h"
#include "sim/actionMap.h"
#include "core/fileObject.h"
#include "console/consoleInternal.h"
#include "platform/profiler.h"
#include "console/typeValidators.h"
#include "core/frameAllocator.h"
#include "math/mMathFn.h"

//-----------------------------------------------------------------------------
// Sim Set
//-----------------------------------------------------------------------------

void SimSet::addObject(SimObject* obj)
{
   lock();
   U32 size = objectList.size();
   objectList.pushBack(obj);
   if( objectList.size() > size )
   {
      deleteNotify(obj);
   }
   unlock();
}

void SimSet::removeObject(SimObject* obj)
{
   lock();
   objectList.remove(obj);
   clearNotify(obj);
   unlock();
}

void SimSet::pushObject(SimObject* pObj)
{
   lock();
   U32 size = objectList.size();
   objectList.pushBackForce(pObj);
   if( objectList.size() > size )
   {
      deleteNotify(pObj);
   }
   unlock();
}

void SimSet::popObject()
{
   MutexHandle handle;
   handle.lock(mMutex);

   if (objectList.size() == 0) 
   {
      AssertWarn(false, "Stack underflow in SimSet::popObject");
      return;
   }

   SimObject* pObject = objectList[objectList.size() - 1];

   objectList.removeStable(pObject);
   clearNotify(pObject);
}

void SimSet::scriptSort( const String &scriptCallbackFn )
{
   lock();
   objectList.scriptSort( scriptCallbackFn );
   unlock();
}

void SimSet::callOnChildren( const String &method, S32 argc, const char *argv[], bool executeOnChildGroups )
{
   // Prep the arguments for the console exec...
   // Make sure and leave args[1] empty.
   const char* args[21];
   args[0] = method.c_str();
   for (S32 i = 0; i < argc; i++)
      args[i + 2] = argv[i];

   for ( iterator i = begin(); i != end(); i++ )
   {
      SimObject *childObj = static_cast<SimObject*>(*i);

      if ( childObj->isMethod( method.c_str() ) )
         Con::execute(childObj, argc + 2, args);

      if ( executeOnChildGroups )
      {
         SimSet* childSet = dynamic_cast<SimSet*>(*i);
         if ( childSet )
            childSet->callOnChildren( method, argc, argv, executeOnChildGroups );
      }
   }
}

U32 SimSet::fullSize()
{
   U32 count = 0;

   for ( iterator i = begin(); i != end(); i++ )
   {
      count++;

      SimSet* childSet = dynamic_cast<SimSet*>(*i);
      if ( childSet )
         count += childSet->fullSize();
   }

   return count;
}

bool SimSet::reOrder( SimObject *obj, SimObject *target )
{
   MutexHandle handle;
   handle.lock(mMutex);

   iterator itrS, itrD;
   if ( (itrS = find(begin(),end(),obj)) == end() )
   {
      // object must be in list
      return false; 
   }

   if ( obj == target )
   {
      // don't reorder same object but don't indicate error
      return true;   
   }

   if ( !target )    
   {
      // if no target, then put to back of list

      // don't move if already last object
      if ( itrS != (end()-1) )
      {
         // remove object from its current location and push to back of list
         objectList.erase(itrS);    
         objectList.push_back(obj);
      }
   }
   else
   {
      // if target, insert object in front of target
      if ( (itrD = find(begin(),end(),target)) == end() )
         // target must be in list
         return false;

      objectList.erase(itrS);

      // once itrS has been erased, itrD won't be pointing at the 
      // same place anymore - re-find...
      itrD = find(begin(),end(),target);
      objectList.insert(itrD, obj);
   }

   return true;
}   

void SimSet::onDeleteNotify(SimObject *object)
{
   removeObject(object);
   Parent::onDeleteNotify(object);
}

void SimSet::onRemove()
{
   MutexHandle handle;
   handle.lock(mMutex);

   objectList.sortId();
   if (objectList.size())
   {
      // This backwards iterator loop doesn't work if the
      // list is empty, check the size first.
      for (SimObjectList::iterator ptr = objectList.end() - 1;
         ptr >= objectList.begin(); ptr--)
      {
         clearNotify(*ptr);
      }
   }

   handle.unlock();

   Parent::onRemove();
}

void SimSet::write(Stream &stream, U32 tabStop, U32 flags)
{
   MutexHandle handle;
   handle.lock(mMutex);

   // export selected only?
   if((flags & SelectedOnly) && !isSelected())
   {
      for(U32 i = 0; i < size(); i++)
         (*this)[i]->write(stream, tabStop, flags);

      return;

   }

   stream.writeTabs(tabStop);
   char buffer[1024];
   dSprintf(buffer, sizeof(buffer), "new %s(%s) {\r\n", getClassName(), getName() ? getName() : "");
   stream.write(dStrlen(buffer), buffer);
   writeFields(stream, tabStop + 1);

   if(size())
   {
      stream.write(2, "\r\n");
      for(U32 i = 0; i < size(); i++)
         (*this)[i]->write(stream, tabStop + 1, flags);
   }

   stream.writeTabs(tabStop);
   stream.write(4, "};\r\n");
}

void SimSet::clear()
{
   lock();
   while (size() > 0)
      removeObject(objectList.last());
   unlock();
}

void SimSet::deleteAllObjects()
{
   lock();
   while (size() > 0)
   {
      SimObject *obj = objectList.last();
      removeObject( obj );
      obj->deleteObject();
   }
   unlock();  
}

SimObject* SimSet::findObjectByInternalName(StringTableEntry internalName, bool searchChildren)
{
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      SimObject *childObj = static_cast<SimObject*>(*i);
      if(childObj->getInternalName() == internalName)
         return childObj;
      else if (searchChildren)
      {
         SimSet* childSet = dynamic_cast<SimSet*>(*i);
         if (childSet)
         {
            SimObject* found = childSet->findObjectByInternalName(internalName, searchChildren);
            if (found) return found;
         }
      }
   }

   return NULL;
}

SimObject* SimSet::findObjectByLineNumber(const char* fileName, S32 declarationLine, bool searchChildren)
{
   if (!fileName)
      return NULL;

   if (declarationLine < 0)
      return NULL;

   StringTableEntry fileEntry = StringTable->insert(fileName);

   for (iterator i = begin(); i != end(); i++)
   {
      SimObject *childObj = static_cast<SimObject*>(*i);

      if(childObj->getFilename() == fileEntry && childObj->getDeclarationLine() == declarationLine)
         return childObj;
      else if (searchChildren)
      {
         SimSet* childSet = dynamic_cast<SimSet*>(*i);

         if (childSet)
         {
            SimObject* found = childSet->findObjectByLineNumber(fileName, declarationLine, searchChildren);
            if (found)
               return found;
         }
      }
   }

   return NULL;
}

SimObject* SimSet::getRandom()
{
   if (size() > 0)
      return objectList[mRandI(0, size() - 1)];

   return NULL;
}

//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(SimSet);

//-----------------------------------------------------------------------------
// SimGroup
//-----------------------------------------------------------------------------

SimGroup::~SimGroup()
{
   lock();
   for (iterator itr = begin(); itr != end(); itr++)
      nameDictionary.remove(*itr);

   // XXX Move this later into Group Class
   // If we have any objects at this point, they should
   // already have been removed from the manager, so we
   // can just delete them directly.
   objectList.sortId();
   while (!objectList.empty()) 
   {
      delete objectList.last();
      objectList.decrement();
   }

   unlock();
}


//-----------------------------------------------------------------------------

void SimGroup::addObject(SimObject* obj)
{
   lock();

   // Make sure we aren't adding ourself.  This isn't the most robust check
   // but it should be good enough to prevent some self-foot-shooting.
   if(obj == this)
   {
      Con::errorf("SimGroup::addObject - (%d) can't add self!", getIdString());
      unlock();
      return;
   }

   if (obj->mGroup != this) 
   {
      if (obj->mGroup)
         obj->mGroup->removeObject(obj);
      nameDictionary.insert(obj);
      obj->mGroup = this;
      objectList.push_back(obj); // force it into the object list
      // doesn't get a delete notify
      obj->onGroupAdd();
   }
   unlock();
}

void SimGroup::removeObject(SimObject* obj)
{
   lock();
   removeObjectNoLock( obj );
   unlock();
}

void SimGroup::removeObjectNoLock(SimObject* obj)
{
   if (obj->mGroup == this) 
   {
      obj->onGroupRemove();
      nameDictionary.remove(obj);
      objectList.remove(obj);
      obj->mGroup = 0;
   }
}

//-----------------------------------------------------------------------------

void SimGroup::onRemove()
{
   lock();
   objectList.sortId();
   if (objectList.size())
   {
      // This backwards iterator loop doesn't work if the
      // list is empty, check the size first.
      for (SimObjectList::iterator ptr = objectList.end() - 1;
         ptr >= objectList.begin(); ptr--)
      {
         (*ptr)->onGroupRemove();
         (*ptr)->mGroup = NULL;
         (*ptr)->unregisterObject();
         (*ptr)->mGroup = this;
      }
   }
   SimObject::onRemove();
   unlock();
}

//-----------------------------------------------------------------------------

void SimGroup::clear()
{
   lock();
   while( size() > 0 )
   {
      SimObject* object = objectList.last();
      removeObjectNoLock( object );
      object->deleteObject();
   }
   unlock();
}

//-----------------------------------------------------------------------------

SimObject *SimGroup::findObject(const char *namePath)
{
   // find the end of the object name
   S32 len;
   for(len = 0; namePath[len] != 0 && namePath[len] != '/'; len++)
      ;

   StringTableEntry stName = StringTable->lookupn(namePath, len);
   if(!stName)
      return NULL;

   SimObject *root = nameDictionary.find(stName);

   if(!root)
      return NULL;

   if(namePath[len] == 0)
      return root;

   return root->findObject(namePath + len + 1);
}

SimObject *SimSet::findObject(const char *namePath)
{
   // find the end of the object name
   S32 len;
   for(len = 0; namePath[len] != 0 && namePath[len] != '/'; len++)
      ;

   StringTableEntry stName = StringTable->lookupn(namePath, len);
   if(!stName)
      return NULL;

   lock();
   for(SimSet::iterator i = begin(); i != end(); i++)
   {
      if((*i)->getName() == stName)
      {
         unlock();
         if(namePath[len] == 0)
            return *i;
         return (*i)->findObject(namePath + len + 1);
      }
   }
   unlock();
   return NULL;
}

SimObject* SimObject::findObject(const char* )
{
   return NULL;
}

//-----------------------------------------------------------------------------

bool SimGroup::processArguments(S32, const char **)
{
   return true;
}

//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(SimGroup);

//-----------------------------------------------------------------------------
// Iterators
//-----------------------------------------------------------------------------

inline void SimSetIterator::Stack::push_back(SimSet* set)
{
   increment();
   last().set = set;
   last().itr = set->begin();
}


//-----------------------------------------------------------------------------

SimSetIterator::SimSetIterator(SimSet* set)
{
   VECTOR_SET_ASSOCIATION(stack);

   if (!set->empty())
      stack.push_back(set);
}


//-----------------------------------------------------------------------------

SimObject* SimSetIterator::operator++()
{
   SimSet* set;
   if ((set = dynamic_cast<SimSet*>(*stack.last().itr)) != 0)
   {
      if (!set->empty())
      {
         stack.push_back(set);
         return *stack.last().itr;
      }
   }

   while (++stack.last().itr == stack.last().set->end())
   {
      stack.pop_back();
      if (stack.empty())
         return 0;
   }
   return *stack.last().itr;
}

SimObject* SimGroupIterator::operator++()
{
   SimGroup* set;
   if ((set = dynamic_cast<SimGroup*>(*stack.last().itr)) != 0)
   {
      if (!set->empty())
      {
         stack.push_back(set);
         return *stack.last().itr;
      }
   }

   while (++stack.last().itr == stack.last().set->end())
   {
      stack.pop_back();
      if (stack.empty())
         return 0;
   }
   return *stack.last().itr;
}

//-----------------------------------------------------------------------------
// Console Methods
//-----------------------------------------------------------------------------

ConsoleMethod(SimSet, listObjects, void, 2, 2, "set.listObjects();")
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);

   object->lock();
   SimSet::iterator itr;
   for(itr = object->begin(); itr != object->end(); itr++)
   {
      SimObject *obj = *itr;
      bool isSet = dynamic_cast<SimSet *>(obj) != 0;
      const char *name = obj->getName();
      if(name)
         Con::printf("   %d,\"%s\": %s %s", obj->getId(), name,
         obj->getClassName(), isSet ? "(g)":"");
      else
         Con::printf("   %d: %s %s", obj->getId(), obj->getClassName(),
         isSet ? "(g)" : "");
   }
   object->unlock();
}

ConsoleMethod(SimSet, add, void, 3, 0, "set.add(obj1,...)")
{
   for(S32 i = 2; i < argc; i++)
   {
      SimObject *obj = Sim::findObject(argv[i]);
      if(obj)
         object->addObject(obj);
      else
         Con::printf("Set::add: Object \"%s\" doesn't exist", argv[i]);
   }
}

ConsoleMethod(SimSet, remove, void, 3, 0, "set.remove(obj1,...)")
{
   for(S32 i = 2; i < argc; i++)
   {
      SimObject *obj = Sim::findObject(argv[i]);
      object->lock();
      if(obj && object->find(object->begin(),object->end(),obj) != object->end())
         object->removeObject(obj);
      else
         Con::printf("Set::remove: Object \"%s\" does not exist in set", argv[i]);
      object->unlock();
   }
}

ConsoleMethod(SimSet, clear, void, 2, 2, "set.clear()")
{
   object->clear();
}

ConsoleMethod(SimSet, deleteAllObjects, void, 2, 2, 
   "Deletes all the objects in the set." )
{
   object->deleteAllObjects();
}

ConsoleMethod(SimSet, getRandom, S32, 2, 2, 
   "Deletes all the objects in the set." )
{
   SimObject* ret = object->getRandom();

   if (ret)
      return ret->getId();

   return -1;
}

ConsoleMethod(SimSet, callOnChildren, void, 3, 0, "callOnChildren(funcName [,args ...])")
{
   object->callOnChildren( argv[2], argc - 3, argv + 3 );
}

//------------------------------------------------------------------------------
// Make Sure Child 1 is Ordered Just Under Child 2.
//------------------------------------------------------------------------------
ConsoleMethod(SimSet, reorderChild, void, 4,4," (child1, child2) uses simset reorder to push child 1 before child 2 - both must already be child controls of this control")
{
   SimObject* pObject = Sim::findObject(argv[2]);
   SimObject* pTarget    = Sim::findObject(argv[3]);

   if(pObject && pTarget)
   {
      object->reOrder(pObject,pTarget);
   }
}

ConsoleMethod(SimSet, getCount, S32, 2, 2, "set.getCount() returns the number of children that directly belong to this set (doesn't include the childrens' child objects")
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   return object->size();
}

ConsoleMethod(SimSet, getFullCount, S32, 2, 2, "set.getFullCount() returns the full recursive count of the children of this group")
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   return object->fullSize();
}

ConsoleMethod(SimSet, getObject, S32, 3, 3, "set.getObject(objIndex)")
{
   TORQUE_UNUSED(argc);
   S32 objectIndex = dAtoi(argv[2]);
   if(objectIndex < 0 || objectIndex >= S32(object->size()))
   {
      Con::printf("Set::getObject index out of range.");
      return -1;
   }
   return ((*object)[objectIndex])->getId();
}

ConsoleMethod(SimSet, getObjectIndex, S32, 3, 3, "set.getObjectIndex(object)")
{
   TORQUE_UNUSED(argc);
   SimObject *testObject = Sim::findObject(argv[2]);
   if(!testObject)
   {
      Con::printf("SimSet::getObjectIndex: %s is not an object.", argv[2]);
      return false;
   }

   object->lock();
   S32 count = 0;
   for(SimSet::iterator i = object->begin(); i != object->end(); i++)
   {
      if(*i == testObject)
      {
         object->unlock();
         return count;
      }

      ++count;
   }
   object->unlock();

   return -1;
}

ConsoleMethod(SimSet, isMember, bool, 3, 3, "set.isMember(object)")
{
   TORQUE_UNUSED(argc);
   SimObject *testObject = Sim::findObject(argv[2]);
   if(!testObject)
   {
      Con::printf("SimSet::isMember: %s is not an object.", argv[2]);
      return false;
   }

   object->lock();
   for(SimSet::iterator i = object->begin(); i != object->end(); i++)
   {
      if(*i == testObject)
      {
         object->unlock();
         return true;
      }
   }
   object->unlock();

   return false;
}

ConsoleMethod( SimSet, findObjectByInternalName, S32, 3, 4, "string InternalName [, bool searchChildren]")
{

   StringTableEntry pcName = StringTable->insert(argv[2]);
   bool searchChildren = false;
   if (argc > 3)
      searchChildren = dAtob(argv[3]);

   SimObject* child = object->findObjectByInternalName(pcName, searchChildren);
   if(child)
      return child->getId();
   return 0;
}

ConsoleMethod(SimSet, bringToFront, void, 3, 3, "set.bringToFront(object)")
{
   TORQUE_UNUSED(argc);
   SimObject *obj = Sim::findObject(argv[2]);
   if(!obj)
      return;
   object->bringObjectToFront(obj);
}

ConsoleMethod(SimSet, pushToBack, void, 3, 3, "set.pushToBack(object)")
{
   TORQUE_UNUSED(argc);
   SimObject *obj = Sim::findObject(argv[2]);
   if(!obj)
      return;
   object->pushObjectToBack(obj);
}

ConsoleMethod(SimSet, sort, void, 3, 3, "set.sort( callbackFunction )\n"
   "Performs a sort of the objects in the set using a script "
   "callback function to do the comparision." )
{
   object->scriptSort( argv[2] );
}

