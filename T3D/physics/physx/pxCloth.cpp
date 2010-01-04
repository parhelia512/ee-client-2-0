//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/physics/physX/pxCloth.h"

#include "console/consoleTypes.h"
#include "sceneGraph/sceneState.h"
#include "renderInstance/renderPassManager.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physx/pxWorld.h"
#include "T3D/physics/physx/pxStream.h"
#include "gfx/gfxDrawUtil.h"
#include "math/mathIO.h"
#include "core/stream/bitStream.h"
#include "materials/materialManager.h"

IMPLEMENT_CO_NETOBJECT_V1( PxCloth );

static EnumTable::Enums gAttachmentFlagEnums[] =
{   
   { 0, "Bottom Right" },
   { 1, "Bottom Left" },
   { 2, "Top Right" },
   { 3, "Top Left" },
   { 4, "Top Center" },
   { 5, "Bottom Center" },
   { 6, "Right Center" },
   { 7, "Left Center" },
   { 8, "Top Edge" },
   { 9, "Bottom Edge" },
   { 10, "Right Edge" },
   { 11, "Left Edge" }
};
EnumTable PxCloth::mAttachmentFlagTable( 12, gAttachmentFlagEnums );

PxCloth::PxCloth()
 : mWorld( NULL ),
   mScene( NULL )
{
   mMaterialName = "wooden_beams";
   mMatInst = NULL;
   mMaterial = NULL;

   mVertexRenderBuffer = NULL; 
   mIndexRenderBuffer = NULL;

   mMaxVertices = 0;
   mMaxIndices = 0;
  
   mTeared = false;
   mIsDummy = false;

   mClothMesh = NULL;
   mCloth = NULL;

   mPatchSamples.set( 8.0f, 8.0f );
   mPatchSize.set( 8.0f, 8.0f );

   mNetFlags.set( Ghostable | ScopeAlways );
   mTypeMask |= StaticObjectType | StaticTSObjectType | StaticRenderedObjectType | ShadowCasterObjectType;

   mClothMeshDesc.setToDefault();
   mClothDesc.setToDefault();
	  mReceiveBuffers.setToDefault();

   mBendingEnabled = false;
   mDampingEnabled = false;
   mTriangleCollisionEnabled = false;
   mSelfCollisionEnabled = false;

   mRecreatePending = false;

   mDensity = 1.0f;
   mThickness = 0.1f;
   mFriction = 0.25f;
   mBendingStiffness = 0.5f;
   mStretchingStiffness = 0.5f;
   mDampingCoefficient = 0.25f;
   mCollisionResponseCoefficient = 1.0f;
   mAttachmentResponseCoefficient = 1.0f;

   mAttachmentMask = 0;
}

PxCloth::~PxCloth()
{
}

bool PxCloth::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   mIsDummy = isClientObject() && gPhysicsPlugin->isSinglePlayer();
   String worldName = isServerObject() ? "server" : "client";

   // SinglePlayer objects only have server-side physics representations.
   if ( mIsDummy )
      worldName = "server";

   mWorld = dynamic_cast<PxWorld*>( gPhysicsPlugin->getWorld( worldName ) );

   if ( !mWorld || !mWorld->getScene() )
   {
      Con::errorf( "PxCloth::onAdd() - PhysXWorld not initialized!" );
      return false;
   }

   mScene = mWorld->getScene();
 
   Point3F halfScale = Point3F::One * 0.5f;
   mObjBox.minExtents = -halfScale;
   mObjBox.maxExtents = halfScale;
   resetWorldBox();

   if ( !mIsDummy )
   {
      _initClothMesh(); 
      _initReceiveBuffers( mClothMeshDesc.numVertices, mClothMeshDesc.numTriangles );
      _createClothPatch( getTransform() );
      _setupAttachments();
      mResetXfm = getTransform();
   }

   if ( !mIsDummy )
      PhysicsPlugin::getPhysicsResetSignal().notify( this, &PxCloth::onPhysicsReset, 1053.0f );

   addToScene();

   return true;
}

