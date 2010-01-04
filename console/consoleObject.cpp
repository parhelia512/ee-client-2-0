//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/consoleObject.h"
#include "core/stringTable.h"
#include "core/crc.h"
#include "console/console.h"
#include "console/consoleInternal.h"
#include "console/typeValidators.h"

AbstractClassRep *                 AbstractClassRep::classLinkList = NULL;
AbstractClassRep::FieldList        sg_tempFieldList( __FILE__, __LINE__ );
U32                                AbstractClassRep::NetClassCount  [NetClassGroupsCount][NetClassTypesCount] = {{0, },};
U32                                AbstractClassRep::NetClassBitSize[NetClassGroupsCount][NetClassTypesCount] = {{0, },};

AbstractClassRep **                AbstractClassRep::classTable[NetClassGroupsCount][NetClassTypesCount];

U32                                AbstractClassRep::classCRC[NetClassGroupsCount] = {CRC::INITIAL_CRC_VALUE, };
bool                               AbstractClassRep::initialized = false;

#ifdef TORQUE_DEBUG
U32                                 ConsoleObject::smNumConObjects;
ConsoleObject*                      ConsoleObject::smFirstConObject;
#endif


void AbstractClassRep::init()
{
   // Register the global visibility boolean.
   Con::addVariable( avar( "$%s::isRenderable", getClassName() ), TypeBool, &mIsRenderEnabled );
}

const AbstractClassRep::Field *AbstractClassRep::findField(StringTableEntry name) const
{
   for(U32 i = 0; i < mFieldList.size(); i++)
      if(mFieldList[i].pFieldname == name)
         return &mFieldList[i];

   return NULL;
}

AbstractClassRep* AbstractClassRep::findClassRep(const char* in_pClassName)
{
   AssertFatal(initialized,
      "AbstractClassRep::findClassRep() - Tried to find an AbstractClassRep before AbstractClassRep::initialize().");

   for (AbstractClassRep *walk = classLinkList; walk; walk = walk->nextClass)
      if (!dStricmp(walk->getClassName(), in_pClassName))
         return walk;

   return NULL;
}

//--------------------------------------
void AbstractClassRep::registerClassRep(AbstractClassRep* in_pRep)
{
   AssertFatal(in_pRep != NULL, "AbstractClassRep::registerClassRep was passed a NULL pointer!");

#ifdef TORQUE_DEBUG  // assert if this class is already registered.
   for(AbstractClassRep *walk = classLinkList; walk; walk = walk->nextClass)
   {
      AssertFatal(dStrcmp(in_pRep->mClassName, walk->mClassName),
         "Duplicate class name registered in AbstractClassRep::registerClassRep()");
   }
#endif

   in_pRep->nextClass = classLinkList;
   classLinkList = in_pRep;
}

//--------------------------------------
void AbstractClassRep::removeClassRep(AbstractClassRep* in_pRep)
{
   for( AbstractClassRep *walk = classLinkList; walk; walk = walk->nextClass )
   {
      // This is the case that will most likely get hit.
      if( walk->nextClass == in_pRep ) 
         walk->nextClass = walk->nextClass->nextClass;
      else if( walk == in_pRep )
      {
         AssertFatal( in_pRep == classLinkList, "Pat failed in his logic for un linking RuntimeClassReps from the class linked list" );
         classLinkList = in_pRep->nextClass;
      }
   }
}

//--------------------------------------

ConsoleObject* AbstractClassRep::create(const char* in_pClassName)
{
   AssertFatal(initialized,
      "AbstractClassRep::create() - Tried to create an object before AbstractClassRep::initialize().");

   const AbstractClassRep *rep = AbstractClassRep::findClassRep(in_pClassName);
   if(rep)
      return rep->create();

   AssertWarn(0, avar("Couldn't find class rep for dynamic class: %s", in_pClassName));
   return NULL;
}

