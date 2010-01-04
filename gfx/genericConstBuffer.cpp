//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/genericConstBuffer.h"
#include "platform/profiler.h"

///
/// GenericConstBufferLayout
///
GenericConstBufferLayout::GenericConstBufferLayout()
{
   VECTOR_SET_ASSOCIATION( mParams );

   mBufferSize = 0;
   mCurrentIndex = 0;
   mTimesCleared = 0;
}

/// Add a parameter to the buffer
void GenericConstBufferLayout::addParameter(const String& name, const GFXShaderConstType constType, const U32 offset, const U32 size, const U32 arraySize, const U32 alignValue)
{
#ifdef TORQUE_DEBUG
   // Make sure we don't have overlapping parameters
   S32 start = offset;
   S32 end = offset + size;
   for (Params::iterator i = mParams.begin(); i != mParams.end(); i++)
   {
      const ParamDesc& dp = *i;
      S32 pstart = dp.offset;
      S32 pend = pstart + dp.size;
      pstart -= start;
      pend -= end;
      // This is like a minkowski sum for two line segments, if the newly formed line contains
      // the origin, then they intersect
      bool intersect = ((pstart >= 0 && 0 >= pend) || ((pend >= 0 && 0 >= pstart)));
      AssertFatal(!intersect, "Overlapping shader parameter!");
   }
#endif
   ParamDesc desc;
   desc.name = name;
   desc.constType = constType;
   desc.offset = offset;
   desc.size = size;
   desc.arraySize = arraySize;
   desc.alignValue = alignValue;
   desc.index = mCurrentIndex++;
   mParams.push_back(desc);
   mBufferSize = getMax(desc.offset + desc.size, mBufferSize);
   AssertFatal(mBufferSize, "Empty constant buffer!");
}

/// Set a parameter, given a base pointer 
/// Set a parameter, given a base pointer
bool GenericConstBufferLayout::set(const ParamDesc& pd, const GFXShaderConstType constType, const U32 size, const void* data, U8* basePointer)
{
   PROFILE_SCOPE(GenericConstBufferLayout_set);
   AssertFatal(pd.constType == constType, "Mismatched const type!");

   // This "cute" bit of code allows us to support 2x3 and 3x3 matrices in shader constants but use our MatrixF class.  Yes, a hack. -BTR
   switch (pd.constType)
   {
   case GFXSCT_Float2x2 :
   case GFXSCT_Float3x3 :
   case GFXSCT_Float4x4 :
      return setMatrix(pd, constType, size, data, basePointer);
      break;
   default :
      break;
   }

   AssertFatal(pd.size >= size, "Not enough room in the buffer for this data!");

   // Ok, we only set data if it's different than the data we already have, this maybe more expensive than just setting the data, but 
   // we'll have to do some timings to see.  For example, the lighting shader constants rarely change, but we can't assume that at the
   // renderInstMgr level, but we can check down here. -BTR
   if (dMemcmp(basePointer+pd.offset, data, size) != 0)      
   {
      dMemcpy(basePointer+pd.offset, data, size);      
      return true;
   }   
   return false;
}

// Matrices are an annoying case because of the alignment issues.  There are alignment issues in the matrix itself, and then potential inter matrices alignment issues.
// So GL and DX will need to derive their own GenericConstBufferLayout classes and override this method to deal with that stuff.  For GenericConstBuffer, copy the whole
// 4x4 matrix regardless of the target case.
bool GenericConstBufferLayout::setMatrix(const ParamDesc& pd, const GFXShaderConstType constType, const U32 size, const void* data, U8* basePointer)
{
   // We're generic, so just copy the full MatrixF in
   AssertFatal(pd.size >= size, "Not enough room in the buffer for this data!");

   if (dMemcmp(basePointer+pd.offset, data, size) != 0)
   {      
      dMemcpy(basePointer+pd.offset, data, size);         
      return true;
   }
      
   return false;
}

/// Returns the description of a parameter 
bool GenericConstBufferLayout::getDesc(const String& name, ParamDesc& param) const
{
   for (U32 i = 0; i < mParams.size(); i++)
   {
      if (mParams[i].name.equal(name))
      {
         param = mParams[i];
         return true;
      }
   } 
   return false;
}

/// Returns the ParamDesc of a parameter 
bool GenericConstBufferLayout::getDesc(const U32 index, ParamDesc& param) const
{
   if ( index < mParams.size() )
   {
      param = mParams[index];
      return true;
   }

   return false;
}

/// Save this layout to a stream
bool GenericConstBufferLayout::write(Stream* s)
{
   // Write out the size of the ParamDesc structure as a sanity check.
   if (!s->write((U32) sizeof(ParamDesc)))
      return false;
   // Next, write out the number of elements we've got.
   if (!s->write(mParams.size()))
      return false;
   for (U32 i = 0; i < mParams.size(); i++)
   {
      s->write(mParams[i].name);
         
      if (!s->write(mParams[i].offset))
         return false;
      if (!s->write(mParams[i].size))
         return false;
      U32 t = (U32) mParams[i].constType;
      if (!s->write(t))
         return false;
      if (!s->write(mParams[i].arraySize))
         return false;
      if (!s->write(mParams[i].alignValue))
         return false;
      if (!s->write(mParams[i].index))
         return false;
   }
   return true;
}

