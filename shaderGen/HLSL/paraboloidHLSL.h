//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _PARABOLOID_HLSL_H_
#define _PARABOLOID_HLSL_H_

#ifndef _SHADERGEN_HLSL_SHADERFEATUREHLSL_H_
#include "shaderGen/HLSL/shaderFeatureHLSL.h"
#endif
#ifndef _SHADEROP_H_
#include "shaderGen/shaderOp.h"
#endif

class GFXShaderConstHandle;


class ParaboloidVertTransformHLSL : public ShaderFeatureHLSL
{
public:

   // ShaderFeature
   virtual void processVert( Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd );
   virtual void processPix( Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd );
   virtual Resources getResources( const MaterialFeatureData &fd );
   virtual String getName() { return "Paraboloid Vert Transform"; }
   virtual Material::BlendOp getBlendOp() { return Material::None; }

};

#endif // _PARABOLOID_HLSL_H_