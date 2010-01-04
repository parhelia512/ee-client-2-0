//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SIM_H_
#include "console/sim.h"
#endif

#include "console/consoleObject.h"
#include "core/bitSet.h"

#ifndef _SIMOBJECT_H_
#define _SIMOBJECT_H_

// Forward Refs
class Stream;
class LightManager;
class SimFieldDictionary;

/// Base class for objects involved in the simulation.
///
/// @section simobject_intro Introduction
///
/// SimObject is a base class for most of the classes you'll encounter
/// working in Torque. It provides fundamental services allowing "smart"
/// object referencing, creation, destruction, organization, and location.
/// Along with SimEvent, it gives you a flexible event-scheduling system,
/// as well as laying the foundation for the in-game editors, GUI system,
/// and other vital subsystems.
///
/// @section simobject_subclassing Subclassing
///
/// You will spend a lot of your time in Torque subclassing, or working
/// with subclasses of, SimObject. SimObject is designed to be easy to
/// subclass.
///
/// You should not need to override anything in a subclass except:
///     - The constructor/destructor.
///     - processArguments()
///     - onAdd()/onRemove()
///     - onGroupAdd()/onGroupRemove()
///     - onNameChange()
///     - onStaticModified()
///     - onDeleteNotify()
///     - onEditorEnable()/onEditorDisable()
///     - inspectPreApply()/inspectPostApply()
///     - things from ConsoleObject (see ConsoleObject docs for specifics)
///
/// Of course, if you know what you're doing, go nuts! But in most cases, you
/// shouldn't need to touch things not on that list.
///
/// When you subclass, you should define a typedef in the class, called Parent,
/// that references the class you're inheriting from.
///
/// @code
/// class mySubClass : public SimObject {
///     typedef SimObject Parent;
///     ...
/// @endcode
///
/// Then, when you override a method, put in:
///
/// @code
/// bool mySubClass::onAdd()
/// {
///     if(!Parent::onAdd())
///         return false;
///
///     // ... do other things ...
/// }
/// @endcode
///
/// Of course, you want to replace onAdd with the appropriate method call.
///
/// @section simobject_lifecycle A SimObject's Life Cycle
///
/// SimObjects do not live apart. One of the primary benefits of using a
/// SimObject is that you can uniquely identify it and easily find it (using
/// its ID). Torque does this by keeping a global hierarchy of SimGroups -
/// a tree - containing every registered SimObject. You can then query
/// for a given object using Sim::findObject() (or SimSet::findObject() if
/// you want to search only a specific set).
///
/// @code
///        // Three examples of registering an object.
///
///        // Method 1:
///        AIClient *aiPlayer = new AIClient();
///        aiPlayer->registerObject();
///
///        // Method 2:
///        ActionMap* globalMap = new ActionMap;
///        globalMap->registerObject("GlobalActionMap");
///
///        // Method 3:
///        bool reg = mObj->registerObject(id);
/// @endcode
///
/// Registering a SimObject performs these tasks:
///     - Marks the object as not cleared and not removed.
///     - Assigns the object a unique SimObjectID if it does not have one already.
///     - Adds the object to the global name and ID dictionaries so it can be found
///       again.
///     - Calls the object's onAdd() method. <b>Note:</b> SimObject::onAdd() performs
///       some important initialization steps. See @ref simobject_subclassing "here
///       for details" on how to properly subclass SimObject.
///     - If onAdd() fails (returns false), it calls unregisterObject().
///     - Checks to make sure that the SimObject was properly initialized (and asserts
///       if not).
///
/// Calling registerObject() and passing an ID or a name will cause the object to be
/// assigned that name and/or ID before it is registered.
///
/// Congratulations, you have now registered your object! What now?
///
/// Well, hopefully, the SimObject will have a long, useful life. But eventually,
/// it must die.
///
/// There are a two ways a SimObject can die.
///         - First, the game can be shut down. This causes the root SimGroup
///           to be unregistered and deleted. When a SimGroup is unregistered,
///           it unregisters all of its member SimObjects; this results in everything
///           that has been registered with Sim being unregistered, as everything
///           registered with Sim is in the root group.
///         - Second, you can manually kill it off, either by calling unregisterObject()
///           or by calling deleteObject().
///
/// When you unregister a SimObject, the following tasks are performed:
///     - The object is flagged as removed.
///     - Notifications are cleaned up.
///     - If the object is in a group, then it removes itself from the group.
///     - Delete notifications are sent out.
///     - Finally, the object removes itself from the Sim globals, and tells
///       Sim to get rid of any pending events for it.
///
/// If you call deleteObject(), all of the above tasks are performed, in addition
/// to some sanity checking to make sure the object was previously added properly,
/// and isn't in the process of being deleted. After the object is unregistered, it
/// deallocates itself.
///
/// @section simobject_editor Torque Editors
///
/// SimObjects are one of the building blocks for the in-game editors. They
/// provide a basic interface for the editor to be able to list the fields
/// of the object, update them safely and reliably, and inform the object
/// things have changed.
///
/// This interface is implemented in the following areas:
///     - onNameChange() is called when the object is renamed.
///     - onStaticModified() is called whenever a static field is modified.
///     - inspectPreApply() is called before the object's fields are updated,
///                     when changes are being applied.
///     - inspectPostApply() is called after the object's fields are updated.
///     - onEditorEnable() is called whenever an editor is enabled (for instance,
///                     when you hit F11 to bring up the world editor).
///     - onEditorDisable() is called whenever the editor is disabled (for instance,
///                     when you hit F11 again to close the world editor).
///
/// (Note: you can check the variable gEditingMission to see if the mission editor
/// is running; if so, you may want to render special indicators. For instance, the
/// fxFoliageReplicator renders inner and outer radii when the mission editor is
/// runnning.)
///
/// @section simobject_console The Console
///
/// SimObject extends ConsoleObject by allowing you to
/// to set arbitrary dynamic fields on the object, as well as
/// statically defined fields. This is done through two methods,
/// setDataField and getDataField, which deal with the complexities of
/// allowing access to two different types of object fields.
///
/// Static fields take priority over dynamic fields. This is to be
/// expected, as the role of dynamic fields is to allow data to be
/// stored in addition to the predefined fields.
///
/// The fields in a SimObject are like properties (or fields) in a class.
///
/// Some fields may be arrays, which is what the array parameter is for; if it's non-null,
/// then it is parsed with dAtoI and used as an index into the array. If you access something
/// as an array which isn't, then you get an empty string.
///
/// <b>You don't need to read any further than this.</b> Right now,
/// set/getDataField are called a total of 6 times through the entire
/// Torque codebase. Therefore, you probably don't need to be familiar
/// with the details of accessing them. You may want to look at Con::setData
/// instead. Most of the time you will probably be accessing fields directly,
/// or using the scripting language, which in either case means you don't
/// need to do anything special.
///
/// The functions to get/set these fields are very straightforward:
///
/// @code
///  setDataField(StringTable->insert("locked", false), NULL, b ? "true" : "false" );
///  curObject->setDataField(curField, curFieldArray, STR.getStringValue());
///  setDataField(slotName, array, value);
/// @endcode
///
/// <i>For advanced users:</i> There are two flags which control the behavior
/// of these functions. The first is ModStaticFields, which controls whether
/// or not the DataField functions look through the static fields (defined
/// with addField; see ConsoleObject for details) of the class. The second
/// is ModDynamicFields, which controls dynamically defined fields. They are
/// set automatically by the console constructor code.
///
/// @nosubgrouping
class SimObject: public ConsoleObject
{
   typedef ConsoleObject Parent;

