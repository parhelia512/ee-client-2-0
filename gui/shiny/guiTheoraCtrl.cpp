//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifdef TORQUE_OGGTHEORA

#include "gui/core/guiControl.h"
#include "gui/shiny/guiTheoraCtrl.h"
#include "console/consoleTypes.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"


static const EnumTable::Enums transcoderEnums[] =
{
   { OggTheoraDecoder::TRANSCODER_Auto,            "Auto"            },
   { OggTheoraDecoder::TRANSCODER_Generic,         "Generic"         },
   { OggTheoraDecoder::TRANSCODER_SSE2420RGBA,     "SSE2420RGBA"     },
};
static const EnumTable gTranscoderTable( sizeof( transcoderEnums ) / sizeof( transcoderEnums[ 0 ] ), transcoderEnums );


//----------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiTheoraCtrl);

GuiTheoraCtrl::GuiTheoraCtrl()
{
   mFilename         = StringTable->insert("");
   mDone             = false;
   mStopOnSleep      = false;
   mMatchVideoSize   = true;
   mPlayOnWake       = true;
   mRenderDebugInfo  = false;
   mTranscoder       = OggTheoraDecoder::TRANSCODER_Auto;
   
   mBackgroundColor.set( 0, 0, 0, 255);
}

//----------------------------------------------------------------------------
GuiTheoraCtrl::~GuiTheoraCtrl()
{
}

void GuiTheoraCtrl::initPersistFields()
{
   addGroup( "Playback");

	addField( "theoraFile",       TypeStringFilename,  Offset( mFilename,         GuiTheoraCtrl ), "Theora video file to play." );
   addField( "backgroundColor",  TypeColorI,          Offset( mBackgroundColor,  GuiTheoraCtrl ), "Fill color when video is not playing." );
   addField( "playOnWake",       TypeBool,            Offset( mPlayOnWake,       GuiTheoraCtrl ), "Start playing video when control is woken up." );
   addField( "stopOnSleep",      TypeBool,            Offset( mStopOnSleep,      GuiTheoraCtrl ), "Stop video when control is set to sleep." );
   addField( "matchVideoSize",   TypeBool,            Offset( mMatchVideoSize,   GuiTheoraCtrl ), "Match control extents with video size." );
   addField( "renderDebugInfo",  TypeBool,            Offset( mRenderDebugInfo,  GuiTheoraCtrl ), "Render text information useful for debugging." );
   addField( "transcoder",       TypeEnum,            Offset( mTranscoder,       GuiTheoraCtrl ), 1, &gTranscoderTable );

   endGroup( "Playback" );

	Parent::initPersistFields();
}

ConsoleMethod( GuiTheoraCtrl, setFile, void, 3, 3, "(string filename) Set an Ogg Theora file to play.")
{
	object->setFile(argv[2]);
}

ConsoleMethod( GuiTheoraCtrl, play, void, 2, 2, "() - Start playback." )
{
   object->play();
}

ConsoleMethod( GuiTheoraCtrl, pause, void, 2, 2, "() - Pause playback." )
{
   object->pause();
}

ConsoleMethod( GuiTheoraCtrl, stop, void, 2, 2, "() - Stop playback.")
{
	object->stop();
}

ConsoleMethod( GuiTheoraCtrl, getCurrentTime, F32, 2, 2, "() - Return the time elapsed in playback, in seconds.")
{
   return object->getCurrentTime();
}

ConsoleMethod( GuiTheoraCtrl, isPlaybackDone, bool, 2, 2, "() - Return true if the video has finished playing." )
{
   return object->isPlaybackDone();
}

void GuiTheoraCtrl::setFile( const String& filename )
{
	mDone = false;
   mFilename = filename;
   mTheoraTexture.setFile( filename );
   
   if( mMatchVideoSize && mTheoraTexture.isReady() )
      setExtent( Point2I( mTheoraTexture.getWidth(), mTheoraTexture.getHeight() ) );

   OggTheoraDecoder* decoder = mTheoraTexture._getTheora();
   if( decoder != NULL )
      decoder->setTranscoder( mTranscoder );
}

void GuiTheoraCtrl::play()
{
   if( mFilename.isEmpty() )
      return;
   
   if( !mTheoraTexture.isPlaying() )
   {
      mDone = false;
      mTheoraTexture.play();
   }
}

void GuiTheoraCtrl::pause()
{
   if( !mTheoraTexture.isPlaying() )
   {
      Con::errorf( "GuiTheoraCtrl::pause - not playing" );
      return;
   }
   
   mTheoraTexture.pause();
}

void GuiTheoraCtrl::stop()
{
	mTheoraTexture.stop();
	mDone = true;
}

//----------------------------------------------------------------------------
bool GuiTheoraCtrl::onWake()
{
	if( !Parent::onWake() )
      return false;

	if( !mTheoraTexture.isReady() )
      setFile( mFilename );
   
   if( mPlayOnWake && !mTheoraTexture.isPlaying() )
      play();

	return true;
}

//----------------------------------------------------------------------------
void GuiTheoraCtrl::onSleep()
{
	Parent::onSleep();

   if( mStopOnSleep )
      stop();
   else
      pause();
}

//----------------------------------------------------------------------------
void GuiTheoraCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   const RectI rect(offset, getBounds().extent);

	if( mTheoraTexture.isReady() )
	{
      mTheoraTexture.refresh();
      if( mTheoraTexture.isPlaying()
          || mTheoraTexture.isPaused() )
      {
         // Draw the frame.
         
         GFXDrawUtil* drawUtil = GFX->getDrawUtil();
         drawUtil->clearBitmapModulation();
         drawUtil->drawBitmapStretch( mTheoraTexture.getTexture(), rect );
         
         // Draw frame info, if requested.
         
         if( mRenderDebugInfo )
         {
            String info = String::ToString( "Frame Number: %i | Frame Time: %.2fs | Playback Time: %.2fs | Dropped: %i",
               mTheoraTexture.getFrameNumber(),
               mTheoraTexture.getFrameTime(),
               F32( mTheoraTexture.getPosition() ) / 1000.f,
               mTheoraTexture.getNumDroppedFrames() );
            
            drawUtil->setBitmapModulation( mProfile->mFontColors[ 0 ] );
            drawUtil->drawText( mProfile->mFont, localToGlobalCoord( Point2I( 0, 0 ) ), info, mProfile->mFontColors );
         }
      }
      else
         mDone = true;
	}
	else
 		GFX->getDrawUtil()->drawRectFill(rect, mBackgroundColor); // black rect

	renderChildControls(offset, updateRect);
}

//----------------------------------------------------------------------------
void GuiTheoraCtrl::inspectPostApply()
{
   if( !mTheoraTexture.getFilename().equal( mFilename, String::NoCase ) )
   {
      stop();
      setFile( mFilename );
      
      if( mPlayOnWake && !mTheoraTexture.isPlaying() )
         play();
   }
   
   if( mMatchVideoSize && mTheoraTexture.isReady() )
      setExtent( Point2I( mTheoraTexture.getWidth(), mTheoraTexture.getHeight() ) );

   OggTheoraDecoder* decoder = mTheoraTexture._getTheora();
   if( decoder != NULL )
      decoder->setTranscoder( mTranscoder );
      
   Parent::inspectPostApply();
}

#endif // TORQUE_OGGTHEORA
