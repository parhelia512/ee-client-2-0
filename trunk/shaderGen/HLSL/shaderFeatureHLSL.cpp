//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "shaderGen/HLSL/shaderFeatureHLSL.h"

#include "shaderGen/langElement.h"
#include "shaderGen/shaderOp.h"
#include "shaderGen/shaderGenVars.h"
#include "gfx/gfxDevice.h"
#include "materials/matInstance.h"
#include "materials/processedMaterial.h"
#include "materials/materialFeatureTypes.h"
#include "core/util/autoPtr.h"

#include "lighting/advanced/advancedLightBinManager.h"

LangElement * ShaderFeatureHLSL::setupTexSpaceMat( Vector<ShaderComponent*> &, // componentList
                                                   Var **texSpaceMat )
{
   Var *N = (Var*) LangElement::find( "normal" );
   Var *B = (Var*) LangElement::find( "B" );
   Var *T = (Var*) LangElement::find( "T" );

   Var *tangentW = (Var*) LangElement::find( "tangentW" );
   
   // setup matrix var
   *texSpaceMat = new Var;
   (*texSpaceMat)->setType( "float3x3" );
   (*texSpaceMat)->setName( "objToTangentSpace" );

   MultiLine * meta = new MultiLine;
   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( *texSpaceMat ) ) );
   meta->addStatement( new GenOp( "   @[0] = @;\r\n", *texSpaceMat, T ) );
   if ( B )
      meta->addStatement( new GenOp( "   @[1] = @;\r\n", *texSpaceMat, B ) );
   else
   {
      if(dStricmp((char*)T->type, "float4") == 0)
         meta->addStatement( new GenOp( "   @[1] = cross( @, normalize(@) ) * @.w;\r\n", *texSpaceMat, T, N, T ) );
      else if(tangentW)
         meta->addStatement( new GenOp( "   @[1] = cross( @, normalize(@) ) * @;\r\n", *texSpaceMat, T, N, tangentW ) );
      else
         meta->addStatement( new GenOp( "   @[1] = cross( @, normalize(@) );\r\n", *texSpaceMat, T, N ) );
   }
   meta->addStatement( new GenOp( "   @[2] = normalize(@);\r\n", *texSpaceMat, N ) );

   return meta;
}

LangElement* ShaderFeatureHLSL::assignColor( LangElement *elem, 
                                             Material::BlendOp blend, 
                                             LangElement *lerpElem, 
                                             ShaderFeature::OutputTarget outputTarget )
{

   // search for color var
   Var *color = (Var*) LangElement::find( getOutputTargetVarName(outputTarget) );

   if ( !color )
   {
      // create color var
      color = new Var;
      color->setType( "fragout" );
      color->setName( getOutputTargetVarName(outputTarget) );
      color->setStructName( "OUT" );

      return new GenOp( "@ = @", color, elem );
   }

   LangElement *assign;

   switch ( blend )
   {
      case Material::Add:
         assign = new GenOp( "@ += @", color, elem );
         break;

      case Material::Sub:
         assign = new GenOp( "@ -= @", color, elem );
         break;

      case Material::Mul:
         assign = new GenOp( "@ *= @", color, elem );
         break;

      case Material::AddAlpha:
         assign = new GenOp( "@ += @ * @.a", color, elem, elem );
         break;

      case Material::LerpAlpha:
         if ( !lerpElem )
            lerpElem = elem;
         assign = new GenOp( "@.rgb = lerp( @.rgb, (@).rgb, (@).a )", color, color, elem, lerpElem );
         break;
      
      case Material::ToneMap:
         assign = new GenOp( "@ = 1.0 - exp(-1.0 * @ * @)", color, color, elem );
         break;
         
      default:
         AssertFatal(false, "Unrecognized color blendOp");
         // Fallthru

      case Material::None:
         assign = new GenOp( "@ = @", color, elem );
         break;    
   }
  
   return assign;
}


LangElement *ShaderFeatureHLSL::expandNormalMap(   LangElement *sampleNormalOp, 
                                                   LangElement *normalDecl, 
                                                   LangElement *normalVar, 
                                                   const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   if ( fd.features.hasFeature( MFT_IsDXTnm, getProcessIndex() ) )
   {
      // DXT Swizzle trick
      meta->addStatement( new GenOp( "   @ = float4( @.ag * 2.0 - 1.0, 0.0, 0.0 ); // DXTnm\r\n", normalDecl, sampleNormalOp ) );
      meta->addStatement( new GenOp( "   @.z = sqrt( 1.0 - dot( @.xy, @.xy ) ); // DXTnm\r\n", normalVar, normalVar, normalVar ) );
   }
   else
   {
      meta->addStatement( new GenOp( "   @ = @;\r\n", normalDecl, sampleNormalOp ) );
      meta->addStatement( new GenOp( "   @.xyz = @.xyz * 2.0 - 1.0;\r\n", normalVar, normalVar ) );
   }

   return meta;
}

ShaderFeatureHLSL::ShaderFeatureHLSL()
{
   output = NULL;
}

Var * ShaderFeatureHLSL::getVertTexCoord( const String &name )
{
   Var *inTex = NULL;

   for( U32 i=0; i<LangElement::elementList.size(); i++ )
   {
      if( !dStrcmp( (char*)LangElement::elementList[i]->name, name.c_str() ) )
      {
         inTex = dynamic_cast<Var*>( LangElement::elementList[i] );

         if( inTex )
         {
            if( !dStrcmp( (char*)inTex->structName, "IN" ) )
            {
               break;
            }
            else
            {
               inTex = NULL;
            }
         }
      }
   }

   return inTex;
}

Var* ShaderFeatureHLSL::getOutObjToTangentSpace(   Vector<ShaderComponent*> &componentList,
                                                   MultiLine *meta )
{
   Var *outObjToTangentSpace = (Var*)LangElement::find( "objToTangentSpace" );
   if ( !outObjToTangentSpace )
      meta->addStatement( setupTexSpaceMat( componentList, &outObjToTangentSpace ) );

   return outObjToTangentSpace;
}

Var* ShaderFeatureHLSL::getOutWorldToTangent(   Vector<ShaderComponent*> &componentList,
                                                MultiLine *meta )
{
   Var *outWorldToTangent = (Var*)LangElement::find( "worldToTangent" );
   if ( !outWorldToTangent )
   {
      Var *texSpaceMat = getOutObjToTangentSpace( componentList, meta );

      // turn obj->tangent into world->tangent
      Var *worldToTangent = new Var;
      worldToTangent->setType( "float3x3" );
      worldToTangent->setName( "worldToTangent" );
      LangElement *worldToTangentDecl = new DecOp( worldToTangent );

      // Get the world->obj transform
      Var *worldToObj = new Var;
      worldToObj->setType( "float4x4" );
      worldToObj->setName( "worldToObj" );
      worldToObj->uniform = true;
      worldToObj->constSortPos = cspPrimitive;

      // assign world->tangent transform
      meta->addStatement( new GenOp( "   @ = mul( @, (float3x3)@ );\r\n", worldToTangentDecl, texSpaceMat, worldToObj ) );

      // send transform to pixel shader
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

      outWorldToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
      outWorldToTangent->setName( "worldToTangent" );
      outWorldToTangent->setStructName( "OUT" );
      outWorldToTangent->setType( "float3x3" );
      meta->addStatement( new GenOp( "   @ = @;\r\n", outWorldToTangent, worldToTangent ) );
   }

   return outWorldToTangent;
}