   friend class SimManager;
   friend class SimGroup;
   friend class SimNameDictionary;
   friend class SimManagerNameDictionary;
   friend class SimIdDictionary;

   //-------------------------------------- Structures and enumerations
private:

   /// Flags for use in mFlags
   enum {
      Deleted   = BIT(0),   ///< This object is marked for deletion.
      Removed   = BIT(1),   ///< This object has been unregistered from the object system.
      Added     = BIT(3),   ///< This object has been registered with the object system.
      Selected  = BIT(4),   ///< This object has been marked as selected. (in editor)
      Expanded  = BIT(5),   ///< This object has been marked as expanded. (in editor)
      ModStaticFields  = BIT(6),    ///< The object allows you to read/modify static fields
      ModDynamicFields = BIT(7),    ///< The object allows you to read/modify dynamic fields
      AutoDelete = BIT( 8 ),        ///< Delete this object when the last ObjectRef is gone.
   };
public:
   /// @name Notification
   /// @{
   struct Notify {
      enum Type {
         ClearNotify,   ///< Notified when the object is cleared.
         DeleteNotify,  ///< Notified when the object is deleted.
         ObjectRef,     ///< Cleverness to allow tracking of references.
         Invalid        ///< Mark this notification as unused (used in freeNotify).
      } type;
      void *ptr;        ///< Data (typically referencing or interested object).
      Notify *next;     ///< Next notification in the linked list.
   };

