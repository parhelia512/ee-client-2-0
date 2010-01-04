//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _BUMP_HLSL_H_
#define _BUMP_HLSL_H_

#ifndef _SHADERGEN_HLSL_SHADERFEATUREHLSL_H_
#include "shaderGen/HLSL/shaderFeatureHLSL.h"
#endif

struct RenderPassData;
class MultiLine;


/// The Bumpmap feature will read the normal map and
/// transform it by the inverse of the worldToTanget 
/// matrix.  This normal is then used by subsequent
/// shader features.
class BumpFeatHLSL : public ShaderFeatureHLSL
{
public:

   // ShaderFeatureHLSL
   virtual void processVert( Vector<ShaderComponent*> &componentList,
                             const MaterialFeatureData &fd );
   virtual void processPix( Vector<ShaderComponent*> &componentList, 
                            const MaterialFeatureData &fd );
   virtual Material::BlendOp getBlendOp(){ return Material::LerpAlpha; }  
   virtual Resources getResources( const MaterialFeatureData &fd );
   virtual void setTexData( Material::StageData &stageDat,
                            const MaterialFeatureData &fd,
                            RenderPassData &passData,
                            U32 &texIndex );
   virtual String getName() { return "Bumpmap"; }
};


/// This feature either generates the cheap yet effective offset
/// mapping style parallax or the much more expensive occlusion 
/// mapping technique based on the enabled feature flags.
class ParallaxFeatHLSL : public ShaderFeatureHLSL
{
protected:

   static Var* _getUniformVar( const char *name, const char *type );

   ShaderIncludeDependency mIncludeDep;

public:

   ParallaxFeatHLSL();

   // ShaderFeatureHLSL
   virtual void processVert(  Vector<ShaderComponent*> &componentList,
                              const MaterialFeatureData &fd );
   virtual void processPix(   Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd );
   virtual Resources getResources( const MaterialFeatureData &fd );
   virtual void setTexData(   Material::StageData &stageDat,
                              const MaterialFeatureData &fd,
                              RenderPassData &passData,
                              U32 &texIndex );
   virtual String getName() { return "Parallax"; }
};


#endif // _BUMP_HLSL_H_