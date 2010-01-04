//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/sim/gfxStateBlockData.h"
#include "console/consoleTypes.h"
#include "gfx/gfxStringEnumTranslate.h"

//
// GFXStateBlockData
//

IMPLEMENT_CONOBJECT( GFXStateBlockData );

GFXStateBlockData::GFXStateBlockData()
{
   for (U32 i = 0; i < TEXTURE_STAGE_COUNT; i++)
   {
      mSamplerStates[i] = NULL;
   }  
}

void GFXStateBlockData::initPersistFields()
{
   // Alpha blending
   addField("blendDefined",      TypeBool, Offset(mState.blendDefined, GFXStateBlockData));
   addField("blendEnable",       TypeBool, Offset(mState.blendEnable, GFXStateBlockData));
   addField("blendSrc",          TypeEnum, Offset(mState.blendSrc, GFXStateBlockData), 1, &gBlendEnumTable);
   addField("blendDest",         TypeEnum, Offset(mState.blendDest, GFXStateBlockData), 1, &gBlendEnumTable);
   addField("blendOp",           TypeEnum, Offset(mState.blendOp, GFXStateBlockData), 1, &gBlendOpEnumTable);   

   // Separate alpha blending
   addField("separateAlphaBlendDefined", TypeBool, Offset(mState.separateAlphaBlendDefined, GFXStateBlockData));
   addField("separateAlphaBlendEnable", TypeBool, Offset(mState.separateAlphaBlendEnable, GFXStateBlockData));
   addField("separateAlphaBlendSrc", TypeEnum, Offset(mState.separateAlphaBlendSrc, GFXStateBlockData), 1, &gBlendEnumTable);
   addField("separateAlphaBlendDest", TypeEnum, Offset(mState.separateAlphaBlendDest, GFXStateBlockData), 1, &gBlendEnumTable);
   addField("separateAlphaBlendOp", TypeEnum, Offset(mState.separateAlphaBlendOp, GFXStateBlockData), 1, &gBlendOpEnumTable);   

   // Alpha test
   addField("alphaDefined",      TypeBool, Offset(mState.alphaDefined, GFXStateBlockData));
   addField("alphaTestEnable",   TypeBool, Offset(mState.alphaTestEnable, GFXStateBlockData));   
   addField("alphaTestFunc",     TypeEnum, Offset(mState.alphaTestFunc, GFXStateBlockData), 1, &gCmpFuncEnumTable);      
   addField("alphaTestRef",      TypeS32,  Offset(mState.alphaTestRef, GFXStateBlockData));     

   // Color Writes
   addField("colorWriteDefined", TypeBool, Offset(mState.colorWriteDefined, GFXStateBlockData));
   addField("colorWriteRed",     TypeBool, Offset(mState.colorWriteRed, GFXStateBlockData));
   addField("colorWriteBlue",    TypeBool, Offset(mState.colorWriteBlue, GFXStateBlockData));
   addField("colorWriteGreen",   TypeBool, Offset(mState.colorWriteGreen, GFXStateBlockData));
   addField("colorWriteAlpha",   TypeBool, Offset(mState.colorWriteAlpha, GFXStateBlockData));

   // Rasterizer
   addField("cullDefined",       TypeBool, Offset(mState.cullDefined, GFXStateBlockData));
   addField("cullMode",          TypeEnum, Offset(mState.cullMode, GFXStateBlockData), 1, &gCullModeEnumTable);         

   // Depth
   addField("zDefined",          TypeBool, Offset(mState.zDefined, GFXStateBlockData));
   addField("zEnable",           TypeBool, Offset(mState.zEnable, GFXStateBlockData));
   addField("zWriteEnable",      TypeBool, Offset(mState.zWriteEnable, GFXStateBlockData));
   addField("zFunc",             TypeEnum, Offset(mState.zFunc, GFXStateBlockData), 1, &gCmpFuncEnumTable);
   addField("zBias",             TypeS32,  Offset(mState.zBias, GFXStateBlockData));
   addField("zSlopeBias",        TypeS32,  Offset(mState.zSlopeBias, GFXStateBlockData));

   // Stencil
   addField("stencilDefined",    TypeBool, Offset(mState.stencilDefined, GFXStateBlockData));
   addField("stencilEnable",     TypeBool, Offset(mState.stencilEnable, GFXStateBlockData));
   addField("stencilFailOp",     TypeEnum, Offset(mState.stencilFailOp, GFXStateBlockData), 1, &gStencilModeEnumTable);
   addField("stencilZFailOp",    TypeEnum, Offset(mState.stencilZFailOp, GFXStateBlockData), 1, &gStencilModeEnumTable);
   addField("stencilPassOp",     TypeEnum, Offset(mState.stencilPassOp, GFXStateBlockData), 1, &gStencilModeEnumTable); 
   addField("stencilFunc",       TypeEnum, Offset(mState.stencilFunc, GFXStateBlockData), 1, &gCmpFuncEnumTable);
   addField("stencilRef",        TypeS32,  Offset(mState.stencilRef, GFXStateBlockData));     
   addField("stencilMask",       TypeS32,  Offset(mState.stencilMask, GFXStateBlockData));
   addField("stencilWriteMask",  TypeS32,  Offset(mState.stencilWriteMask, GFXStateBlockData)); 

   // FF lighting
   addField("ffLighting",        TypeBool, Offset(mState.ffLighting, GFXStateBlockData));
   addField("vertexColorEnable", TypeBool, Offset(mState.vertexColorEnable, GFXStateBlockData));

   // Sampler states
   addField("samplersDefined",   TypeBool, Offset(mState.samplersDefined, GFXStateBlockData));
   addField("samplerStates",     TypeSimObjectPtr, Offset(mSamplerStates, GFXStateBlockData), TEXTURE_STAGE_COUNT);
   addField("textureFactor",     TypeColorI, Offset(mState.textureFactor, GFXStateBlockData));

   Parent::initPersistFields();
}