   /// @}

   /// Flags passed to SimObject::write 
   enum WriteFlags {
      SelectedOnly = BIT(0), ///< Indicates that only objects marked as selected should be outputted. Used in SimSet.
      NoName       = BIT(1)  ///< Indicates that the object name should not be saved.
   };

private:
   // dictionary information stored on the object
   StringTableEntry objectName;
   StringTableEntry mOriginalName;
   SimObject*       nextNameObject;
   SimObject*       nextManagerNameObject;
   SimObject*       nextIdObject;

   SimGroup*   mGroup;  ///< SimGroup we're contained in, if any.
   BitSet32    mFlags;

   /// @name Notification
   /// @{
   Notify*     mNotifyList;
   /// @}

   Vector<StringTableEntry> mFieldFilter;

   // File this SimObject was loaded from
   StringTableEntry mFilename;

   // The line number that the object was
   // declared on if it was loaded from a file
   S32 mDeclarationLine;

protected:
   SimObjectId mId;         ///< Id number for this object.
   Namespace*  mNameSpace;
   U32         mTypeMask;

   static bool          mForceId;   ///< Force a registered object to use the given Id.  Cleared upon use.
   static SimObjectId   mForcedId;  ///< The Id to force upon the object.  Poor object.

protected:
   /// @name Notification
   /// Helper functions for notification code.
   /// @{

   static SimObject::Notify *mNotifyFreeList;
   static SimObject::Notify *allocNotify();     ///< Get a free Notify structure.
   static void freeNotify(SimObject::Notify*);  ///< Mark a Notify structure as free.

   /// @}

private:
   SimFieldDictionary *mFieldDictionary;    ///< Storage for dynamic fields.

protected:
   bool	mCanSaveFieldDictionary; ///< true if dynamic fields (added at runtime) should be saved, defaults to true
   StringTableEntry mInternalName; ///< Stores object Internal Name

   // Namespace linking
   StringTableEntry mClassName;     ///< Stores the class name to link script class namespaces
   StringTableEntry mSuperClassName;   ///< Stores super class name to link script class namespaces

   bool mEnabled;   ///< Flag used to indicate whether object is enabled or not.

   // Namespace protected set methods
   static bool setClass(void* obj, const char* data)          { static_cast<SimObject*>(obj)->setClassNamespace(data); return false; };
   static bool setSuperClass(void* obj, const char* data)     { static_cast<SimObject*>(obj)->setSuperClassNamespace(data); return false; };

   // Group hierarchy protected set method 
   static bool setProtectedParent(void* obj, const char* data);

   // Object name protected set method
   static bool setProtectedName(void* obj, const char* data);
   
   /// We can provide more detail, like object name and id.
   virtual String _getLogMessage(const char* fmt, void* args) const;

   // Accessors
public:
   virtual void setEnabled( const bool enabled ) { mEnabled = enabled; }
   bool isEnabled() const { return mEnabled; }

   StringTableEntry getClassNamespace() const { return mClassName; };
   StringTableEntry getSuperClassNamespace() const { return mSuperClassName; };
   void setClassNamespace( const char* classNamespace );
   void setSuperClassNamespace( const char* superClassNamespace );
   // By setting the value of mNSLinkMask in the constructor of a class that 
   // inherits from SimObject, you can specify how the namespaces are linked
   // for that class. An easy way to think about this change, if you have worked
   // with this in the past is that ScriptObject uses:
   //    mNSLinkMask = LinkSuperClassName | LinkClassName;
   // which will attempt to do a full namespace link checking mClassName and mSuperClassName
   // 
   // and BehaviorTemplate does not set the value of NSLinkMask, which means that
   // only the default link will be made, which is: ObjectName -> ClassName
   enum SimObjectNSLinkType
   {
      LinkClassName = BIT(0),
      LinkSuperClassName = BIT(1)
   };
   U8 mNSLinkMask;
   void linkNamespaces();
   void unlinkNamespaces();

public:
   virtual String describeSelf();

