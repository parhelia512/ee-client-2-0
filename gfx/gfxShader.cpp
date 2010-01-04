//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gfx/gfxShader.h"

#include "shaderGen/conditionerFeature.h"
#include "core/volume.h"


Vector<GFXShaderMacro> GFXShader::smGlobalMacros;
bool GFXShader::smLogErrors = true;
bool GFXShader::smLogWarnings = true;


GFXShader::GFXShader()
   :  mPixVersion( 0.0f ),
      mVertexFormat( NULL ),
      mReloadKey( 0 )
{
}

GFXShader::~GFXShader()
{
   Torque::FS::RemoveChangeNotification( mVertexFile, this, &GFXShader::_onFileChanged );
   Torque::FS::RemoveChangeNotification( mPixelFile, this, &GFXShader::_onFileChanged );
}

bool GFXShader::init(   const Torque::Path &vertFile, 
                        const Torque::Path &pixFile, 
                        F32 pixVersion, 
                        const Vector<GFXShaderMacro> &macros )
{
   // Store the inputs for use in reloading.
   mVertexFile = vertFile;
   mPixelFile = pixFile;
   mPixVersion = pixVersion;
   mMacros = macros;

   // Before we compile the shader make sure the
   // conditioner features have been updated.
   ConditionerFeature::updateConditioners();

   // Now do the real initialization.
   if ( !_init() )
      return false;

   // Add file change notifications for reloads.
   Torque::FS::AddChangeNotification( mVertexFile, this, &GFXShader::_onFileChanged );
   Torque::FS::AddChangeNotification( mPixelFile, this, &GFXShader::_onFileChanged );

   return true;
}

bool GFXShader::reload()
{
   // Before we compile the shader make sure the
   // conditioner features have been updated.
   ConditionerFeature::updateConditioners();

   mReloadKey++;

   // Init does the work.
   bool success = _init();

   // Let anything that cares know that
   // this shader has reloaded
   mReloadSignal.trigger();

   return success;
}

void GFXShader::addGlobalMacro( const String &name, const String &value )
{
   // Check to see if we already have this macro.
   Vector<GFXShaderMacro>::iterator iter = smGlobalMacros.begin();
   for ( ; iter != smGlobalMacros.end(); iter++ )
   {
      if ( iter->name == name )
      {
         if ( iter->value != value )
            iter->value = value;
         return;
      }
   }

   // Add a new macro.
   smGlobalMacros.increment();
   smGlobalMacros.last().name = name;
   smGlobalMacros.last().value = value;
}

bool GFXShader::removeGlobalMacro( const String &name )
{
   Vector<GFXShaderMacro>::iterator iter = smGlobalMacros.begin();
   for ( ; iter != smGlobalMacros.end(); iter++ )
   {
      if ( iter->name == name )
      {
         smGlobalMacros.erase( iter );
         return true;
      }
   }

   return false;
}

void GFXShader::_unlinkBuffer( GFXShaderConstBuffer *buf )
{   
   Vector<GFXShaderConstBuffer*>::iterator iter = mActiveBuffers.begin();
   for ( ; iter != mActiveBuffers.end(); iter++ )
   {
      if ( *iter == buf )
      {
         mActiveBuffers.erase_fast( iter );
         return;
      }
   }

   AssertFatal( false, "GFXShader::_unlinkBuffer - buffer was not found?" );
}

ConsoleFunction( addGlobalShaderMacro, void, 2, 3,
   "Adds a global shader macro which will be merged with the script defined "
   "macros on every shader reload.  The macro will replace the value of an "
   "existing macro of the same name.  For the new macro to take effect all "
   "the shaders/materials in the system need to be reloaded." )   
{
   String value = String::EmptyString;
   if ( argc > 2 )
      value = argv[2];

   GFXShader::addGlobalMacro( argv[1], value );
}

ConsoleFunction( removeGlobalShaderMacro, void, 2, 2, 
   "Removes an existing global macro by name." )
{
   GFXShader::removeGlobalMacro( argv[1] );
}