Var* ShaderFeatureHLSL::getOutViewToTangent(   Vector<ShaderComponent*> &componentList,
                                             MultiLine *meta )
{
   Var *outViewToTangent = (Var*)LangElement::find( "viewToTangent" );
   if ( !outViewToTangent )
   {
      Var *texSpaceMat = getOutObjToTangentSpace( componentList, meta );

      // turn obj->tangent into world->tangent
      Var *viewToTangent = new Var;
      viewToTangent->setType( "float3x3" );
      viewToTangent->setName( "viewToTangent" );
      LangElement *viewToTangentDecl = new DecOp( viewToTangent );

      // Get the view->obj transform
      Var *viewToObj = new Var;
      viewToObj->setType( "float4x4" );
      viewToObj->setName( "viewToObj" );
      viewToObj->uniform = true;
      viewToObj->constSortPos = cspPrimitive;

      // assign world->tangent transform
      meta->addStatement( new GenOp( "   @ = mul( @, (float3x3)@ );\r\n", viewToTangentDecl, texSpaceMat, viewToObj ) );

      // send transform to pixel shader
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

      outViewToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
      outViewToTangent->setName( "viewToTangent" );
      outViewToTangent->setStructName( "OUT" );
      outViewToTangent->setType( "float3x3" );
      meta->addStatement( new GenOp( "   @ = @;\r\n", outViewToTangent, viewToTangent ) );
   }

   return outViewToTangent;
}

Var* ShaderFeatureHLSL::getOutTexCoord(   const char *name,
                                          const char *type,
                                          bool mapsToSampler,
                                          bool useTexAnim,
                                          MultiLine *meta,
                                          Vector<ShaderComponent*> &componentList )
{
   String outTexName = String::ToString( "out_%s", name );
   Var *texCoord = (Var*)LangElement::find( outTexName );
   if ( !texCoord )
   {
      Var *inTex = getVertTexCoord( name );
      AssertFatal( inTex, "ShaderFeatureHLSL::getOutTexCoord - Unknown vertex input coord!" );

      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

      texCoord = connectComp->getElement( RT_TEXCOORD );
      texCoord->setName( outTexName );
      texCoord->setStructName( "OUT" );
      texCoord->setType( type );
      texCoord->mapsToSampler = mapsToSampler;

      if( useTexAnim )
      {
         inTex->setType( "float4" );
         
         // create texture mat var
         Var *texMat = new Var;
         texMat->setType( "float4x4" );
         texMat->setName( "texMat" );
         texMat->uniform = true;
         texMat->constSortPos = cspPass;      
         
         meta->addStatement( new GenOp( "   @ = mul(@, @).xy;\r\n", texCoord, texMat, inTex ) );
      }
      else
         meta->addStatement( new GenOp( "   @ = @;\r\n", texCoord, inTex ) );
   }

   AssertFatal( dStrcmp( type, (const char*)texCoord->type ) == 0, 
      "ShaderFeatureHLSL::getOutTexCoord - Type mismatch!" );

   return texCoord;
}

Var* ShaderFeatureHLSL::getInTexCoord( const char *name,
                                       const char *type,
                                       bool mapsToSampler,
                                       Vector<ShaderComponent*> &componentList )
{
   Var* texCoord = (Var*)LangElement::find( name );
   if ( !texCoord )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector*>( componentList[C_CONNECTOR] );
      texCoord = connectComp->getElement( RT_TEXCOORD );
      texCoord->setName( name );
      texCoord->setStructName( "IN" );
      texCoord->setType( type );
      texCoord->mapsToSampler = mapsToSampler;
   }

   AssertFatal( dStrcmp( type, (const char*)texCoord->type ) == 0, 
      "ShaderFeatureHLSL::getInTexCoord - Type mismatch!" );

   return texCoord;
}

Var* ShaderFeatureHLSL::getInWorldToTangent( Vector<ShaderComponent*> &componentList )
{
   Var *worldToTangent = (Var*)LangElement::find( "worldToTangent" );
   if ( !worldToTangent )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      worldToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
      worldToTangent->setName( "worldToTangent" );
      worldToTangent->setStructName( "IN" );
      worldToTangent->setType( "float3x3" );
   }

   return worldToTangent;
}

Var* ShaderFeatureHLSL::getInViewToTangent( Vector<ShaderComponent*> &componentList )
{
   Var *viewToTangent = (Var*)LangElement::find( "viewToTangent" );
   if ( !viewToTangent )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      viewToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
      viewToTangent->setName( "viewToTangent" );
      viewToTangent->setStructName( "IN" );
      viewToTangent->setType( "float3x3" );
   }

   return viewToTangent;
}

Var* ShaderFeatureHLSL::getNormalMapTex()
{
   Var *normalMap = (Var*)LangElement::find( "bumpMap" );
   if ( !normalMap )
   {
      normalMap = new Var;
      normalMap->setType( "sampler2D" );
      normalMap->setName( "bumpMap" );
      normalMap->uniform = true;
      normalMap->sampler = true;
      normalMap->constNum = Var::getTexUnitNum();
   }

   return normalMap;
}

void ShaderFeatureHLSL::getWsPosition( MultiLine *meta, LangElement *wsPosition )
{
   // Get the input position.
   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );

   AssertFatal( inPosition, "ShaderFeatureHLSL::getWsPosition - The vertex position was not found!" );

   // Get the transform to world space.
   Var *objTrans = (Var*)LangElement::find( "objTrans" );
   if ( !objTrans )
   {
      objTrans = new Var;
      objTrans->setType( "float4x4" );
      objTrans->setName( "objTrans" );
      objTrans->uniform = true;
      objTrans->constSortPos = cspPrimitive;      
   }

   meta->addStatement( new GenOp( "   @ = mul( @, float4( @.xyz, 1 ) ).xyz;\r\n", 
      wsPosition, objTrans, inPosition ) );
}

Var* ShaderFeatureHLSL::addOutWsPosition( Vector<ShaderComponent*> &componentList, MultiLine *meta )
{
   Var *outWsPosition = (Var*)LangElement::find( "wsPosition" );
   if ( !outWsPosition )
   {
      // Setup the connector.
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      outWsPosition = connectComp->getElement( RT_TEXCOORD );
      outWsPosition->setName( "wsPosition" );
      outWsPosition->setStructName( "OUT" );
      outWsPosition->setType( "float3" );
      outWsPosition->mapsToSampler = false;

      getWsPosition( meta, outWsPosition );
   }

   return outWsPosition;
}