void PxCloth::onRemove()
{   
   SAFE_DELETE( mMatInst );

   mWorld->getPhysicsResults();

   if ( mCloth && !mIsDummy )
      mWorld->releaseCloth( *mCloth );

   if ( mClothMesh && !mIsDummy )
      mWorld->releaseClothMesh( *mClothMesh );
   
   mCloth = NULL;
   mClothMesh = NULL;

   if ( !mIsDummy )
   {
      delete [] mVertexRenderBuffer;
      delete [] mIndexRenderBuffer;
   }

   removeFromScene();

   if ( !mIsDummy )
      PhysicsPlugin::getPhysicsResetSignal().remove( this, &PxCloth::onPhysicsReset );

   Parent::onRemove();
}

void PxCloth::onPhysicsReset( PhysicsResetEvent reset )
{ 
   PxCloth *serverObj = NULL;
   if ( isServerObject() )
      serverObj = this;
   else
      serverObj = static_cast<PxCloth*>( mServerObject.getObject() );

   if ( !serverObj )
      return;

   // Store the reset transform for later use.
   if ( reset == PhysicsResetEvent_Store )
   {
      serverObj->mResetXfm = serverObj->getTransform();
      mRecreatePending = true;
   }  
   else if ( reset == PhysicsResetEvent_Restore )
      mRecreatePending = true;
}

void PxCloth::initPersistFields()
{
   Parent::initPersistFields();

   addField( "material", TypeMaterialName, Offset( mMaterialName, PxCloth ) );
   addField( "samples", TypePoint2F, Offset( mPatchSamples, PxCloth ) );
   addField( "size", TypePoint2F, Offset( mPatchSize, PxCloth ) );

   addField( "bending", TypeBool, Offset( mBendingEnabled, PxCloth ) );
   addField( "damping", TypeBool, Offset( mDampingEnabled, PxCloth ) );
   addField( "triangleCollision", TypeBool, Offset( mTriangleCollisionEnabled, PxCloth ) );
   addField( "selfCollision", TypeBool, Offset( mSelfCollisionEnabled, PxCloth ) );

   addField( "density", TypeF32, Offset( mDensity, PxCloth ) );
   addField( "thickness", TypeF32, Offset( mThickness, PxCloth ) );
   addField( "friction", TypeF32, Offset( mFriction, PxCloth ) );

   addField( "bendingStiffness", TypeF32, Offset( mBendingStiffness, PxCloth ) );
   addField( "stretchingStiffness", TypeF32, Offset( mStretchingStiffness, PxCloth ) );
   
   addField( "dampingCoefficient", TypeF32, Offset( mDampingCoefficient, PxCloth ) );
   addField( "collisionResponseCoefficient", TypeF32, Offset( mCollisionResponseCoefficient, PxCloth ) );
   addField( "attachmentResponseCoefficient", TypeF32, Offset( mAttachmentResponseCoefficient, PxCloth ) );

   addField( "attachments", TypeBitMask32, Offset( mAttachmentMask, PxCloth ), 1, &mAttachmentFlagTable );
}

void PxCloth::inspectPostApply()
{
   Parent::inspectPostApply();

   setMaskBits( UpdateMask );
   mRecreatePending = true;
}

U32 PxCloth::packUpdate( NetConnection *conn, U32 mask, BitStream *stream )
{
   U32 retMask = Parent::packUpdate( conn, mask, stream );

   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      mathWrite( *stream, getTransform() );
      mathWrite( *stream, getScale() );
      mathWrite( *stream, mPatchSamples );
      mathWrite( *stream, mPatchSize );

      stream->write( mMaterialName );

      stream->writeFlag( mBendingEnabled );
      stream->writeFlag( mDampingEnabled );
      stream->writeFlag( mTriangleCollisionEnabled );
      stream->writeFlag( mSelfCollisionEnabled );
      
      stream->write( mDensity );
      stream->write( mThickness );
      stream->write( mFriction );
      stream->write( mBendingStiffness );
      stream->write( mStretchingStiffness );
      stream->write( mDampingCoefficient );
      stream->write( mCollisionResponseCoefficient );
      stream->write( mAttachmentResponseCoefficient );

      stream->write( mAttachmentMask );
   }

   return retMask;
}

