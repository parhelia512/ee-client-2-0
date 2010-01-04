//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _GENERICCONSTBUFFER_H_
#define _GENERICCONSTBUFFER_H_

#ifndef _TORQUE_STRING_H_
#include "core/util/str.h"
#endif

#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif

#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif

#ifndef _ALIGNEDARRAY_H_
#include "core/util/tAlignedArray.h"
#endif

#ifndef _COLOR_H_
#include "core/color.h"
#endif

#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif

#ifndef _MPOINT2_H_
#include "math/mPoint2.h"
#endif

#ifndef _GFXENUMS_H_
#include "gfx/gfxEnums.h"
#endif

#ifndef _STREAM_H_
#include "core/stream/stream.h"
#endif

///
/// This class describes a memory layout for a GenericConstBuffer
///
class GenericConstBufferLayout 
{   
public:
   /// Describes the parameters we contain
   struct ParamDesc
   {
      /// Parameter name
      String name;
      /// Offset into the memory block
      U32 offset;
      /// Size of the block
      U32 size;
      /// Type of data
      GFXShaderConstType constType;
      // For arrays, how many elements
      U32 arraySize;
      // Array element alignment value
      U32 alignValue;
      /// 0 based index of this param, in order of addParameter calls.
      U32 index;
   };

   GenericConstBufferLayout();
   virtual ~GenericConstBufferLayout() {}

   /// Add a parameter to the buffer
   void addParameter(const String& name, const GFXShaderConstType constType, const U32 offset, const U32 size, const U32 arraySize, const U32 alignValue);

   /// Get the size of the buffer
   U32 getBufferSize() const { return mBufferSize; }

   /// Get the number of parameters
   U32 getParameterCount() const { return mParams.size(); }

   /// Returns the ParamDesc of a parameter 
   bool getDesc(const String& name, ParamDesc& param) const;

   /// Returns the ParamDesc of a parameter 
   bool getDesc(const U32 index, ParamDesc& param) const;

   /// Set a parameter, given a base pointer
   virtual bool set(const ParamDesc& pd, const GFXShaderConstType constType, const U32 size, const void* data, U8* basePointer);

   /// Save this layout to a stream
   bool write(Stream* s);

   /// Load this layout from a stream
   bool read(Stream* s);

   /// Restore to initial state.
   void clear();

protected:
   /// Set a matrix, given a base pointer
   virtual bool setMatrix(const ParamDesc& pd, const GFXShaderConstType constType, const U32 size, const void* data, U8* basePointer);

   /// Vector of parameter descriptions
   typedef Vector<ParamDesc> Params;
   
   /// Vector of parameter descriptions
   Params mParams;
   U32 mBufferSize;
   U32 mCurrentIndex;

   // This if for debugging shader reloading and can be removed later.
   U32 mTimesCleared;
};

/// This class will be used by other const buffers and the material system.  Takes a set of variable names and maps them to
/// a section of memory.  Should this descend from GFXConstBuffer?  I don't know, we need two of them (one for vert, one for
/// pixel shaders for D3D9, maybe it'd be useful in OpenGL?)
class GenericConstBuffer
{
public:
   GenericConstBuffer(GenericConstBufferLayout* layout);
   ~GenericConstBuffer();

