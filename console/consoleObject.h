//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _CONSOLEOBJECT_H_
#define _CONSOLEOBJECT_H_

#include <typeinfo>

//Includes
#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _STRINGTABLE_H_
#include "core/stringTable.h"
#endif
#ifndef _STRINGFUNCTIONS_H_
#include "core/strings/stringFunctions.h"
#endif
#ifndef _BITSET_H_
#include "core/bitSet.h"
#endif
#ifndef _CONSOLE_H_
#include "console/console.h"
#endif

#include "core/util/safeDelete.h"


class Namespace;
class ConsoleObject;

enum NetClassTypes {
   NetClassTypeObject = 0,
   NetClassTypeDataBlock,
   NetClassTypeEvent,
   NetClassTypesCount,
};

enum NetClassGroups {
   NetClassGroupGame = 0,
   NetClassGroupCommunity,
   NetClassGroup3,
   NetClassGroup4,
   NetClassGroupsCount,
};

enum NetClassMasks {
   NetClassGroupGameMask      = BIT(NetClassGroupGame),
   NetClassGroupCommunityMask = BIT(NetClassGroupCommunity),
};

enum NetDirection
{
   NetEventDirAny,
   NetEventDirServerToClient,
   NetEventDirClientToServer,
};

class SimObject;
class TypeValidator;

/// Core functionality for class manipulation.
///
/// @section AbstractClassRep_intro Introduction (or, Why AbstractClassRep?)
///
/// Many of Torque's subsystems, especially network, console, and sim,
/// require the ability to programatically instantiate classes. For instance,
/// when objects are ghosted, the networking layer needs to be able to create
/// an instance of the object on the client. When the console scripting
/// language runtime encounters the "new" keyword, it has to be able to fill
/// that request.
///
/// Since standard C++ doesn't provide a function to create a new instance of
/// an arbitrary class at runtime, one must be created. This is what
/// AbstractClassRep and ConcreteClassRep are all about. They allow the registration
/// and instantiation of arbitrary classes at runtime.
///
/// In addition, ACR keeps track of the fields (registered via addField() and co.) of
/// a class, allowing programmatic access of class fields.
///
/// @see ConsoleObject
///
/// @note In general, you will only access the functionality implemented in this class via
///       ConsoleObject::create(). Most of the time, you will only ever need to use this part
///       part of the engine indirectly - ie, you will use the networking system or the console,
///       or ConsoleObject, and they will indirectly use this code. <b>The following discussion
///       is really only relevant for advanced engine users.</b>
///
/// @section AbstractClassRep_netstuff NetClasses and Class IDs
///
/// Torque supports a notion of group, type, and direction for objects passed over
/// the network. Class IDs are assigned sequentially per-group, per-type, so that, for instance,
/// the IDs assigned to Datablocks are seperate from the IDs assigned to NetObjects or NetEvents.
/// This can translate into significant bandwidth savings (especially since the size of the fields
/// for transmitting these bits are determined at run-time based on the number of IDs given out.
///
/// @section AbstractClassRep_details AbstractClassRep Internals
///
/// Much like ConsoleConstructor, ACR does some preparatory work at runtime before execution
/// is passed to main(). In actual fact, this preparatory work is done by the ConcreteClassRep
/// template. Let's examine this more closely.
///
/// If we examine ConsoleObject, we see that two macros must be used in the definition of a
/// properly integrated objects. From the ConsoleObject example:
///
/// @code
///      // This is from inside the class definition...
///      DECLARE_CONOBJECT(TorqueObject);
///
/// // And this is from outside the class definition...
/// IMPLEMENT_CONOBJECT(TorqueObject);
/// @endcode
///
/// What do these things actually do?
///
/// Not all that much, in fact. They expand to code something like this:
///
/// @code
///      // This is from inside the class definition...
///      static ConcreteClassRep<TorqueObject> dynClassRep;
///      static AbstractClassRep* getParentStaticClassRep();
///      static AbstractClassRep* getStaticClassRep();
///      virtual AbstractClassRep* getClassRep() const;
/// @endcode
///
/// @code
/// // And this is from outside the class definition...
/// AbstractClassRep* TorqueObject::getClassRep() const { return &TorqueObject::dynClassRep; }
/// AbstractClassRep* TorqueObject::getStaticClassRep() { return &dynClassRep; }
/// AbstractClassRep* TorqueObject::getParentStaticClassRep() { return Parent::getStaticClassRep(); }
/// ConcreteClassRep<TorqueObject> TorqueObject::dynClassRep("TorqueObject", 0, -1, 0);
/// @endcode
///
/// As you can see, getClassRep(), getStaticClassRep(), and getParentStaticClassRep() are just
/// accessors to allow access to various ConcreteClassRep instances. This is where the Parent
/// typedef comes into play as well - it lets getParentStaticClassRep() get the right
/// class rep.
///
/// In addition, dynClassRep is declared as a member of TorqueObject, and defined later
/// on. Much like ConsoleConstructor, ConcreteClassReps add themselves to a global linked
/// list in their constructor.
///
/// Then, when AbstractClassRep::initialize() is called, from Con::init(), we iterate through
/// the list and perform the following tasks:
///      - Sets up a Namespace for each class.
///      - Call the init() method on each ConcreteClassRep. This method:
///         - Links namespaces between parent and child classes, using Con::classLinkNamespaces.
///         - Calls initPersistFields() and consoleInit().
///      - As a result of calling initPersistFields, the field list for the class is populated.
///      - Assigns network IDs for classes based on their NetGroup membership. Determines
///        bit allocations for network ID fields.
///
/// @nosubgrouping
class AbstractClassRep
{
   friend class ConsoleObject;

public:

   /// @name 'Tructors
   /// @{

   AbstractClassRep() 
   {
      VECTOR_SET_ASSOCIATION(mFieldList);
      parentClass  = NULL;
      mIsRenderEnabled = true;
   }
   virtual ~AbstractClassRep() { }

   /// @}

   /// @name Representation Interface
   /// @{

   S32 mClassGroupMask;                ///< Mask indicating in which NetGroups this object belongs.
   S32 mClassType;                     ///< Stores the NetClass of this class.
   S32 mNetEventDir;                   ///< Stores the NetDirection of this class.
   S32 mClassId[NetClassGroupsCount];  ///< Stores the IDs assigned to this class for each group.
#ifdef TORQUE_DEBUG
   S32 mClassSizeof;
#endif
#ifdef TORQUE_NET_STATS
   struct NetStatInstance
   {
      U32 numEvents;
      U32 total;
      S32 min;
      S32 max;

      void reset()
      {
         numEvents = 0;
         total = 0;
         min = S32_MAX;
         max = S32_MIN;
      }

      void update(U32 amount)
      {
         numEvents++;
         total += amount;
         min = getMin((S32)amount, min);
         max = getMax((S32)amount, max);
      }

      NetStatInstance()
      {
         reset();
      }
   };

   NetStatInstance mNetStatPack;
   NetStatInstance mNetStatUnpack;
   NetStatInstance mNetStatWrite;
   NetStatInstance mNetStatRead;

   U32 mDirtyMaskFrequency[32];
   U32 mDirtyMaskTotal[32];

   void resetNetStats()
   {
      mNetStatPack.reset();
      mNetStatUnpack.reset();
      mNetStatWrite.reset();
      mNetStatRead.reset();

      for(S32 i=0; i<32; i++)
      {
         mDirtyMaskFrequency[i] = 0;
         mDirtyMaskTotal[i] = 0;
      }
   }

   void updateNetStatPack(U32 dirtyMask, U32 length)
   {
      mNetStatPack.update(length);

      for(S32 i=0; i<32; i++)
         if(BIT(i) & dirtyMask)
         {
            mDirtyMaskFrequency[i]++;
            mDirtyMaskTotal[i] += length;
         }
   }

   void updateNetStatUnpack(U32 length)
   {
      mNetStatUnpack.update(length);
   }

   void updateNetStatWriteData(U32 length)
   {
      mNetStatWrite.update(length);
   }