void PxCloth::unpackUpdate( NetConnection *conn, BitStream *stream )
{
   Parent::unpackUpdate( conn, stream );

   // UpdateMask
   if ( stream->readFlag() )
   {
      MatrixF mat;
      mathRead( *stream, &mat );
      Point3F scale;
      mathRead( *stream, &scale );

      setScale( scale );
      setTransform( mat );

      mathRead( *stream, &mPatchSamples );
      mathRead( *stream, &mPatchSize );

      stream->read( &mMaterialName );
      _updateMaterial();

      mBendingEnabled = stream->readFlag();
      mDampingEnabled = stream->readFlag();
      mTriangleCollisionEnabled = stream->readFlag();
      mSelfCollisionEnabled = stream->readFlag();
      
      stream->read( &mDensity );
      stream->read( &mThickness );
      stream->read( &mFriction );
      stream->read( &mBendingStiffness );
      stream->read( &mStretchingStiffness );
      stream->read( &mDampingCoefficient );
      stream->read( &mCollisionResponseCoefficient );
      stream->read( &mAttachmentResponseCoefficient );

      stream->read( &mAttachmentMask );
   }
}

void PxCloth::_recreateCloth( const MatrixF &transform )
{
   if ( !mWorld )
      return;

   mWorld->getPhysicsResults();

   if ( mCloth )
   {
      mWorld->releaseCloth( *mCloth );
      mCloth = NULL;
   }

   if ( mClothMesh )
   {
      mWorld->releaseClothMesh( *mClothMesh );
      mClothMesh = NULL;
   }

   // TODO: We don't need to recreate the mesh if just
   // a parameter of the cloth was changed.
   _initClothMesh();

   _initReceiveBuffers( mClothMeshDesc.numVertices, mClothMeshDesc.numTriangles );

   _createClothPatch( transform );

   _setupAttachments();
}

void PxCloth::_setClothFromServer( PxCloth *serverObj )
{
   mCloth = serverObj->mCloth;
   mClothMesh = serverObj->mClothMesh;

   mClothDesc = serverObj->mClothDesc;
   mClothMeshDesc = serverObj->mClothMeshDesc;

   mReceiveBuffers = serverObj->mReceiveBuffers;

   mVertexRenderBuffer = serverObj->mVertexRenderBuffer; 
   mIndexRenderBuffer = serverObj->mIndexRenderBuffer;

   mNumVertices = serverObj->mNumVertices;
   mNumIndices = serverObj->mNumIndices;
   mMaxVertices = serverObj->mMaxVertices;
   mMaxIndices = serverObj->mMaxIndices;
}

void PxCloth::setTransform( const MatrixF &mat )
{
   Parent::setTransform( mat );

   if ( isServerObject() )
      mRecreatePending = true;
   else
   {
      PxCloth *serverObj = static_cast<PxCloth*>( mServerObject.getObject() );

      if ( !serverObj )
         return;
   
      _setClothFromServer( serverObj );
   }  
}

void PxCloth::setScale( const VectorF &scale )
{
   Parent::setScale( scale );
}