   /// @name Accessors
   /// @{

   /// Get the value of a field on the object.
   ///
   /// See @ref simobject_console "here" for a detailed discussion of what this
   /// function does.
   ///
   /// @param   slotName    Field to access.
   /// @param   array       String containing index into array
   ///                      (if field is an array); if NULL, it is ignored.
   const char *getDataField(StringTableEntry slotName, const char *array);

   /// Set the value of a field on the object.
   ///
   /// See @ref simobject_console "here" for a detailed discussion of what this
   /// function does.
   ///
   /// @param   slotName    Field to access.
   /// @param   array       String containing index into array; if NULL, it is ignored.
   /// @param   value       Value to store.
   void setDataField(StringTableEntry slotName, const char *array, const char *value);

   /// Get the type of a field on the object.
   ///
   /// @param   slotName    Field to access.
   /// @param   array       String containing index into array
   ///                      (if field is an array); if NULL, it is ignored.
   U32 getDataFieldType(StringTableEntry slotName, const char *array);

   /// Set the type of a *dynamic* field on the object.
   ///
   /// @param   typeName/Id Console base type name/id to assign to a dynamic field.
   /// @param   slotName    Field to access.
   /// @param   array       String containing index into array
   ///                      (if field is an array); if NULL, it is ignored.
   void setDataFieldType(const U32 fieldTypeId, StringTableEntry slotName, const char *array);
   void setDataFieldType(const char *typeName, StringTableEntry slotName, const char *array);

   /// Get reference to the dictionary containing dynamic fields.
   ///
   /// See @ref simobject_console "here" for a detailed discussion of what this
   /// function does.
   ///
   /// This dictionary can be iterated over using a SimFieldDictionaryIterator.
   SimFieldDictionary * getFieldDictionary() {return(mFieldDictionary);}

   // Component Information
   inline virtual StringTableEntry  getComponentName() { return StringTable->insert( getClassName() ); };

   /// Set whether fields created at runtime should be saved. Default is true.
   void		setCanSaveDynamicFields(bool bCanSave){ mCanSaveFieldDictionary	=	bCanSave;}
   /// Get whether fields created at runtime should be saved. Default is true.
   bool		getCanSaveDynamicFields(bool bCanSave){ return	mCanSaveFieldDictionary;}

   /// These functions support internal naming that is not namespace
   /// bound for locating child controls in a generic way.
   ///
   /// Set the internal name of this control (Not linked to a namespace)
   void setInternalName(const char* newname);

   /// Get the internal name of this control
   StringTableEntry getInternalName() const;

   /// Set the original name of this control
   void setOriginalName(const char* originalName);

   /// Get the original name of this control
   StringTableEntry getOriginalName() const;

   /// These functions allow you to set and access the filename
   /// where this object was created.
   ///
   /// Set the filename
   void setFilename(const char* file);

   /// Get the filename
   StringTableEntry getFilename() const;

   /// These functions are used to track the line number (1-based)
   /// on which the object was created if it was loaded from script
   ///
   /// Set the declaration line number
   void setDeclarationLine(U32 lineNumber);

   /// Get the declaration line number
   S32 getDeclarationLine() const;

   /// Save object as a TorqueScript File.
   virtual bool		save(const char* pcFilePath, bool bOnlySelected=false);

   /// Check if a method exists in the objects current namespace.
   virtual bool isMethod( const char* methodName );
   /// @}

   /// @name Initialization
   /// @{

   ///added this so that you can print the entire class hierarchy, including script objects, 
   //from the console or C++.
   virtual void			dumpClassHierarchy();
   ///
   SimObject( const U8 namespaceLinkMask = 0 );
   virtual ~SimObject();

   virtual bool processArguments(S32 argc, const char **argv);  ///< Process constructor options. (ie, new SimObject(1,2,3))

   /// @}

