//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/lightInfo.h"

#include "math/mMath.h"
#include "core/color.h"
#include "gfx/gfxCubemap.h"
#include "console/simObject.h"
#include "math/mathUtils.h"


LightInfoExType::LightInfoExType( const char *type )
{
   TypeMap::Iterator iter = getTypeMap().find( type );
   if ( iter == getTypeMap().end() )
      iter = getTypeMap().insertUnique( type, getTypeMap().size() );

   mTypeIndex = iter->value;
}


LightInfo::LightInfo() 
   :  mTransform( true ), 
      mColor( 0.0f, 0.0f, 0.0f, 1.0f ), 
      mBrightness( 1.0f ),
      mAmbient( 0.0f, 0.0f, 0.0f, 1.0f ), 
      mRange( 1.0f, 1.0f, 1.0f ),
      mInnerConeAngle( 90.0f ), 
      mOuterConeAngle( 90.0f ),
      mType( Vector ),
      mCastShadows( false ),
      mPriority( 1.0f ),
      mScore( 0.0f )
{
}

LightInfo::~LightInfo()
{
   deleteAllLightInfoEx();
}

void LightInfo::set( const LightInfo *light )
{
   mTransform = light->mTransform;
   mColor = light->mColor;
   mBrightness = light->mBrightness;
   mAmbient = light->mAmbient;
   mRange = light->mRange;
   mInnerConeAngle = light->mInnerConeAngle;
   mOuterConeAngle = light->mOuterConeAngle;
   mType = light->mType;
   mCastShadows = light->mCastShadows;

   for ( U32 i=0; i < mExtended.size(); i++ )
   {
      LightInfoEx *ex = light->mExtended[ i ];
      if ( ex )
         mExtended[i]->set( ex );
      else
      {
         delete mExtended[i];
         mExtended[i] = NULL;
      }
   }
}

void LightInfo::setGFXLight( GFXLightInfo *outLight )
{
   switch( getType() )
   {
      case LightInfo::Point :
         outLight->mType = GFXLightInfo::Point;
         break;
      case LightInfo::Spot :
         outLight->mType = GFXLightInfo::Spot;
         break;
      case LightInfo::Vector:
         outLight->mType = GFXLightInfo::Vector;
         break;
      case LightInfo::Ambient:
         outLight->mType = GFXLightInfo::Ambient;
         break;
      default:
         break;
   }

   outLight->mPos = getPosition();
   outLight->mDirection = getDirection();
   outLight->mColor = mColor * mBrightness;
   outLight->mAmbient = mAmbient;
   outLight->mRadius = mRange.x;
   outLight->mInnerConeAngle = mInnerConeAngle;
   outLight->mOuterConeAngle = mOuterConeAngle;
}

void LightInfo::setDirection( const VectorF &dir )
{
   MathUtils::getMatrixFromForwardVector( mNormalize( dir ), &mTransform );
}

void LightInfo::deleteAllLightInfoEx()
{
   for ( U32 i = 0; i < mExtended.size(); i++ )
      delete mExtended[ i ];

   mExtended.clear();
}

LightInfoEx* LightInfo::getExtended( const LightInfoExType &type ) const
{
   if ( type >= mExtended.size() )
      return NULL;

   return mExtended[ type ];
}

void LightInfo::addExtended( LightInfoEx *lightInfoEx )
{
   AssertFatal( lightInfoEx, "LightInfo::addExtended() - Got null extended light info!" );
   
   const LightInfoExType &type = lightInfoEx->getType();

   while ( mExtended.size() <= type )
      mExtended.push_back( NULL );

   delete mExtended[type];
   mExtended[type] = lightInfoEx;
}

void LightInfo::packExtended( BitStream *stream ) const
{
   for ( U32 i = 0; i < mExtended.size(); i++ )
      if ( mExtended[ i ] ) 
         mExtended[ i ]->packUpdate( stream );
}

void LightInfo::unpackExtended( BitStream *stream )
{
   for ( U32 i = 0; i < mExtended.size(); i++ )
      if ( mExtended[ i ] ) 
         mExtended[ i ]->unpackUpdate( stream );
}

void LightInfoList::registerLight( LightInfo *light )
{
   if(!light)
      return;
   // just add the light, we'll try to scan for dupes later...
   push_back(light);
}

void LightInfoList::unregisterLight( LightInfo *light )
{
   // remove all of them...
   LightInfoList &list = *this;
   for(U32 i=0; i<list.size(); i++)
   {
      if(list[i] != light)
         continue;

      // this moves last to i, which allows
      // the search to continue forward...
      list.erase_fast(i);
      // want to check this location again...
      i--;
   }
}
