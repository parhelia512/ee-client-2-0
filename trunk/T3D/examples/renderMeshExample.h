//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _RENDERMESHEXAMPLE_H_
#define _RENDERMESHEXAMPLE_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif
#ifndef _GFXPRIMITIVEBUFFER_H_
#include "gfx/gfxPrimitiveBuffer.h"
#endif

//-----------------------------------------------------------------------------
// This class implements a basic SceneObject that can exist in the world at a
// 3D position and render itself. There are several valid ways to render an
// object in Torque. This class implements the preferred rendering method which
// is to submit a MeshRenderInst along with a Material, vertex buffer,
// primitive buffer, and transform and allow the RenderMeshMgr handle the
// actual setup and rendering for you.
//-----------------------------------------------------------------------------

class RenderMeshExample : public SceneObject
{
   typedef SceneObject Parent;

   // Networking masks
   // We need to implement a mask specifically to handle
   // updating our transform from the server object to its
   // client-side "ghost". We also need to implement a
   // maks for handling editor updates to our properties
   // (like material).
   enum MaskBits 
   {
      TransformMask = Parent::NextFreeMask << 0,
      UpdateMask    = Parent::NextFreeMask << 1,
      NextFreeMask  = Parent::NextFreeMask << 2
   };

   //--------------------------------------------------------------------------
   // Rendering variables
   //--------------------------------------------------------------------------
   // The name of the Material we will use for rendering
   String            mMaterialName;
   // The actual Material instance
   BaseMatInstance*  mMaterialInst;

   // Define our vertex format here so we don't have to
   // change it in multiple spots later
   typedef GFXVertexPNT VertexType;

   // The GFX vertex and primitive buffers
   GFXVertexBufferHandle< VertexType > mVertexBuffer;
   GFXPrimitiveBufferHandle            mPrimitiveBuffer;

public:
   RenderMeshExample();
   virtual ~RenderMeshExample();

   // Declare this object as a ConsoleObject so that we can
   // instantiate it into the world and network it
   DECLARE_CONOBJECT(RenderMeshExample);

   //--------------------------------------------------------------------------
   // Object Editing
   // Since there is always a server and a client object in Torque and we
   // actually edit the server object we need to implement some basic
   // networking functions
   //--------------------------------------------------------------------------
   // Set up any fields that we want to be editable (like position)
   static void initPersistFields();

   // Allows the object to update its editable settings
   // from the server object to the client
   virtual void inspectPostApply();

   // Handle when we are added to the scene and removed from the scene
   bool onAdd();
   void onRemove();

   // Override this so that we can dirty the network flag when it is called
   void setTransform( const MatrixF &mat );

   // This function handles sending the relevant data from the server
   // object to the client object
   U32 packUpdate( NetConnection *conn, U32 mask, BitStream *stream );
   // This function handles receiving relevant data from the server
   // object and applying it to the client object
   void unpackUpdate( NetConnection *conn, BitStream *stream );

   //--------------------------------------------------------------------------
   // Object Rendering
   // Torque utilizes a "batch" rendering system. This means that it builds a
   // list of objects that need to render (via RenderInst's) and then renders
   // them all in one batch. This allows it to optimized on things like
   // minimizing texture, state, and shader switching by grouping objects that
   // use the same Materials.
   //--------------------------------------------------------------------------
   // Create the geometry for rendering
   void createGeometry();

   // Get the Material instance
   void updateMaterial();

   // This is the function that allows this object to submit itself for rendering
   bool prepRenderImage( SceneState *state, const U32 stateKey, 
                         const U32 startZone, const bool modifyBaseZoneState = false);
};

#endif // _RENDERMESHEXAMPLE_H_