   /// @name Events
   /// @{
   virtual bool onAdd();                                ///< Called when the object is added to the sim.
   virtual void onRemove();                             ///< Called when the object is removed from the sim.
   virtual void onGroupAdd();                           ///< Called when the object is added to a SimGroup.
   virtual void onGroupRemove();                        ///< Called when the object is removed from a SimGroup.
   virtual void onNameChange(const char *name);         ///< Called when the object's name is changed.
   ///
   ///  Specifically, these are called by setDataField
   ///  when a static or dynamic field is modified, see
   ///  @ref simobject_console "the console details".
   virtual void onStaticModified(const char* slotName, const char*newValue = NULL); ///< Called when a static field is modified.
   virtual void onDynamicModified(const char* slotName, const char*newValue = NULL); ///< Called when a dynamic field is modified.

   /// Called before any property of the object is changed in the world editor.
   ///
   /// The calling order here is:
   ///  - inspectPreApply()
   ///  - ...
   ///  - calls to setDataField()
   ///  - ...
   ///  - inspectPostApply()
   virtual void inspectPreApply();

   /// Called after any property of the object is changed in the world editor.
   ///
   /// @see inspectPreApply
   virtual void inspectPostApply();

   /// Called when a SimObject is deleted.
   ///
   /// When you are on the notification list for another object
   /// and it is deleted, this method is called.
   virtual void onDeleteNotify(SimObject *object);

   /// Called when the editor is activated.
   virtual void onEditorEnable(){};

   /// Called when the editor is deactivated.
   virtual void onEditorDisable(){};

   /// @}

   /// Find a named sub-object of this object.
   ///
   /// This is subclassed in the SimGroup and SimSet classes.
   ///
   /// For a single object, it just returns NULL, as normal objects cannot have children.
   virtual SimObject *findObject(const char *name);

   /// @name Notification
   /// @{
   Notify *removeNotify(void *ptr, Notify::Type);   ///< Remove a notification from the list.
   void deleteNotify(SimObject* obj);               ///< Notify an object when we are deleted.
   void clearNotify(SimObject* obj);                ///< Notify an object when we are cleared.
   void clearAllNotifications();                    ///< Remove all notifications for this object.
   void processDeleteNotifies();                    ///< Send out deletion notifications.

   /// Register a reference to this object.
   ///
   /// You pass a pointer to your reference to this object.
   ///
   /// When the object is deleted, it will null your
   /// pointer, ensuring you don't access old memory.
   ///
   /// @param obj   Pointer to your reference to the object.
   void registerReference(SimObject **obj);

   /// Unregister a reference to this object.
   ///
   /// Remove a reference from the list, so that it won't
   /// get nulled inappropriately.
   ///
   /// Call this when you're done with your reference to
   /// the object, especially if you're going to free the
   /// memory. Otherwise, you may erroneously get something
   /// overwritten.
   ///
   /// @see registerReference
   void unregisterReference(SimObject **obj);

   /// @}

   /// @name Registration
   ///
   /// SimObjects must be registered with the object system.
   /// @{


   /// Register an object with the object system.
   ///
   /// This must be called if you want to keep the object around.
   /// In the rare case that you will delete the object immediately, or
   /// don't want to be able to use Sim::findObject to locate it, then
   /// you don't need to register it.
   ///
   /// registerObject adds the object to the global ID and name dictionaries,
   /// after first assigning it a new ID number. It calls onAdd(). If onAdd fails,
   /// it unregisters the object and returns false.
   ///
   /// If a subclass's onAdd doesn't eventually call SimObject::onAdd(), it will
   /// cause an assertion.
   bool registerObject();

   /// Register the object, forcing the id.
   ///
   /// @see registerObject()
   /// @param   id  ID to assign to the object.
   bool registerObject(U32 id);

   /// Register the object, assigning the name.
   ///
   /// @see registerObject()
   /// @param   name  Name to assign to the object.
   bool registerObject(const char *name);

   /// Register the object, assigning a name and ID.
   ///
   /// @see registerObject()
   /// @param   name  Name to assign to the object.
   /// @param   id  ID to assign to the object.
   bool registerObject(const char *name, U32 id);

   /// Unregister the object from Sim.
   ///
   /// This performs several operations:
   ///  - Sets the removed flag.
   ///  - Call onRemove()
   ///  - Clear out notifications.
   ///  - Remove the object from...
   ///      - its group, if any. (via getGroup)
   ///      - Sim::gNameDictionary
   ///      - Sim::gIDDictionary
   ///  - Finally, cancel any pending events for this object (as it can't receive them now).
   void unregisterObject();

