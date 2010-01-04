//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/spotLight.h"

#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "gfx/gfxDrawUtil.h"


IMPLEMENT_CO_NETOBJECT_V1( SpotLight );

SpotLight::SpotLight()
   :  mRange( 10.0f ),
      mInnerConeAngle( 40.0f ),
      mOuterConeAngle( 45.0f )
{
   // We set the type here to ensure the extended
   // parameter validation works when setting fields.
   mLight->setType( LightInfo::Spot );
}

SpotLight::~SpotLight()
{
}

void SpotLight::initPersistFields()
{
   addGroup( "Light" );
      
      addField( "range", TypeF32, Offset( mRange, SpotLight ) );
      addField( "innerAngle", TypeF32, Offset( mInnerConeAngle, SpotLight ) );
      addField( "outerAngle", TypeF32, Offset( mOuterConeAngle, SpotLight ) );

   endGroup( "Light" );

   // We do the parent fields at the end so that
   // they show up that way in the inspector.
   Parent::initPersistFields();

   // Remove the scale field... it's already 
   // defined by the range and angle.
   removeField( "scale" );
}

void SpotLight::_conformLights()
{
   mLight->setTransform( getTransform() );

   mRange = getMax( mRange, 0.05f );
   mLight->setRange( mRange );

   mLight->setColor( mColor );
   mLight->setBrightness( mBrightness );
   mLight->setCastShadows( mCastShadows );
   mLight->setPriority( mPriority );

   mOuterConeAngle = getMax( 0.01f, mOuterConeAngle );
   mInnerConeAngle = getMin( mInnerConeAngle, mOuterConeAngle );

   mLight->setInnerConeAngle( mInnerConeAngle );
   mLight->setOuterConeAngle( mOuterConeAngle );

   // Update the bounds and scale to fit our spotlight.
   F32 radius = mRange * mSin( mDegToRad( mOuterConeAngle ) * 0.5f );
   mObjBox.minExtents.set( -1, 0, -1 );
   mObjBox.maxExtents.set( 1, 1, 1 );
   mObjScale.set( radius, mRange, radius );

   // Skip our transform... it just dirties mask bits.
   Parent::setTransform( mObjToWorld );
}

U32 SpotLight::packUpdate(NetConnection *conn, U32 mask, BitStream *stream )
{
   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      stream->write( mRange );
      stream->write( mInnerConeAngle );
      stream->write( mOuterConeAngle );
   }
   
   return Parent::packUpdate( conn, mask, stream );
}

void SpotLight::unpackUpdate( NetConnection *conn, BitStream *stream )
{
   if ( stream->readFlag() ) // UpdateMask
   {   
      stream->read( &mRange );
      stream->read( &mInnerConeAngle );
      stream->read( &mOuterConeAngle );
   }
   
   Parent::unpackUpdate( conn, stream );
}

void SpotLight::setScale( const VectorF &scale )
{
   // The y coord is the spotlight range.
   mRange = getMax( scale.y, 0.05f );

   // Use the average of the x and z to get a radius.  This
   // is the best method i've found to make the manipulation
   // from the WorldEditor gizmo to feel right.
   F32 radius = mClampF( ( scale.x + scale.z ) * 0.5f, 0.05f, mRange );
   mOuterConeAngle = mRadToDeg( mAsin( radius / mRange ) ) * 2.0f;

   // Make sure the inner angle is less than the outer.
   //
   // TODO: Maybe we should make the inner angle a scale
   // and not an absolute angle?
   //
   mInnerConeAngle = getMin( mInnerConeAngle, mOuterConeAngle );

   // We changed a bunch of our settings 
   // so notify the client.
   setMaskBits( UpdateMask );

   // Let the parent do the final scale.
   Parent::setScale( VectorF( radius, mRange, radius ) );
}

void SpotLight::_renderViz( SceneState *state )
{
   GFXDrawUtil *draw = GFX->getDrawUtil();

   GFXStateBlockDesc desc;
   desc.setZReadWrite( true, false );
   desc.setCullMode( GFXCullNone );
   desc.setBlend( true );

   // Base the color on the light color.
   ColorI color( mColor );
   color.alpha = 16;

   F32 radius = mRange * mSin( mDegToRad( mOuterConeAngle * 0.5f ) );
   draw->drawCone(   desc, 
                     getPosition() + ( getTransform().getForwardVector() * mRange ),
                     getPosition(),
                     radius,
                     color );
}
