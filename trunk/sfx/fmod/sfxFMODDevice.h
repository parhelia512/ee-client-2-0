//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXFMODDEVICE_H_
#define _SFXFMODDEVICE_H_

#ifndef _SFXDEVICE_H_
   #include "sfx/sfxDevice.h"
#endif
#ifndef _SFXFMODVOICE_H_
   #include "sfx/fmod/sfxFMODVoice.h"
#endif
#ifndef _SFXFMODBUFFER_H_
   #include "sfx/fmod/sfxFMODBuffer.h"
#endif

#include "core/util/tDictionary.h"


// Disable warning for unused static functions.
#ifdef TORQUE_COMPILER_VISUALC
#  pragma warning( disable : 4505 )
#endif


#include "fmod.h"
#include "fmod_errors.h"

#include "platform/platformDlibrary.h"
#include "platform/threads/mutex.h"


// This doesn't appear to exist in some contexts, so let's just add it.
#ifdef TORQUE_OS_WIN32
#ifndef WINAPI
#define WINAPI __stdcall
#endif
#else
#define WINAPI
#endif


#define FModAssert(x, msg) \
   { FMOD_RESULT result = ( x ); \
     AssertISV( result == FMOD_OK, String::ToString( "%s: %s", msg, FMOD_ErrorString( result ) ) ); }

#define FMOD_FN_FILE "sfx/fmod/fmodFunctions.h"


// Typedefs
#define FMOD_FUNCTION(fn_name, fn_args) \
   typedef FMOD_RESULT (WINAPI *FMODFNPTR##fn_name)fn_args;
#include FMOD_FN_FILE
#undef FMOD_FUNCTION


/// FMOD API function table.
///
/// FMOD doesn't want to be called concurrently so in order to
/// not force everything to the main thread (where sound updates
/// would just stall during loading), we thunk all the API
/// calls and lock all API entry points to a single mutex.
struct FModFNTable
{
   FModFNTable()
      : isLoaded( false )
   {
      AssertFatal( mutex == NULL,
         "FModFNTable::FModFNTable() - this should be a singleton" );
      mutex = new Mutex;
   }
   ~FModFNTable()
   {
      delete mutex;
   }

   bool isLoaded;
   DLibraryRef dllRef;
   static Mutex* mutex;

   template< typename FN >
   struct Thunk
   {
      FN fn;

      template< typename A >
      FMOD_RESULT operator()( A a )
      {
         mutex->lock();
         FMOD_RESULT result = fn( a );
         mutex->unlock();
         return result;
      }
      template< typename A, typename B >
      FMOD_RESULT operator()( A a, B b )
      {
         mutex->lock();
         FMOD_RESULT result = fn( a, b );
         mutex->unlock();
         return result;
      }
      template< typename A, typename B, typename C >
      FMOD_RESULT operator()( A a, B b, C c )
      {
         mutex->lock();
         FMOD_RESULT result = fn( a, b, c );
         mutex->unlock();
         return result;
      }
      template< typename A, typename B, typename C, typename D >
      FMOD_RESULT operator()( A a, B b, C c, D d )
      {
         mutex->lock();
         FMOD_RESULT result = fn( a, b, c, d );
         mutex->unlock();
         return result;
      }
      template< typename A, typename B, typename C, typename D, typename E >
      FMOD_RESULT operator()( A a, B b, C c, D d, E e )
      {
         mutex->lock();
         FMOD_RESULT result = fn( a, b, c, d, e );
         mutex->unlock();
         return result;
      }
      template< typename A, typename B, typename C, typename D, typename E, typename F >
      FMOD_RESULT operator()( A a, B b, C c, D d, E e, F f )
      {
         mutex->lock();
         FMOD_RESULT result = fn( a, b, c, d, e, f );
         mutex->unlock();
         return result;
      }
      template< typename A, typename B, typename C, typename D, typename E, typename F, typename G >
      FMOD_RESULT operator()( A a, B b, C c, D d, E e, F f, G g )
      {
         mutex->lock();
         FMOD_RESULT result = fn( a, b, c, d, e, f, g );
         mutex->unlock();
         return result;
      }
   };

#define FMOD_FUNCTION(fn_name, fn_args) \
   Thunk< FMODFNPTR##fn_name > fn_name;
#include FMOD_FN_FILE
#undef FMOD_FUNCTION
};



class SFXProvider;



class SFXFMODDevice : public SFXDevice
{
   public:

      typedef SFXDevice Parent;
      friend class SFXFMODProvider; // _init

      explicit SFXFMODDevice();

      SFXFMODDevice( SFXProvider* provider, FModFNTable *fmodFnTbl, int deviceIdx, String name );

      virtual ~SFXFMODDevice();

   protected:

      FMOD_MODE m3drolloffmode;
      int mDeviceIndex;
      
      bool _init();
      
   public:

      FMOD_MODE get3dRollOffMode() { return m3drolloffmode; }

      static FMOD_SYSTEM *smSystem;
      static FModFNTable *smFunc;

      // SFXDevice.
      virtual SFXBuffer* createBuffer( const ThreadSafeRef< SFXStream >& stream, SFXDescription* description );
      virtual SFXBuffer* createBuffer( const String& filename, SFXDescription* description );
      virtual SFXVoice* createVoice( bool is3D, SFXBuffer* buffer );
      virtual void update( const SFXListener& listener );
      virtual void setDistanceModel( SFXDistanceModel model );
      virtual void setDopplerFactor( F32 factor );
      virtual void setRolloffFactor( F32 factor );
};

#endif // _SFXFMODDEVICE_H_