bool PxCloth::prepRenderImage( SceneState *state,
                               const U32 stateKey,
                               const U32 startZone, 
                               const bool modifyBaseState )
{
   if ( isLastState(state, stateKey) || 
        !state->isObjectRendered(this) )
      return false;

   setLastState( state, stateKey );
   
   if ( mIsVBDirty )
      _updateVBIB();

   MeshRenderInst *ri = state->getRenderPass()->allocInst<MeshRenderInst>();

   // If this isn't the shadow pass then setup 
   // lights for the cloth mesh.
   if ( !state->isShadowPass() )
   {
      LightManager *lm = gClientSceneGraph->getLightManager();
      lm->setupLights( this, getWorldSphere() );
      lm->getBestLights( ri->lights, 8 );
      lm->resetLights();
   }

   ri->projection = state->getRenderPass()->allocSharedXform(RenderPassManager::Projection);
  
   if ( mNumIndices > 0 )
      ri->objectToWorld = &MatrixF::Identity;
   else
      ri->objectToWorld = state->getRenderPass()->allocUniqueXform( getTransform() );

   ri->worldToCamera = state->getRenderPass()->allocSharedXform(RenderPassManager::View);   
   ri->type = RenderPassManager::RIT_Mesh;

   ri->primBuff = &mPrimBuffer;
   ri->vertBuff = &mVB;

   ri->matInst = mMatInst;

   ri->prim = state->getRenderPass()->allocPrim();
   ri->prim->type = GFXTriangleList;
   ri->prim->minIndex = 0;
   ri->prim->startIndex = 0;
   
   if ( mNumIndices > 0 )
      ri->prim->numPrimitives = mNumIndices / 3;
   else 
      ri->prim->numPrimitives = 2;

   ri->prim->startVertex = 0;

   if ( mNumVertices > 0 )
      ri->prim->numVertices = mNumVertices;
   else
      ri->prim->numVertices = 4;

   ri->defaultKey = (U32)ri->vertBuff;
   ri->defaultKey2 = 0;

   state->getRenderPass()->addInst( ri );

   return true;
}

void PxCloth::_initClothMesh()
{
   // Generate a uniform cloth patch, 
   // w and h are the width and height, 
   // d is the distance between vertices.  
   U32 numX = (U32)mPatchSamples.x + 1;    
   U32 numY = (U32)mPatchSamples.x + 1;    
   
   mClothMeshDesc.numVertices = (numX+1) * (numY+1);    
   mClothMeshDesc.numTriangles = numX*numY*2;   
   mClothMeshDesc.pointStrideBytes = sizeof(NxVec3);    
   mClothMeshDesc.triangleStrideBytes = 3*sizeof(NxU32);    
   mClothMeshDesc.points = (NxVec3*)dMalloc(sizeof(NxVec3)*mClothMeshDesc.numVertices);    
   mClothMeshDesc.triangles = (NxU32*)dMalloc(sizeof(NxU32)*mClothMeshDesc.numTriangles*3);    
   mClothMeshDesc.flags = 0;    

   U32 i,j;    
   NxVec3 *p = (NxVec3*)mClothMeshDesc.points;    
   
   F32 patchWidth = (F32)(mPatchSize.x / (F32)mPatchSamples.x);
   F32 patchHeight = (F32)(mPatchSize.y / (F32)mPatchSamples.y);

   for (i = 0; i <= numY; i++) 
   {        
      for (j = 0; j <= numX; j++) 
      {            
         p->set( patchWidth * j, 0.0f, patchHeight * i );     
         p++;
      }    
   }    
   
   NxU32 *id = (NxU32*)mClothMeshDesc.triangles;    
   
   for (i = 0; i < numY; i++) 
   {        
      for (j = 0; j < numX; j++) 
      {            
         NxU32 i0 = i * (numX+1) + j;            
         NxU32 i1 = i0 + 1;            
         NxU32 i2 = i0 + (numX+1);            
         NxU32 i3 = i2 + 1;            
         if ( (j+i) % 2 ) 
         {                
            *id++ = i0; 
            *id++ = i2; 
            *id++ = i1;                
            *id++ = i1; 
            *id++ = i2; 
            *id++ = i3;            
         }            
         else 
         {                
            *id++ = i0; 
            *id++ = i2; 
            *id++ = i3;                
            *id++ = i0; 
            *id++ = i3; 
            *id++ = i1;            
         }        
      }    
   }   

   NxInitCooking();

   // Ok... cook the mesh!
   NxCookingParams params;
   params.targetPlatform = PLATFORM_PC;
   params.skinWidth = 0.01f;
   params.hintCollisionSpeed = false;

   NxSetCookingParams( params );
  
   PxMemStream cooked;	
  
   if ( NxCookClothMesh( mClothMeshDesc, cooked ) )
   {
      cooked.resetPosition();
      mClothMesh = gPhysicsSDK->createClothMesh( cooked );
   }
   
   NxCloseCooking();
	  NxVec3 *ppoints = (NxVec3*)mClothMeshDesc.points;
	  NxU32 *triangs = (NxU32*)mClothMeshDesc.triangles;

   dFree( ppoints );
   dFree( triangs );
}

