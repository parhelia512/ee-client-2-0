//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "shaderGen/GLSL/bumpGLSL.h"

#include "shaderGen/shaderOp.h"
#include "gfx/gfxDevice.h"
#include "materials/matInstance.h"
#include "materials/processedMaterial.h"
#include "materials/materialFeatureTypes.h"
#include "shaderGen/shaderGenVars.h"


void BumpFeatGLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   // Output the texture coord.
   getOutTexCoord(   "texCoord", 
                     "vec2", 
                     true, 
                     fd.features[MFT_TexAnim], 
                     meta, 
                     componentList );

   // Also output the worldToTanget transform which
   // we use to create the world space normal.
   getOutWorldToTangent( componentList, meta );


   // TODO: Restore this!
   /*
   // Check to see if we're rendering world space normals.
   if ( fd.materialFeatures[MFT_NormalsOut] )
   {
      Var *inNormal = (Var*)LangElement::find( "normal" );

      Var *outNormal = connectComp->getElement( RT_TEXCOORD );
      outNormal->setName( "normal" );
      outNormal->setStructName( "OUT" );
      outNormal->setType( "float3" );
      outNormal->mapsToSampler = false;

      meta->addStatement( new GenOp( "   @ = @; // MFT_NormalsOut\r\n", outNormal, inNormal ) );      
      output = meta;
      return;
   }
   */

   output = meta;
}

void BumpFeatGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   // Get the texture coord.
   Var *texCoord = getInTexCoord( "out_texCoord", "vec2", true, componentList );

   // Sample the bumpmap.
   Var *bumpMap = getNormalMapTex();
   LangElement *texOp = new GenOp( "texture2D(@, @)", bumpMap, texCoord );
   Var *bumpNorm = new Var( "bumpNormal", "vec4" );
   meta->addStatement( expandNormalMap( texOp, new DecOp( bumpNorm ), bumpNorm, fd ) );

   // We transform it into world space by reversing the 
   // multiplication by the worldToTanget transform.
   Var *wsNormal = new Var( "wsNormal", "vec3" );
   Var *worldToTanget = getInWorldToTangent( componentList );
   meta->addStatement( new GenOp( "   @ = normalize( vec3( @.xyz * @ ) );\r\n", new DecOp( wsNormal ), bumpNorm, worldToTanget ) );

   // TODO: Restore this!
   /*
   // Check to see if we're rendering world space normals.
   if ( fd.materialFeatures[MFT_NormalsOut]  )
   {
      Var *inNormal = getInTexCoord( "normal", "float3", false, componentList );

      LangElement *normalOut;
      Var *outColor = (Var*)LangElement::find( "col" );
      if ( outColor )
         normalOut = new GenOp( "float4( ( -@ + 1 ) * 0.5, @.a )", inNormal, outColor );
      else
         normalOut = new GenOp( "float4( ( -@ + 1 ) * 0.5, 1 )", inNormal );

      meta->addStatement( new GenOp( "   @; // MFT_NormalsOut\r\n", 
         assignColor( normalOut, Material::None ) ) );
         
      output = meta;
      return;
   }
   */

   output = meta;
}

ShaderFeature::Resources BumpFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 

   // If we have no parallax then we bring on the normal tex.
   if ( !fd.features[MFT_Parallax] )
      res.numTex = 1;

   // Only the parallax or diffuse map will add texture
   // coords other than us.
   if (  !fd.features[MFT_Parallax] &&
         !fd.features[MFT_DiffuseMap] &&
         !fd.features[MFT_OverlayMap] &&
         !fd.features[MFT_DetailMap] )
      res.numTexReg++;

   // We pass the world to tanget space transform.
   res.numTexReg += 3;

   return res;
}

