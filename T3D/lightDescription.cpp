//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/lightDescription.h"

#include "lighting/lightManager.h"
#include "T3D/lightFlareData.h"
#include "T3D/lightAnimData.h"
#include "core/stream/bitStream.h"
#include "lighting/lightInfo.h"

LightDescription::LightDescription()
 : color( ColorF::WHITE ),
   brightness( 1.0f ),
   range( 5.0f ),
   castShadows( false ),
   animationData( NULL ),
   animationDataId( 0 ),
   animationPeriod( 1.0f ),
   animationPhase( 1.0f ),
   flareData( NULL ),
   flareDataId( 0 ),
   flareScale( 1.0f )
{   
}

LightDescription::~LightDescription()
{

}

IMPLEMENT_CO_DATABLOCK_V1( LightDescription );

void LightDescription::initPersistFields()
{
   addGroup( "Light" );

      addField( "color", TypeColorF, Offset( color, LightDescription ) );
      addField( "brightness", TypeF32, Offset( brightness, LightDescription ) );  
      addField( "range", TypeF32, Offset( range, LightDescription ) );
      addField( "castShadows", TypeBool, Offset( castShadows, LightDescription ) );

   endGroup( "Light" );

   addGroup( "Light Animation" );

      addField( "animationType", TypeLightAnimDataPtr, Offset( animationData, LightDescription ) );
      addField( "animationPeriod", TypeF32, Offset( animationPeriod, LightDescription ) );
      addField( "animationPhase", TypeF32, Offset( animationPhase, LightDescription ) );      

   endGroup( "Light Animation" );

   addGroup( "Misc" );

      addField( "flareType", TypeLightFlareDataPtr, Offset( flareData, LightDescription ) );
      addField( "flareScale", TypeF32, Offset( flareScale, LightDescription ) );

   endGroup( "Misc" );

   LightManager::initLightFields();

   Parent::initPersistFields();
}

void LightDescription::inspectPostApply()
{
   Parent::inspectPostApply();

   // Hack to allow changing properties in game.
   // Do the same work as preload.
   animationData = NULL;
   flareData = NULL;

   String str;
   _preload( false, str );
}

bool LightDescription::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   return true;
}
bool LightDescription::preload( bool server, String &errorStr )
{
   if ( !Parent::preload( server, errorStr ) )
      return false;

   return _preload( server, errorStr );  
}

void LightDescription::packData( BitStream *stream )
{
   Parent::packData( stream );

   stream->write( color );
   stream->write( brightness );
   stream->write( range );
   stream->writeFlag( castShadows );

   stream->write( animationPeriod );
   stream->write( animationPhase );
   stream->write( flareScale );

   if ( stream->writeFlag( animationData ) )
   {
      stream->writeRangedU32( animationData->getId(),
         DataBlockObjectIdFirst, 
         DataBlockObjectIdLast );
   }

   if ( stream->writeFlag( flareData ) )
   {
      stream->writeRangedU32( flareData->getId(),
         DataBlockObjectIdFirst, 
         DataBlockObjectIdLast );
   }
}

void LightDescription::unpackData( BitStream *stream )
{
   Parent::unpackData( stream );

   stream->read( &color );
   stream->read( &brightness );     
   stream->read( &range );
   castShadows = stream->readFlag();

   stream->read( &animationPeriod );
   stream->read( &animationPhase );
   stream->read( &flareScale );
   
   if ( stream->readFlag() )   
      animationDataId = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );  

   if ( stream->readFlag() )
      flareDataId = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );  
}

void LightDescription::submitLight( LightState *state, const MatrixF &xfm, LightManager *lm, SimObject *object )
{
   LightInfo *li = state->lightInfo;
   
   li->setRange( range );
   li->setColor( color );
   li->setCastShadows( castShadows );
   li->setTransform( xfm );

   if ( animationData )
   {      
      LightAnimState *animState = &state->animState;   

      animState->fullBrightness = brightness;
      animState->animationPeriod = animationPeriod;
      animState->animationPhase = animationPhase;      

      animationData->animate( animState );
   }

   lm->registerGlobalLight( li, object );
}

void LightDescription::prepRender( SceneState *sceneState, LightState *lightState, const MatrixF &xfm )
{
   if ( flareData )
   {
      LightFlareState *flareState = &lightState->flareState;
      flareState->fullBrightness = brightness;
      flareState->scale = flareScale;
      flareState->lightMat = xfm;
      flareState->lightInfo = lightState->lightInfo;

      flareData->prepRender( sceneState, flareState );
   }
}

bool LightDescription::_preload( bool server, String &errorStr )
{
   if (!animationData && animationDataId != 0)
      if (Sim::findObject(animationDataId, animationData) == false)
         Con::errorf(ConsoleLogEntry::General, "LightDescription::onAdd: Invalid packet, bad datablockId(animationData): %d", animationDataId);

   if (!flareData && flareDataId != 0)
      if (Sim::findObject(flareDataId, flareData) == false)
         Con::errorf(ConsoleLogEntry::General, "LightDescription::onAdd: Invalid packet, bad datablockId(flareData): %d", flareDataId); 

   return true;
}

IMPLEMENT_CONSOLETYPE( LightDescription )
IMPLEMENT_GETDATATYPE( LightDescription )
IMPLEMENT_SETDATATYPE( LightDescription )

ConsoleMethod( LightDescription, apply, void, 2, 2, "force an inspectPostApply for the benefit of tweaking via the console" )
{
   object->inspectPostApply();
}