   void deleteObject();     ///< Unregister, mark as deleted, and free the object.

   /// Performs a safe delayed delete of the object using a sim event.
   void safeDeleteObject();

   ///
   /// This helper function can be used when you're done with the object
   /// and don't want to be bothered with the details of cleaning it up.

   /// @}

   /// @name Accessors
   /// @{
   SimObjectId getId() const { return mId; }
   const char* getIdString() const;
   U32         getTypeMask() const { return mTypeMask; }
   U32         getType() const  { return mTypeMask; } ///< same as getTypeMask() - left around for legacy code
   const char* getName() const { return objectName; }

   void setId(SimObjectId id);
   static void setForcedId(SimObjectId id) {mForceId = true; mForcedId = id;} ///< Force an Id on the next registered object.
   void assignName(const char* name);
   SimGroup* getGroup() const { return mGroup; }
   bool isChildOfGroup(SimGroup* pGroup);
   bool isProperlyAdded() const { return mFlags.test(Added); }
   bool isDeleted() const { return mFlags.test(Deleted); }
   bool isRemoved() const { return mFlags.test(Deleted | Removed); }
   virtual bool isLocked() const;
   virtual void setLocked( bool b );
   virtual bool isHidden() const;
   virtual void setHidden(bool b);

   /// @}

   /// @name Sets
   ///
   /// The object must be properly registered before you can add/remove it to/from a set.
   ///
   /// All these functions accept either a name or ID to identify the set you wish
   /// to operate on. Then they call addObject or removeObject on the set, which
   /// sets up appropriate notifications.
   ///
   /// An object may be in multiple sets at a time.
   /// @{
   bool addToSet(SimObjectId);
   bool addToSet(const char *);
   bool removeFromSet(SimObjectId);
   bool removeFromSet(const char *);

   /// @}

   /// @name Serialization
   /// @{

   /// Determine whether or not a field should be written.
   ///
   /// @param   fiedname The name of the field being written.
   /// @param   value The value of the field.
   virtual bool writeField(StringTableEntry fieldname, const char* value);

   /// Output the TorqueScript to recreate this object.
   ///
   /// This calls writeFields internally.
   /// @param   stream  Stream to output to.
   /// @param   tabStop Indentation level for this object.
   /// @param   flags   If SelectedOnly is passed here, then
   ///                  only objects marked as selected (using setSelected)
   ///                  will output themselves.
   virtual void write(Stream &stream, U32 tabStop, U32 flags = 0);

   /// Write the fields of this object in TorqueScript.
   ///
   /// @param   stream  Stream for output.
   /// @param   tabStop Indentation level for the fields.
   virtual void writeFields(Stream &stream, U32 tabStop);

   virtual bool writeObject(Stream *stream);
   virtual bool readObject(Stream *stream);
   virtual void buildFilterList();

   void addFieldFilter(const char *fieldName);
   void removeFieldFilter(const char *fieldName);
   void clearFieldFilters();
   bool isFiltered(const char *fieldName);

   /// Copy fields from another object onto this one.
   ///
   /// Objects must be of same type. Everything from obj
   /// will overwrite what's in this object; extra fields
   /// in this object will remain. This includes dynamic
   /// fields.
   ///
   /// @param   obj Object to copy from.
   void assignFieldsFrom(SimObject *obj);

   /// Copy dynamic fields from another object onto this one.
   ///
   /// Everything from obj will overwrite what's in this
   /// object.
   ///
   /// @param   obj Object to copy from.
   void assignDynamicFieldsFrom(SimObject *obj);

   /// @}

   /// Return the object's namespace.
   Namespace* getNamespace() { return mNameSpace; }

   /// Get next matching item in namespace.
   ///
   /// This wraps a call to Namespace::tabComplete; it gets the
   /// next thing in the namespace, given a starting value
   /// and a base length of the string. See
   /// Namespace::tabComplete for details.
   const char *tabComplete(const char *prevText, S32 baseLen, bool);

