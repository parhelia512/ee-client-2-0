//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "shaderGen/GLSL/depthGLSL.h"

#include "materials/materialFeatureTypes.h"


void EyeSpaceDepthOutGLSL::processVert(   Vector<ShaderComponent*> &componentList, 
                                          const MaterialFeatureData &fd )
{
   // grab incoming vert position
   Var *inPosition = (Var*) LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*) LangElement::find( "position" );

   AssertFatal( inPosition, "Something went bad with ShaderGen. The position should be already defined." );

   // grab output
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outWSEyeVec = connectComp->getElement( RT_TEXCOORD );
   outWSEyeVec->setName( "outWSEyeVec" );

   // create modelview variable
   Var *objToWorld = (Var*) LangElement::find( "objTrans" );
   if( !objToWorld )
   {
      objToWorld = new Var;
      objToWorld->setType( "mat4x4" );
      objToWorld->setName( "objTrans" );
      objToWorld->uniform = true;
      objToWorld->constSortPos = cspPrimitive;   
   }

   Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
   if( !eyePos )
   {
      eyePos = new Var;
      eyePos->setType("vec3");
      eyePos->setName("eyePosWorld");
      eyePos->uniform = true;
      eyePos->constSortPos = cspPass;
   }

   LangElement *statement = new GenOp( "   @ = vec4(@ * vec4(@.xyz,1)) - vec4(@, 0.0);\r\n", 
      outWSEyeVec, objToWorld, inPosition, eyePos );

   output = statement;
}

void EyeSpaceDepthOutGLSL::processPix( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{      
   MultiLine *meta = new MultiLine;

   // grab connector position
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *wsEyeVec = connectComp->getElement( RT_TEXCOORD );
   wsEyeVec->setName( "outWSEyeVec" );
   wsEyeVec->setType( "vec4" );
   wsEyeVec->mapsToSampler = false;
   wsEyeVec->uniform = false;

   // get shader constants
   Var *vEye = new Var;
   vEye->setType("vec3");
   vEye->setName("vEye");
   vEye->uniform = true;
   vEye->constSortPos = cspPass;

   // Expose the depth to the depth format feature
   Var *depthOut = new Var;
   depthOut->setType("float");
   depthOut->setName(getOutputVarName());

   LangElement *depthOutDecl = new DecOp( depthOut );

   meta->addStatement( new GenOp( "   @ = dot(@, (@.xyz / @.w));\r\n", depthOutDecl, vEye, wsEyeVec, wsEyeVec ) );

   // If there isn't an output conditioner for the pre-pass, than just write
   // out the depth to rgba and return.
   if( !fd.features[MFT_PrePassConditioner] )
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( new GenOp( "vec4(@)", depthOut ), Material::None ) ) );
   
   output = meta;
}

ShaderFeature::Resources EyeSpaceDepthOutGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources temp;

   // Passing from VS->PS:
   // - world space position (wsPos)
   temp.numTexReg = 1; 

   return temp; 
}


void DepthOutGLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // Grab the output vert.
   Var *outPosition = (Var*)LangElement::find( "gl_Position" );

   // Grab our output depth.
   Var *outDepth = connectComp->getElement( RT_TEXCOORD );
   outDepth->setName( "outDepth" );
   outDepth->setType( "float" );

   output = new GenOp( "   @ = @.z / @.w;\r\n", outDepth, outPosition, outPosition );
}

void DepthOutGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // grab connector position
   Var *depthVar = connectComp->getElement( RT_TEXCOORD );
   depthVar->setName( "outDepth" );
   depthVar->setType( "float" );
   depthVar->mapsToSampler = false;
   depthVar->uniform = false;

   /*
   // Expose the depth to the depth format feature
   Var *depthOut = new Var;
   depthOut->setType("float");
   depthOut->setName(getOutputVarName());
   */

   LangElement *depthOut = new GenOp( "vec4( @, @ * @, 0, 1 )", depthVar, depthVar, depthVar );

   output = new GenOp( "   @;\r\n", assignColor( depthOut, Material::None ) );
}

ShaderFeature::Resources DepthOutGLSL::getResources( const MaterialFeatureData &fd )
{
   // We pass the depth to the pixel shader.
   Resources temp;
   temp.numTexReg = 1; 

   return temp; 
}