   void updateNetStatReadData(U32 length)
   {
      mNetStatRead.update(length);
   }
#endif
   S32                        getClassId  (U32 netClassGroup)   const;
   static U32                 getClassCRC (U32 netClassGroup);
   const char*                getClassName() const;
   static AbstractClassRep*   getClassList();
   Namespace*                 getNameSpace();
   AbstractClassRep*          getNextClass();
   AbstractClassRep*          getParentClass();
   AbstractClassRep*          getCommonParent( const AbstractClassRep *otherClass ) const;
#ifdef TORQUE_DEBUG  
   S32                        getSizeof() const;
#endif
   /// Helper class to see if we are a given class, or a subclass thereof.
   bool                       isClass(AbstractClassRep  *acr)
   {
      AbstractClassRep  *walk = this;

      //  Walk up parents, checking for equivalence.
      while(walk)
      {
         if(walk == acr)
            return true;

         walk = walk->parentClass;
      };

      return false;
   }

   virtual ConsoleObject*     create      () const = 0;

protected:

   virtual void init();

   const char *       mClassName;
   AbstractClassRep * nextClass;
   AbstractClassRep * parentClass;
   Namespace *        mNamespace;

   /// @}
   
public:

   bool mIsRenderEnabled;

   bool isRenderEnabled() const { return mIsRenderEnabled; }
   
   /// @name Categories
   
protected:

   String mCategory;
   String mDescription;
   
public:

   const String& getCategory() const { return mCategory; }
   const String& getDescription() const { return mDescription; }

   /// @name Fields
   /// @{
public:

   /// This is a function pointer typedef to support get/set callbacks for fields
   typedef bool (*SetDataNotify)( void *obj, const char *data );
   typedef const char *(*GetDataNotify)( void *obj, const char *data );

   /// These are special field type values used to mark
   /// groups and arrays in the field list.
   /// @see Field::type
   /// @see addArray, endArray
   /// @see addGroup, endGroup   
   /// @see addGroup, endGroup   
   /// @see addDeprecatedField
   enum ACRFieldTypes
   {
      /// The first custom field type... all fields
      /// types greater or equal to this one are not
      /// console data types.
      ARCFirstCustomField = 0xFFFFFFFB,
      
      /// Marks the start of a fixed size array of fields.
      /// @see addArray
      StartArrayFieldType = 0xFFFFFFFB,
      
      /// Marks the end of a fixed size array of fields.
      /// @see endArray
      EndArrayFieldType   = 0xFFFFFFFC,
      
      /// Marks the beginning of a group of fields.
      /// @see addGroup
      StartGroupFieldType = 0xFFFFFFFD,
      
      /// Marks the beginning of a group of fields.
      /// @see endGroup
      EndGroupFieldType   = 0xFFFFFFFE,
      
      /// Marks a field that is depreciated and no 
      /// longer stores a value.
      /// @see addDeprecatedField
      DeprecatedFieldType = 0xFFFFFFFF
   };

   struct Field 
   {
      const char* pFieldname;    ///< Name of the field.
      const char* pGroupname;      ///< Optionally filled field containing the group name.
                                 ///
                                 ///  This is filled when type is StartField or EndField

      const char*    pFieldDocs;    ///< Documentation about this field; see consoleDoc.cc.
      bool           groupExpand;   ///< Flag to track expanded/not state of this group in the editor.
      U32            type;          ///< A data type ID or one of the special custom fields. @see ACRFieldTypes
      U32            offset;        ///< Memory offset from beginning of class for this field.
      S32            elementCount;  ///< Number of elements, if this is an array.
      const EnumTable *    table;   ///< If this is an enum, this points to the table defining it.
      BitSet32       flag;          ///< Stores various flags
      TypeValidator *validator;     ///< Validator, if any.
      SetDataNotify  setDataFn;     ///< Set data notify Fn
      GetDataNotify  getDataFn;     ///< Get data notify Fn
   };
   typedef Vector<Field> FieldList;

   FieldList mFieldList;

   bool mDynamicGroupExpand;

