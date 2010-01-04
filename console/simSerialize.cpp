//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "console/simBase.h"
#include "core/stream/bitStream.h"
#include "core/stream/fileStream.h"

//-----------------------------------------------------------------------------
// SimObject Methods
//-----------------------------------------------------------------------------

bool SimObject::writeObject(Stream *stream)
{
   clearFieldFilters();
   buildFilterList();

   stream->writeString(getName() ? getName() : "");

   // Static fields
   AbstractClassRep *rep = getClassRep();
   AbstractClassRep::FieldList &fieldList = rep->mFieldList;
   AbstractClassRep::FieldList::iterator itr;
   
   U32 savePos = stream->getPosition();
   U32 numFields = fieldList.size();
   stream->write(numFields);

   for(itr = fieldList.begin();itr != fieldList.end();itr++)
   {
      if(itr->type >= AbstractClassRep::ARCFirstCustomField || isFiltered(itr->pFieldname))
      {
         numFields--;
         continue;
      }

      const char *field = getDataField(itr->pFieldname, NULL);
      if(field == NULL)
         field = "";

      stream->writeString(itr->pFieldname);
      stream->writeString(field);
   }

   // Dynamic Fields
   if(mCanSaveFieldDictionary)
   {
      SimFieldDictionary * fieldDictionary = getFieldDictionary();
      for(SimFieldDictionaryIterator ditr(fieldDictionary); *ditr; ++ditr)
      {
         SimFieldDictionary::Entry * entry = (*ditr);

         if(isFiltered(entry->slotName))
            continue;

         stream->writeString(entry->slotName);
         stream->writeString(entry->value);
         numFields++;
      }
   }

   // Overwrite the number of fields with the correct value
   U32 savePos2 = stream->getPosition();
   stream->setPosition(savePos);
   stream->write(numFields);
   stream->setPosition(savePos2);

   return true;
}

bool SimObject::readObject(Stream *stream)
{
   const char *name = stream->readSTString(true);
   if(name && *name)
      assignName(name);

   U32 numFields;
   stream->read(&numFields);

   for(S32 i = 0;i < numFields;i++)
   {
      const char *fieldName = stream->readSTString();
      const char *data = stream->readSTString();

      setDataField(fieldName, NULL, data);
   }
   return true;
}

//-----------------------------------------------------------------------------

void SimObject::buildFilterList()
{
   Con::executef(this, "buildFilterList");
}

//-----------------------------------------------------------------------------

void SimObject::addFieldFilter( const char *fieldName )
{
   StringTableEntry st = StringTable->insert(fieldName);
   for(S32 i = 0;i < mFieldFilter.size();i++)
   {
      if(mFieldFilter[i] == st)
         return;
   }

   mFieldFilter.push_back(st);
}

void SimObject::removeFieldFilter( const char *fieldName )
{
   StringTableEntry st = StringTable->insert(fieldName);
   for(S32 i = 0;i < mFieldFilter.size();i++)
   {
      if(mFieldFilter[i] == st)
      {
         mFieldFilter.erase(i);
         return;
      }
   }
}

void SimObject::clearFieldFilters()
{
   mFieldFilter.clear();
}

//-----------------------------------------------------------------------------

bool SimObject::isFiltered( const char *fieldName )
{
   StringTableEntry st = StringTable->insert(fieldName);
   for(S32 i = 0;i < mFieldFilter.size();i++)
   {
      if(mFieldFilter[i] == st)
         return true;
   }

   return false;
}

//-----------------------------------------------------------------------------
// SimSet Methods
//-----------------------------------------------------------------------------

bool SimSet::writeObject( Stream *stream )
{
   if(! Parent::writeObject(stream))
      return false;

   stream->write(size());
   for(SimSet::iterator i = begin();i < end();++i)
   {
      if(! Sim::saveObject((*i), stream))
         return false;
   }
   return true;
}

bool SimSet::readObject( Stream *stream )
{
   if(! Parent::readObject(stream))
      return false;

   U32 numObj;
   stream->read(&numObj);

   for(U32 i = 0;i < numObj;i++)
   {
      SimObject *obj = Sim::loadObjectStream(stream);
      if(obj == NULL)
         return false;

      addObject(obj);
   }

   return true;
}

//-----------------------------------------------------------------------------
// Sim Functions
//-----------------------------------------------------------------------------

namespace Sim
{

bool saveObject(SimObject *obj, const char *filename)
{
   FileStream *stream;
   if((stream = FileStream::createAndOpen( filename, Torque::FS::File::Write )) == NULL)
      return false;

   bool ret = saveObject(obj, stream);
   delete stream;

   return ret;
}

bool saveObject(SimObject *obj, Stream *stream)
{
   stream->writeString(obj->getClassName());
   return obj->writeObject(stream);
}

//-----------------------------------------------------------------------------

SimObject *loadObjectStream(const char *filename)
{
   FileStream * stream;
   if((stream = FileStream::createAndOpen( filename, Torque::FS::File::Read )) == NULL)
      return NULL;

   SimObject * ret = loadObjectStream(stream);
   delete stream;
   return ret;
}

SimObject *loadObjectStream(Stream *stream)
{
   const char *className = stream->readSTString(true);
   ConsoleObject *conObj = ConsoleObject::create(className);
   if(conObj == NULL)
   {
      Con::errorf("Sim::restoreObjectStream - Could not create object of class \"%s\"", className);
      return NULL;
   }

   SimObject *simObj = dynamic_cast<SimObject *>(conObj);
   if(simObj == NULL)
   {
      Con::errorf("Sim::restoreObjectStream - Object of class \"%s\" is not a SimObject", className);
      delete simObj;
      return NULL;
   }

   if( simObj->readObject(stream)
       && simObj->registerObject() )
      return simObj;

   delete simObj;
   return NULL;
}

} // end namespace Sim

//-----------------------------------------------------------------------------
// Console Methods
//-----------------------------------------------------------------------------

ConsoleMethod(SimObject, addFieldFilter, void, 3, 3, "(fieldName)")
{
   object->addFieldFilter(argv[2]);
}

ConsoleMethod(SimObject, removeFieldFilter, void, 3, 3, "(fieldName)")
{
   object->removeFieldFilter(argv[2]);
}


//-----------------------------------------------------------------------------
// Console Functions
//-----------------------------------------------------------------------------

ConsoleFunction(saveObject, bool, 3, 3, "(object, filename)")
{
   SimObject *obj = dynamic_cast<SimObject *>(Sim::findObject(argv[1]));
   if(obj == NULL)
      return false;
   
   return Sim::saveObject(obj, argv[2]);
}

ConsoleFunction(loadObject, S32, 2, 2, "(filename)")
{
   SimObject *obj = Sim::loadObjectStream(argv[1]);
   return obj ? obj->getId() : 0;
}