void PxCloth::_createClothPatch( const MatrixF &transform )
{
   mClothDesc.globalPose.setRowMajor44( transform );
   mClothDesc.thickness = mThickness;
   mClothDesc.density = mDensity;
   mClothDesc.bendingStiffness = mBendingStiffness;
   //clothDesc.stretchingStiffness = mStretchingStiffness;
   mClothDesc.dampingCoefficient = mDampingCoefficient;
   mClothDesc.friction = mFriction;
   mClothDesc.collisionResponseCoefficient = mCollisionResponseCoefficient;
   //clothDesc.attachmentResponseCoefficient = mAttachmentResponseCoefficient;
   //clothDesc.solverIterations = 5;
   //clothDesc.flags |= NX_CLF_STATIC;
   //clothDesc.flags |= NX_CLF_DISABLE_COLLISION;
   //clothDesc.flags |= NX_CLF_VISUALIZATION;
   //	clothDesc.flags &= ~NX_CLF_GRAVITY;
   if ( mBendingEnabled )
      mClothDesc.flags |= NX_CLF_BENDING;

   //clothDesc.flags |= NX_CLF_BENDING_ORTHO;
   
   if ( mDampingEnabled )
      mClothDesc.flags |= NX_CLF_DAMPING;
   //clothDesc.flags |= NX_CLF_COMDAMPING;
   if ( mTriangleCollisionEnabled )
      mClothDesc.flags |= NX_CLF_TRIANGLE_COLLISION;
   if ( mSelfCollisionEnabled )
      mClothDesc.flags |= NX_CLF_SELFCOLLISION;
   //clothDesc.flags |= NX_CLF_COLLISION_TWOWAY;

   mClothDesc.clothMesh = mClothMesh;    
   mClothDesc.meshData = mReceiveBuffers;

   if ( !mClothDesc.isValid() )
      return;

   mCloth = mScene->createCloth( mClothDesc );
   if ( !mCloth )
      return;

   NxBounds3 box;
   mCloth->getWorldBounds( box ); 

   Point3F min = pxCast<Point3F>( box.min );
   Point3F max = pxCast<Point3F>( box.max );

   mWorldBox.set( min, max );
   mObjBox = mWorldBox;

   getWorldTransform().mul( mObjBox );
   resetWorldBox();
}

