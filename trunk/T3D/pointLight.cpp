//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/pointLight.h"

#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "gfx/gfxDrawUtil.h"


IMPLEMENT_CO_NETOBJECT_V1( PointLight );

PointLight::PointLight()
   : mRadius( 5.0f )
{
   // We set the type here to ensure the extended
   // parameter validation works when setting fields.
   mLight->setType( LightInfo::Point );
}

PointLight::~PointLight()
{
}

void PointLight::initPersistFields()
{
   addGroup( "Light" );

      addField( "radius", TypeF32, Offset( mRadius, PointLight ) );

   endGroup( "Light" );

   // We do the parent fields at the end so that
   // they show up that way in the inspector.
   Parent::initPersistFields();

   // Remove the scale field... it's already 
   // defined by the light radius.
   removeField( "scale" );
}

void PointLight::_conformLights()
{
   mLight->setTransform( getTransform() );

   mLight->setRange( mRadius );

   mLight->setColor( mColor );

   mLight->setBrightness( mBrightness );
   mLight->setCastShadows( mCastShadows );
   mLight->setPriority( mPriority );

   // Update the bounds and scale to fit our light.
   mObjBox.minExtents.set( -1, -1, -1 );
   mObjBox.maxExtents.set( 1, 1, 1 );
   mObjScale.set( mRadius, mRadius, mRadius );

   // Skip our transform... it just dirties mask bits.
   Parent::setTransform( mObjToWorld );
}

U32 PointLight::packUpdate(NetConnection *conn, U32 mask, BitStream *stream )
{
   if ( stream->writeFlag( mask & UpdateMask ) )
      stream->write( mRadius );

   return Parent::packUpdate( conn, mask, stream );
}

void PointLight::unpackUpdate( NetConnection *conn, BitStream *stream )
{
   if ( stream->readFlag() ) // UpdateMask
      stream->read( &mRadius );

   Parent::unpackUpdate( conn, stream );
}

void PointLight::setScale( const VectorF &scale )
{
   // Use the average of the three coords.
   mRadius = ( scale.x + scale.y + scale.z ) / 3.0f;

   // We changed our settings so notify the client.
   setMaskBits( UpdateMask );

   // Let the parent do the final scale.
   Parent::setScale( VectorF( mRadius, mRadius, mRadius ) );
}

void PointLight::_renderViz( SceneState *state )
{
   GFXDrawUtil *draw = GFX->getDrawUtil();

   GFXStateBlockDesc desc;
   desc.setZReadWrite( true, false );
   desc.setCullMode( GFXCullNone );
   desc.setBlend( true );

   // Base the sphere color on the light color.
   ColorI color( mColor );
   color.alpha = 16;

   draw->drawSphere( desc, mRadius, getPosition(), color );
}
