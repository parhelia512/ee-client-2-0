//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXSHADER_H_
#define _GFXSHADER_H_

#ifndef _GFXRESOURCE_H_
#include "gfx/gfxResource.h"
#endif
#ifndef _TORQUE_STRING_H_
#include "core/util/str.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _ALIGNEDARRAY_H_
#include "core/util/tAlignedArray.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif
#ifndef _GFXENUMS_H_
#include "gfx/gfxEnums.h"
#endif
#ifndef _GFXSTRUCTS_H_
#include "gfx/gfxStructs.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _REFBASE_H_
#include "core/util/refBase.h"
#endif
#ifndef _PATH_H_
#include "core/util/path.h"
#endif
#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif

class Point2I;
class Point2F;
class ColorF;
class MatrixF;
class GFXShader;
class GFXVertexFormat;


/// Instances of this struct are returned GFXShaderConstBuffer
struct GFXShaderConstDesc 
{
public:
   String name;
   GFXShaderConstType constType;   
   U32 arraySize; // > 1 means it is an array!
};

/// This is an opaque handle used by GFXShaderConstBuffer clients to set individual shader constants.
/// Derived classes can put whatever info they need into here, these handles are owned by the shader constant buffer
/// (or shader).  Client code should not free these.
class GFXShaderConstHandle 
{
public:

   GFXShaderConstHandle() { mValid = false; }
   virtual ~GFXShaderConstHandle() {}

   bool isValid() { return mValid; }   
   
   /// Returns the name of the constant handle.
   virtual const String& getName() const = 0;

   /// Returns the type of the constant handle.
   virtual GFXShaderConstType getType() const = 0;

   virtual U32 getArraySize() const = 0;
   
   /// Returns -1 if this handle does not point to a Sampler.
   virtual S32 getSamplerRegister() const = 0;

protected:

   bool mValid;
};

/// GFXShaderConstBuffer is a collection of string/value pairs that are sent to a shader.
/// Under the hood, the string value pair is mapped to a block of memory that can
/// be blasted to a shader with one call (ideally)
class GFXShaderConstBuffer : public GFXResource, public StrongRefBase
{
public:
   /// Return the shader that created this buffer
   virtual GFXShader* getShader() = 0;

   /// @name Set shader constant values
   /// @{
   /// Actually set shader constant values
   /// @param name Name of the constant, this should be a name contained in the array returned in getShaderConstDesc,
   /// if an invalid name is used, its ignored.
   virtual void set(GFXShaderConstHandle* handle, const F32 f) = 0;
   virtual void set(GFXShaderConstHandle* handle, const Point2F& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const Point3F& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const Point4F& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const PlaneF& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const ColorF& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const S32 f) = 0;
   virtual void set(GFXShaderConstHandle* handle, const Point2I& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const Point3I& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const Point4I& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<F32>& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point2F>& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point3F>& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point4F>& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<S32>& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point2I>& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point3I>& fv) = 0;
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point4I>& fv) = 0;
   /// Specify the type of the matrix, only the GFXSCT types ending in NxN are valid 
   virtual void set(GFXShaderConstHandle* handle, const MatrixF& mat, const GFXShaderConstType matrixType = GFXSCT_Float4x4) = 0;
   /// Same as above, but in array form.  We don't use an AlignedArray here because the packing of non 4x4 arrays will differ more
   /// than we can express with an AlignedArray.  So the API is responsible for marshaling the data into the format it needs.  In practice,
   /// that means that 4x4 matrices are going to be quickest (straight memory copy on D3D and GL).  Other dimensions will require "interesting"
   /// code to handle marshaling.
   virtual void set(GFXShaderConstHandle* handle, const MatrixF* mat, const U32 arraySize, const GFXShaderConstType matrixType = GFXSCT_Float4x4) = 0;

   ///
   /// @name GFXResource interface
   /// @{

   /// The resource should put a description of itself (number of vertices, size/width of texture, etc.) in buffer
   virtual const String describeSelf() const = 0;

   /// @}

   /// Called when the shader this buffer references is reloaded.
   virtual void onShaderReload( GFXShader *shader ) = 0;
};

typedef StrongRefPtr<GFXShaderConstBuffer> GFXShaderConstBufferRef;

//**************************************************************************
// Shader
//**************************************************************************
class GFXShader : public StrongRefBase, public GFXResource
{
   friend class GFXShaderConstBuffer;

protected:
  