void PxCloth::_initReceiveBuffers( U32 numVertices, U32 numTriangles )
{
   // here we setup the buffers through which the SDK returns the dynamic cloth data
   // we reserve more memory for vertices than the initial mesh takes
   // because tearing creates new vertices
   // the SDK only tears cloth as long as there is room in these buffers

   mMaxVertices = 3 * numVertices;
   mMaxIndices = 3 * numTriangles;

   mNumIndices = numTriangles;
   mNumVertices = numVertices;
   
   // Allocate Render Buffer for Vertices if it hasn't been done before
   delete [] mVertexRenderBuffer;
   mVertexRenderBuffer = new GFXVertexPNTT[mMaxVertices];

   delete [] mIndexRenderBuffer;
   mIndexRenderBuffer = new U16[mMaxIndices];

   mReceiveBuffers.verticesPosBegin = &(mVertexRenderBuffer[0].point);
   mReceiveBuffers.verticesNormalBegin = &(mVertexRenderBuffer[0].normal);
   mReceiveBuffers.verticesPosByteStride = sizeof(GFXVertexPNTT);
   mReceiveBuffers.verticesNormalByteStride = sizeof(GFXVertexPNTT);
   mReceiveBuffers.maxVertices = mMaxVertices;
   mReceiveBuffers.numVerticesPtr = &mNumVertices;

   // the number of triangles is constant, even if the cloth is torn
   mReceiveBuffers.indicesBegin = &mIndexRenderBuffer[0];
   mReceiveBuffers.indicesByteStride = sizeof(NxU16);
   mReceiveBuffers.maxIndices = mMaxIndices;
   mReceiveBuffers.numIndicesPtr = &mNumIndices;

   // Set up texture coords.

   U32 numX = (U32)(mPatchSamples.x) + 1;    
   U32 numY = (U32)(mPatchSamples.y) + 1; 

   F32 dx = 1.0f; if (numX > 0) dx /= numX;
   F32 dy = 1.0f; if (numY > 0) dy /= numY;

   F32 *coord = (F32*)&mVertexRenderBuffer[0].texCoord;
   for ( U32 i = 0; i <= numY; i++) 
   {
      for ( U32 j = 0; j <= numX; j++) 
      {
         coord[0] = j*dx;
         coord[1] = i*-dy;
         coord += sizeof( GFXVertexPNTT ) / sizeof( F32 );
      }
   }

   // the parent index information would be needed if we used textured cloth
   //mReceiveBuffers.parentIndicesBegin       = (U32*)malloc(sizeof(U32)*mMaxVertices);
   //mReceiveBuffers.parentIndicesByteStride  = sizeof(U32);
   //mReceiveBuffers.maxParentIndices         = mMaxVertices;
   //mReceiveBuffers.numParentIndicesPtr      = &mNumParentIndices;

   mReceiveBuffers.dirtyBufferFlagsPtr = &mMeshDirtyFlags;

   // init the buffers in case we want to draw the mesh 
   // before the SDK as filled in the correct values

   mReceiveBuffers.flags |= NX_MDF_16_BIT_INDICES;

   mMeshDirtyFlags = 0;
   mNumParentIndices = 0;
   mNumVertices = 0;
   mNumIndices = 0;
}

void PxCloth::_updateMaterial()
{
   if ( mMaterialName.isEmpty() )
      return;

   Material *pMat = NULL;
   if ( !Sim::findObject( mMaterialName, pMat ) )
   {
      Con::printf( "PxCloth::unpackUpdate, failed to find Material of name &s!", mMaterialName.c_str() );
      return;
   }

   mMaterial = pMat;

   // Only update material instance if we have one allocated.
   _initMaterial();
}

void PxCloth::_initMaterial()
{
   SAFE_DELETE( mMatInst );

   if ( mMaterial )
      mMatInst = mMaterial->createMatInstance();
   else
      mMatInst = MATMGR->createMatInstance( "WarningMaterial" );

   GFXStateBlockDesc desc;
   desc.setCullMode( GFXCullNone );
   mMatInst->addStateBlockDesc( desc );

   mMatInst->init( MATMGR->getDefaultFeatures(), getGFXVertexFormat<GFXVertexPNTT>() );
}

void PxCloth::_updateVBIB()
{
   PROFILE_SCOPE( PxCloth_UpdateVBIB );

   if ( mIsDummy )
   {
      PxCloth *serverObj = static_cast<PxCloth*>( mServerObject.getObject() );
      if ( serverObj )
         _setClothFromServer( serverObj );
   }
   
   mIsVBDirty = false;

   if ( !mNumIndices )
   {
      _alternateUpdateVBIB();
      return;
   }

   // Don't set the VB if the vertex count is the same!
   if ( mVB.isNull() || mVB->mNumVerts < mNumVertices )
      mVB.set( GFX, mNumVertices, GFXBufferTypeDynamic );

   GFXVertexPNTT *vert = mVertexRenderBuffer;
   GFXVertexPNTT *secondVert = NULL;

   for ( U32 i = 0; i < mNumVertices; i++ )
   {
      if ( i % (U32)mPatchSize.x == 0 && i != 0 )
      {
         secondVert = vert;
         secondVert--;
         vert->tangent = -(vert->point - secondVert->point);
      }
      else
      {      
         secondVert = vert;
         secondVert++;
         vert->tangent = vert->point - secondVert->point;
      }

      vert->tangent.normalize();
      vert++;
   }

   GFXVertexPNTT *vpPtr = mVB.lock();
   dMemcpy( vpPtr, mVertexRenderBuffer, sizeof( GFXVertexPNTT ) * mNumVertices );
   mVB.unlock();

   if ( mPrimBuffer.isNull() || mPrimBuffer->mIndexCount < mNumIndices )
      mPrimBuffer.set( GFX, mNumIndices, 0, GFXBufferTypeDynamic );

   U16 *pbPtr;
   mPrimBuffer.lock( &pbPtr );
   dMemcpy( pbPtr, mIndexRenderBuffer, sizeof( U16 ) * mNumIndices );
   mPrimBuffer.unlock();
}

