//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gfx/gfxVertexFormat.h"

#include "platform/profiler.h"
#include "core/util/hashFunction.h"


const String GFXSemantic::POSITION = String( "POSITION" ).intern();
const String GFXSemantic::NORMAL = String( "NORMAL" ).intern();
const String GFXSemantic::BINORMAL = String( "BINORMAL" ).intern();
const String GFXSemantic::TANGENT = String( "TANGENT" ).intern();
const String GFXSemantic::TANGENTW = String( "TANGENTW" ).intern();
const String GFXSemantic::COLOR = String( "COLOR" ).intern();
const String GFXSemantic::TEXCOORD = String( "TEXCOORD" ).intern();



U32 GFXVertexElement::getSizeInBytes() const
{
   switch ( mType )
   {
      case GFXDeclType_Float:
         return 4;

      case GFXDeclType_Float2:
         return 8;

      case GFXDeclType_Float3:
         return 12;

      case GFXDeclType_Float4:
         return 16;

      case GFXDeclType_Color:
         return 4;

      default:
         return 0;
   };
}


GFXVertexFormat::GFXVertexFormat()
   :  mDirty( true ),
      mHasColor( false ),
      mHasNormalAndTangent( false ),
      mTexCoordCount( 0 )
{
   VECTOR_SET_ASSOCIATION( mElements );
}

void GFXVertexFormat::clear() 
{ 
   mDirty = true;
   mElements.clear(); 
}

void GFXVertexFormat::addElement( const String& semantic, GFXDeclType type, U32 index ) 
{ 
   mDirty = true;
   mElements.increment();
   mElements.last().mSemantic = semantic.intern();
   mElements.last().mSemanticIndex = index;
   mElements.last().mType = type;      
}

const String& GFXVertexFormat::getDescription() const
{
   if ( mDirty )
      const_cast<GFXVertexFormat*>(this)->_updateDirty();

   return mDescription;
}

bool GFXVertexFormat::hasNormalAndTangent() const
{
   if ( mDirty )
      const_cast<GFXVertexFormat*>(this)->_updateDirty();

   return mHasNormalAndTangent;
}

bool GFXVertexFormat::hasColor() const
{
   if ( mDirty )
      const_cast<GFXVertexFormat*>(this)->_updateDirty();

   return mHasColor;
}

U32 GFXVertexFormat::getTexCoordCount() const
{
   if ( mDirty )
      const_cast<GFXVertexFormat*>(this)->_updateDirty();

   return mTexCoordCount;
}

bool GFXVertexFormat::isEqual( const GFXVertexFormat &format ) const
{
   return getDescription().equal( format.getDescription(), String::NoCase );
}

void GFXVertexFormat::_updateDirty()
{
   PROFILE_SCOPE( FeatureSet_UpdateDirty );

   mTexCoordCount = 0;

   mHasColor = false;

   bool hasNormal = false;
   bool hasTangent = false;

   mDescription.clear();

   for ( U32 i=0; i < mElements.size(); i++ )
   {
      const GFXVertexElement &element = mElements[i];

      mDescription += String::ToString( "%s,%d,%d\n", element.mSemantic.c_str(), 
                                                      element.mSemanticIndex, 
                                                      element.mType );

      if ( element.isSemantic( GFXSemantic::NORMAL ) )
         hasNormal = true;
      else if ( element.isSemantic( GFXSemantic::TANGENT ) )
         hasTangent = true;
      else if ( element.isSemantic( GFXSemantic::COLOR ) )
         mHasColor = true;
      else if ( element.isSemantic( GFXSemantic::TEXCOORD ) )
         ++mTexCoordCount;
   }

   mHasNormalAndTangent = hasNormal && hasTangent;

   // Make sure the hash is created here once
   // so that it can be used in comparisions later.
   mDescription.getHashCaseInsensitive();

   mDirty = false;
}