   const Field* findField( StringTableEntry fieldName ) const;

   /// @}

   /// @name Abstract Class Database
   /// @{

protected:
   static AbstractClassRep ** classTable[NetClassGroupsCount][NetClassTypesCount];
   static AbstractClassRep *  classLinkList;
   static U32                 classCRC[NetClassGroupsCount];
   static bool                initialized;

   static ConsoleObject* create(const char*  in_pClassName);
   static ConsoleObject* create(const U32 groupId, const U32 typeId, const U32 in_classId);

public:
   static U32  NetClassCount [NetClassGroupsCount][NetClassTypesCount];
   static U32  NetClassBitSize[NetClassGroupsCount][NetClassTypesCount];

   static void registerClassRep(AbstractClassRep*);
   static AbstractClassRep* findClassRep(const char* in_pClassName);
   static void removeClassRep(AbstractClassRep*); // This should not be used lightly
   static void initialize(); // Called from Con::init once on startup
   static void shutdown();


   /// @}
};

extern AbstractClassRep::FieldList sg_tempFieldList;

inline AbstractClassRep *AbstractClassRep::getClassList()
{
   return classLinkList;
}

inline U32 AbstractClassRep::getClassCRC(U32 group)
{
   return classCRC[group];
}

inline AbstractClassRep *AbstractClassRep::getNextClass()
{
   return nextClass;
}

inline AbstractClassRep *AbstractClassRep::getParentClass()
{
   return parentClass;
}

inline S32 AbstractClassRep::getClassId(U32 group) const
{
   return mClassId[group];
}

inline const char* AbstractClassRep::getClassName() const
{
   return mClassName;
}

inline Namespace *AbstractClassRep::getNameSpace()
{
   return mNamespace;
}

#ifdef   TORQUE_DEBUG
inline S32 AbstractClassRep::getSizeof() const
{
   return mClassSizeof;
}
#endif 
//------------------------------------------------------------------------------
//-------------------------------------- ConcreteClassRep
//


/// Helper class for AbstractClassRep.
///
/// @see AbtractClassRep
/// @see ConsoleObject
template <class T>
class ConcreteClassRep : public AbstractClassRep
{
public:
   ConcreteClassRep(const char *name, S32 netClassGroupMask, S32 netClassType, S32 netEventDir, AbstractClassRep *parent, const char* ( *parentDesc )() )
   {
      mClassName = name; // name is a static compiler string so no need to worry about copying or deleting
      mCategory = T::__category();
      
      if( &T::__description != parentDesc )
         mDescription = T::__description();

      // Clean up mClassId
      for(U32 i = 0; i < NetClassGroupsCount; i++)
         mClassId[i] = -1;

      // Set properties for this ACR
      mClassType      = netClassType;
      mClassGroupMask = netClassGroupMask;
      mNetEventDir    = netEventDir;
      parentClass     = parent;
#ifdef TORQUE_DEBUG
      mClassSizeof    = sizeof(T);
#endif
      // Finally, register ourselves.
      registerClassRep(this);
   };

   /// Perform class specific initialization tasks.
   ///
   /// Link namespaces, call initPersistFields() and consoleInit().
   void init()
   {
      // Get handle to our parent class, if any, and ourselves (we are our parent's child).
      AbstractClassRep *parent = T::getParentStaticClassRep();
      AbstractClassRep *child  = T::getStaticClassRep();

      // If we got reps, then link those namespaces! (To get proper inheritance.)
      if(parent && child)
         Con::classLinkNamespaces(parent->getNameSpace(), child->getNameSpace());

      // Finally, do any class specific initialization...
      T::initPersistFields();
      T::consoleInit();

      // Let the base finish up.
      AbstractClassRep::init();
   }

   /// Wrap constructor.
   ConsoleObject* create() const { return new T; }
};

//------------------------------------------------------------------------------
// Forward declaration of this function so  it can be used in the class
const char *defaultProtectedGetFn( void *obj, const char *data );