void PxCloth::_alternateUpdateVBIB()
{
   if ( mVB.isNull() || mVB->mNumVerts < 4 )
      mVB.set( GFX, 4, GFXBufferTypeDynamic );

   GFXVertexPNTT *vpPtr = mVB.lock();

   vpPtr[0].point.set( 0, 0, mPatchSize.y + 1.0f );
   vpPtr[1].point.set( mPatchSize.x + 1.0f, 0, mPatchSize.y + 1.0f );
   vpPtr[2].point.set( 0, 0, 0 );
   vpPtr[3].point.set( mPatchSize.x + 1.0f, 0, 0 );

   vpPtr[0].normal.set( getTransform().getForwardVector() );
   vpPtr[1].normal.set( getTransform().getForwardVector() );
   vpPtr[2].normal.set( getTransform().getForwardVector() );
   vpPtr[3].normal.set( getTransform().getForwardVector() );

   vpPtr[0].texCoord.set( 0, -1.0f );
   vpPtr[1].texCoord.set( 1.0f, -1.0f );
   vpPtr[2].texCoord.set( 0, 0 );
   vpPtr[3].texCoord.set( 1.0f, 0 );

   vpPtr[0].tangent.set( getTransform().getRightVector() );
   vpPtr[1].tangent.set( getTransform().getRightVector() );
   vpPtr[2].tangent.set( getTransform().getRightVector() );
   vpPtr[3].tangent.set( getTransform().getRightVector() );

   mVB.unlock();

   if ( mPrimBuffer.isNull() || mPrimBuffer->mIndexCount < 6 )
      mPrimBuffer.set( GFX, 6, 0, GFXBufferTypeDynamic );

   U16 *pbPtr;
   mPrimBuffer.lock( &pbPtr );

   pbPtr[0] = 0;
   pbPtr[1] = 1;
   pbPtr[2] = 2;
   pbPtr[3] = 2;
   pbPtr[4] = 1;
   pbPtr[5] = 3;

   mPrimBuffer.unlock();
}

void PxCloth::processTick( const Move *move )
{
   PxCloth *serverObj = static_cast<PxCloth*>( mServerObject.getObject() );
   if ( serverObj )
      _setClothFromServer( serverObj );
   else if ( !serverObj && mIsDummy )
      mCloth = NULL;
  
   if ( mRecreatePending && !mIsDummy )
   {
      mRecreatePending = false;
      _recreateCloth( mResetXfm );
   }

   if ( !mCloth )
      return;

   if ( mWorld->isWritable() && Con::getBoolVariable( "$PxCloth::enableWind", false ) )
   {
      NxVec3 windVec(   25.0f + NxMath::rand(-5.0f, 5.0f),
			                     NxMath::rand(-5.0f, 5.0f),
			                     NxMath::rand(-5.0f, 5.0f) );

      mCloth->setWindAcceleration( windVec );

      // Wake the cloth!
      mCloth->wakeUp();
   }
   else if ( mWorld->isWritable() && !Con::getBoolVariable( "$PxCloth::enableWind", false ) )
      mCloth->setWindAcceleration( NxVec3( 0, 0, 0 ) );

   // Update bounds.
   if ( mWorld->getEnabled() )
   {
      NxBounds3 box;
      mCloth->getWorldBounds( box ); 

      Point3F min = pxCast<Point3F>( box.min );
      Point3F max = pxCast<Point3F>( box.max );

      mWorldBox.set( min, max );
      mObjBox = mWorldBox;

      getWorldTransform().mul( mObjBox );
   }
   else
   {
      Point3F extents( mPatchSize.x + 1.0f, mThickness * 2.0f, mPatchSize.y + 1.0f );
      mObjBox.set( Point3F::Zero, extents );
   }

   resetWorldBox();

   // Mark VB as dirty
   mIsVBDirty = true;
}

