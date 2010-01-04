//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _RUNTIME_CLASSREP_H_
#define _RUNTIME_CLASSREP_H_

#include "console/consoleObject.h"
#include "console/consoleInternal.h"

/// This class is to allow new types to be added, at run-time, to Torque.
/// This is primarily for dynamic libraries, plugins etc.
///
/// The idea is the same, one instance per type which is getting registered
/// however it doesn't get added to the dictionary until it is told to
/// add itself. It can be removed, as well, though no safe-execution
/// assurances are made by this class. If an object type is removed
/// behavior of existing console objects of that type is undefined. 
/// (aka bad stuff will probably happen but I don't know exactly what)
template <class T>
class RuntimeClassRep : public AbstractClassRep
{
protected:
   static bool smConRegistered; ///< This variable will be set to true if this class-rep is currently registered

public:
   RuntimeClassRep( const char *name, S32 netClassGroupMask, S32 netClassType, S32 netEventDir, AbstractClassRep *parent )
   {
      // name is a static compiler string so no need to worry about copying or deleting
      mClassName = name;

      // Clean up mClassId
      for( U32 i = 0; i < NetClassGroupsCount; i++ )
         mClassId[i] = -1;

      // Set properties for this ACR
      mClassType      = netClassType;
      mClassGroupMask = netClassGroupMask;
      mNetEventDir    = netEventDir;
      parentClass     = parent;
   };

   /// Perform class specific initialization tasks.
   ///
   /// Link namespaces, call initPersistFields() and consoleInit().
   void init() const
   {
      // Get handle to our parent class, if any, and ourselves (we are our parent's child).
      AbstractClassRep *parent = T::getParentStaticClassRep();
      AbstractClassRep *child  = T::getStaticClassRep      ();

      // If we got reps, then link those namespaces! (To get proper inheritance.)
      if( parent && child )
         Con::classLinkNamespaces( parent->getNameSpace(), child->getNameSpace() );

      // Finally, do any class specific initialization...
      T::initPersistFields();
      T::consoleInit();
   }

   /// Wrap constructor.
   ConsoleObject *create() const { return new T; }

   //-----------------------------------------------------------------------------

   /// Register this class with the Torque console
   void consoleRegister()
   {
      AssertFatal( !smConRegistered, "Calling consoleRegister, but this type is already linked into the class list" );

      if( !smConRegistered )
         registerClassRep( this );

      // Now initialize the namespace
      mNamespace = Con::lookupNamespace( StringTable->insert( getClassName() ) );
      mNamespace->mClassRep = this;

      // Perform field initialization
      sg_tempFieldList.setSize(0);

      init();

      // So if we have things in it, copy it over...
      if ( sg_tempFieldList.size() != 0 )
         mFieldList = sg_tempFieldList;

      // And of course delete it every round.
      sg_tempFieldList.clear();

      smConRegistered = true;
   }

   /// Unregister this class with the Torque console
   void consoleUnRegister()
   {
      AssertFatal( smConRegistered, "Calling consoleUnRegister, but this type is not linked into the class list" );
      if( !smConRegistered )
         return;

      removeClassRep( this );
      smConRegistered = false;
   }

   /// Returns true if this class type is registered with the console system
   static const bool isRegistered() { return smConRegistered; }
};

template<class T> bool RuntimeClassRep<T>::smConRegistered = false;

//-----------------------------------------------------------------------------

#define DECLARE_RUNTIME_CONOBJECT(className)                    \
   static RuntimeClassRep<className> dynRTClassRep;      \
   static AbstractClassRep* getParentStaticClassRep();  \
   static AbstractClassRep* getStaticClassRep();        \
   virtual AbstractClassRep* getClassRep() const

#define IMPLEMENT_RUNTIME_CONOBJECT(className)                                                            \
   AbstractClassRep* className::getClassRep() const { return &className::dynRTClassRep; }           \
   AbstractClassRep* className::getStaticClassRep() { return &dynRTClassRep; }                      \
   AbstractClassRep* className::getParentStaticClassRep() { return Parent::getStaticClassRep(); } \
   RuntimeClassRep<className> className::dynRTClassRep(#className, 0, -1, 0, className::getParentStaticClassRep())

#endif