//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXPROFILE_H_
#define _SFXPROFILE_H_

#ifndef _CONSOLETYPES_H_
   #include "console/consoleTypes.h"
#endif
#ifndef _SIMDATABLOCK_H_
   #include "console/simDataBlock.h"
#endif
#ifndef _SFXDESCRIPTION_H_
   #include "sfx/sfxDescription.h"
#endif
#ifndef _SFXDEVICE_H_
   #include "sfx/sfxDevice.h"
#endif
#ifndef _SFXRESOURCE_H_
   #include "sfx/sfxResource.h"
#endif
#ifndef _SFXBUFFER_H_
   #include "sfx/sfxBuffer.h"
#endif
#ifndef _SFXSYSTEM_H_
   #include "sfx/sfxSystem.h"
#endif
#ifndef _TSIGNAL_H_
   #include "core/util/tSignal.h"
#endif


class SFXDescription;


/// The SFXProfile is used to define a sound for playback.
///
/// An SFXProfile will first try to load its file directly through the SFXDevice.
/// Only if this fails (which is the case for most SFXDevices as these do not
/// implement their own custom sound format loading), the file is loaded through
/// SFXResource.
///
/// A few tips:
///
/// Make sure each of the defined SFXProfile's fileName doesn't specify 
/// an extension. An extension does not need to be specified and by not
/// explicitly saying .ogg or .wav it will allow you to change from one 
/// format to the other without having to change the scripts.
///
/// Make sure that server SFXProfiles are defined with the datablock 
/// keyword, and that client SFXProfiles are defined with the 'new' 
/// keyword.
///
/// Make sure SFXDescriptions exist for your SFXProfiles. Also make sure
/// that SFXDescriptions are defined BEFORE SFXProfiles. This is especially
/// important if your SFXProfiles are located in different files than your
/// SFXDescriptions. In this case, make sure the files containing SFXDescriptions
/// are exec'd before the files containing the SFXProfiles.
///
class SFXProfile : public SimDataBlock
{
   public:
   
      friend class SFXEmitter; // For access to mFilename
      
      typedef SimDataBlock Parent;
      
      typedef Signal< void( SFXProfile* ) > ChangedSignal;

   protected:

      /// Used on the client side during onAdd.
      S32 mDescriptionId;

      /// The sound data.
      /// @note ATM only valid if loaded through SFX's loading system rather than
      ///   through the SFXDevice's loading system.
      Resource< SFXResource > mResource;
      
      /// The description which controls playback settings.
      SFXDescription *mDescription;

      /// The sound filename.  If no extension is specified
      /// the system will try .wav first then other formats.
      String mFilename;

      /// If true the sound data will be loaded from
      /// disk and possibly cached with the active 
      /// device before the first call for playback.
      bool mPreload;

      /// The device specific data buffer.
      /// This is only used if for non-streaming sounds.
      StrongWeakRefPtr< SFXBuffer > mBuffer;
      
      ///
      ChangedSignal mChangedSignal;

      /// Called when the buffer needs to be preloaded.
      bool _preloadBuffer();

      /// Callback for device events.
      void _onDeviceEvent( SFXSystemEventType evt );

      ///
      SFXBuffer* _createBuffer();
      
      ///
      void _onResourceChanged( ResourceBase::Signature sig, const Torque::Path& path );
      
      ///
      void _registerSignals();
      
      ///
      void _unregisterSignals();

   public:

      /// This is only here to allow DECLARE_CONOBJECT 
      /// to create us from script.  You shouldn't use
      /// this constructor from C++.
      explicit SFXProfile();

      /// The constructor.
      SFXProfile( SFXDescription* desc, 
                  const String& filename = String(), 
                  bool preload = false );

      /// The destructor.
      virtual ~SFXProfile();

      DECLARE_CONOBJECT( SFXProfile );

      static void initPersistFields();

      // SimObject 
      bool onAdd();
      void onRemove();
      void packData( BitStream* stream );
      void unpackData( BitStream* stream );

      /// Returns the sound filename.
      const String& getFileName() const { return mFilename; }

      /// @note This has nothing to do with mPreload.
      /// @see SimDataBlock::preload
      bool preload( bool server, String &errorStr );

      /// Returns the description object for this sound profile.
      SFXDescription* getDescription() const { return mDescription; }

      /// Returns the sound resource loading it from
      /// disk if it hasn't been preloaded.
      ///
      /// @note May be NULL if file is loaded directly through SFXDevice.
      Resource<SFXResource>& getResource();

      /// Returns the device specific buffer for this for this 
      /// sound.  If it hasn't been preloaded it will be loaded
      /// at this time.
      ///
      /// If this is a streaming profile then the buffer
      /// returned must be deleted by the caller.
      SFXBuffer* getBuffer();

      /// Gets the sound duration in milliseconds or 
      /// returns 0 if the resource was not found.
      U32 getSoundDuration();
      
      ///
      ChangedSignal& getChangedSignal() { return mChangedSignal; }
};

DECLARE_CONSOLETYPE( SFXProfile );


#endif  // _SFXPROFILE_H_