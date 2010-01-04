//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXEMITTER_H_
#define _SFXEMITTER_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _SFXPROFILE_H_
#include "sfx/sfxProfile.h"
#endif

class SFXSource;


/// The SFXEmitter is used to place 2D or 3D sounds into a 
/// mission.
///
/// If the profile is set then the emitter plays that.  If the
/// profile is null and the filename is set then the local emitter
/// options are used.
///
/// Note that you can call SFXEmitter.play() and SFXEmitter.stop()
/// to control playback from script.
///
class SFXEmitter : public SceneObject
{
   typedef SceneObject Parent;

   protected:

      /// The sound source for the emitter.
      SFXSource *mSource;

      /// The selected profile or null if the local
      /// profile should be used.
      SFXProfile *mProfile;

      /// A local profile object used to coax the
      /// sound system to play a custom sound.
      SFXProfile mLocalProfile;

      /// The description used by the local profile.
      SFXDescription mDescription;

      /// If true playback starts when the emitter
      /// is added to the scene.
      bool mPlayOnAdd;

      /// Network update masks.
      enum UpdateMasks 
      {
         InitialUpdateMask    = BIT(0),
         TransformUpdateMask  = BIT(1),
         DirtyUpdateMask      = BIT(2),

         SourcePlayMask       = BIT(3),
         SourceStopMask       = BIT(4),

         AllSourceMasks = SourcePlayMask | SourceStopMask,
      };

      /// Dirty flags used to handle sound property
      /// updates locally and across the network.
      enum Dirty
      {
         Profile                    = BIT(0),
         Filename                   = BIT(2),
         Volume                     = BIT(4),
         IsLooping                  = BIT(5),
         Is3D                       = BIT(6),
         ReferenceDistance          = BIT(7),
         MaxDistance                = BIT(8),
         ConeInsideAngle            = BIT(9),
         ConeOutsideAngle           = BIT(10),
         ConeOutsideVolume          = BIT(11),
         Transform                  = BIT(12),
         Channel                    = BIT(13),
         OutsideAmbient             = BIT(14),
         IsStreaming                = BIT(15),
         FadeInTime                 = BIT(16),
         FadeOutTime                = BIT(17),
         Pitch                      = BIT(18),

         AllDirtyMask               = 0xFFFFFFFF,
      };

      /// The current dirty flags.
      BitSet32 mDirty;

      /// Helper which reads a flag from the stream and 
      /// updates the mDirty bits.
      bool _readDirtyFlag( BitStream *stream, U32 flag );

      /// Called when the emitter state has been marked
      /// dirty and the source needs to be updated.
      void _update();

   public:

      SFXEmitter();
      virtual ~SFXEmitter();

      DECLARE_CONOBJECT( SFXEmitter );
      
      /// Return the playback status of the emitter's associated sound.
      SFXStatus getPlaybackStatus() const;
      
      ///
      const SFXDescription& getSFXDescription() const { return ( mProfile ? *( mProfile->getDescription() ) : mDescription ); }
      
      /// Return true if the SFX system's listener is in range of this emitter.
      bool isInRange() const;
      
      /// Return true if the emitter defines a 3D sound.
      bool is3D() const { return getSFXDescription().mIs3D; }
      
      /// Return true if the emitter using streaming playback.
      bool isStreaming() const { return getSFXDescription().mIsStreaming; }
      
      /// Return true if the emitter loops playback of the associated sound.
      bool isLooping() const { return getSFXDescription().mIsLooping; }

      // SimObject
      bool onAdd();
      void onRemove();
      void onStaticModified( const char *slotName, const char *newValue = NULL );
      static void initPersistFields();

      // NetObject
      U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
      void unpackUpdate( NetConnection *conn, BitStream *stream );

      // SceneObject
      void setTransform( const MatrixF &mat );
      void setScale( const VectorF &scale );

      /// Sends network event to start playback if 
      /// the emitter source is not already playing.
      void play();

      /// Sends network event to stop emitter 
      /// playback on all ghosted clients.
      void stop();

};

#endif // _SFXEMITTER_H_