void PxCloth::interpolateTick( F32 delta )
{
}

bool PxCloth::onNewDataBlock( GameBaseData *dptr )
{
   return false;
}

void PxCloth::_setupAttachments()
{
   if ( !mCloth || !mWorld )
      return;

   // Set up attachments
   // Bottom right = bit 0
   // Bottom left = bit 1
   // Top right = bit 2
   // Top left = bit 3
   U32 numX = (U32)(mPatchSamples.x) + 1;    
   U32 numY = (U32)(mPatchSamples.y) + 1;    

   if ( mAttachmentMask & BIT( 0 ) )
      mCloth->attachVertexToGlobalPosition( 0, mCloth->getPosition( 0 ) );
   if ( mAttachmentMask & BIT( 1 ) )
      mCloth->attachVertexToGlobalPosition( numX, mCloth->getPosition( numX ) );   
   if ( mAttachmentMask & BIT( 2 ) )
      mCloth->attachVertexToGlobalPosition( (numX+1) * (numY+1) - (numX+1), mCloth->getPosition( (numX+1) * (numY+1) - (numX+1) ) );
   if ( mAttachmentMask & BIT( 3 ) )
      mCloth->attachVertexToGlobalPosition( (numX+1) * (numY+1) - 1, mCloth->getPosition( (numX+1) * (numY+1) - 1 ) );   
   if ( mAttachmentMask & BIT( 4 ) )
      mCloth->attachVertexToGlobalPosition( (numX+1) * (numY+1) - ((numX+1)/2), mCloth->getPosition( (numX+1) * (numY+1) - ((numX+1)/2) ) );
   if ( mAttachmentMask & BIT( 5 ) )
      mCloth->attachVertexToGlobalPosition( ((numX+1)/2), mCloth->getPosition( ((numX+1)/2) ) );
   if ( mAttachmentMask & BIT( 6 ) )
      mCloth->attachVertexToGlobalPosition( (numX+1) * ((numY+1)/2), mCloth->getPosition( (numX+1) * ((numY+1)/2) ) );
   if ( mAttachmentMask & BIT( 7 ) )
      mCloth->attachVertexToGlobalPosition( (numX+1) * ((numY+1)/2) + (numX), mCloth->getPosition( (numX+1) * ((numY+1)/2) + (numX) ) );
   
   if ( mAttachmentMask & BIT( 8 ) )
      for ( U32 i = (numX+1) * (numY+1) - (numX+1); i < (numX+1) * (numY+1); i++ )
         mCloth->attachVertexToGlobalPosition( i, mCloth->getPosition( i ) );
   
   if ( mAttachmentMask & BIT( 9 ) )
      for ( U32 i = 0; i < (numX+1); i++ )
         mCloth->attachVertexToGlobalPosition( i, mCloth->getPosition( i ) );

   if ( mAttachmentMask & BIT( 10 ) )
      for ( U32 i = 0; i < (numX+1) * (numY+1); i+=(numX+1) )
         mCloth->attachVertexToGlobalPosition( i, mCloth->getPosition( i ) );

   if ( mAttachmentMask & BIT( 11 ) )
      for ( U32 i = numX; i < (numX+1) * (numY+1); i+=(numX+1) )
         mCloth->attachVertexToGlobalPosition( i, mCloth->getPosition( i ) );
}