Var* ShaderFeatureHLSL::getInWsPosition( Vector<ShaderComponent*> &componentList )
{
   Var *wsPosition = (Var*)LangElement::find( "wsPosition" );
   if ( !wsPosition )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      wsPosition = connectComp->getElement( RT_TEXCOORD );
      wsPosition->setName( "wsPosition" );
      wsPosition->setStructName( "IN" );
      wsPosition->setType( "float3" );
   }

   return wsPosition;
}

Var* ShaderFeatureHLSL::getWsView( Var *wsPosition, MultiLine *meta )
{
   Var *wsView = (Var*)LangElement::find( "wsView" );
   if ( !wsView )
   {
      wsView = new Var( "wsView", "float3" );

      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var;
         eyePos->setType( "float3" );
         eyePos->setName( "eyePosWorld" );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }

      meta->addStatement( new GenOp( "   @ = normalize( @ - @ );\r\n", 
         new DecOp( wsView ), eyePos, wsPosition ) );
   }

   return wsView;
}


//****************************************************************************
// Base Texture
//****************************************************************************

void DiffuseMapFeatHLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   getOutTexCoord(   "texCoord", 
                     "float2", 
                     true, 
                     fd.features[MFT_TexAnim], 
                     meta, 
                     componentList );
   output = meta;
}

void DiffuseMapFeatHLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // grab connector texcoord register
   Var *inTex = getInTexCoord( "texCoord", "float2", true, componentList );

   // create texture var
   Var *diffuseMap = new Var;
   diffuseMap->setType( "sampler2D" );
   diffuseMap->setName( "diffuseMap" );
   diffuseMap->uniform = true;
   diffuseMap->sampler = true;
   diffuseMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   if (  fd.features[MFT_CubeMap] )
   {
      MultiLine * meta = new MultiLine;
      
      // create sample color
      Var *diffColor = new Var;
      diffColor->setType( "float4" );
      diffColor->setName( "diffuseColor" );
      LangElement *colorDecl = new DecOp( diffColor );
   
      meta->addStatement(  new GenOp( "   @ = tex2D(@, @);\r\n", 
                           colorDecl, 
                           diffuseMap, 
                           inTex ) );
      
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( diffColor, Material::Mul ) ) );
      output = meta;
   }
   else
   {
      LangElement *statement = new GenOp( "tex2D(@, @)", diffuseMap, inTex );
      output = new GenOp( "   @;\r\n", assignColor( statement, Material::Mul ) );
   }
   
}

ShaderFeature::Resources DiffuseMapFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void DiffuseMapFeatHLSL::setTexData(   Material::StageData &stageDat,
                                       const MaterialFeatureData &fd,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_DiffuseMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
}


//****************************************************************************
// Overlay Texture
//****************************************************************************

void OverlayTexFeatHLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   Var *inTex = getVertTexCoord( "texCoord2" );
   AssertFatal( inTex, "OverlayTexFeatHLSL::processVert() - The second UV set was not found!" );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outTex = connectComp->getElement( RT_TEXCOORD );
   outTex->setName( "outTexCoord2" );
   outTex->setStructName( "OUT" );
   outTex->setType( "float2" );
   outTex->mapsToSampler = true;

   if( fd.features[MFT_TexAnim] )
   {
      inTex->setType( "float4" );

      // Find or create the texture matrix.
      Var *texMat = (Var*)LangElement::find( "texMat" );
      if ( !texMat )
      {
         texMat = new Var;
         texMat->setType( "float4x4" );
         texMat->setName( "texMat" );
         texMat->uniform = true;
         texMat->constSortPos = cspPass;   
      }
     
      output = new GenOp( "   @ = mul(@, @);\r\n", outTex, texMat, inTex );
      return;
   }
   
   // setup language elements to output incoming tex coords to output
   output = new GenOp( "   @ = @;\r\n", outTex, inTex );
}

void OverlayTexFeatHLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *inTex = connectComp->getElement( RT_TEXCOORD );
   inTex->setName( "texCoord2" );
   inTex->setStructName( "IN" );
   inTex->setType( "float2" );
   inTex->mapsToSampler = true;

   // create texture var
   Var *diffuseMap = new Var;
   diffuseMap->setType( "sampler2D" );
   diffuseMap->setName( "overlayMap" );
   diffuseMap->uniform = true;
   diffuseMap->sampler = true;
   diffuseMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   LangElement *statement = new GenOp( "tex2D(@, @)", diffuseMap, inTex );
   output = new GenOp( "   @;\r\n", assignColor( statement, Material::LerpAlpha ) );
}

ShaderFeature::Resources OverlayTexFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;
   return res;
}

void OverlayTexFeatHLSL::setTexData(   Material::StageData &stageDat,
                                       const MaterialFeatureData &fd,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_OverlayMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
}


//****************************************************************************
// Diffuse color
//****************************************************************************

void DiffuseFeatureHLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   Var *diffuseMaterialColor  = new Var;
   diffuseMaterialColor->setType( "float4" );
   diffuseMaterialColor->setName( "diffuseMaterialColor" );
   diffuseMaterialColor->uniform = true;
   diffuseMaterialColor->constSortPos = cspPotentialPrimitive;

   MultiLine * meta = new MultiLine;
   meta->addStatement( new GenOp( "   @;\r\n", assignColor( diffuseMaterialColor, Material::Add ) ) );
   output = meta;
}


//****************************************************************************
// Lightmap
//****************************************************************************

void LightmapFeatHLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // grab tex register from incoming vert
   Var *inTex = getVertTexCoord( "texCoord2" );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outTex = connectComp->getElement( RT_TEXCOORD );
   outTex->setName( "texCoord2" );
   outTex->setStructName( "OUT" );
   outTex->setType( "float2" );
   outTex->mapsToSampler = true;

   // setup language elements to output incoming tex coords to output
   output = new GenOp( "   @ = @;\r\n", outTex, inTex );
}

