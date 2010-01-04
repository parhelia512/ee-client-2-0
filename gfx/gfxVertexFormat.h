//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXVERTEXFORMAT_H_
#define _GFXVERTEXFORMAT_H_

#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _GFXENUMS_H_
#include "gfx/gfxEnums.h"
#endif


/// The known Torque vertex element semantics.  You can use
/// other semantic strings, but they will be interpreted as
/// a TEXCOORD.
/// @see GFXVertexElement
/// @see GFXVertexFormat
struct GFXSemantic
{
   static const String POSITION;
   static const String NORMAL;
   static const String BINORMAL;
   static const String TANGENT;
   static const String TANGENTW;
   static const String COLOR;
   static const String TEXCOORD;
};


/// The element structure helps define the data layout 
/// for GFXVertexFormat.
///
/// @see GFXVertexFormat
///
class GFXVertexElement
{
   friend class GFXVertexFormat;

protected:

   /// A valid Torque shader symantic.
   /// @see GFXSemantic   
   String mSemantic;

   /// The semantic index is used where there are
   /// multiple semantics of the same type.  For 
   /// instance with texcoords.
   U32 mSemanticIndex;

   /// The element type.
   GFXDeclType mType;

public:

   /// Default constructor.
   GFXVertexElement()
      :  mSemanticIndex( 0 ),
         mType( GFXDeclType_Float4 )
   {
   }

   /// Copy constructor.
   GFXVertexElement( const GFXVertexElement &elem )
      :  mSemantic( elem.mSemantic ),
         mSemanticIndex( elem.mSemanticIndex ),
         mType( elem.mType )
   {
   }

   /// Returns the semantic name which is usually a
   /// valid Torque semantic.
   /// @see GFXSemantic
   const String& getSemantic() const { return mSemantic; }

   /// Returns the semantic index which is used where there
   /// are multiple semantics of the same type.  For instance
   /// with texcoords.
   U32 getSemanticIndex() const { return mSemanticIndex; }

   /// Returns the type for the semantic.
   GFXDeclType getType() const { return mType; }

   /// Returns true of the semantic matches.
   bool isSemantic( const String& str ) const { return ( mSemantic == str ); }

   /// Returns the size in bytes of the semantic type.
   U32 getSizeInBytes() const;

};


/// The vertex format structure usually created via the declare and 
/// implement macros.
///
/// You can use this class directly to create a vertex format, but
/// note that it is expected to live as long as the VB that uses it 
/// exists.
///
/// @see GFXDeclareVertexFormat
/// @see GFXImplementVertexFormat
/// @see GFXVertexElement
///
class GFXVertexFormat
{
public:

   /// Default constructor for an empty format.
   GFXVertexFormat();

   /// Returns a 64bit hash string which uniquely identifies
   /// this vertex format.
   //const String& getCacheString() const;

   /// Returns a unique description string for this vertex format.
   const String& getDescription() const;

   /// Clears all the vertex elements.
   void clear();

   /// Adds a vertex element to the format.
   ///
   /// @param semantic A valid Torque semantic string.
   /// @param type The element type.
   /// @param index The semantic index which is typically only used for texcoords.
   ///
   void addElement( const String& semantic, GFXDeclType type, U32 index = 0 );

   /// Returns true if the format has a normal 
   /// and a tangent at each vertex.
   bool hasNormalAndTangent() const;

   /// Returns true if there is at least one color 
   /// symantic in the vertex format.
   bool hasColor() const;

   /// Returns the texture coordinate count by 
   /// counting the number of "TEXCOORD" semantics.
   U32 getTexCoordCount() const;

   /// Returns true if these two formats are equal.
   bool isEqual( const GFXVertexFormat &format ) const;

   /// Returns the total elements in this format.
   U32 getElementCount() const { return mElements.size(); }

   /// Returns the vertex element by index.
   const GFXVertexElement& getElement( U32 index ) const { return mElements[index]; }

protected:

   /// Recreates the description and state when 
   /// the format has been modified.
   void _updateDirty();

   /// Set when the element list is changed.
   bool mDirty;

   /// Is set to true if there is a normal and
   /// a tanget and/or binormal in this format.
   bool mHasNormalAndTangent;

   /// Is true if there is at least one color 
   /// symantic in the vertex format.
   bool mHasColor;

   /// The texture coordinate count by counting the 
   /// number of "TEXCOORD" semantics.
   U32 mTexCoordCount;

   /// The a string which uniquely identifies
   /// this vertex format.
   String mDescription;
   
   /// The elements of the vertex format.
   Vector<GFXVertexElement> mElements;

};


/// This template class is usused to initialize the format in 
/// the GFXImplement/DeclareVertexFormat macros.  You shouldn't
/// need to use it directly in your code.
///
/// @see GFXVertexFormat
/// @see GFXImplementVertexFormat
///
template<class T>
class _GFXVertexFormatConstructor : public GFXVertexFormat
{
protected:

   void _construct();

public:

   _GFXVertexFormatConstructor() { _construct(); }
};


/// Helper template function which returns the correct 
/// GFXVertexFormat object for a vertex structure.
/// @see GFXVertexFormat
template<class T> inline const GFXVertexFormat* getGFXVertexFormat();

#ifdef TORQUE_OS_XENON

   /// On the Xbox360 we want we want to be sure that verts
   /// are on aligned boundariess.
   #define GFX_VERTEX_STRUCT __declspec(align(16)) struct

#else
   #define GFX_VERTEX_STRUCT struct
#endif


/// The vertex format declaration which is usally placed in your header 
/// file.  It should be used in conjunction with the implementation macro.
/// 
/// @param name The name for the vertex structure.
///
/// @code
///   
///   // A simple vertex format declaration.
///   GFXDeclareVertexFormat( GFXVertexPCT )
///   {
///      Point3F pos;
///      GFXVertexColor color;
///      Point2F texCoord;
///   }
///
/// @endcode
///
/// @see GFXImplementVertexFormat
///
#define GFXDeclareVertexFormat( name ) \
   GFX_VERTEX_STRUCT name; \
   extern const GFXVertexFormat _gfxVertexFormat##name; \
   template<> inline const GFXVertexFormat* getGFXVertexFormat<name>() { static _GFXVertexFormatConstructor<name> vertexFormat; return &vertexFormat; } \
   GFX_VERTEX_STRUCT name \


/// The vertex format implementation which is usally placed in your source 
/// file.  It should be used in conjunction with the declaration macro.
/// 
/// @param name The name of the vertex structure.
///
/// @code
///   
///   // A simple vertex format implementation.
///   GFXImplementVertexFormat( GFXVertexPCT )
///   {
///      addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
///      addElement( GFXSemantic::COLOR, GFXDeclType_Color );
///      addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
///   }
///
/// @endcode
///
/// @see GFXDeclareVertexFormat
///
#define GFXImplementVertexFormat( name ) \
   template<>   void _GFXVertexFormatConstructor<name>::_construct() \

#endif // _GFXVERTEXFORMAT_H_
