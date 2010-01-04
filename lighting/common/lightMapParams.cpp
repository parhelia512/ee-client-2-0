//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "lighting/common/lightMapParams.h"
#include "core/stream/bitStream.h"

const LightInfoExType LightMapParams::Type( "LightMapParams" );

LightMapParams::LightMapParams( LightInfo *light ) :
   representedInLightmap(false), 
   includeLightmappedGeometryInShadow(false), 
   shadowDarkenColor(0.0f, 0.0f, 0.0f, -1.0f)
{
   
}

LightMapParams::~LightMapParams()
{

}

void LightMapParams::set( const LightInfoEx *ex )
{
   // TODO: Do we even need this?
}

void LightMapParams::packUpdate( BitStream *stream ) const
{
   stream->writeFlag(representedInLightmap);
   stream->writeFlag(includeLightmappedGeometryInShadow);
   stream->write(shadowDarkenColor);
}

void LightMapParams::unpackUpdate( BitStream *stream )
{
   representedInLightmap = stream->readFlag();
   includeLightmappedGeometryInShadow = stream->readFlag();
   stream->read(&shadowDarkenColor);

   // Always make sure that the alpha value of the shadowDarkenColor is -1.0
   shadowDarkenColor.alpha = -1.0f;
}