//--------------------------------------
ConsoleObject* AbstractClassRep::create(const U32 groupId, const U32 typeId, const U32 in_classId)
{
   AssertFatal(initialized,
      "AbstractClassRep::create() - Tried to create an object before AbstractClassRep::initialize().");
   AssertFatal(in_classId < NetClassCount[groupId][typeId],
      "AbstractClassRep::create() - Class id out of range.");
   AssertFatal(classTable[groupId][typeId][in_classId] != NULL,
      "AbstractClassRep::create() - No class with requested ID type.");

   // Look up the specified class and create it.
   if(classTable[groupId][typeId][in_classId])
      return classTable[groupId][typeId][in_classId]->create();

   return NULL;
}

//--------------------------------------

static S32 QSORT_CALLBACK ACRCompare(const void *aptr, const void *bptr)
{
   const AbstractClassRep *a = *((const AbstractClassRep **) aptr);
   const AbstractClassRep *b = *((const AbstractClassRep **) bptr);

   if(a->mClassType != b->mClassType)
      return a->mClassType - b->mClassType;
   return dStrnatcasecmp(a->getClassName(), b->getClassName());
}

void AbstractClassRep::initialize()
{
   AssertFatal(!initialized, "Duplicate call to AbstractClassRep::initialize()!");
   Vector<AbstractClassRep *> dynamicTable(__FILE__, __LINE__);

   AbstractClassRep *walk;

   // Initialize namespace references...
   for (walk = classLinkList; walk; walk = walk->nextClass)
   {
      walk->mNamespace = Con::lookupNamespace(StringTable->insert(walk->getClassName()));
      walk->mNamespace->mClassRep = walk;
   }

   // Initialize field lists... (and perform other console registration).
   for (walk = classLinkList; walk; walk = walk->nextClass)
   {
      // sg_tempFieldList is used as a staging area for field lists
      // (see addField, addGroup, etc.)
      sg_tempFieldList.setSize(0);

      walk->init();

      // So if we have things in it, copy it over...
      if (sg_tempFieldList.size() != 0)
         walk->mFieldList = sg_tempFieldList;

      // And of course delete it every round.
      sg_tempFieldList.clear();
   }

   // Calculate counts and bit sizes for the various NetClasses.
   for (U32 group = 0; group < NetClassGroupsCount; group++)
   {
      U32 groupMask = 1 << group;

      // Specifically, for each NetClass of each NetGroup...
      for(U32 type = 0; type < NetClassTypesCount; type++)
      {
         // Go through all the classes and find matches...
         for (walk = classLinkList; walk; walk = walk->nextClass)
         {
            if(walk->mClassType == type && walk->mClassGroupMask & groupMask)
               dynamicTable.push_back(walk);
         }

         // Set the count for this NetGroup and NetClass
         NetClassCount[group][type] = dynamicTable.size();
         if(!NetClassCount[group][type])
            continue; // If no classes matched, skip to next.

         // Sort by type and then by name.
         dQsort((void *) &dynamicTable[0], dynamicTable.size(), sizeof(AbstractClassRep *), ACRCompare);

         // Allocate storage in the classTable
         classTable[group][type] = new AbstractClassRep*[NetClassCount[group][type]];

         // Fill this in and assign class ids for this group.
         for(U32 i = 0; i < NetClassCount[group][type];i++)
         {
            classTable[group][type][i] = dynamicTable[i];
            dynamicTable[i]->mClassId[group] = i;
         }

         // And calculate the size of bitfields for this group and type.
         NetClassBitSize[group][type] =
               getBinLog2(getNextPow2(NetClassCount[group][type] + 1));
         AssertFatal(NetClassCount[group][type] < (1 << NetClassBitSize[group][type]), "NetClassBitSize too small!");

         dynamicTable.clear();
      }
   }

   // Ok, we're golden!
   initialized = true;
}

void AbstractClassRep::shutdown()
{
   AssertFatal( initialized, "AbstractClassRep::shutdown - not initialized" );

   // Release storage allocated to the class table.

   for (U32 group = 0; group < NetClassGroupsCount; group++)
      for(U32 type = 0; type < NetClassTypesCount; type++)
         if( classTable[ group ][ type ] )
            SAFE_DELETE_ARRAY( classTable[ group ][ type ] );

   initialized = false;
}

