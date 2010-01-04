//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUITHEORACTRL_H_
#define _GUITHEORACTRL_H_

#ifdef TORQUE_OGGTHEORA

#ifndef _THEORATEXTURE_H_
   #include "gfx/video/theoraTexture.h"
#endif
#ifndef _OGGTHEORADECODER_H_
   #include "core/ogg/oggTheoraDecoder.h"
#endif


/// Play back a Theora video file.
class GuiTheoraCtrl : public GuiControl
{
   public:
   
      typedef GuiControl Parent;
      
   protected:
   
      /// The Theora file we should play.
      String mFilename;
      
      ///
      TheoraTexture mTheoraTexture;
      
      /// If true, the control's extents will be matched to the video size.
      bool mMatchVideoSize;
      
      /// If true, playback will start automatically when the control receives its
      /// onWake().
      bool mPlayOnWake;
      
      /// Which transcoder to use on the Theora decoder.  This is mostly
      /// meant as a development aid.
      OggTheoraDecoder::ETranscoder mTranscoder;

      /// If true, stop video playback when the control goes to sleep.  Otherwise,
      /// the video will be paused.
      ///
      /// @note We do not currently support to keep video running in the background
      ///   as the Theora decoder does not yet support skipping through bulks of
      ///   outdated data.  This means that when the Theora texture gets its next
      ///   refresh, the decoder will frantically try to wade through a huge amount
      ///   of outdated ogg_packets which even though the actual decoding does not
      ///   take place takes a lot of time.
      bool mStopOnSleep;

      /// Are we done with playback?
      bool mDone;
      
      /// If true, renders some text information into the frame.
      bool mRenderDebugInfo;
      
      /// Our background color.
      ColorI mBackgroundColor;

   public:
   
      DECLARE_CONOBJECT( GuiTheoraCtrl );
      DECLARE_CATEGORY( "Gui Images" );
      DECLARE_DESCRIPTION( "A control for playing Theora videos." );

      GuiTheoraCtrl();
      ~GuiTheoraCtrl();

      static void initPersistFields();

      void setFile( const String& filename );
      void play();
      void pause();
      void stop();
      
      bool isPlaybackDone() const { return mDone; }

      // GuiControl.
      virtual bool onWake();
      virtual void onSleep();
      virtual void onRender(Point2I offset, const RectI &updateRect);
      virtual void inspectPostApply();

      F32 getCurrentTime()
      {
         return F32( mTheoraTexture.getPosition() ) / 1000.f;
      }
};

#endif // TORQUE_OGGTHEORA
#endif // !_GUITHEORACTRL_H_
