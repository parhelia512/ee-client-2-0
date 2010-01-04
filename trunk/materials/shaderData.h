//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _SHADERTDATA_H_
#define _SHADERTDATA_H_

#ifndef _SIMOBJECT_H_
#include "console/simObject.h"
#endif
#ifndef _GFXSHADER_H_
#include "gfx/gfxShader.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif

class GFXShader;
class ShaderData;
struct GFXShaderMacro;


///
class ShaderData : public SimObject
{
   typedef SimObject Parent;

protected:

   ///
   static Vector<ShaderData*> smAllShaderData;

   typedef HashTable<String,GFXShaderRef> ShaderCache;

   ShaderCache mShaders;

   bool mUseDevicePixVersion;

   F32 mPixVersion;

   FileName mDXVertexShaderName;

   FileName mDXPixelShaderName;

   FileName mOGLVertexShaderName;

   FileName mOGLPixelShaderName;

   String mSamplerNames[TEXTURE_STAGE_COUNT];

   /// A semicolon, tab, or newline delimited string of case
   /// sensitive defines that are passed to the shader compiler.
   ///
   /// For example:
   ///
   /// SAMPLE_TAPS=10;USE_TEXKILL;USE_TORQUE_FOG=1
   ///
   String mDefines;

   /// The shader macros built from mDefines.
   /// @see _getMacros()
   Vector<GFXShaderMacro> mShaderMacros;

   /// Returns the shader macros taking care to rebuild
   /// them if the content has changed.
   const Vector<GFXShaderMacro>& _getMacros();

   /// Helper for converting an array of macros 
   /// into a formatted string.
   void _stringizeMacros(  const Vector<GFXShaderMacro> &macros, 
                           String *outString );

   /// Creates a new shader returning NULL on error.
   GFXShader* _createShader( const Vector<GFXShaderMacro> &macros );

   /// @see LightManager::smActivateSignal
   static void _onLMActivate( const char *lm, bool activate );

public:


   ShaderData();

   /// Returns an initialized shader instance or NULL 
   /// if the shader failed to be created.
   GFXShader* getShader( const Vector<GFXShaderMacro> &macros = Vector<GFXShaderMacro>() );

   /// Forces a reinitialization of all the instanced shaders.
   void reloadShaders();

   /// Forces a reinitialization of the instanced shaders for
   /// all loaded ShaderData objects in the system.
   static void reloadAllShaders();

   void mapSamplerNames( GFXShaderConstBufferRef constBuffer );
   
   const String& getSamplerName(U32 idx);
   
   // SimObject
   virtual bool onAdd();
   virtual void onRemove();

   // ConsoleObject
   static void initPersistFields();
   DECLARE_CONOBJECT(ShaderData);
};

#endif // _SHADERTDATA_H_