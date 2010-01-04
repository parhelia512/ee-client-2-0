//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TERRFILE_H_
#define _TERRFILE_H_

#ifndef _TVECTOR_H_
#include <core/util/tVector.h>
#endif
#ifndef _PATH_H_
#include <core/util/path.h>
#endif
#ifndef _MATERIALLIST_H_
#include "materials/materialList.h"
#endif

class TerrainMaterial;
class FileStream;
class GBitmap;


///
struct TerrainSquare
{
   U16 minHeight;

   U16 maxHeight;

   U16 heightDeviance;

   U16 flags;

   enum 
   {
      Split45  = BIT(0),

      Empty    = BIT(1),

      HasEmpty = BIT(2),
   };
};


/// NOTE:  The terrain uses 11.5 fixed point which gives
/// us a height range from 0->2048 in 1/32 increments.
typedef U16 TerrainHeight;


/// 
class TerrainFile
{
protected:

   friend class TerrainBlock;

   /// The materials used to render the terrain.
   Vector<TerrainMaterial*> mMaterials;

   /// The dimensions of the layer and height maps.
   U32 mSize;

   /// The layer index at each height map sample.
   Vector<U8> mLayerMap;

   /// The fixed point height map.
   /// @see fixedToFloat
   Vector<U16> mHeightMap;

   /// The memory pool used by the grid map layers.
   Vector<TerrainSquare> mGridMapPool;

   ///
   U32 mGridLevels;

   /// The grid map layers used to accelerate collision
   /// queries for the height map data.
   Vector<TerrainSquare*> mGridMap;
   
   /// MaterialList used to map terrain materials to material instances for the
   /// sake of collision (physics, etc.).
   MaterialList mMaterialInstMapping;

   /// The file version.
   U32 mFileVersion;     

   /// The dirty flag.
   bool mNeedsResaving;  

   /// The full path and name of the TerrainFile
   Torque::Path mFilePath;

   /// The internal loading function.
   void _load( FileStream &stream );

   /// The legacy file loading code.
   void _loadLegacy( FileStream &stream );

   /// Used to populate the materail vector by finding the 
   /// TerrainMaterial objects by name.
   void _resolveMaterials( const Vector<String> &materials );

   /// 
   void _buildGridMap();
   
   ///
   void _initMaterialInstMapping();

public:

   enum Constants
   {
      FILE_VERSION = 7
   };

   TerrainFile();

   virtual ~TerrainFile();

   ///
   static void create(  String *inOutFilename, 
                        U32 newSize, 
                        const Vector<String> &materials );

   ///
   static TerrainFile* load( const Torque::Path &path );

   bool save( const char *filename );

   ///
   void import(   const GBitmap &heightMap, 
                  F32 heightScale,
                  const Vector<U8> &layerMap, 
                  const Vector<String> &materials );

   /// Updates the terrain grid for the specified area.
   void updateGrid( const Point2I &minPt, const Point2I &maxPt );

   /// Performs multiple smoothing steps on the heightmap.
   void smooth( F32 factor, U32 steps, bool updateCollision );

   void setSize( U32 newResolution, bool clear );

   TerrainSquare* findSquare( U32 level, U32 x, U32 y ) const;
   
   BaseMatInstance* getMaterialMapping( U32 index ) const;
   
   void setLayerIndex( U32 x, U32 y, U8 index );

   U8 getLayerIndex( U32 x, U32 y ) const;

   bool isEmptyAt( U32 x, U32 y ) const { return getLayerIndex( x, y ) == U8_MAX; }

   void setHeight( U32 x, U32 y, U16 height );

   const U16* getHeightAddress( U32 x, U32 y ) const;

   U16 getHeight( U32 x, U32 y ) const;

   U16 getMaxHeight() const { return mGridMap[mGridLevels]->maxHeight; }

   /// Returns the constant heightmap vector.
   const Vector<U16>& getHeightMap() const { return mHeightMap; }

   /// Sets a new heightmap state.
   void setHeightMap( const Vector<U16> &heightmap, bool updateCollision );

};


inline TerrainSquare* TerrainFile::findSquare( U32 level, U32 x, U32 y ) const
{
   x %= mSize;
   y %= mSize;
   x >>= level;
   y >>= level;

   return mGridMap[level] + x + ( y << ( mGridLevels - level ) );
}

inline void TerrainFile::setHeight( U32 x, U32 y, U16 height )
{
   x %= mSize;
   y %= mSize;
   mHeightMap[ x + ( y * mSize ) ] = height;
}

inline const U16* TerrainFile::getHeightAddress( U32 x, U32 y ) const
{
   x %= mSize;
   y %= mSize;
   return &mHeightMap[ x + ( y * mSize ) ];
}

inline U16 TerrainFile::getHeight( U32 x, U32 y ) const
{
   x %= mSize;
   y %= mSize;
   return mHeightMap[ x + ( y * mSize ) ];
}

inline U8 TerrainFile::getLayerIndex( U32 x, U32 y ) const
{
   x %= mSize;
   y %= mSize;
   return mLayerMap[ x + ( y * mSize ) ];
}

inline void TerrainFile::setLayerIndex( U32 x, U32 y, U8 index )
{
   x %= mSize;
   y %= mSize;
   mLayerMap[ x + ( y * mSize ) ] = index;
}

inline BaseMatInstance* TerrainFile::getMaterialMapping( U32 index ) const
{
   if ( index < mMaterialInstMapping.size() )
      return mMaterialInstMapping.getMaterialInst( index );
   else
      return NULL;
}



/// Conversion from 11.5 fixed point to floating point.
inline F32 fixedToFloat( U16 val )
{
   return F32(val) * 0.03125f;
}

/// Conversion from floating point to 11.5 fixed point.
inline U16 floatToFixed( F32 val )
{
   return U16(val * 32.0);
}

#endif // _TERRFILE_H_ 