void LightmapFeatHLSL::processPix(  Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *inTex = connectComp->getElement( RT_TEXCOORD );
   inTex->setName( "texCoord2" );
   inTex->setStructName( "IN" );
   inTex->setType( "float2" );
   inTex->mapsToSampler = true;

   // create texture var
   Var *lightMap = new Var;
   lightMap->setType( "sampler2D" );
   lightMap->setName( "lightMap" );
   lightMap->uniform = true;
   lightMap->sampler = true;
   lightMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   
   // argh, pixel specular should prob use this too
   if( fd.features[MFT_NormalMap] )
   {
      Var *lmColor = new Var;
      lmColor->setName( "lmColor" );
      lmColor->setType( "float4" );
      LangElement *lmColorDecl = new DecOp( lmColor );
      
      output = new GenOp( "   @ = tex2D(@, @);\r\n", lmColorDecl, lightMap, inTex );
      return;
   }

   // Add realtime lighting, if it is available
   LangElement *statement = NULL;
   if( fd.features[MFT_RTLighting] )
   {
      // Advanced lighting is the only dynamic lighting supported right now
      Var *inColor = (Var*) LangElement::find( "d_lightcolor" );
      if(inColor != NULL)
      {
         // Find out if RTLighting should be added or substituted
         bool bPreProcessedLighting = false;
         MatTextureTarget *texTarget = MatTextureTarget::findTargetByName(AdvancedLightBinManager::smBufferName);
         if(texTarget)
         {
            AssertFatal(dynamic_cast<AdvancedLightBinManager *>(texTarget), "Bad buffer type!");
            AdvancedLightBinManager *lightBin = static_cast<AdvancedLightBinManager *>(texTarget);
            bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();
         }

         // Lightmap has already been included in the advanced light bin, so
         // no need to do any sampling or anything
         if(bPreProcessedLighting)
            statement = new GenOp( "float4(@, 1.0)", inColor );
         else
            statement = new GenOp( "tex2D(@, @) + float4(@.rgb, 0.0)", lightMap, inTex, inColor );
      }
   }
   
   // If we still don't have it... then just sample the lightmap.   
   if ( !statement )
      statement = new GenOp( "tex2D(@, @)", lightMap, inTex );

   // Assign to proper render target
   MultiLine *meta = new MultiLine;
   if( fd.features[MFT_LightbufferMRT] )
   {
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( statement, Material::None, NULL, ShaderFeature::RenderTarget1 ) ) );
      meta->addStatement( new GenOp( "   @.a = 0.0001;\r\n", LangElement::find( getOutputTargetVarName(ShaderFeature::RenderTarget1) ) ) );
   }
   else
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( statement, Material::Mul ) ) );

   output = meta;
}

ShaderFeature::Resources LightmapFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void LightmapFeatHLSL::setTexData(  Material::StageData &stageDat,
                                    const MaterialFeatureData &fd,
                                    RenderPassData &passData,
                                    U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_LightMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   else
      passData.mTexType[ texIndex++ ] = Material::Lightmap;
}

U32 LightmapFeatHLSL::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}

//****************************************************************************
// Tonemap
//****************************************************************************

void TonemapFeatHLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // Grab the connector
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // Set up the second set of texCoords
   Var *inTex2 = getVertTexCoord( "texCoord2" );

   if ( inTex2 )
   {
      Var *outTex2 = connectComp->getElement( RT_TEXCOORD );
      outTex2->setName( "texCoord2" );
      outTex2->setStructName( "OUT" );
      outTex2->setType( "float2" );
      outTex2->mapsToSampler = true;

      output = new GenOp( "   @ = @;\r\n", outTex2, inTex2 );
   }
}

void TonemapFeatHLSL::processPix(  Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // Grab connector
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   Var *inTex2 = connectComp->getElement( RT_TEXCOORD );
   inTex2->setName( "texCoord2" );
   inTex2->setStructName( "IN" );
   inTex2->setType( "float2" );
   inTex2->mapsToSampler = true;

   // create texture var
   Var *toneMap = new Var;
   toneMap->setType( "sampler2D" );
   toneMap->setName( "toneMap" );
   toneMap->uniform = true;
   toneMap->sampler = true;
   toneMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   MultiLine * meta = new MultiLine;

   // First get the toneMap color
   Var *toneMapColor = new Var;
   toneMapColor->setType( "float4" );
   toneMapColor->setName( "toneMapColor" );
   LangElement *toneMapColorDecl = new DecOp( toneMapColor );

   meta->addStatement( new GenOp( "   @ = tex2D(@, @);\r\n", toneMapColorDecl, toneMap, inTex2 ) );

   // We do a different calculation if there is a diffuse map or not
   Material::BlendOp blendOp = Material::Mul;
   if ( fd.features[MFT_DiffuseMap] )
   {
      // Reverse the tonemap
      meta->addStatement( new GenOp( "   @ = -1.0f * log(1.0f - @);\r\n", toneMapColor, toneMapColor ) );

      // Re-tonemap with the current color factored in
      blendOp = Material::ToneMap;
   }

   // Find out if RTLighting should be added
   bool bPreProcessedLighting = false;
   MatTextureTarget *texTarget = MatTextureTarget::findTargetByName(AdvancedLightBinManager::smBufferName);
   if(texTarget)
   {
      AssertFatal(dynamic_cast<AdvancedLightBinManager *>(texTarget), "Bad buffer type!");
      AdvancedLightBinManager *lightBin = static_cast<AdvancedLightBinManager *>(texTarget);
      bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();
   }

   // Add in the realtime lighting contribution
   if ( fd.features[MFT_RTLighting] )
   {
      // Right now, only Advanced Lighting is supported
      Var *inColor = (Var*) LangElement::find( "d_lightcolor" );
      if(inColor != NULL)
      {
         // Assign value in d_lightcolor to toneMapColor if it exists. This is
         // the dynamic light buffer, and it already has the tonemap included
         if(bPreProcessedLighting)
            meta->addStatement( new GenOp( "   @.rgb = @;\r\n", toneMapColor, inColor ) );
         else
            meta->addStatement( new GenOp( "   @.rgb += @.rgb;\r\n", toneMapColor, inColor ) );
      }
   }

   // Assign to proper render target
   if( fd.features[MFT_LightbufferMRT] )
   {
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( toneMapColor, Material::None, NULL, ShaderFeature::RenderTarget1 ) ) );
      meta->addStatement( new GenOp( "   @.a = 0.0001;\r\n", LangElement::find( getOutputTargetVarName(ShaderFeature::RenderTarget1) ) ) );
   }
   else
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( toneMapColor, blendOp ) ) );
  
   output = meta;
}

ShaderFeature::Resources TonemapFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void TonemapFeatHLSL::setTexData(  Material::StageData &stageDat,
                                    const MaterialFeatureData &fd,
                                    RenderPassData &passData,
                                    U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_ToneMap );
   if ( tex )
   {
      passData.mTexType[ texIndex ] = Material::ToneMapTex;
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}

U32 TonemapFeatHLSL::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}

//****************************************************************************
// pureLIGHT Lighting
//****************************************************************************

void VertLitHLSL::processVert(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   // If we have a lightMap or toneMap then our lighting will be
   // handled by the MFT_LightMap or MFT_ToneNamp feature instead
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] )
   {
      output = NULL;
      return;
   }

   // Search for vert color
   Var *inColor = (Var*) LangElement::find( "diffuse" );   

   // If there isn't a vertex color then we can't do anything
   if( !inColor )
   {
      output = NULL;
      return;
   }

   // Grab the connector color
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outColor = connectComp->getElement( RT_COLOR );
   outColor->setName( "vertColor" );
   outColor->setStructName( "OUT" );
   outColor->setType( "float4" );

   output = new GenOp( "   @ = @;\r\n", outColor, inColor );
}

void VertLitHLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // If we have a lightMap or toneMap then our lighting will be
   // handled by the MFT_LightMap or MFT_ToneNamp feature instead
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] )
   {
      output = NULL;
      return;
   }
   
   // Grab the connector color register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *vertColor = connectComp->getElement( RT_COLOR );
   vertColor->setName( "vertColor" );
   vertColor->setStructName( "IN" );
   vertColor->setType( "float4" );

   MultiLine * meta = new MultiLine;

   // Defaults (no diffuse map)
   Material::BlendOp blendOp = Material::Mul;
   LangElement *outColor = vertColor;

   // We do a different calculation if there is a diffuse map or not
   if ( fd.features[MFT_DiffuseMap] || fd.features[MFT_VertLitTone] )
   {
      Var * finalVertColor = new Var;
      finalVertColor->setName( "finalVertColor" );
      finalVertColor->setType( "float4" );
      LangElement *finalVertColorDecl = new DecOp( finalVertColor );
      
      // Reverse the tonemap
      meta->addStatement( new GenOp( "   @ = -1.0f * log(1.0f - @);\r\n", finalVertColorDecl, vertColor ) );

      // Set the blend op to tonemap
      blendOp = Material::ToneMap;
      outColor = finalVertColor;
   }

   // Add in the realtime lighting contribution, if applicable
   if ( fd.features[MFT_RTLighting] )
   {
      Var *rtLightingColor = (Var*) LangElement::find( "d_lightcolor" );
      if(rtLightingColor != NULL)
      {
         // Find out if RTLighting should be added or substituted
         bool bPreProcessedLighting = false;
         MatTextureTarget *texTarget = MatTextureTarget::findTargetByName(AdvancedLightBinManager::smBufferName);
         if(texTarget)
         {
            AssertFatal(dynamic_cast<AdvancedLightBinManager *>(texTarget), "Bad buffer type!");
            AdvancedLightBinManager *lightBin = static_cast<AdvancedLightBinManager *>(texTarget);
            bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();
         }

         // Assign value in d_lightcolor to toneMapColor if it exists. This is
         // the dynamic light buffer, and it already has the baked-vertex-color 
         // included in it
         if(bPreProcessedLighting)
            outColor = new GenOp( "float4(@.rgb, 1.0)", rtLightingColor );
         else
            outColor = new GenOp( "float4(@.rgb + @.rgb, 1.0)", rtLightingColor, outColor );
      }
   }

   // Output the color
   if ( fd.features[MFT_LightbufferMRT] )
   {
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( outColor, Material::None, NULL, ShaderFeature::RenderTarget1 ) ) );
      meta->addStatement( new GenOp( "   @.a = 0.0001;\r\n", LangElement::find( getOutputTargetVarName(ShaderFeature::RenderTarget1) ) ) );
   }
   else
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( outColor, blendOp ) ) );

   output = meta;
}

U32 VertLitHLSL::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}

//****************************************************************************
// Detail map
//****************************************************************************

void DetailFeatHLSL::processVert(   Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // grab incoming texture coords
   Var *inTex = getVertTexCoord( "texCoord" );

   // create detail variable
   Var *detScale = new Var;
   detScale->setType( "float2" );
   detScale->setName( "detailScale" );
   detScale->uniform = true;
   detScale->constSortPos = cspPass;
   
   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outTex = connectComp->getElement( RT_TEXCOORD );
   outTex->setName( "detCoord" );
   outTex->setStructName( "OUT" );
   outTex->setType( "float2" );
   outTex->mapsToSampler = true;

   if( fd.features[MFT_TexAnim] )
   {
      inTex->setType( "float4" );

      // Find or create the texture matrix.
      Var *texMat = (Var*)LangElement::find( "texMat" );
      if ( !texMat )
      {
         texMat = new Var;
         texMat->setType( "float4x4" );
         texMat->setName( "texMat" );
         texMat->uniform = true;
         texMat->constSortPos = cspPass;   
      }

      output = new GenOp( "   @ = mul(@, @) * @;\r\n", outTex, texMat, inTex, detScale );
      return;
   }

   // setup output to mul texCoord by detail scale
   output = new GenOp( "   @ = @ * @;\r\n", outTex, inTex, detScale );
}

void DetailFeatHLSL::processPix( Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *inTex = connectComp->getElement( RT_TEXCOORD );
   inTex->setName( "detCoord" );
   inTex->setStructName( "IN" );
   inTex->setType( "float2" );
   inTex->mapsToSampler = true;

   // create texture var
   Var *detailMap = new Var;
   detailMap->setType( "sampler2D" );
   detailMap->setName( "detailMap" );
   detailMap->uniform = true;
   detailMap->sampler = true;
   detailMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   // We're doing the standard greyscale detail map
   // technique which can darken and lighten the 
   // diffuse texture.

   // TODO: We could add a feature to toggle between this
   // and a simple multiplication with the detail map.

   LangElement *statement = new GenOp( "( tex2D(@, @) * 2.0 ) - 1.0", detailMap, inTex );
   output = new GenOp( "   @;\r\n", assignColor( statement, Material::Add ) );
}

ShaderFeature::Resources DetailFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void DetailFeatHLSL::setTexData( Material::StageData &stageDat,
                                 const MaterialFeatureData &fd,
                                 RenderPassData &passData,
                                 U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_DetailMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
}


//****************************************************************************
// Vertex position
//****************************************************************************

void VertPositionHLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // First check for an input position from a previous feature
   // then look for the default vertex position.
   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );

   // grab connector position
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outPosition = connectComp->getElement( RT_POSITION );
   outPosition->setName( "hpos" );
   outPosition->setStructName( "OUT" );

   // create modelview variable
   Var *modelview = new Var;
   modelview->setType( "float4x4" );
   modelview->setName( "modelview" );
   modelview->uniform = true;
   modelview->constSortPos = cspPrimitive;   

   MultiLine *meta = new MultiLine;
   meta->addStatement( new GenOp( "   @ = mul(@, float4(@.xyz,1));\r\n", outPosition, modelview, inPosition ) );
   output = meta;
}


//****************************************************************************
// Reflect Cubemap
//****************************************************************************

void ReflectCubeFeatHLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // search for vert normal
   Var *inNormal = (Var*) LangElement::find( "normal" );
   if ( !inNormal )
      return;

   MultiLine * meta = new MultiLine;

   // If a base or bump tex is present in the material, but not in the
   // current pass - we need to add one to the current pass to use
   // its alpha channel as a gloss map.  Here we just need the tex coords.
   if( !fd.features[MFT_DiffuseMap] &&
       !fd.features[MFT_NormalMap] )
   {
      if( fd.materialFeatures[MFT_DiffuseMap] ||
          fd.materialFeatures[MFT_NormalMap] )
      {
         // find incoming texture var
         Var *inTex = getVertTexCoord( "texCoord" );

         // grab connector texcoord register
         ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
         Var *outTex = connectComp->getElement( RT_TEXCOORD );
         outTex->setName( "texCoord" );
         outTex->setStructName( "OUT" );
         outTex->setType( "float2" );
         outTex->mapsToSampler = true;

         // setup language elements to output incoming tex coords to output
         meta->addStatement( new GenOp( "   @ = @;\r\n", outTex, inTex ) );
      }
   }

   // create cubeTrans
   Var *cubeTrans = new Var;
   cubeTrans->setType( "float3x3" );
   cubeTrans->setName( "cubeTrans" );
   cubeTrans->uniform = true;
   cubeTrans->constSortPos = cspPrimitive;   

   // create cubeEye position
   Var *cubeEyePos = new Var;
   cubeEyePos->setType( "float3" );
   cubeEyePos->setName( "cubeEyePos" );
   cubeEyePos->uniform = true;
   cubeEyePos->constSortPos = cspPrimitive;   

   // cube vert position
   Var * cubeVertPos = new Var;
   cubeVertPos->setName( "cubeVertPos" );
   cubeVertPos->setType( "float3" );
   LangElement *cubeVertPosDecl = new DecOp( cubeVertPos );

   meta->addStatement( new GenOp( "   @ = mul(@, @).xyz;\r\n", 
                       cubeVertPosDecl, cubeTrans, LangElement::find( "position" ) ) );

   // cube normal
   Var * cubeNormal = new Var;
   cubeNormal->setName( "cubeNormal" );
   cubeNormal->setType( "float3" );
   LangElement *cubeNormDecl = new DecOp( cubeNormal );

   meta->addStatement( new GenOp( "   @ = normalize( mul(@, normalize(@)).xyz );\r\n", 
                       cubeNormDecl, cubeTrans, inNormal ) );

   // eye to vert
   Var * eyeToVert = new Var;
   eyeToVert->setName( "eyeToVert" );
   eyeToVert->setType( "float3" );
   LangElement *e2vDecl = new DecOp( eyeToVert );

   meta->addStatement( new GenOp( "   @ = @ - @;\r\n", 
                       e2vDecl, cubeVertPos, cubeEyePos ) );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *reflectVec = connectComp->getElement( RT_TEXCOORD );
   reflectVec->setName( "reflectVec" );
   reflectVec->setStructName( "OUT" );
   reflectVec->setType( "float3" );
   reflectVec->mapsToSampler = true;

   meta->addStatement( new GenOp( "   @ = reflect(@, @);\r\n", reflectVec, eyeToVert, cubeNormal ) );

   output = meta;
}

void ReflectCubeFeatHLSL::processPix(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine * meta = new MultiLine;
   Var *glossColor = NULL;
   
   // If a base or bump tex is present in the material, but not in the
   // current pass - we need to add one to the current pass to use
   // its alpha channel as a gloss map.
   if( !fd.features[MFT_DiffuseMap] &&
       !fd.features[MFT_NormalMap] )
   {
      if( fd.materialFeatures[MFT_DiffuseMap] ||
          fd.materialFeatures[MFT_NormalMap] )
      {
         // grab connector texcoord register
         Var *inTex = getInTexCoord( "texCoord", "float2", true, componentList );
      
         // create texture var
         Var *newMap = new Var;
         newMap->setType( "sampler2D" );
         newMap->setName( "glossMap" );
         newMap->uniform = true;
         newMap->sampler = true;
         newMap->constNum = Var::getTexUnitNum();     // used as texture unit num here
      
         // create sample color
         Var *color = new Var;
         color->setType( "float4" );
         color->setName( "diffuseColor" );
         LangElement *colorDecl = new DecOp( color );

         glossColor = color;
         
         meta->addStatement( new GenOp( "   @ = tex2D( @, @ );\r\n", colorDecl, newMap, inTex ) );
      }
   }
   else
   {
      glossColor = (Var*) LangElement::find( "diffuseColor" );
      if( !glossColor )
         glossColor = (Var*) LangElement::find( "bumpNormal" );
   }

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *reflectVec = connectComp->getElement( RT_TEXCOORD );
   reflectVec->setName( "reflectVec" );
   reflectVec->setStructName( "IN" );
   reflectVec->setType( "float3" );
   reflectVec->mapsToSampler = true;

   // create cubemap var
   Var *cubeMap = new Var;
   cubeMap->setType( "samplerCUBE" );
   cubeMap->setName( "cubeMap" );
   cubeMap->uniform = true;
   cubeMap->sampler = true;
   cubeMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   // TODO: Restore the lighting attenuation here!
   Var *attn = NULL;
   //if ( fd.materialFeatures[MFT_DynamicLight] )
	   //attn = (Var*)LangElement::find("attn");
   //else 
      if ( fd.materialFeatures[MFT_RTLighting] )
      attn =(Var*)LangElement::find("d_NL_Att");

   LangElement *texCube = new GenOp( "texCUBE( @, @ )", cubeMap, reflectVec );
   LangElement *lerpVal = NULL;
   Material::BlendOp blendOp = Material::LerpAlpha;

   // Note that the lerpVal needs to be a float4 so that
   // it will work with the LerpAlpha blend.

   if ( glossColor )
   {
      if ( attn )
         lerpVal = new GenOp( "@ * saturate( @ )", glossColor, attn );
      else
         lerpVal = glossColor;
   }
   else
   {
      if ( attn )
         lerpVal = new GenOp( "saturate( @ ).xxxx", attn );
      else
         blendOp = Material::None;
   }

   meta->addStatement( new GenOp( "   @;\r\n", assignColor( texCube, blendOp, lerpVal ) ) );         
   output = meta;
}

ShaderFeature::Resources ReflectCubeFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 

   if( fd.features[MFT_DiffuseMap] ||
       fd.features[MFT_NormalMap] )
   {
      res.numTex = 1;
      res.numTexReg = 1;
   }
   else
   {
      res.numTex = 2;
      res.numTexReg = 2;
   }

   return res;
}

void ReflectCubeFeatHLSL::setTexData(  Material::StageData &stageDat,
                                       const MaterialFeatureData &stageFeatures,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   // set up a gloss map if one is not present in the current pass
   // but is present in the current material stage
   if( !passData.mFeatureData.features[MFT_DiffuseMap] &&
       !passData.mFeatureData.features[MFT_NormalMap] )
   {
      GFXTextureObject *tex = stageDat.getTex( MFT_DetailMap );
      if (  tex &&
            stageFeatures.features[MFT_DiffuseMap] )
         passData.mTexSlot[ texIndex++ ].texObject = tex;
      else
      {
         tex = stageDat.getTex( MFT_NormalMap );

         if (  tex &&
               stageFeatures.features[ MFT_NormalMap ] )
            passData.mTexSlot[ texIndex++ ].texObject = tex;
      }
   }
   
   if( stageDat.getCubemap() )
   {
      passData.mCubeMap = stageDat.getCubemap();
      passData.mTexType[texIndex++] = Material::Cube;
   }
   else
   {
      if( stageFeatures.features[MFT_CubeMap] )
      {
         // assuming here that it is a scenegraph cubemap
         passData.mTexType[texIndex++] = Material::SGCube;
      }
   }

}


//****************************************************************************
// RTLighting
//****************************************************************************

RTLightingFeatHLSL::RTLightingFeatHLSL()
   : mDep( "shaders/common/lighting.hlsl" )
{
   addDependency( &mDep );
}

void RTLightingFeatHLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // Find the incoming vertex normal.
   Var *inNormal = (Var*)LangElement::find( "normal" );   

   // Skip out on realtime lighting if we don't have a normal
   // or we're doing some sort of baked lighting.
   if (  !inNormal || 
         fd.features[MFT_LightMap] || 
         fd.features[MFT_ToneMap] || 
         fd.features[MFT_VertLit] )
      return;

   MultiLine *meta = new MultiLine;

   // If there isn't a normal map then we need to pass
   // the world space normal to the pixel shader ourselves.
   if ( !fd.features[MFT_NormalMap] )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

      Var *outNormal = connectComp->getElement( RT_TEXCOORD );
      outNormal->setName( "wsNormal" );
      outNormal->setStructName( "OUT" );
      outNormal->setType( "float3" );
      outNormal->mapsToSampler = false;

      // Get the transform to world space.
      Var *objTrans = (Var*)LangElement::find( "objTrans" );
      if ( !objTrans )
      {
         objTrans = new Var;
         objTrans->setType( "float4x4" );
         objTrans->setName( "objTrans" );
         objTrans->uniform = true;
         objTrans->constSortPos = cspPrimitive;      
      }

      // Transform the normal to world space.
      meta->addStatement( new GenOp( "   @ = mul( @, float4( normalize( @ ), 0.0 ) ).xyz;\r\n", outNormal, objTrans, inNormal ) );
   }

   addOutWsPosition( componentList, meta );

   output = meta;
}

void RTLightingFeatHLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // Skip out on realtime lighting if we don't have a normal
   // or we're doing some sort of baked lighting.
   //
   // TODO: We can totally detect for this in the material
   // feature setup... we should move it out of here!
   //
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] || fd.features[MFT_VertLit] )
      return;
  
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   MultiLine *meta = new MultiLine;

   // Look for a wsNormal or grab it from the connector.
   Var *wsNormal = (Var*)LangElement::find( "wsNormal" );
   if ( !wsNormal )
   {
      wsNormal = connectComp->getElement( RT_TEXCOORD );
      wsNormal->setName( "wsNormal" );
      wsNormal->setStructName( "IN" );
      wsNormal->setType( "float3" );

      // If we loaded the normal its our resposibility
      // to normalize it... the interpolators won't.
      //
      // Note we cast to half here to get partial precision
      // optimized code which is an acceptable loss of
      // precision for normals and performs much better
      // on older Geforce cards.
      //
      meta->addStatement( new GenOp( "   @ = normalize( half3( @ ) );\r\n", wsNormal, wsNormal ) );
   }

   // Now the wsPosition and wsView.
   Var *wsPosition = getInWsPosition( componentList );
   Var *wsView = getWsView( wsPosition, meta );

   // Create temporaries to hold results of lighting.
   Var *rtShading = new Var( "rtShading", "float4" );
   Var *specular = new Var( "specular", "float4" );
   meta->addStatement( new GenOp( "   @; @;\r\n", 
      new DecOp( rtShading ), new DecOp( specular ) ) );   

   // Look for a light mask generated from a previous
   // feature (this is done for BL terrain lightmaps).
   LangElement *lightMask = LangElement::find( "lightMask" );
   if ( !lightMask )
      lightMask = new GenOp( "float4( 1, 1, 1, 1 )" );

   // Calculate the diffuse shading and specular powers.
   meta->addStatement( new GenOp( "   compute4Lights( @, @, @, @, @, @ );\r\n", 
      wsView, wsPosition, wsNormal, lightMask, rtShading, specular ) );

   // Apply the lighting to the diffuse color.
   LangElement *lighting = new GenOp( "float4( @.rgb + ambient.rgb, 1 )", rtShading );
   meta->addStatement( new GenOp( "   @;\r\n", assignColor( lighting, Material::Mul ) ) );
   output = meta;  
}

ShaderFeature::Resources RTLightingFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res;

   // These features disable realtime lighting.
   if (  !fd.features[MFT_LightMap] && 
         !fd.features[MFT_ToneMap] &&
         !fd.features[MFT_VertLit] )
   {
      // If enabled we pass the position.
      res.numTexReg = 1;

      // If there isn't a bump map then we pass the
      // world space normal as well.
      if ( !fd.features[MFT_NormalMap] )
         res.numTexReg++;
   }

   return res;
}


//****************************************************************************
// Fog
//****************************************************************************

FogFeatHLSL::FogFeatHLSL()
   : mFogDep( "shaders/common/torque.hlsl" )
{
   addDependency( &mFogDep );
}

void FogFeatHLSL::processVert(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   const bool vertexFog = Con::getBoolVariable( "$useVertexFog", false );
   if ( vertexFog || GFX->getPixelShaderVersion() < 3.0 )
   {
      // Grab the eye position.
      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var( "eyePosWorld", "float3" );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }

      Var *fogData = new Var( "fogData", "float3" );
      fogData->uniform = true;
      fogData->constSortPos = cspPass;   

      Var *wsPosition = new Var( "fogPos", "float3" );
      getWsPosition( meta, new DecOp( wsPosition ) );

      // We pass the fog amount to the pixel shader.
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      Var *fogAmount = connectComp->getElement( RT_TEXCOORD );
      fogAmount->setName( "fogAmount" );
      fogAmount->setStructName( "OUT" );
      fogAmount->setType( "float" );
      fogAmount->mapsToSampler = false;

      meta->addStatement( new GenOp( "   @ = saturate( computeSceneFog( @, @, @.r, @.g, @.b ) );\r\n", 
         fogAmount, eyePos, wsPosition, fogData, fogData, fogData ) );
   }
   else
   {
      // We fog in world space... make sure the world space
      // position is passed to the pixel shader.  This is
      // often already passed for lighting, so it takes up
      // no extra output registers.
      addOutWsPosition( componentList, meta );
   }

   output = meta;
}

void FogFeatHLSL::processPix( Vector<ShaderComponent*> &componentList, 
                              const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   Var *fogColor = new Var;
   fogColor->setType( "float4" );
   fogColor->setName( "fogColor" );
   fogColor->uniform = true;
   fogColor->constSortPos = cspPass;

   // Get the out color.
   Var *color = (Var*) LangElement::find( "col" );
   if ( !color )
   {
      color = new Var;
      color->setType( "fragout" );
      color->setName( "col" );
      color->setStructName( "OUT" );
   }

   Var *fogAmount;

   const bool vertexFog = Con::getBoolVariable( "$useVertexFog", false );
   if ( vertexFog || GFX->getPixelShaderVersion() < 3.0 )
   {
      // Per-vertex.... just get the fog amount.
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      fogAmount = connectComp->getElement( RT_TEXCOORD );
      fogAmount->setName( "fogAmount" );
      fogAmount->setStructName( "IN" );
      fogAmount->setType( "float" );
   }
   else
   {
      Var *wsPosition = getInWsPosition( componentList );

      // grab the eye position
      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var( "eyePosWorld", "float3" );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }

      Var *fogData = new Var( "fogData", "float3" );
      fogData->uniform = true;
      fogData->constSortPos = cspPass;   

      /// Get the fog amount.
      fogAmount = new Var( "fogAmount", "float" );
      meta->addStatement( new GenOp( "   @ = saturate( computeSceneFog( @, @, @.r, @.g, @.b ) );\r\n", 
         new DecOp( fogAmount ), eyePos, wsPosition, fogData, fogData, fogData ) );
   }

   // Lerp between the fog color and diffuse color.
   LangElement *fogLerp = new GenOp( "lerp( @, @, @ )", fogColor, color, fogAmount );
   meta->addStatement( new GenOp( "   @ = @;\r\n", color, fogLerp ) );

   output = meta;
}