   /// These are system wide shader macros which are 
   /// merged with shader specific macros at creation.
   static Vector<GFXShaderMacro> smGlobalMacros;

   /// If true the shader errors are spewed to the console.
   static bool smLogErrors;

   /// If true the shader warnings are spewed to the console.
   static bool smLogWarnings;

   /// The vertex shader file.
   Torque::Path mVertexFile;  

   /// The pixel shader file.
   Torque::Path mPixelFile;  

   /// The macros to be passed to the shader.      
   Vector<GFXShaderMacro> mMacros;

   /// The pixel version this is compiled for.
   F32 mPixVersion;

   ///
   String mDescription;

   /// Counter that is incremented each time this shader is reloaded.
   U32 mReloadKey;

   Signal<void()> mReloadSignal;

   /// Vector of buffers that reference this shader.
   /// It is the responsibility of the derived shader class to populate this 
   /// vector and to notify them when this shader is reloaded.  Classes
   /// derived from GFXShaderConstBuffer should call _unlinkBuffer from
   /// their destructor.
   Vector<GFXShaderConstBuffer*> mActiveBuffers;

   /// A protected constructor so it cannot be instantiated.
   GFXShader();

public:

   // TODO: Make these protected!
   const GFXVertexFormat *mVertexFormat;

   /// Adds a global shader macro which will be merged with
   /// the script defined macros on every shader reload.
   ///
   /// The macro will replace the value of an existing macro
   /// of the same name.
   ///
   /// For the new macro to take effect all the shaders/materials
   /// in the system need to be reloaded.
   ///
   /// @see MaterialManager::flushAndReInitInstances
   /// @see ShaderData::reloadAll
   static void addGlobalMacro( const String &name, const String &value = String::EmptyString );

   /// Removes an existing global macro by name.
   /// @see addGlobalMacro
   static bool removeGlobalMacro( const String &name );

   /// Toggle logging for shader errors.
   static void setLogging( bool logErrors,
                           bool logWarning ) 
   {
      smLogErrors = logErrors; 
      smLogWarnings = logWarning;
   }

   /// The destructor.
   virtual ~GFXShader();

   ///
   bool init(  const Torque::Path &vertFile, 
               const Torque::Path &pixFile, 
               F32 pixVersion, 
               const Vector<GFXShaderMacro> &macros );

   /// Reloads the shader from disk.
   bool reload();

   Signal<void()> getReloadSignal() { return mReloadSignal; }

   /// Allocate a constant buffer
   virtual GFXShaderConstBufferRef allocConstBuffer() = 0;  

   /// Returns our list of shader constants, the material can get this and just set the constants it knows about
   virtual const Vector<GFXShaderConstDesc>& getShaderConstDesc() const = 0;

   /// Returns a shader constant handle for the name constant.
   ///
   /// Since shaders can reload and later have handles that didn't 
   /// exist originally this will return a handle in an invalid state
   /// if the constant doesn't exist at this time.
   virtual GFXShaderConstHandle* getShaderConstHandle( const String& name ) = 0; 

   /// Returns the alignment value for constType
   virtual U32 getAlignmentValue(const GFXShaderConstType constType) const = 0;   

   /// Used to store filename and other info used for info dumps
   void setDescription( const String &desc ) { mDescription = desc; }

   const GFXVertexFormat* getVertexFormat() const { return mVertexFormat; }

   F32 getPixVersion() const { return mPixVersion; }

   /// Returns a counter which is incremented each time this shader is reloaded.
   U32 getReloadKey() const { return mReloadKey; }

   /// Device specific shaders can override this method to return
   /// the shader disassembly.
   virtual bool getDisassembly( String &outStr ) const { return false; }

   // GFXResource
   const String describeSelf() const { return mDescription; }

protected:

   /// Called when the shader files change on disk.
   void _onFileChanged( const Torque::Path &path ) { reload(); }

   /// Internal initialization function overloaded for
   /// each GFX device type.
   virtual bool _init() = 0;

   /// Buffers call this from their destructor (so we don't have to ref count them).
   void _unlinkBuffer( GFXShaderConstBuffer *buf );
};

/// A strong pointer to a reference counted GFXShader.
typedef StrongRefPtr<GFXShader> GFXShaderRef;


/// A weak pointer to a reference counted GFXShader.
typedef WeakRefPtr<GFXShader> GFXShaderWeakRef;


#endif // GFXSHADER