   /// @name Accessors
   /// @{
   bool isSelected() const { return mFlags.test(Selected); }
   bool isExpanded() const { return mFlags.test(Expanded); }
   void setSelected(bool sel) { if(sel) mFlags.set(Selected); else mFlags.clear(Selected); }
   void setExpanded(bool exp) { if(exp) mFlags.set(Expanded); else mFlags.clear(Expanded); }
   void setModDynamicFields(bool dyn) { if(dyn) mFlags.set(ModDynamicFields); else mFlags.clear(ModDynamicFields); }
   void setModStaticFields(bool sta) { if(sta) mFlags.set(ModStaticFields); else mFlags.clear(ModStaticFields); }
   bool canModDynamicFields() { return mFlags.test(ModDynamicFields); }
   bool canModStaticFields() { return mFlags.test(ModStaticFields); }
   void setAutoDelete( bool val ) { if( val ) mFlags.set( AutoDelete ); else mFlags.clear( AutoDelete ); }

   /// @}


   /// Dump the contents of this object to the console.  Use the Torque Script dump() and dumpF() functions to 
   /// call this.  
   void dumpToConsole(bool includeFunctions=true);

   static void initPersistFields();

   /// Copy SimObject to another SimObject (Originally designed for T2D).
   virtual void copyTo(SimObject* object);

   // Component Console Overrides
   virtual bool handlesConsoleMethod(const char * fname, S32 * routingId) { return false; }
   virtual void getConsoleMethodData(const char * fname, S32 routingId, S32 * type, S32 * minArgs, S32 * maxArgs, void ** callback, const char ** usage) {}
   DECLARE_CONOBJECT(SimObject);
};

//---------------------------------------------------------------------------
/// Smart SimObject pointer.
///
/// This class keeps track of the book-keeping necessary
/// to keep a registered reference to a SimObject or subclass
/// thereof.
///
/// Normally, if you want the SimObject to be aware that you
/// have a reference to it, you must call SimObject::registerReference()
/// when you create the reference, and SimObject::unregisterReference() when
/// you're done. If you change the reference, you must also register/unregister
/// it. This is a big headache, so this class exists to automatically
/// keep track of things for you.
///
/// @code
///     // Assign an object to the
///     SimObjectPtr<GameBase> mOrbitObject = Sim::findObject("anObject");
///
///     // Use it as a GameBase*.
///     mOrbitObject->getWorldBox().getCenter(&mPosition);
///
///     // And reassign it - it will automatically update the references.
///     mOrbitObject = Sim::findObject("anotherObject");
/// @endcode
template <class T> class SimObjectPtr
{
private:
   SimObject *mObj;

public:
   SimObjectPtr() { mObj = 0; }
   SimObjectPtr(T* ptr)
   {
      mObj = ptr;
      if(mObj)
         mObj->registerReference(&mObj);
   }
   SimObjectPtr(const SimObjectPtr<T>& rhs)
   {
      mObj = const_cast<T*>(static_cast<const T*>(rhs));
      if(mObj)
         mObj->registerReference(&mObj);
   }
   SimObjectPtr<T>& operator=(const SimObjectPtr<T>& rhs)
   {
      if(this == &rhs)
         return(*this);
      if(mObj)
         mObj->unregisterReference(&mObj);
      mObj = const_cast<T*>(static_cast<const T*>(rhs));
      if(mObj)
         mObj->registerReference(&mObj);
      return(*this);
   }
   ~SimObjectPtr()
   {
      if(mObj)
         mObj->unregisterReference(&mObj);
   }
   SimObjectPtr<T>& operator= (T *ptr)
   {
      if(mObj != (SimObject *) ptr)
      {
         if(mObj)
            mObj->unregisterReference(&mObj);
         mObj = (SimObject *) ptr;
         if (mObj)
            mObj->registerReference(&mObj);
      }
      return *this;
   }
#if defined(__MWERKS__) && (__MWERKS__ < 0x2400)
   // CW 5.3 seems to get confused comparing SimObjectPtrs...
   bool operator == (const SimObject *ptr) { return mObj == ptr; }
   bool operator != (const SimObject *ptr) { return mObj != ptr; }
#endif
   bool isNull() const   { return mObj == 0; }
   bool isValid() const  { return mObj != 0; }
   T* operator->() const { return static_cast<T*>(mObj); }
   T& operator*() const  { return *static_cast<T*>(mObj); }
   operator T*() const   { return static_cast<T*>(mObj)? static_cast<T*>(mObj) : 0; }
   T* getObject() const  { return static_cast<T*>(mObj); }
};

#endif // _SIMOBJECT_H_
