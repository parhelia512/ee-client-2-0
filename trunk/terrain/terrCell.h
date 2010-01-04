//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TERRCELL_H_
#define _TERRCELL_H_

#ifndef _GFXVERTEXBUFFER_H_
   #include "gfx/gfxVertexBuffer.h"
#endif
#ifndef _TDICTIONARY_H_
   #include "core/util/tDictionary.h"
#endif

class TerrainBlock;
class TerrainCellMaterial;
class Frustum;
class SceneState;
class GFXPrimitiveBufferHandle;


/// The TerrainCell vertex format optimized to
/// 32 bytes for optimal vertex cache performance.
GFXDeclareVertexFormat( TerrVertex )
{
   /// The position.
   Point3F point;

   /// The normal.
   Point3F normal;

   /// The height for calculating the
   /// tangent vector on the GPU.
   F32 tangentZ;

   /// The empty flag state which is either 
   /// -1 or 1 so we can do the special 
   /// interpolation trick.
   F32 empty;
};


/// The TerrCell is a single quadrant of the terrain geometry quadtree.
class TerrCell
{
protected:

   /// The handle to the static vertex buffer which holds the 
   /// vertices for this cell.
   GFXVertexBufferHandle<TerrVertex> mVertexBuffer;

   ///
   Point2I mPoint;
   
   ///
   U32 mSize;

   /// The level of this cell within the quadtree (of cells) where
   /// zero is the root and one is a direct child of the root, etc.
   U32 mLevel;

   /// Statics used in VB and PB generation.   
   static const U32 smVBStride;
   static const U32 smMinCellSize;
   static const U32 smVBSize;
   static const U32 smPBSize;
   static const U32 smTriCount;

   /// The terrain this cell is based on.
   TerrainBlock *mTerrain;

   /// The material used to render the cell.
   TerrainCellMaterial *mMaterial;

   /// The bounding box of this cell in 
   /// TerrainBlock object space.
   Box3F mBounds;

   /// The bounding radius of this cell.
   F32 mRadius;

   /// The child cells of this one.
   TerrCell *mChildren[4];

   /// This bit flag tells us which materials effect
   /// this cell and is used for optimizing rendering.
   /// @see TerrainFile::mMaterialAlphaMap
   U64 mMaterials;

   ///
   void _updateBounds();

   //
   void _init( TerrainBlock *terrain,
               const Point2I &point,
               U32 size,
               U32 level );

   // 
   void _updateVertexBuffer();

   // 
   void _updateMaterials();

public:

   TerrCell();
   virtual ~TerrCell();

   static TerrCell* init( TerrainBlock *terrain );

   void getRenderPrimitive(   GFXPrimitive *prim,
                              GFXVertexBufferHandleBase *vertBuff ) const;

   void updateGrid( const RectI &gridRect, bool opacityOnly = false );

   void cullCells( const Frustum *culler, 
                   const SceneState *state,
                   const Point3F &objLodPos,
                   Vector<TerrCell*> *outCells );

   const Box3F& getBounds() const { return mBounds; }

   /// Returns the object space sphere bounds.
   SphereF getSphereBounds() const { return SphereF( mBounds.getCenter(), mRadius ); }

   F32 getSqDistanceTo( const Point3F &pt ) const;

   F32 getDistanceTo( const Point3F &pt ) const;

   U64 getMaterials() const { return mMaterials; }

   TerrainCellMaterial* getMaterial();

   /// Deletes the materials for this cell 
   /// and all its children.  They will be
   /// recreate on the next request.
   void deleteMaterials();

   U32 getSize() const { return mSize; }

   Point2I getPoint() const { return mPoint; }   

   /// Initializes a primitive buffer for rendering any cell.
   static void createPrimBuffer( GFXPrimitiveBufferHandle *primBuffer );

   /// Debug Rendering
   /// @{

   /// Renders the debug bounds for this cell.
   void renderBounds() const;

   /// @}
};

inline F32 TerrCell::getDistanceTo( const Point3F &pt ) const
{
   return ( mBounds.getCenter() - pt ).len() - mRadius;
}

#endif // _TERRCELL_H_