ShaderFeature::Resources FogFeatHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res;
   res.numTexReg = 1;
   return res;
}


//****************************************************************************
// Visibility
//****************************************************************************

void VisibilityFeatHLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // create visibility var
   Var *visibility = new Var;
   visibility->setType( "float" );
   visibility->setName( "visibility" );
   visibility->uniform = true;
   visibility->constSortPos = cspPass;   

   // search for color var
   Var *color = (Var*) LangElement::find( "col" );

   // Looks like its going to be a multiline statement
   MultiLine * meta = new MultiLine;

   if( !color )
   {
      // create color var
      color = new Var;
      color->setType( "fragout" );
      color->setName( "col" );
      color->setStructName( "OUT" );

      // link it to ConnectData.shading
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      Var *inColor = connectComp->getElement( RT_COLOR );
      inColor->setName( "shading" );
      inColor->setStructName( "IN" );
      inColor->setType( "float4" );

      meta->addStatement( new GenOp( "   @ = @;\r\n", color, inColor ) );
   }

   meta->addStatement( new GenOp( "   @.w *= @;\r\n", color, visibility ) );

   output = meta;
}


//****************************************************************************
// ColorMultiply
//****************************************************************************

void ColorMultiplyFeatHLSL::processPix(   Vector<ShaderComponent*> &componentList,
                                          const MaterialFeatureData &fd )
{
   Var *colorMultiply  = new Var;
   colorMultiply->setType( "float4" );
   colorMultiply->setName( "colorMultiply" );
   colorMultiply->uniform = true;
   colorMultiply->constSortPos = cspPass;   

   // search for color var
   Var *color = (Var*) LangElement::find( "col" );
   if (color)
   {
      MultiLine* meta = new MultiLine;
      LangElement* statement = new GenOp("lerp(@.rgb, @.rgb, @.a)", color, colorMultiply, colorMultiply);
      meta->addStatement(new GenOp("   @.rgb = @;\r\n", color, statement));
      output = meta;
   }   
}


//****************************************************************************
// AlphaTest
//****************************************************************************

void AlphaTestHLSL::processPix(  Vector<ShaderComponent*> &componentList,
                                 const MaterialFeatureData &fd )
{
   // If we're below SM3 and don't have a depth output
   // feature then don't waste an instruction here.
   if ( GFX->getPixelShaderVersion() < 3.0 &&
        !fd.features[ MFT_EyeSpaceDepthOut ]  &&
        !fd.features[ MFT_DepthOut ] )
   {
      output = NULL;
      return;
   }

   // If we don't have a color var then we cannot do an alpha test.
   Var *color = (Var*)LangElement::find( "col" );
   if ( !color )
   {
      output = NULL;
      return;
   }

   // Now grab the alpha test value.
   Var *alphaTestVal  = new Var;
   alphaTestVal->setType( "float" );
   alphaTestVal->setName( "alphaTestValue" );
   alphaTestVal->uniform = true;
   alphaTestVal->constSortPos = cspPrimitive;

   // Do the clip.
   output = new GenOp( "   clip( @.a - @ );\r\n", color, alphaTestVal );
}


//****************************************************************************
// GlowMask
//****************************************************************************

void GlowMaskHLSL::processPix(   Vector<ShaderComponent*> &componentList,
                                 const MaterialFeatureData &fd )
{
   output = NULL;

   // Get the output color... and make it black to mask out 
   // glow passes rendered before us.
   //
   // The shader compiler will optimize out all the other
   // code above that doesn't contribute to the alpha mask.
   Var *color = (Var*)LangElement::find( "col" );
   if ( color )
      output = new GenOp( "   @.rgb = 0;\r\n", color );
}


//****************************************************************************
// RenderTargetZero
//****************************************************************************

void RenderTargetZeroHLSL::processPix( Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd )
{
   // Do not actually assign zero, but instead a number so close to zero it may as well be zero.
   // This will prevent a divide by zero causing an FP special on float render targets
   output = new GenOp( "   @;\r\n", assignColor( new GenOp( "0.00001" ), Material::None, NULL, mOutputTargetMask ) );
}


//****************************************************************************
// HDR Output
//****************************************************************************

HDROutHLSL::HDROutHLSL()
   : mTorqueDep( "shaders/common/torque.hlsl" )
{
   addDependency( &mTorqueDep );
}

void HDROutHLSL::processPix(  Vector<ShaderComponent*> &componentList,
                              const MaterialFeatureData &fd )
{
   // Let the helper function do the work.
   Var *color = (Var*)LangElement::find( "col" );
   output = new GenOp( "   hdrEncode( @ );\r\n", color );
}

//****************************************************************************
// FoliageFeatureHLSL
//****************************************************************************

FoliageFeatureHLSL::FoliageFeatureHLSL()
: mDep( "shaders/common/foliage.hlsl" )
{
   addDependency( &mDep );
}

void FoliageFeatureHLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                      const MaterialFeatureData &fd )
{      
   // Get the input variables we need.

   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );
      
   Var *inColor = (Var*)LangElement::find( "diffuse" );   
   
   Var *inParams = (Var*)LangElement::find( "texCoord" );   

   MultiLine *meta = new MultiLine;

   // Declare the normal and tangent variables since they do not exist
   // in this vert type, but we do need to set them up for others.

   Var *normal = new Var;
   normal->setType( "float3" );
   normal->setName( "normal" );
   LangElement *normalDec = new DecOp( normal );
   meta->addStatement( new GenOp( "   @;\n", normalDec ) );   

   Var *tangent = new Var;
   tangent->setType( "float3" );
   tangent->setName( "T" );
   LangElement *tangentDec = new DecOp( tangent );
   meta->addStatement( new GenOp( "   @;\n", tangentDec ) );         

   // All actual work is offloaded to this method.
   meta->addStatement( new GenOp( "   foliageProcessVert( @, @, @, @, @ );\r\n", inPosition, inColor, inParams, normal, tangent ) );

   output = meta;
}

void FoliageFeatureHLSL::determineFeature( Material *material, const GFXVertexFormat *vertexFormat, U32 stageNum, const FeatureType &type, const FeatureSet &features, MaterialFeatureData *outFeatureData )
{      
   outFeatureData->features.addFeature( type );
}