AbstractClassRep *AbstractClassRep::getCommonParent( const AbstractClassRep *otherClass ) const
{
   // CodeReview: This may be a noob way of doing it. There may be some kind of
   // super-spiffy algorithm to do what the code below does, but this appeared
   // to make sense to me, and it is pretty easy to see what it is doing [6/23/2007 Pat]

   static VectorPtr<AbstractClassRep *> thisClassHeirarchy;
   thisClassHeirarchy.clear();

   AbstractClassRep *walk = const_cast<AbstractClassRep *>( this );

   while( walk != NULL )
   {
      thisClassHeirarchy.push_front( walk );
      walk = walk->getParentClass();
   }

   static VectorPtr<AbstractClassRep *> compClassHeirarchy;
   compClassHeirarchy.clear();
   walk = const_cast<AbstractClassRep *>( otherClass );
   while( walk != NULL )
   {
      compClassHeirarchy.push_front( walk );
      walk = walk->getParentClass();
   }

   // Make sure we only iterate over the list the number of times we can
   S32 maxIterations = getMin( compClassHeirarchy.size(), thisClassHeirarchy.size() );

   U32 i = 0;
   for( ; i < maxIterations; i++ )
   {
      if( compClassHeirarchy[i] != thisClassHeirarchy[i] )
         break;
   }

   return compClassHeirarchy[i];
}

//------------------------------------------------------------------------------
//-------------------------------------- ConsoleObject

static char replacebuf[1024];
static char* suppressSpaces(const char* in_pname)
{
	U32 i = 0;
	char chr;
	do
	{
		chr = in_pname[i];
		replacebuf[i++] = (chr != 32) ? chr : '_';
	} while(chr);

	return replacebuf;
}

void ConsoleObject::addGroup(const char* in_pGroupname, const char* in_pGroupDocs)
{
   // Remove spaces.
   char* pFieldNameBuf = suppressSpaces(in_pGroupname);

   // Append group type to fieldname.
   dStrcat(pFieldNameBuf, "_begingroup");

   // Create Field.
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(pFieldNameBuf);
   f.pGroupname   = StringTable->insert(in_pGroupname);

   if(in_pGroupDocs)
      f.pFieldDocs   = StringTable->insert(in_pGroupDocs);
   else
      f.pFieldDocs   = NULL;

   f.type         = AbstractClassRep::StartGroupFieldType;
   f.elementCount = 0;
   f.groupExpand  = false;
   f.validator    = NULL;
   f.setDataFn    = &defaultProtectedSetFn;
   f.getDataFn    = &defaultProtectedGetFn;

   // Add to field list.
   sg_tempFieldList.push_back(f);
}