/// Interface class to the console.
///
/// @section ConsoleObject_basics The Basics
///
/// Any object which you want to work with the console system should derive from this,
/// and access functionality through the static interface.
///
/// This class is always used with the DECLARE_CONOBJECT and IMPLEMENT_* macros.
///
/// @code
/// // A very basic example object. It will do nothing!
/// class TorqueObject : public ConsoleObject {
///      // Must provide a Parent typedef so the console system knows what we inherit from.
///      typedef ConsoleObject Parent;
///
///      // This does a lot of menial declaration for you.
///      DECLARE_CONOBJECT(TorqueObject);
///
///      // This is for us to register our fields in.
///      static void initPersistFields();
///
///      // A sample field.
///      S8 mSample;
/// }
/// @endcode
///
/// @code
/// // And the accordant implementation...
/// IMPLEMENT_CONOBJECT(TorqueObject);
///
/// void TorqueObject::initPersistFields()
/// {
///   // If you want to inherit any fields from the parent (you do), do this:
///   Parent::initPersistFields();
///
///   // Pass the field, the type, the offset,                  and a usage string.
///   addField("sample", TypeS8, Offset(mSample, TorqueObject), "A test field.");
/// }
/// @endcode
///
/// That's all you need to do to get a class registered with the console system. At this point,
/// you can instantiate it via script, tie methods to it using ConsoleMethod, register fields,
/// and so forth. You can also register any global variables related to the class by creating
/// a consoleInit() method.
///
/// You will need to use different IMPLEMENT_ macros in different cases; for instance, if you
/// are making a NetObject (for ghosting), a DataBlock, or a NetEvent.
///
/// @see AbstractClassRep for gory implementation details.
/// @nosubgrouping
class ConsoleObject
{
protected:

   /// @deprecated This is disallowed.
   ConsoleObject();
   /// @deprecated This is disallowed.
   ConsoleObject(const ConsoleObject&);

public:

   /// Get a reference to a field by name.
   const AbstractClassRep::Field *findField(StringTableEntry fieldName) const;

   /// Gets the ClassRep.
   virtual AbstractClassRep* getClassRep() const;

   /// Set the value of a field.
   bool setField(const char *fieldName, const char *value);
   virtual ~ConsoleObject();

   /// Return a string that describes this instance.  Meant primarily for debugging.
   virtual String describeSelf();
   
public:

   /// @name Object Creation
   /// @{
   static ConsoleObject* create(const char*  in_pClassName);
   static ConsoleObject* create(const U32 groupId, const U32 typeId, const U32 in_classId);
   /// @}

public:

   /// Get the classname from a class tag.
   static const char* lookupClassName(const U32 in_classTag);

   /// @name Fields
   /// @{

   /// Mark the beginning of a group of fields.
   ///
   /// This is used in the consoleDoc system.
   /// @see console_autodoc
   static void addGroup(const char*  in_pGroupname, const char* in_pGroupDocs = NULL);

   /// Mark the end of a group of fields.
   ///
   /// This is used in the consoleDoc system.
   /// @see console_autodoc
   static void endGroup(const char*  in_pGroupname);

   /// Marks the start of a fixed size array of fields.
   /// @see console_autodoc   
   static void addArray( const char *arrayName, S32 count );

   /// Marks the end of an array of fields.
   /// @see console_autodoc      
   static void endArray( const char *arrayName );

   /// Register a complex field.
   ///
   /// @param  in_pFieldname     Name of the field.
   /// @param  in_fieldType      Type of the field. @see ConsoleDynamicTypes
   /// @param  in_fieldOffset    Offset to  the field from the start of the class; calculated using the Offset() macro.
   /// @param  in_elementCount   Number of elements in this field. Arrays of elements are assumed to be contiguous in memory.
   /// @param  in_table          An EnumTable, if this is an enumerated field.
   /// @param  in_pFieldDocs     Usage string for this field. @see console_autodoc
   static void addField(const char*   in_pFieldname,
      const U32     in_fieldType,
      const dsize_t in_fieldOffset,
      const U32     in_elementCount = 1,
      const EnumTable *   in_table  = NULL,
      const char*   in_pFieldDocs   = NULL);

