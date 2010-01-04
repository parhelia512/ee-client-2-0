//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXD3D9SHADER_H_
#define _GFXD3D9SHADER_H_

#include "gfx/D3D9/platformD3D.h"
#ifndef _PATH_H_
#include "core/util/path.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif
#ifndef _GFXSHADER_H_
#include "gfx/gfxShader.h"
#endif
#ifndef _GFXRESOURCE_H_
#include "gfx/gfxResource.h"
#endif
#ifndef _GENERICCONSTBUFFER_H_
#include "gfx/genericConstBuffer.h"
#endif


class GFXD3D9Shader;

class GFXD3D9ShaderBufferLayout : public GenericConstBufferLayout
{
protected:
   /// Set a matrix, given a base pointer
   virtual bool setMatrix(const ParamDesc& pd, const GFXShaderConstType constType, const U32 size, const void* data, U8* basePointer);
};

class GFXD3D9ShaderConstHandle : public GFXShaderConstHandle
{
public:   

   // GFXShaderConstHandle
   const String& getName() const;
   GFXShaderConstType getType() const;
   U32 getArraySize() const;

   WeakRefPtr<GFXD3D9Shader> mShader;

   bool mVertexConstant;
   GenericConstBufferLayout::ParamDesc mVertexHandle;
   bool mPixelConstant;
   GenericConstBufferLayout::ParamDesc mPixelHandle;

   void setValid( bool valid ) { mValid = valid; }
   S32 getSamplerRegister() const;

   // Returns true if this is a handle to a sampler register.
   bool isSampler() const 
   {
      return ( mPixelConstant && mPixelHandle.constType >= GFXSCT_Sampler ) ||
             ( mVertexConstant && mVertexHandle.constType >= GFXSCT_Sampler );
   }

   GFXD3D9ShaderConstHandle();
};

class GFXD3D9ShaderConstBuffer : public GFXShaderConstBuffer
{
   friend class GFXD3D9Shader;

public:

   GFXD3D9ShaderConstBuffer( GFXD3D9Shader* shader,
                             GFXD3D9ShaderBufferLayout* vertexLayoutF, 
                             GFXD3D9ShaderBufferLayout* vertexLayoutI,
                             GFXD3D9ShaderBufferLayout* pixelLayoutF, 
                             GFXD3D9ShaderBufferLayout* pixelLayoutI );
   ~GFXD3D9ShaderConstBuffer();   

   /// Called by GFXD3D9Device to activate this buffer.
   /// @param mPrevShaderBuffer The previously active buffer
   void activate(GFXD3D9ShaderConstBuffer* mPrevShaderBuffer);
   
   /// Used internally by GXD3D9ShaderConstBuffer to determine if it's dirty.
   bool isDirty();

   ///
   /// @name GFXShaderConstBuffer interface
   /// @{

   virtual GFXShader* getShader();

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
   virtual void zombify();

   /// When called the resource should restore all device sensitive information destroyed by zombify()
   virtual void resurrect();
   /// @}

   /// Called when the shader this buffer references is reloaded.
   virtual void onShaderReload( GFXShader *shader );

protected:
   // Empty constructor for sub-classing
   GFXD3D9ShaderConstBuffer() {}

   /// We keep a weak reference to the shader 
   /// because it will often be deleted.
   WeakRefPtr<GFXD3D9Shader> mShader;
   
   GFXD3D9ShaderBufferLayout* mVertexConstBufferLayoutF;
   GenericConstBuffer* mVertexConstBufferF;
   GFXD3D9ShaderBufferLayout* mPixelConstBufferLayoutF;
   GenericConstBuffer* mPixelConstBufferF;   
   GFXD3D9ShaderBufferLayout* mVertexConstBufferLayoutI;
   GenericConstBuffer* mVertexConstBufferI;
   GFXD3D9ShaderBufferLayout* mPixelConstBufferLayoutI;
   GenericConstBuffer* mPixelConstBufferI;   
};