void ConsoleObject::endGroup(const char*  in_pGroupname)
{
   // Remove spaces.
   char* pFieldNameBuf = suppressSpaces(in_pGroupname);

   // Append group type to fieldname.
   dStrcat(pFieldNameBuf, "_endgroup");

   // Create Field.
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(pFieldNameBuf);
   f.pGroupname   = StringTable->insert(in_pGroupname);
   f.pFieldDocs   = NULL;
   f.type         = AbstractClassRep::EndGroupFieldType;
   f.groupExpand  = false;
   f.validator    = NULL;
   f.setDataFn    = &defaultProtectedSetFn;
   f.getDataFn    = &defaultProtectedGetFn;
   f.elementCount = 0;

   // Add to field list.
   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addArray( const char *arrayName, S32 count )
{
   char *nameBuff = suppressSpaces(arrayName);
   dStrcat(nameBuff, "_beginarray");

   // Create Field.
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(nameBuff);
   f.pGroupname   = StringTable->insert(arrayName);
   f.pFieldDocs   = NULL;

   f.type         = AbstractClassRep::StartArrayFieldType;
   f.elementCount = count;
   f.groupExpand  = false;
   f.validator    = NULL;
   f.setDataFn    = &defaultProtectedSetFn;
   f.getDataFn    = &defaultProtectedGetFn;

   // Add to field list.
   sg_tempFieldList.push_back(f);
}

void ConsoleObject::endArray( const char *arrayName )
{
   char *nameBuff = suppressSpaces(arrayName);
   dStrcat(nameBuff, "_endarray");

   // Create Field.
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(nameBuff);
   f.pGroupname   = StringTable->insert(arrayName);
   f.pFieldDocs   = NULL;
   f.type         = AbstractClassRep::EndArrayFieldType;
   f.groupExpand  = false;
   f.validator    = NULL;
   f.setDataFn    = &defaultProtectedSetFn;
   f.getDataFn    = &defaultProtectedGetFn;
   f.elementCount = 0;

   // Add to field list.
   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       const char* in_pFieldDocs)
{
   addField(
      in_pFieldname,
      in_fieldType,
      in_fieldOffset,
      1,
      NULL,
      in_pFieldDocs);
}

void ConsoleObject::addProtectedField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       AbstractClassRep::SetDataNotify in_setDataFn,
                       AbstractClassRep::GetDataNotify in_getDataFn,
                       const char* in_pFieldDocs)
{
   addProtectedField(
      in_pFieldname,
      in_fieldType,
      in_fieldOffset,
      in_setDataFn,
      in_getDataFn,
      1,
      NULL,
      in_pFieldDocs);
}