   /// @name Set shader constant values
   /// @{
   /// Actually set shader constant values
   /// @param name Name of the constant, this should be a name contained in the array returned in getShaderConstDesc,
   /// if an invalid name is used, its ignored, but it's not an error.
   void set(const GenericConstBufferLayout::ParamDesc& pd, const F32 f) { internalSet(pd, GFXSCT_Float, sizeof(F32), &f); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const Point2F& fv) { internalSet(pd, GFXSCT_Float2, sizeof(Point2F), &fv); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const Point3F& fv) { internalSet(pd, GFXSCT_Float3, sizeof(Point3F), &fv); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const Point4F& fv) { internalSet(pd, GFXSCT_Float4, sizeof(Point4F), &fv); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const PlaneF& fv) { internalSet(pd, GFXSCT_Float4, sizeof(PlaneF), &fv); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const ColorF& fv) { internalSet(pd, GFXSCT_Float4, sizeof(Point4F), &fv); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const S32 f) { internalSet(pd, GFXSCT_Int, sizeof(S32), &f); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const Point2I& fv) { internalSet(pd, GFXSCT_Int2, sizeof(Point2I), &fv); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const Point3I& fv) { internalSet(pd, GFXSCT_Int3, sizeof(Point3I), &fv); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const Point4I& fv) { internalSet(pd, GFXSCT_Int4, sizeof(Point4I), &fv); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const AlignedArray<F32>& fv) { internalSet(pd, GFXSCT_Float, fv.getElementSize() * fv.size(), fv.getBuffer()); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const AlignedArray<Point2F>& fv) { internalSet(pd, GFXSCT_Float2, fv.getElementSize() * fv.size(), fv.getBuffer()); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const AlignedArray<Point3F>& fv) { internalSet(pd, GFXSCT_Float3, fv.getElementSize() * fv.size(), fv.getBuffer()); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const AlignedArray<Point4F>& fv) { internalSet(pd, GFXSCT_Float4, fv.getElementSize() * fv.size(), fv.getBuffer()); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const AlignedArray<S32>& fv) { internalSet(pd, GFXSCT_Int, fv.getElementSize() * fv.size(), fv.getBuffer()); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const AlignedArray<Point2I>& fv) { internalSet(pd, GFXSCT_Int2, fv.getElementSize() * fv.size(), fv.getBuffer()); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const AlignedArray<Point3I>& fv) { internalSet(pd, GFXSCT_Int3, fv.getElementSize() * fv.size(), fv.getBuffer()); }
   void set(const GenericConstBufferLayout::ParamDesc& pd, const AlignedArray<Point4I>& fv) { internalSet(pd, GFXSCT_Int4, fv.getElementSize() * fv.size(), fv.getBuffer()); }

   void set(const GenericConstBufferLayout::ParamDesc& pd, const MatrixF& mat, const GFXShaderConstType matrixType)
   {
      AssertFatal(matrixType == GFXSCT_Float2x2 || matrixType == GFXSCT_Float3x3 || matrixType == GFXSCT_Float4x4, "Invalid matrix type!");
      internalSet(pd, matrixType, sizeof(MatrixF), &mat);
   }

   void set(const GenericConstBufferLayout::ParamDesc& pd, const MatrixF* mat, const U32 arraySize, const GFXShaderConstType matrixType)
   {
      AssertFatal(matrixType == GFXSCT_Float2x2 || matrixType == GFXSCT_Float3x3 || matrixType == GFXSCT_Float4x4, "Invalid matrix type!");
      internalSet(pd, matrixType, sizeof(MatrixF)*arraySize, mat);
   }

   /// This scans the fields and returns a pointer to the first dirty field
   /// and the length of the dirty bytes
   const U8* getDirtyBuffer(U32& start, U32& size);

   void setDirty(bool dirty);
   bool isDirty() const { return mDirty; }

   /// Returns true if we hold the same data as buffer and have the same layout
   bool isEqual(GenericConstBuffer* buffer) const;

   /// Returns our layout
   GenericConstBufferLayout* getLayout() { return mLayout; }
private:
   /// Returns a pointer to the raw buffer
   const U8* getBuffer() const { return mBuffer; }

   /// Called by the set functions above.
   void internalSet(const GenericConstBufferLayout::ParamDesc& pd, const GFXShaderConstType constType, const U32 size, const void* data);

   /// Returns true if the field is marked dirty
   /// @param index is the ParamDesc.Index in GenericConstBufferLayout
   bool isFieldDirty(const U32 index) const { return mDirtyFields[index] && mHasData[index]; }

   /// Buffer layout
   GenericConstBufferLayout* mLayout;

   /// Buffer
   U8* mBuffer;

   Vector<bool> mDirtyFields;
   Vector<bool> mHasData;
   bool mDirty;
};
#endif
