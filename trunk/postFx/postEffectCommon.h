//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _POSTEFFECTCOMMON_H_
#define _POSTEFFECTCOMMON_H_


/// 
enum PFXRenderTime
{
   /// Before a RenderInstManager bin.
   PFXBeforeBin,

   /// After a RenderInstManager bin.
   PFXAfterBin,

   /// After the diffuse rendering pass.
   PFXAfterDiffuse,

   /// When the end of the frame is reached.
   PFXEndOfFrame,

   /// This PostEffect is not processed by the manager.
   /// It will generate its texture when it is requested.
   PFXTexGenOnDemand
};

/// PFXTargetClear specifies whether and how
/// often a given PostEffect's target will be cleared.
enum PFXTargetClear
{
   /// Never clear the PostEffect target.
   PFXTargetClear_None,

   /// Clear once on create.
   PFXTargetClear_OnCreate,

   /// Clear before every draw.
   PFXTargetClear_OnDraw,
};

///
struct PFXFrameState
{
   MatrixF worldToCamera;
   MatrixF cameraToScreen;

   PFXFrameState() 
      :  worldToCamera( true ),
         cameraToScreen( true )
   {
   }
};

///
GFX_DeclareTextureProfile( PostFxTextureProfile );

///
GFXDeclareVertexFormat( PFXVertex )
{
   /// xyz position.
   Point3F point;

   /// The screen space texture coord.
   Point2F texCoord;

   /// 
   Point3F wsEyeRay;
};

#endif // _POSTEFFECTCOMMON_H_