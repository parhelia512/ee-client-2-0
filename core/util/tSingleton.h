//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TSINGLETON_H_
#define _TSINGLETON_H_


/// This is a simple thread safe singleton class based on the 
/// design of boost::singleton_default (see http://www.boost.org/).
///
/// This singleton is guaranteed to be constructed before main() is called
/// and destroyed after main() exits.  It will also be created on demand
/// if Singleton<T>::instance() is called before main() begins.
/// 
/// This thread safety only works within the execution context of main().
/// Access to the singleton from multiple threads outside of main() is
/// is not guaranteed to be thread safe.
///
/// To create a singleton you only need to access it once in your code:
///
///   Singleton<MySingletonClass>::instance()->myFunction();
///
/// You do not need to derive from this class.
///
template <typename T>
class Singleton
{
private:

   // This is the creator object which ensures that the
   // singleton is created before main() begins.
   struct SingletonCreator
   {
      SingletonCreator() { Singleton<T>::instance(); } 

      // This dummy function is used below to force 
      // singleton creation at startup.
      inline void forceSafeCreate() const {}
   };

   // The creator object instance.
   static SingletonCreator smSingletonCreator;
   
   /// This is private on purpose.
   Singleton();

public:

   /// Returns the singleton instance.
   static T* instance()
   {
      // The singleton.
      static T theSingleton;

      // This is here to force the compiler to create
      // the singleton before main() is called.
      smSingletonCreator.forceSafeCreate();

      return &theSingleton;
   }

};

template <typename T> 
typename Singleton<T>::SingletonCreator Singleton<T>::smSingletonCreator;


/// This is a managed singleton class with explict creation
/// and destruction functions which must be called at startup
/// and shutdown of the engine.
///
/// Your class to be managed must implement the following
/// function:
///
/// static const char* getSingletonName() { return "YourClassName"; }
///
/// This allow us to provide better asserts.
///
template <typename T>
class ManagedSingleton
{
private:

   static T *smSingleton;

public:

   /// 
   static void createSingleton() 
   {
      AssertFatal( smSingleton == NULL, String::ToString( "%s::createSingleton() - The singleton is already created!", T::getSingletonName() ) );
      smSingleton = new T(); 
   }

   /// 
   static void deleteSingleton()
   {
      AssertFatal( smSingleton, String::ToString( "%s::deleteSingleton() - The singleton doest not exist!", T::getSingletonName() ) );
      delete smSingleton;
      smSingleton = NULL;
   }

   ///
   static T* instance() 
   { 
      AssertFatal( smSingleton, String::ToString( "%s::instance() - The singleton has not been created!", T::getSingletonName() ) );
      return smSingleton; 
   }

};

///
template <typename T> 
T* ManagedSingleton<T>::smSingleton = NULL;


#endif //_TSINGLETON_H_

