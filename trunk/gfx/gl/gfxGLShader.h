//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXGLSHADER_H_
#define _GFXGLSHADER_H_

#include "core/util/refBase.h"
#include "gfx/gfxShader.h"
#include "gfx/gl/ggl/ggl.h"
#include "core/util/tSignal.h"
#include "core/util/tDictionary.h"

class GFXGLShaderConstHandle;
class FileStream;
class GFXGLShaderConstBuffer;

class GFXGLShader : public GFXShader
{
   typedef Map<String, GFXGLShaderConstHandle*> HandleMap;
public:
   GFXGLShader();
   virtual ~GFXGLShader();
   
   /// @name GFXShader interface
   /// @{
   virtual GFXShaderConstHandle* getShaderConstHandle(const String& name);

   /// Returns our list of shader constants, the material can get this and just set the constants it knows about
   virtual const Vector<GFXShaderConstDesc>& getShaderConstDesc() const;

   /// Returns the alignment value for constType
   virtual U32 getAlignmentValue(const GFXShaderConstType constType) const; 

   virtual GFXShaderConstBufferRef allocConstBuffer();

   /// @}
   
   /// @name GFXResource interface
   /// @{
   virtual void zombify();
   virtual void resurrect() { reload(); }
   virtual const String describeSelf() const;
   /// @}      

   /// Activates this shader in the GL context.
   void useProgram();
   
protected:

   friend class GFXGLShaderConstBuffer;
   friend class GFXGLShaderConstHandle;
   
   virtual bool _init();   

   bool initShader(  const Torque::Path &file, 
                     bool isVertex, 
                     const Vector<GFXShaderMacro> &macros );

   void clearShaders();
   void initConstantDescs();
   void initHandles();
   void setConstantsFromBuffer(GFXGLShaderConstBuffer* buffer);
   
   static char* _handleIncludes( const Torque::Path &path, FileStream *s );

   static bool _loadShaderFromStream(  GLuint shader, 
                                       const Torque::Path& path, 
                                       FileStream* s, 
                                       const Vector<GFXShaderMacro>& macros );

   /// @name Internal GL handles
   /// @{
   GLuint mVertexShader;
   GLuint mPixelShader;
   GLuint mProgram;
   /// @}
    
   Vector<GFXShaderConstDesc> mConstants;
   U32 mConstBufferSize;
   U8* mConstBuffer;
   HandleMap mHandles;
};

class GFXGLShaderConstBuffer : public GFXShaderConstBuffer
{
public:
   GFXGLShaderConstBuffer(GFXGLShader* shader, U32 bufSize, U8* existingConstants);
   ~GFXGLShaderConstBuffer();
   
   /// Called by GFXGLDevice to activate this buffer.
   void activate();

   ///
   /// @name GFXShaderConstBuffer interface
   /// @{

   /// Return the shader that created this buffer
   virtual GFXShader* getShader() { return mShader; }

   /// @name Set shader constant values
   /// @{
   /// Actually set shader constant values
   /// @param name Name of the constant, this should be a name contained in the array returned in getShaderConstDesc,
   /// if an invalid name is used, its ignored.
   virtual void set(GFXShaderConstHandle* handle, const F32 fv);
   virtual void set(GFXShaderConstHandle* handle, const Point2F& fv);
   virtual void set(GFXShaderConstHandle* handle, const Point3F& fv);
   virtual void set(GFXShaderConstHandle* handle, const Point4F& fv);
   virtual void set(GFXShaderConstHandle* handle, const PlaneF& fv);
   virtual void set(GFXShaderConstHandle* handle, const ColorF& fv);   
   virtual void set(GFXShaderConstHandle* handle, const S32 f);
   virtual void set(GFXShaderConstHandle* handle, const Point2I& fv);
   virtual void set(GFXShaderConstHandle* handle, const Point3I& fv);
   virtual void set(GFXShaderConstHandle* handle, const Point4I& fv);
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<F32>& fv);
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point2F>& fv);
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point3F>& fv);
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point4F>& fv);   
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<S32>& fv);
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point2I>& fv);
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point3I>& fv);
   virtual void set(GFXShaderConstHandle* handle, const AlignedArray<Point4I>& fv);
   virtual void set(GFXShaderConstHandle* handle, const MatrixF& mat, const GFXShaderConstType matType = GFXSCT_Float4x4);
   virtual void set(GFXShaderConstHandle* handle, const MatrixF* mat, const U32 arraySize, const GFXShaderConstType matrixType = GFXSCT_Float4x4);   
   /// @}

   /// @}

   ///
   /// @name GFXResource interface
   /// @{

   /// The resource should put a description of itself (number of vertices, size/width of texture, etc.) in buffer
   virtual const String describeSelf() const;

   /// When called the resource should destroy all device sensitive information (e.g. D3D resources in D3DPOOL_DEFAULT
   virtual void zombify() {}

   /// When called the resource should restore all device sensitive information destroyed by zombify()
   virtual void resurrect() {}
   
   /// @}

   /// Called when the shader this buffer references is reloaded.
   virtual void onShaderReload( GFXShader *shader );

private:
   friend class GFXGLShader;
   U8* mBuffer;
   WeakRefPtr<GFXGLShader> mShader;
   
   template<typename ConstType>
   void internalSet(GFXShaderConstHandle* handle, const ConstType& param);
   
   template<typename ConstType>
   void internalSet(GFXShaderConstHandle* handle, const AlignedArray<ConstType>& fv);
};

#endif