//------------------------------------------------------------------------------

class _gfxD3DXInclude;
typedef StrongRefPtr<_gfxD3DXInclude> _gfxD3DXIncludeRef;

//------------------------------------------------------------------------------

class GFXD3D9Shader : public GFXShader
{
   friend class GFXD3D9Device;
   friend class GFX360Device;
   friend class GFXD3D9ShaderConstBuffer;
   friend class GFX360ShaderConstBuffer;
public:
   typedef Map<String, GFXD3D9ShaderConstHandle*> HandleMap;

   GFXD3D9Shader();
   virtual ~GFXD3D9Shader();   

   // GFXShader
   virtual GFXShaderConstBufferRef allocConstBuffer();
   virtual const Vector<GFXShaderConstDesc>& getShaderConstDesc() const;
   virtual GFXShaderConstHandle* getShaderConstHandle(const String& name); 
   virtual U32 getAlignmentValue(const GFXShaderConstType constType) const;
   virtual bool getDisassembly( String &outStr ) const;

   // GFXResource
   virtual void zombify();
   virtual void resurrect();

protected:

   virtual bool _init();   

   static const U32 smCompiledShaderTag;

   LPDIRECT3DDEVICE9 mD3D9Device;

   IDirect3DVertexShader9 *mVertShader;
   IDirect3DPixelShader9 *mPixShader;

   GFXD3D9ShaderBufferLayout* mVertexConstBufferLayoutF;   
   GFXD3D9ShaderBufferLayout* mPixelConstBufferLayoutF;
   GFXD3D9ShaderBufferLayout* mVertexConstBufferLayoutI;   
   GFXD3D9ShaderBufferLayout* mPixelConstBufferLayoutI;

   static _gfxD3DXIncludeRef smD3DXInclude;

   HandleMap mHandles;

   /// The shader disassembly from DX when this shader is compiled.
   /// We only store this data in non-release builds.
   String mDissasembly;

   /// Vector of sampler type descriptions consolidated from _compileShader.
   Vector<GFXShaderConstDesc> mSamplerDescriptions;

   /// Vector of descriptions (consolidated for the getShaderConstDesc call)
   Vector<GFXShaderConstDesc> mShaderConsts;
   
   // These two functions are used when compiling shaders from hlsl
   virtual bool _compileShader( const Torque::Path &filePath, 
                                const String &target, 
                                const D3DXMACRO *defines, 
                                GenericConstBufferLayout *bufferLayoutF, 
                                GenericConstBufferLayout *bufferLayoutI,
                                Vector<GFXShaderConstDesc> &samplerDescriptions );

   void _getShaderConstants( ID3DXConstantTable* table, 
                             GenericConstBufferLayout *bufferLayoutF, 
                             GenericConstBufferLayout *bufferLayoutI,
                             Vector<GFXShaderConstDesc> &samplerDescriptions );

   bool _saveCompiledOutput( const Torque::Path &filePath, 
                             LPD3DXBUFFER buffer, 
                             GenericConstBufferLayout *bufferLayoutF, 
                             GenericConstBufferLayout *bufferLayoutI,
                             Vector<GFXShaderConstDesc> &samplerDescriptions );

   // Loads precompiled shaders
   bool _loadCompiledOutput( const Torque::Path &filePath, 
                             const String &target, 
                             GenericConstBufferLayout *bufferLayoutF, 
                             GenericConstBufferLayout *bufferLayoutI,
                             Vector<GFXShaderConstDesc> &samplerDescriptions );

   // This is used in both cases
   virtual void _buildShaderConstantHandles( GenericConstBufferLayout *layout, bool vertexConst );
   
   virtual void _buildSamplerShaderConstantHandles( Vector<GFXShaderConstDesc> &samplerDescriptions );
};

inline bool GFXD3D9Shader::getDisassembly( String &outStr ) const
{
   outStr = mDissasembly;
   return ( outStr.isNotEmpty() );
}

#endif // _GFXD3D9SHADER_H_