void BumpFeatGLSL::setTexData(   Material::StageData &stageDat,
                                 const MaterialFeatureData &fd,
                                 RenderPassData &passData,
                                 U32 &texIndex )
{
   // If we had a parallax feature then it takes
   // care of hooking up the normal map texture.
   if ( fd.features[MFT_Parallax] )
      return;

   GFXTextureObject *tex = stageDat.getTex( MFT_NormalMap );
   if ( tex )
   {
      passData.mTexType[ texIndex ] = Material::Bump;
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}

/*
Var* ParallaxFeatHLSL::_getUniformVar( const char *name, const char *type )
{
   Var *theVar = (Var*)LangElement::find( name );
   if ( !theVar )
   {
      theVar = new Var;
      theVar->setType( type );
      theVar->setName( name );
      theVar->uniform = true;
      theVar->constSortPos = cspPass;
   }

   return theVar;
}

void ParallaxFeatHLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   AssertFatal( GFX->getPixelShaderVersion() >= 2.0, 
      "ParallaxFeatHLSL::processVert - We don't support SM 1.x!" );

   MultiLine *meta = new MultiLine;

   // Add the texture coords.
   getOutTexCoord(   "texCoord", 
                     "float2", 
                     true, 
                     fd.features[MFT_TexAnim], 
                     meta, 
                     componentList );

   // Grab the input position.
   Var *inPos = (Var*)LangElement::find( "inPosition" );
   if ( !inPos )
      inPos = (Var*)LangElement::find( "position" );

   // Get the object space eye position and the world
   // to tangent transform.
   Var *eyePos = _getUniformVar( "eyePos", "float3" );
   Var *objToTangentSpace = getOutObjToTangentSpace( componentList, meta );

   // send transform to pixel shader
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outViewTS = connectComp->getElement( RT_TEXCOORD, 1 );
   outViewTS->setName( "outViewTS" );
   outViewTS->setStructName( "OUT" );
   outViewTS->setType( "float3" );
   meta->addStatement( new GenOp( "   @ = mul( @ - @.xyz, transpose( @ ) );\r\n", 
      outViewTS, inPos, eyePos, objToTangentSpace ) );

   output = meta;
}

void ParallaxFeatHLSL::processPix(  Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   AssertFatal( GFX->getPixelShaderVersion() >= 2.0, 
      "ParallaxFeatHLSL::processPix - We don't support SM 1.x!" );

   MultiLine *meta = new MultiLine;

   // Order matters... get this first!
   Var *texCoord = getInTexCoord( "texCoord", "float2", true, componentList );

   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // We need the negative tangent space view vector
   // as in parallax mapping we step towards the camera.
   Var *negViewTS = (Var*)LangElement::find( "negViewTS" );
   if ( !negViewTS )
   {
      Var *inViewTS = (Var*)LangElement::find( "outViewTS" );
      if ( !inViewTS )
      {
         inViewTS = connectComp->getElement( RT_TEXCOORD, 1 );
         inViewTS->setName( "outViewTS" );
         inViewTS->setStructName( "IN" );
         inViewTS->setType( "float3" );
      }

      negViewTS = new Var( "negViewTS", "float3" );
      meta->addStatement( new GenOp( "   @ = -normalize( @ );\r\n", new DecOp( negViewTS ), inViewTS ) );
   }

   // Get the rest of our inputs.
   Var *parallaxInfo = _getUniformVar( "parallaxInfo", "float" );
   Var *normalMap = getNormalMapTex();

   // Do 3 parallax samples to get acceptable
   // quality without too much overhead.
   Var *pdepth = findOrCreateLocal( "pdepth", "float", meta );
   Var *poffset = findOrCreateLocal( "poffset", "float2", meta );
   meta->addStatement( new GenOp( "   @ = tex2D( @, @.xy ).a;\r\n", pdepth, normalMap, texCoord ) );
   meta->addStatement( new GenOp( "   @ = @.xy * ( @ * @ );\r\n", poffset, negViewTS, pdepth, parallaxInfo ) );

   meta->addStatement( new GenOp( "   @ = ( @ + tex2D( @, @.xy + @ ).a ) * 0.5;\r\n", pdepth, pdepth, normalMap, texCoord, poffset ) );
   meta->addStatement( new GenOp( "   @ = @.xy * ( @ * @ );\r\n", poffset, negViewTS, pdepth, parallaxInfo ) );

   meta->addStatement( new GenOp( "   @ = ( @ + tex2D( @, @.xy + @ ).a ) * 0.5;\r\n", pdepth, pdepth, normalMap, texCoord, poffset ) );
   meta->addStatement( new GenOp( "   @ = @.xy * ( @ * @ );\r\n", poffset, negViewTS, pdepth, parallaxInfo ) );

   meta->addStatement( new GenOp( "   @.xy += @;\r\n", texCoord, poffset ) );
   
   // TODO: Fix second UV.

   output = meta;
}

ShaderFeature::Resources ParallaxFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   AssertFatal( GFX->getPixelShaderVersion() >= 2.0, 
      "ParallaxFeatHLSL::getResources - We don't support SM 1.x!" );

   Resources res;

   // We add the outViewTS to the outputstructure.
   res.numTexReg = 1;

   // If this isn't a prepass then we will be
   // creating the normal map here.
   if ( !fd.features.hasFeature( MFT_PrePassConditioner ) )
      res.numTex = 1;

   return res;
}

void ParallaxFeatHLSL::setTexData(  Material::StageData &stageDat,
                                    const MaterialFeatureData &fd,
                                    RenderPassData &passData,
                                    U32 &texIndex )
{
   AssertFatal( GFX->getPixelShaderVersion() >= 2.0, 
      "ParallaxFeatHLSL::setTexData - We don't support SM 1.x!" );

   GFXTextureObject *tex = stageDat.getTex( MFT_NormalMap );
   if ( tex )
   {
      passData.mTexType[ texIndex ] = Material::Bump;
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}
*/