   /// Register a simple field.
   ///
   /// @param  in_pFieldname  Name of the field.
   /// @param  in_fieldType   Type of the field. @see ConsoleDynamicTypes
   /// @param  in_fieldOffset Offset to  the field from the start of the class; calculated using the Offset() macro.
   /// @param  in_pFieldDocs  Usage string for this field. @see console_autodoc
   static void addField(const char*   in_pFieldname,
      const U32     in_fieldType,
      const dsize_t in_fieldOffset,
      const char*   in_pFieldDocs);

   /// Register a validated field.
   ///
   /// A validated field is just like a normal field except that you can't
   /// have it be an array, and that you give it a pointer to a TypeValidator
   /// subclass, which is then used to validate any value placed in it. Invalid
   /// values are ignored and an error is printed to the console.
   ///
   /// @see addField
   /// @see typeValidators.h
   static void addFieldV(const char*   in_pFieldname,
      const U32      in_fieldType,
      const dsize_t  in_fieldOffset,
      TypeValidator *v,
      const char *   in_pFieldDocs = NULL);

   /// Register a complex protected field.
   ///
   /// @param  in_pFieldname     Name of the field.
   /// @param  in_fieldType      Type of the field. @see ConsoleDynamicTypes
   /// @param  in_fieldOffset    Offset to  the field from the start of the class; calculated using the Offset() macro.
   /// @param  in_setDataFn      When this field gets set, it will call the callback provided. @see console_protected
   /// @param  in_getDataFn      When this field is accessed for it's data, it will return the value of this function
   /// @param  in_elementCount   Number of elements in this field. Arrays of elements are assumed to be contiguous in memory.
   /// @param  in_table          An EnumTable, if this is an enumerated field.
   /// @param  in_pFieldDocs     Usage string for this field. @see console_autodoc
   static void addProtectedField(const char*   in_pFieldname,
      const U32     in_fieldType,
      const dsize_t in_fieldOffset,
      AbstractClassRep::SetDataNotify in_setDataFn,
      AbstractClassRep::GetDataNotify in_getDataFn = &defaultProtectedGetFn,
      const U32     in_elementCount = 1,
      const EnumTable *   in_table  = NULL,
      const char*   in_pFieldDocs   = NULL);

   /// Register a simple protected field.
   ///
   /// @param  in_pFieldname  Name of the field.
   /// @param  in_fieldType   Type of the field. @see ConsoleDynamicTypes
   /// @param  in_fieldOffset Offset to  the field from the start of the class; calculated using the Offset() macro.
   /// @param  in_setDataFn   When this field gets set, it will call the callback provided. @see console_protected
   /// @param  in_getDataFn   When this field is accessed for it's data, it will return the value of this function
   /// @param  in_pFieldDocs  Usage string for this field. @see console_autodoc
   static void addProtectedField(const char*   in_pFieldname,
      const U32     in_fieldType,
      const dsize_t in_fieldOffset,
      AbstractClassRep::SetDataNotify in_setDataFn,
      AbstractClassRep::GetDataNotify in_getDataFn = &defaultProtectedGetFn,
      const char*   in_pFieldDocs = NULL);

   /// Add a deprecated field.
   ///
   /// A deprecated field will always be undefined, even if you assign a value to it. This
   /// is useful when you need to make sure that a field is not being used anymore.
   static void addDeprecatedField(const char *fieldName);

   /// Remove a field.
   ///
   /// Sometimes, you just have to remove a field!
   /// @returns True on success.
   static bool removeField(const char* in_pFieldname);

   /// @}
   
   /// @name Logging
   /// @{
   
   /// Overload this in subclasses to change the message formatting.
   /// @param fmt A printf style format string.
   /// @param args A va_list containing the args passed ot a log function.
   /// @note It is suggested that you use String::VToString.
   virtual String _getLogMessage(const char* fmt, void* args) const;
   
   /// @}

public:

   /// @name Logging
   /// These functions will try to print out a message along the lines
   /// of "ObjectClass - ObjectName(ObjectId) - formatted message"
   /// @{
   
