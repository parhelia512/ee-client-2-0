//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/shaderData.h"

#include "console/consoleTypes.h"
#include "gfx/gfxDevice.h"
#include "core/strings/stringUnit.h"
#include "lighting/lightManager.h"

using namespace Torque;


Vector<ShaderData*> ShaderData::smAllShaderData;


IMPLEMENT_CONOBJECT( ShaderData );

ShaderData::ShaderData()
{
   VECTOR_SET_ASSOCIATION( mShaderMacros );

   mUseDevicePixVersion = false;
   mPixVersion = 1.0;
}

void ShaderData::initPersistFields()
{
   addField("DXVertexShaderFile",   TypeStringFilename,  Offset(mDXVertexShaderName,   ShaderData));
   addField("DXPixelShaderFile",    TypeStringFilename,  Offset(mDXPixelShaderName,  ShaderData));

   addField("OGLVertexShaderFile",  TypeStringFilename,  Offset(mOGLVertexShaderName,   ShaderData));
   addField("OGLPixelShaderFile",   TypeStringFilename,  Offset(mOGLPixelShaderName,  ShaderData));

   addField("samplerNames",         TypeRealString,      Offset(mSamplerNames, ShaderData), TEXTURE_STAGE_COUNT);

   addField("useDevicePixVersion",  TypeBool,            Offset(mUseDevicePixVersion,   ShaderData));
   addField("pixVersion",           TypeF32,             Offset(mPixVersion,   ShaderData));
   addField("defines",              TypeRealString,      Offset(mDefines,   ShaderData));

   Parent::initPersistFields();

   // Make sure we get activation signals.
   LightManager::smActivateSignal.notify( &ShaderData::_onLMActivate );
}

bool ShaderData::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   mShaderMacros.clear();

   // Keep track of it.
   smAllShaderData.push_back( this );

   // NOTE: We initialize the shader on request.

   return true;
}

void ShaderData::onRemove()
{
   // Remove it from the all shaders list.
   smAllShaderData.remove( this );

   Parent::onRemove();
}

const Vector<GFXShaderMacro>& ShaderData::_getMacros()
{
   // If they have already been processed then 
   // return the cached result.
   if ( mShaderMacros.size() != 0 || mDefines.isEmpty() )
      return mShaderMacros;

   mShaderMacros.clear();  
   GFXShaderMacro macro;
   const U32 defineCount = StringUnit::getUnitCount( mDefines, ";\n\t" );
   for ( U32 i=0; i < defineCount; i++ )
   {
      String define = StringUnit::getUnit( mDefines, i, ";\n\t" );

      macro.name   = StringUnit::getUnit( define, 0, "=" );
      macro.value  = StringUnit::getUnit( define, 1, "=" );
      mShaderMacros.push_back( macro );
   }

   return mShaderMacros;
}

GFXShader* ShaderData::getShader( const Vector<GFXShaderMacro> &macros )
{
   PROFILE_SCOPE( ShaderData_GetShader );

   // Combine the dynamic macros with our script defined macros.
   Vector<GFXShaderMacro> finalMacros;
   finalMacros.merge( _getMacros() );
   finalMacros.merge( macros );

   // Convert the final macro list to a string.
   String cacheKey;
   GFXShaderMacro::stringize( macros, &cacheKey );   

   // Lookup the shader for this instance.
   ShaderCache::Iterator iter = mShaders.find( cacheKey );
   if ( iter != mShaders.end() )
      return iter->value;

   // Create the shader instance... if it fails then
   // bail out and return nothing to the caller.
   GFXShader *shader = _createShader( finalMacros );
   if ( !shader )
      return NULL;

   // Store the shader in the cache and return it.
   mShaders.insertUnique( cacheKey, shader );
   return shader;
}

void ShaderData::mapSamplerNames( GFXShaderConstBufferRef constBuffer )
{
   if ( constBuffer.isNull() )
      return;

   GFXShader *shader = constBuffer->getShader();

   for ( U32 i = 0; i < TEXTURE_STAGE_COUNT; i++ )
   {
      GFXShaderConstHandle *handle = shader->getShaderConstHandle( mSamplerNames[i] );
      if ( handle->isValid() )
         constBuffer->set( handle, (S32)i );
   }
}

GFXShader* ShaderData::_createShader( const Vector<GFXShaderMacro> &macros )
{
   F32 pixver = mPixVersion;
   if ( mUseDevicePixVersion )
      pixver = getMax( pixver, GFX->getPixelShaderVersion() );

   // Enable shader error logging.
   GFXShader::setLogging( true, true );

   GFXShader *shader = GFX->createShader();
   bool success = false;

   // Initialize the right shader type.
   switch( GFX->getAdapterType() )
   {
      case Direct3D9_360:
      case Direct3D9:
      {
         success = shader->init( mDXVertexShaderName, 
                                 mDXPixelShaderName, 
                                 pixver,
                                 macros );
         break;
      }

      case OpenGL:
      {
         success = shader->init( mOGLVertexShaderName,
                                 mOGLPixelShaderName,
                                 pixver,
                                 macros );
         break;
      }
         
      default:
         // Other device types are assumed to not support shaders.
         success = false;
         break;
   }

   // If we failed to load the shader then
   // cleanup and return NULL.
   if ( !success )
      SAFE_DELETE( shader );

   return shader;
}

void ShaderData::reloadShaders()
{
   ShaderCache::Iterator iter = mShaders.begin();
   for ( ; iter != mShaders.end(); iter++ )
      iter->value->reload();
}

void ShaderData::reloadAllShaders()
{
   Vector<ShaderData*>::iterator iter = smAllShaderData.begin();
   for ( ; iter != smAllShaderData.end(); iter++ )
      (*iter)->reloadShaders();
}

void ShaderData::_onLMActivate( const char *lm, bool activate )
{
   // Only on activations do we do anything.
   if ( !activate )
      return;

   // Since the light manager usually swaps shadergen features
   // and changes system wide shader defines we need to completely
   // flush and rebuild all shaders.

   reloadAllShaders();
}

const String& ShaderData::getSamplerName(U32 idx)
{
   AssertFatal(idx < TEXTURE_STAGE_COUNT, "ShaderData::getSamplerName - idx out of range");
   return mSamplerNames[idx];
}

ConsoleMethod(ShaderData, reload, void, 2, 2, 
   "Rebuilds all the vertex and pixel shaders instances created from this ShaderData.")
{
   object->reloadShaders();
}
