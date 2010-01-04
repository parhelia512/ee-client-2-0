//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/matTextureTarget.h"

#include "console/console.h"
#include "platform/profiler.h"
#include "shaderGen/conditionerFeature.h"
#include "gfx/gfxTextureObject.h"
#include "gfx/gfxStructs.h"

MatTextureTarget::TexTargetMap MatTextureTarget::smRegisteredTargets;


bool MatTextureTarget::registerTarget( const String &name, MatTextureTarget *target )
{
   // Don't register targets to names already taken.
   if ( smRegisteredTargets.contains( name ) )
      return false;

   target->mRegTargetName = name;
   smRegisteredTargets.insert( name, target );
   return true;
}

void MatTextureTarget::unregisterTarget( const String &name, MatTextureTarget *target )
{
   TexTargetMap::Iterator iter = smRegisteredTargets.find( name );
   if ( iter == smRegisteredTargets.end() || ( target != NULL && iter->value != target ) )
      return;

   smRegisteredTargets.erase( iter );
}

MatTextureTarget* MatTextureTarget::findTargetByName( const String &name )
{
   PROFILE_SCOPE( MatTextureTarget_FindTargetByName );

   TexTargetMap::Iterator iter = smRegisteredTargets.find( name );
   if ( iter != smRegisteredTargets.end() )
      return iter->value;
   else
      return NULL;
}

MatTextureTarget::~MatTextureTarget()
{
   // Remove ourselves from the registered targets.
   TexTargetMap::Iterator iter = smRegisteredTargets.begin();
   for ( ; iter != smRegisteredTargets.end(); iter++ )
   {
      if ( iter->value == this )
      {
         TexTargetMap::Iterator prev = iter++;
         smRegisteredTargets.erase( prev );
      }
   }
}

void MatTextureTarget::getTargetShaderMacros( Vector<GFXShaderMacro> *outMacros )
{
   ConditionerFeature *cond = getTargetConditioner();
   if ( !cond )
      return;

   // TODO: No check for duplicates is 
   // going on here which might be a problem?

   String targetName = String::ToLower( mRegTargetName );

   // Add both the condition and uncondition macros.
   const String &condMethod = cond->getShaderMethodName( ConditionerFeature::ConditionMethod );
   if ( condMethod.isNotEmpty() )
   {
      GFXShaderMacro macro;
      macro.name = targetName + "Condition";
      macro.value = condMethod;
      outMacros->push_back( macro );
   }

   const String &uncondMethod = cond->getShaderMethodName( ConditionerFeature::UnconditionMethod );
   if ( uncondMethod.isNotEmpty() )
   {   
      GFXShaderMacro macro;
      macro.name = targetName + "Uncondition";
      macro.value = uncondMethod;
      outMacros->push_back( macro );
   }
}