/// Load this layout from a stream
bool GenericConstBufferLayout::read(Stream* s)
{
   U32 structSize;
   if (!s->read(&structSize))
      return false;
   if (structSize != sizeof(ParamDesc))
   {
      AssertFatal(false, "Invalid shader layout structure size!");
      return false;
   }
   U32 numParams;
   if (!s->read(&numParams))
      return false;
   mParams.setSize(numParams);
   mBufferSize = 0;
   mCurrentIndex = 0;
   for (U32 i = 0; i < mParams.size(); i++)
   {
      s->read(&mParams[i].name);         
      if (!s->read(&mParams[i].offset))
         return false;
      if (!s->read(&mParams[i].size))
         return false;
      U32 t;
      if (!s->read(&t))
         return false;
      mParams[i].constType = (GFXShaderConstType) t;
      if (!s->read(&mParams[i].arraySize))
         return false;
      if (!s->read(&mParams[i].alignValue))
         return false;
      if (!s->read(&mParams[i].index))
         return false;
      mBufferSize = getMax(mParams[i].offset + mParams[i].size, mBufferSize);
      mCurrentIndex = getMax(mParams[i].index, mCurrentIndex);
   }
   mCurrentIndex++;
   return true;
}

void GenericConstBufferLayout::clear()
{
   mParams.clear();    
   mBufferSize = 0;
   mCurrentIndex = 0;
   mTimesCleared++;
}

///
/// GenericConstBuffer
///
GenericConstBuffer::GenericConstBuffer(GenericConstBufferLayout* layout)
{
   VECTOR_SET_ASSOCIATION( mDirtyFields );
   VECTOR_SET_ASSOCIATION( mHasData );

   if (layout)
   {
      mLayout = layout;
      mBuffer = new U8[mLayout->getBufferSize()];   
      mDirtyFields.setSize(mLayout->getParameterCount());
      mHasData.setSize(mLayout->getParameterCount());
      for (U32 i = 0; i < mHasData.size(); i++)
         mHasData[i] = false;
      setDirty(false);
      // Always set a default value, that way our isEqual checks will work in release as well.
      dMemset(mBuffer, 0xFFFF, mLayout->getBufferSize());
   }
   else
   {
      mBuffer = NULL;
   }
}

GenericConstBuffer::~GenericConstBuffer() 
{
   delete[] mBuffer;
}

void GenericConstBuffer::internalSet(const GenericConstBufferLayout::ParamDesc& pd, const GFXShaderConstType constType, const U32 size, const void* data)
{   
   if (!mBuffer)
      return;
   
   //AssertFatal( mDirtyFields.size() == pd.arraySize, "Buffer size does not equal ParamDesc!" );

   if (mLayout->set(pd, constType, size, data, mBuffer))
   {
      mDirty = true;      
      mDirtyFields[pd.index] = true;
      mHasData[pd.index] = true;
   }
}

void GenericConstBuffer::setDirty(bool dirty)
{ 
   mDirty = dirty; 
   for (Vector<bool>::iterator i = mDirtyFields.begin(); i != mDirtyFields.end(); i++)
   {
      *i = dirty;
   }
}

/// This scans the fields and returns a pointer to the first dirty field
/// and the length of the dirty bytes
const U8* GenericConstBuffer::getDirtyBuffer(U32& start, U32& size)
{
   PROFILE_SCOPE(GenericConstBuffer_getDirtyBuffer);

   U32 dirtyStart = mLayout->getBufferSize();
   U32 dirtyEnd = 0;

   static GenericConstBufferLayout::ParamDesc pd;

   for (U32 i = 0; i < mDirtyFields.size(); i++)
   {
      if ( isFieldDirty(i) && mLayout->getDesc(i, pd) )
      {
         dirtyStart = getMin(pd.offset, dirtyStart);
         dirtyEnd = getMax(pd.offset + pd.size, dirtyEnd);
      }
   }

   if (dirtyEnd > dirtyStart)
   {
      size = dirtyEnd - dirtyStart;
      start = dirtyStart;
      return mBuffer + dirtyStart;
   }
   else
   {
      // Nothing has changed.
      start = 0;
      size = 0;
      return NULL;
   }   
}

/// Returns true if we hold the same data as buffer and have the same layout
bool GenericConstBuffer::isEqual(GenericConstBuffer* buffer) const
{      
   PROFILE_SCOPE(GenericConstBuffer_isEqual);

   U32 bsize = mLayout->getBufferSize();
   if (bsize == buffer->mLayout->getBufferSize())
   {
      return dMemcmp(mBuffer, buffer->getBuffer(), bsize) == 0;
   }

   return false;
}