   /// Logs with Con::printf.
   void logMessage(const char* fmt, ...) const;
   
   /// Logs with Con::warnf.
   void logWarning(const char* fmt, ...) const;
   
   /// Logs with Con::errorf.
   void logError(const char* fmt, ...) const;
   
   /// @}

   /// Register dynamic fields in a subclass of ConsoleObject.
   ///
   /// @see addField(), addFieldV(), addDeprecatedField(), addGroup(), endGroup()
   static void initPersistFields();

   /// Register global constant variables and do other one-time initialization tasks in
   /// a subclass of ConsoleObject.
   ///
   /// @deprecated You should use ConsoleMethod and ConsoleFunction, not this, to
   ///             register methods or commands.
   /// @see console
   static void consoleInit();

   /// @name Field List
   /// @{

   /// Get a list of all the fields. This information cannot be modified.
   const AbstractClassRep::FieldList& getFieldList() const;

   /// Get a list of all the fields, set up so we can modify them.
   ///
   /// @note This is a bad trick to pull if you aren't very careful,
   ///       since you can blast field data!
   AbstractClassRep::FieldList& getModifiableFieldList();

   /// Get a handle to a boolean telling us if we expanded the dynamic group.
   ///
   /// @see GuiInspector::Inspect()
   bool& getDynamicGroupExpand();
   /// @}

   /// @name ConsoleObject Implementation
   ///
   /// These functions are implemented in every subclass of
   /// ConsoleObject by an IMPLEMENT_CONOBJECT or IMPLEMENT_CO_* macro.
   /// @{

   /// Get the abstract class information for this class.
   static AbstractClassRep *getStaticClassRep() { return NULL; }

   /// Get the abstract class information for this class's superclass.
   static AbstractClassRep *getParentStaticClassRep() { return NULL; }

   /// Get our network-layer class id.
   ///
   /// @param  netClassGroup  The net class for which we want our ID.
   /// @see
   S32 getClassId(U32 netClassGroup) const;

   /// Get our compiler and platform independent class name.
   ///
   /// @note This name can be used to instantiate another instance using create()
   const char *getClassName() const;

   /// @}

   static const char* __category() { return ""; }
   static const char* __description() { return ""; }

#ifdef TORQUE_DEBUG
   /// @name Instance Tracking (debugging only)
   /// @note This is NOT thread-safe.
   /// @{

public:
   typedef void ( *DebugEnumInstancesCallback )( ConsoleObject* );

   /// Dump describeSelf()s of all live ConsoleObjects to the console.
   static void debugDumpInstances();

   /// Call the given callback for all instances of the given type.
   /// Callback may also be NULL in which case the method just iterates
   /// over all instances of the given type.  This is useful for setting
   /// a breakpoint during debugging.
   static void debugEnumInstances( const std::type_info& type, DebugEnumInstancesCallback callback );

private:
   ConsoleObject*          mNextConObject;
   ConsoleObject*          mPrevConObject;

   static U32              smNumConObjects;
   static ConsoleObject*   smFirstConObject;

   /// @}
#endif
};

#define addNamedField(fieldName,type,className) addField(#fieldName, type, Offset(fieldName,className))
#define addNamedFieldV(fieldName,type,className, validator) addFieldV(#fieldName, type, Offset(fieldName,className), validator)


//------------------------------------------------------------------------------
//-------------------------------------- Inlines
//
inline S32 ConsoleObject::getClassId(U32 netClassGroup) const
{
   AssertFatal(getClassRep() != NULL,"Cannot get tag from non-declared dynamic class!");
   return getClassRep()->getClassId(netClassGroup);
}

inline const char * ConsoleObject::getClassName() const
{
   AssertFatal(getClassRep() != NULL,
      "Cannot get tag from non-declared dynamic class");
   return getClassRep()->getClassName();
}

inline const AbstractClassRep::Field * ConsoleObject::findField(StringTableEntry name) const
{
   AssertFatal(getClassRep() != NULL,
      avar("Cannot get field '%s' from non-declared dynamic class.", name));
   return getClassRep()->findField(name);
}

