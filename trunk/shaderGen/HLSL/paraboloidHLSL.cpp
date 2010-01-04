//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "shaderGen/hlsl/paraboloidHLSL.h"

#include "lighting/lightInfo.h"
#include "materials/sceneData.h"
#include "materials/materialFeatureTypes.h"
#include "gfx/gfxShader.h"


void ParaboloidVertTransformHLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                                const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   // First check for an input position from a previous feature
   // then look for the default vertex position.
   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );

   const bool isSinglePass = fd.features[ MFT_IsSinglePassParaboloid ];

   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // Grab connector out position.
   Var *outPosition = connectComp->getElement( RT_POSITION );
   outPosition->setName( "hpos" );
   outPosition->setStructName( "OUT" );

   // Get the atlas scale.
   Var *atlasScale = new Var;
   atlasScale->setType( "float2" );
   atlasScale->setName( "atlasScale" );
   atlasScale->uniform = true;
   atlasScale->constSortPos = cspPrimitive;  

   // Transform into camera space
   Var *worldViewOnly = new Var;
   worldViewOnly->setType( "float4x4" );
   worldViewOnly->setName( "worldViewOnly" );
   worldViewOnly->uniform = true;
   worldViewOnly->constSortPos = cspPrimitive;

   // So what we're doing here is transforming into camera space, and
   // then directly manipulate into shadowmap space.
   //
   // http://www.gamedev.net/reference/articles/article2308.asp

   // Swizzle z and y post-transform
   meta->addStatement( new GenOp( "   @ = mul(@, float4(@.xyz,1)).xzyw;\r\n", outPosition, worldViewOnly, inPosition ) );
   meta->addStatement( new GenOp( "   float L = length(@.xyz);\r\n", outPosition ) ); 

   if ( isSinglePass )
   {
      // Flip the z in the back case
      Var *outIsBack = connectComp->getElement( RT_TEXCOORD );
      outIsBack->setType( "float" );
      outIsBack->setName( "isBack" );
      outIsBack->setStructName( "OUT" );

      meta->addStatement( new GenOp( "   bool isBack = @.z < 0.0;\r\n", outPosition ) ); 
      meta->addStatement( new GenOp( "   @ = isBack ? -1.0 : 1.0;\r\n", outIsBack ) ); 
      meta->addStatement( new GenOp( "   if ( isBack ) @.z = -@.z;\r\n", outPosition, outPosition ) );
   }

   meta->addStatement( new GenOp( "   @ /= L;\r\n", outPosition ) ); 
   meta->addStatement( new GenOp( "   @.z = @.z + 1.0;\r\n", outPosition, outPosition ) ); 
   meta->addStatement( new GenOp( "   @.xy /= @.z;\r\n", outPosition, outPosition ) ); 

   // Get the light parameters.
   Var *lightParams = new Var;
   lightParams->setType( "float4" );
   lightParams->setName( "lightParams" );
   lightParams->uniform = true;
   lightParams->constSortPos = cspPrimitive;  

   // TODO: If we change other shadow shaders to write out
   // linear depth, than fix this as well!
   //
   // (L - zNear)/(lightParams.x - zNear);
   //
   meta->addStatement( new GenOp( "   @.z = L / @.x;\r\n", outPosition, lightParams ) ); 
   meta->addStatement( new GenOp( "   @.w = 1.0;\r\n", outPosition ) ); 

   // Pass unmodified to pixel shader to allow it to clip properly.
   Var *outPosXY = connectComp->getElement( RT_TEXCOORD );
   outPosXY->setType( "float2" );
   outPosXY->setName( "posXY" );
   outPosXY->setStructName( "OUT" );
   meta->addStatement( new GenOp( "   @ = @.xy;\r\n", outPosXY, outPosition ) ); 

   // Scale and offset so it shows up in the atlas properly.
   meta->addStatement( new GenOp( "   @.xy *= @.xy;\r\n", outPosition, atlasScale ) ); 

   if ( isSinglePass )
      meta->addStatement( new GenOp( "   @.x += isBack ? 0.5 : -0.5;\r\n", outPosition ) );
   else
   {
      Var *atlasOffset = new Var;
      atlasOffset->setType( "float2" );
      atlasOffset->setName( "atlasXOffset" );
      atlasOffset->uniform = true;
      atlasOffset->constSortPos = cspPrimitive;  

      meta->addStatement( new GenOp( "   @.xy += @;\r\n", outPosition, atlasOffset ) );
   }

   output = meta;
}

void ParaboloidVertTransformHLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                                const MaterialFeatureData &fd )
{      
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   MultiLine *meta = new MultiLine;

   const bool isSinglePass = fd.features[ MFT_IsSinglePassParaboloid ];
   if ( isSinglePass )
   {
      // Cull things on the back side of the map.
      Var *isBack = connectComp->getElement( RT_TEXCOORD );
      isBack->setName( "isBack" );
      isBack->setStructName( "IN" );
      isBack->setType( "float" );
      meta->addStatement( new GenOp( "   clip( abs( @ ) - 0.999 );\r\n", isBack ) );
   }

   // Cull pixels outside of the valid paraboloid.
   Var *posXY = connectComp->getElement( RT_TEXCOORD );
   posXY->setName( "posXY" );
   posXY->setStructName( "IN" );
   posXY->setType( "float2" );
   meta->addStatement( new GenOp( "   clip( 1.0 - abs(@.x) );\r\n", posXY ) );

   output = meta;
}

ShaderFeature::Resources ParaboloidVertTransformHLSL::getResources( const MaterialFeatureData &fd )
{
   Resources temp;
   temp.numTexReg = 2;
   return temp; 
}
