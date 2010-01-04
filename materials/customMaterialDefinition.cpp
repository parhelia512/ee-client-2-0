//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "materials/customMaterialDefinition.h"
#include "materials/materialManager.h"
#include "console/consoleTypes.h"
#include "materials/shaderData.h"
#include "gfx/sim/cubemapData.h"
#include "gfx/gfxCubemap.h"
#include "gfx/sim/gfxStateBlockData.h"


//****************************************************************************
// Custom Material
//****************************************************************************
IMPLEMENT_CONOBJECT(CustomMaterial);

//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------
CustomMaterial::CustomMaterial()
{  
   mFallback = NULL;
   mMaxTex = 0;
   mVersion = 1.1f;
   mTranslucent = false;
   dMemset( mFlags, 0, sizeof( mFlags ) );   
   mShaderData = NULL;
   mRefract = false;
   mStateBlockData = NULL;   
}

//--------------------------------------------------------------------------
// Init fields
//--------------------------------------------------------------------------
void CustomMaterial::initPersistFields()
{
   addField("texture",     TypeStringFilename,  Offset(mTexFilename, CustomMaterial), MAX_TEX_PER_PASS);
   addField("version",     TypeF32,             Offset(mVersion, CustomMaterial));
   addField("fallback",    TypeSimObjectPtr,    Offset(mFallback,  CustomMaterial));
   addField("shader",      TypeRealString,      Offset(mShaderDataName, CustomMaterial));
   addField("stateBlock",  TypeSimObjectPtr,    Offset(mStateBlockData,  CustomMaterial));
   addField("target",      TypeRealString,      Offset(mOutputTarget, CustomMaterial));

   Parent::initPersistFields();
}

//--------------------------------------------------------------------------
// On add - verify data settings
//--------------------------------------------------------------------------
bool CustomMaterial::onAdd()
{
   if (Parent::onAdd() == false)
      return false;

   mShaderData = dynamic_cast<ShaderData*>(Sim::findObject( mShaderDataName ) );
   if(mShaderDataName.isNotEmpty() && mShaderData == NULL)
   {
      logError("Failed to find ShaderData %s", mShaderDataName.c_str());
      return false;
   }

   return true;
}

//--------------------------------------------------------------------------
// On remove
//--------------------------------------------------------------------------
void CustomMaterial::onRemove()
{
   Parent::onRemove();
}

//--------------------------------------------------------------------------
// Map this material to the texture specified in the "mapTo" data variable
//--------------------------------------------------------------------------
void CustomMaterial::_mapMaterial()
{
   if( String(getName()).isEmpty() )
   {
      Con::warnf( "Unnamed Material!  Could not map to: %s", mMapTo.c_str() );
      return;
   }

   if( mMapTo.isEmpty() )
      return;

   MATMGR->mapMaterial(mMapTo, getName());
}

const GFXStateBlockData* CustomMaterial::getStateBlockData() const
{
   return mStateBlockData;
}