inline bool ConsoleObject::setField(const char *fieldName, const char *value)
{
   //sanity check
   if ((! fieldName) || (! fieldName[0]) || (! value))
      return false;

   if (! getClassRep())
      return false;

   const AbstractClassRep::Field *myField = getClassRep()->findField(StringTable->insert(fieldName));

   if (! myField)
      return false;

   Con::setData(
      myField->type,
      (void *) (((const char *)(this)) + myField->offset),
      0,
      1,
      &value,
      myField->table,
      myField->flag);

   return true;
}

inline ConsoleObject* ConsoleObject::create(const char* in_pClassName)
{
   return AbstractClassRep::create(in_pClassName);
}

inline ConsoleObject* ConsoleObject::create(const U32 groupId, const U32 typeId, const U32 in_classId)
{
   return AbstractClassRep::create(groupId, typeId, in_classId);
}

inline const AbstractClassRep::FieldList& ConsoleObject::getFieldList() const
{
   return getClassRep()->mFieldList;
}

inline AbstractClassRep::FieldList& ConsoleObject::getModifiableFieldList()
{
   return getClassRep()->mFieldList;
}

inline bool& ConsoleObject::getDynamicGroupExpand()
{
   return getClassRep()->mDynamicGroupExpand;
}

/// @name ConsoleObject Macros
/// @{

#define DECLARE_CONOBJECT(className)                    \
   static ConcreteClassRep<className> dynClassRep;      \
   static AbstractClassRep* getParentStaticClassRep();  \
   static AbstractClassRep* getStaticClassRep();        \
   virtual AbstractClassRep* getClassRep() const
   
#define DECLARE_CATEGORY( string )                      \
   static const char* __category() { return string; }
   
#define DECLARE_DESCRIPTION( string )                   \
   static const char* __description() { return string; }

#define IMPLEMENT_CONOBJECT(className)                                                            \
   AbstractClassRep* className::getClassRep() const { return &className::dynClassRep; }           \
   AbstractClassRep* className::getStaticClassRep() { return &dynClassRep; }                      \
   AbstractClassRep* className::getParentStaticClassRep() { return Parent::getStaticClassRep(); } \
   ConcreteClassRep<className> className::dynClassRep(#className, 0, -1, 0, className::getParentStaticClassRep(), &Parent::__description)

#define IMPLEMENT_CO_NETOBJECT_V1(className)                    \
   AbstractClassRep* className::getClassRep() const { return &className::dynClassRep; }                 \
   AbstractClassRep* className::getStaticClassRep() { return &dynClassRep; }                            \
   AbstractClassRep* className::getParentStaticClassRep() { return Parent::getStaticClassRep(); }       \
   ConcreteClassRep<className> className::dynClassRep(#className, NetClassGroupGameMask, NetClassTypeObject, 0, className::getParentStaticClassRep(), &Parent::__description)

#define IMPLEMENT_CO_DATABLOCK_V1(className)                                                            \
   AbstractClassRep* className::getClassRep() const { return &className::dynClassRep; }                 \
   AbstractClassRep* className::getStaticClassRep() { return &dynClassRep; }                            \
   AbstractClassRep* className::getParentStaticClassRep() { return Parent::getStaticClassRep(); }       \
   ConcreteClassRep<className> className::dynClassRep(#className, NetClassGroupGameMask, NetClassTypeDataBlock, 0, className::getParentStaticClassRep(), &Parent::__description)

/// @}

//------------------------------------------------------------------------------
// Protected field default get/set functions
//
// The reason for these functions is that it will save one branch per console 
// data request and script functions will still execute at the same speed as
// before the modifications to allow protected static fields. These will just
// inline and the code should be roughly the same size, and just as fast as
// before the modifications. -pw
inline bool defaultProtectedSetFn( void *obj, const char *data )
{
   return true;
}

inline const char *defaultProtectedGetFn( void *obj, const char *data )
{
   return data;
}

inline const char *emptyStringProtectedGetFn( void *obj, const char *data )
{
   return "";
}

#endif //_CONSOLEOBJECT_H_