void ConsoleObject::addField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       const U32 in_elementCount,
                       const EnumTable *in_table,
                       const char* in_pFieldDocs)
{
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(in_pFieldname);
   f.pGroupname   = NULL;

   if(in_pFieldDocs)
      f.pFieldDocs   = StringTable->insert(in_pFieldDocs);
   else
      f.pFieldDocs   = NULL;

   f.type         = in_fieldType;
   f.offset       = in_fieldOffset;
   f.elementCount = in_elementCount;
   f.table        = in_table;
   f.validator    = NULL;

   f.setDataFn = &defaultProtectedSetFn;
   f.getDataFn    = &defaultProtectedGetFn;

   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addProtectedField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       AbstractClassRep::SetDataNotify in_setDataFn,
                       AbstractClassRep::GetDataNotify in_getDataFn,
                       const U32 in_elementCount,
                       const EnumTable *in_table,
                       const char* in_pFieldDocs)
{
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(in_pFieldname);
   f.pGroupname   = NULL;

   if(in_pFieldDocs)
      f.pFieldDocs   = StringTable->insert(in_pFieldDocs);
   else
      f.pFieldDocs   = NULL;

   f.type         = in_fieldType;
   f.offset       = in_fieldOffset;
   f.elementCount = in_elementCount;
   f.table        = in_table;
   f.validator    = NULL;

   f.setDataFn = in_setDataFn;
   f.getDataFn = in_getDataFn;

   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addFieldV(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       TypeValidator *v,
                       const char* in_pFieldDocs)
{
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(in_pFieldname);
   f.pGroupname   = NULL;
   if(in_pFieldDocs)
      f.pFieldDocs   = StringTable->insert(in_pFieldDocs);
   else
      f.pFieldDocs   = NULL;
   f.type         = in_fieldType;
   f.offset       = in_fieldOffset;
   f.elementCount = 1;
   f.table        = NULL;
   f.setDataFn    = &defaultProtectedSetFn;
   f.getDataFn    = &defaultProtectedGetFn;
   f.validator    = v;
   v->fieldIndex  = sg_tempFieldList.size();

   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addDeprecatedField(const char *fieldName)
{
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(fieldName);
   f.pGroupname   = NULL;
   f.pFieldDocs   = NULL;
   f.type         = AbstractClassRep::DeprecatedFieldType;
   f.offset       = 0;
   f.elementCount = 0;
   f.table        = NULL;
   f.validator    = NULL;
   f.setDataFn    = &defaultProtectedSetFn;
   f.getDataFn    = &defaultProtectedGetFn;

   sg_tempFieldList.push_back(f);
}


bool ConsoleObject::removeField(const char* in_pFieldname)
{
   for (U32 i = 0; i < sg_tempFieldList.size(); i++) {
      if (dStricmp(in_pFieldname, sg_tempFieldList[i].pFieldname) == 0) {
         sg_tempFieldList.erase(i);
         return true;
      }
   }

   return false;
}

//--------------------------------------
void ConsoleObject::initPersistFields()
{
}

//--------------------------------------
void ConsoleObject::consoleInit()
{
}

ConsoleObject::ConsoleObject()
{
#ifdef TORQUE_DEBUG
   // Add to instance list.

   mNextConObject = smFirstConObject;
   mPrevConObject = NULL;

   if( smFirstConObject )
      smFirstConObject->mPrevConObject = this;
   smFirstConObject = this;

   smNumConObjects ++;
#endif
}

ConsoleObject::~ConsoleObject()
{
#ifdef TORQUE_DEBUG
   // Unlink from instance list.

   if( mPrevConObject )
      mPrevConObject->mNextConObject = mNextConObject;
   else
      smFirstConObject = mNextConObject;
   if( mNextConObject )
      mNextConObject->mPrevConObject = mPrevConObject;

   smNumConObjects --;
#endif
}

//--------------------------------------
String ConsoleObject::describeSelf()
{
   String desc = avar( "%s|c++: %s", getClassName(), typeid( *this ).name() );
   return desc;
}

//--------------------------------------
AbstractClassRep* ConsoleObject::getClassRep() const
{
   return NULL;
}

String ConsoleObject::_getLogMessage(const char* fmt, void* args) const
{
   String objClass = "UnknownClass";
   if(getClassRep())
      objClass = getClassRep()->getClassName();
   
   String formattedMessage = String::VToString(fmt, args);
   return String::ToString("%s - Object at %x - %s", 
      objClass.c_str(), this, formattedMessage.c_str());
}

void ConsoleObject::logMessage(const char* fmt, ...) const
{
   va_list args;
   va_start(args, fmt);
   Con::printf(_getLogMessage(fmt, args));
   va_end(args);
}

void ConsoleObject::logWarning(const char* fmt, ...) const
{
   va_list args;
   va_start(args, fmt);
   Con::warnf(_getLogMessage(fmt, args));
   va_end(args);
}

void ConsoleObject::logError(const char* fmt, ...) const
{
   va_list args;
   va_start(args, fmt);
   Con::errorf(_getLogMessage(fmt, args));
   va_end(args);
}


//------------------------------------------------------------------------------

static const char* returnClassList( Vector< AbstractClassRep* >& classes, U32 bufSize )
{
   if( !classes.size() )
      return "";
      
   dQsort( classes.address(), classes.size(), sizeof( AbstractClassRep* ), ACRCompare );

   char* ret = Con::getReturnBuffer( bufSize );
   dStrcpy( ret, classes[ 0 ]->getClassName() );
   for( U32 i = 1; i < classes.size(); i ++ )
   {
      dStrcat( ret, "\t" );
      dStrcat( ret, classes[ i ]->getClassName() );
   }
   
   return ret;
}

static const char* returnString( const String& str )
{
   const U32 size = str.size();
   
   char* buffer = Con::getReturnBuffer( size );
   dMemcpy( buffer, str.c_str(), size );
   
   return buffer;
}

//------------------------------------------------------------------------------

ConsoleFunction( isClass, bool, 2, 2, "( string className ) - Returns if the passed string is a defined class" )
{
   AbstractClassRep* rep = AbstractClassRep::findClassRep( argv[ 1 ] );
   return ( rep != NULL );
}

ConsoleFunction( isMemberOfClass, bool, 3, 3, "(classA, classB")
{
   AbstractClassRep *pRep = AbstractClassRep::findClassRep(argv[1]);
   while (pRep)
   {
      if (!dStricmp(pRep->getClassName(), argv[2]))
         return true;
      pRep = pRep->getParentClass();
   }
   return false;
}

ConsoleFunction( getDescriptionOfClass, const char*, 2, 2, "( string className ) - Return the description string for the given class." )
{
   AbstractClassRep* rep = AbstractClassRep::findClassRep( argv[ 1 ] );
   if( !rep )
   {
      Con::errorf( "getDescriptionOfClass - no class called '%s'", argv[ 1 ] );
      return "";
   }
   else
      return returnString( rep->getDescription() );
}

ConsoleFunction( getCategoryOfClass, const char*, 2, 2, "( string className ) - Return the category of the given class." )
{
   AbstractClassRep* rep = AbstractClassRep::findClassRep( argv[ 1 ] );
   if( !rep )
   {
      Con::errorf( "getCategoryOfClass - no class called '%s'", argv[ 1 ] );
      return "";
   }
   else
      return returnString( rep->getCategory() );
}

ConsoleFunction( enumerateConsoleClasses, const char*, 1, 2, "enumerateConsoleClasses(<\"base class\">);")
{
   AbstractClassRep *base = NULL;    
   if(argc > 1)
   {
      base = AbstractClassRep::findClassRep(argv[1]);
      if(!base)
         return "";
   }
   
   Vector<AbstractClassRep*> classes;
   U32 bufSize = 0;
   for(AbstractClassRep *rep = AbstractClassRep::getClassList(); rep; rep = rep->getNextClass())
   {
      if( !base || rep->isClass(base))
      {
         classes.push_back(rep);
         bufSize += dStrlen(rep->getClassName()) + 1;
      }
   }
         
   return returnClassList( classes, bufSize );
}

ConsoleFunction( enumerateConsoleClassesByCategory, const char*, 2, 2, "( string category ) - Return a list of classes that belong to the given category." )
{
   String category = argv[ 1 ];
   U32 categoryLength = category.length();
   
   U32 bufSize = 0;
   Vector< AbstractClassRep* > classes;
   
   for( AbstractClassRep* rep = AbstractClassRep::getClassList(); rep != NULL; rep = rep->getNextClass() )
   {
      const String& repCategory = rep->getCategory();
      
      if( repCategory.length() >= categoryLength
          && ( repCategory.compare( category, categoryLength, String::NoCase ) == 0 )
          && ( repCategory[ categoryLength ] == ' ' || repCategory[ categoryLength ] == '\0' ) )
      {
         classes.push_back( rep );
         bufSize += dStrlen( rep->getClassName() + 1 );
      }
   }
      
   return returnClassList( classes, bufSize );
}

ConsoleFunction(dumpNetStats,void,1,1,"")
{
#ifdef TORQUE_NET_STATS
   for (AbstractClassRep * rep = AbstractClassRep::getClassList(); rep; rep = rep->getNextClass())
   {
      if (rep->mNetStatPack.numEvents || rep->mNetStatUnpack.numEvents || rep->mNetStatWrite.numEvents || rep->mNetStatRead.numEvents)
      {
         Con::errorf("class %s net info",rep->getClassName());
         if (rep->mNetStatPack.numEvents)
            Con::errorf("   packUpdate: avg (%f), min (%i), max (%i), num (%i)",
                                       F32(rep->mNetStatPack.total)/F32(rep->mNetStatPack.numEvents),
                                       rep->mNetStatPack.min,
                                       rep->mNetStatPack.max,
                                       rep->mNetStatPack.numEvents);
         if (rep->mNetStatUnpack.numEvents)
            Con::errorf("   unpackUpdate: avg (%f), min (%i), max (%i), num (%i)",
                                       F32(rep->mNetStatUnpack.total)/F32(rep->mNetStatUnpack.numEvents),
                                       rep->mNetStatUnpack.min,
                                       rep->mNetStatUnpack.max,
                                       rep->mNetStatUnpack.numEvents);
         if (rep->mNetStatWrite.numEvents)
            Con::errorf("   write: avg (%f), min (%i), max (%i), num (%i)",
                                       F32(rep->mNetStatWrite.total)/F32(rep->mNetStatWrite.numEvents),
                                       rep->mNetStatWrite.min,
                                       rep->mNetStatWrite.max,
                                       rep->mNetStatWrite.numEvents);
         if (rep->mNetStatRead.numEvents)
            Con::errorf("   read: avg (%f), min (%i), max (%i), num (%i)",
                                       F32(rep->mNetStatRead.total)/F32(rep->mNetStatRead.numEvents),
                                       rep->mNetStatRead.min,
                                       rep->mNetStatRead.max,
                                       rep->mNetStatRead.numEvents);
         S32 sum = 0;
         for (S32 i=0; i<32; i++)
            sum  += rep->mDirtyMaskFrequency[i];
         if (sum)
         {
            Con::errorf("   Mask bits:");
            for (S32 i=0; i<8; i++)
            {
               F32 avg0  = rep->mDirtyMaskFrequency[i] ? F32(rep->mDirtyMaskTotal[i])/F32(rep->mDirtyMaskFrequency[i]) : 0.0f;
               F32 avg8  = rep->mDirtyMaskFrequency[i+8] ? F32(rep->mDirtyMaskTotal[i+8])/F32(rep->mDirtyMaskFrequency[i+8]) : 0.0f;
               F32 avg16 = rep->mDirtyMaskFrequency[i+16] ? F32(rep->mDirtyMaskTotal[i+16])/F32(rep->mDirtyMaskFrequency[i+16]) : 0.0f;
               F32 avg24 = rep->mDirtyMaskFrequency[i+24] ? F32(rep->mDirtyMaskTotal[i+24])/F32(rep->mDirtyMaskFrequency[i+24]) : 0.0f;
               Con::errorf("      %2i - %4i (%6.2f)     %2i - %4i (%6.2f)     %2i - %4i (%6.2f)     %2i - %4i, (%6.2f)",
                  i   ,rep->mDirtyMaskFrequency[i],avg0,
                  i+8 ,rep->mDirtyMaskFrequency[i+8],avg8,
                  i+16,rep->mDirtyMaskFrequency[i+16],avg16,
                  i+24,rep->mDirtyMaskFrequency[i+24],avg24);
            }
         }
      }
      rep->resetNetStats();
   }
#endif
}

#if defined(TORQUE_DEBUG)

ConsoleFunction( sizeof, S32, 2, 2, "sizeof( object | classname)")
{
   AbstractClassRep *acr = NULL;
   U32 objId = dAtoi(argv[1]);
   SimObject *obj = Sim::findObject(objId);
   if(obj)
      acr = obj->getClassRep();

   if(!acr)
      acr = AbstractClassRep::findClassRep(argv[1]);

   if(acr)
      return acr->getSizeof();

   if(dStricmp("ConsoleObject", argv[1]) == 0)
     return sizeof(ConsoleObject);

   Con::warnf("could not find a class rep for that object or class name.");
   return 0;
}

void ConsoleObject::debugDumpInstances()
{
   Con::printf( "----------- Dumping ConsoleObjects ----------------" );
   for( ConsoleObject* object = smFirstConObject; object != NULL; object = object->mNextConObject )
      Con::printf( object->describeSelf() );
   Con::printf( "%i ConsoleObjects", smNumConObjects );
}

void ConsoleObject::debugEnumInstances( const std::type_info& type, DebugEnumInstancesCallback callback )
{
   for( ConsoleObject* object = smFirstConObject; object != NULL; object = object->mNextConObject )
      if( typeid( *object ) == type )
      {
         // Set breakpoint here to break for each instance.
         if( callback )
            callback( object );
      }
}

ConsoleFunction( dumpAllObjects, void, 1, 1, "dumpAllObjects() - dump information about all ConsoleObject instances" )
{
   ConsoleObject::debugDumpInstances();
}

#endif //defined(TORQUE_DEBUG)