bool GFXStateBlockData::onAdd()
{
   if (!Parent::onAdd())
      return false;

   for (U32 i = 0; i < TEXTURE_STAGE_COUNT; i++)
   {  
      if (mSamplerStates[i])
         mSamplerStates[i]->setSamplerState(mState.samplers[i]);
   }
   return true;
}

//
// GFXSamplerStateData
//
IMPLEMENT_CONOBJECT( GFXSamplerStateData );

void GFXSamplerStateData::initPersistFields()
{
   Parent::initPersistFields();

   addField("textureTransform",  TypeEnum,         Offset(mState.textureTransform, GFXSamplerStateData), 1, &gTextureTransformEnumTable);
   addField("addressModeU",      TypeEnum,         Offset(mState.addressModeU, GFXSamplerStateData), 1, &gSamplerAddressModeEnumTable);
   addField("addressModeV",      TypeEnum,         Offset(mState.addressModeV, GFXSamplerStateData), 1, &gSamplerAddressModeEnumTable);
   addField("addressModeW",      TypeEnum,         Offset(mState.addressModeW, GFXSamplerStateData), 1, &gSamplerAddressModeEnumTable);

   addField("magFilter",         TypeEnum,         Offset(mState.magFilter, GFXSamplerStateData), 1, &gTextureFilterModeEnumTable);
   addField("minFilter",         TypeEnum,         Offset(mState.minFilter, GFXSamplerStateData), 1, &gTextureFilterModeEnumTable);
   addField("mipFilter",         TypeEnum,         Offset(mState.mipFilter, GFXSamplerStateData), 1, &gTextureFilterModeEnumTable);

   addField("maxAnisotropy",     TypeS32,          Offset(mState.maxAnisotropy, GFXSamplerStateData));

   addField("mipLODBias",        TypeF32,          Offset(mState.mipLODBias, GFXSamplerStateData));

   addField("textureColorOp",    TypeEnum,         Offset(mState.textureColorOp, GFXSamplerStateData), 1, &gTextureColorOpEnumTable);
   addField("colorArg1",         TypeModifiedEnum, Offset(mState.colorArg1, GFXSamplerStateData), 1, &gTextureArgumentEnumTable_M);
   addField("colorArg2",         TypeModifiedEnum, Offset(mState.colorArg2, GFXSamplerStateData), 1, &gTextureArgumentEnumTable_M);
   addField("colorArg3",         TypeModifiedEnum, Offset(mState.colorArg3, GFXSamplerStateData), 1, &gTextureArgumentEnumTable_M);

   addField("alphaOp",           TypeEnum,         Offset(mState.alphaOp, GFXSamplerStateData), 1, &gTextureColorOpEnumTable);
   addField("alphaArg1",         TypeModifiedEnum, Offset(mState.alphaArg1, GFXSamplerStateData), 1, &gTextureArgumentEnumTable_M);
   addField("alphaArg2",         TypeModifiedEnum, Offset(mState.alphaArg2, GFXSamplerStateData), 1, &gTextureArgumentEnumTable_M);
   addField("alphaArg3",         TypeModifiedEnum, Offset(mState.alphaArg3, GFXSamplerStateData), 1, &gTextureArgumentEnumTable_M);

   addField("resultArg",         TypeEnum,         Offset(mState.resultArg, GFXSamplerStateData), 1, &gTextureArgumentEnumTable);
}

/// Copies the data of this object into desc
void GFXSamplerStateData::setSamplerState(GFXSamplerStateDesc& desc)
{
   desc = mState;
}
