//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "ts/tsShapeInstance.h"

#include "ts/tsLastDetail.h"
#include "console/consoleTypes.h"
#include "ts/tsDecal.h"
#include "platform/profiler.h"
#include "core/frameAllocator.h"
#include "gfx/gfxDevice.h"
#include "materials/materialManager.h"
#include "materials/materialFeatureTypes.h"
#include "materials/sceneData.h"
#include "materials/matInstance.h"
#include "sceneGraph/sceneState.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxDrawUtil.h"

F32                           TSShapeInstance::smDetailAdjust = 1.0f;
F32                           TSShapeInstance::smSmallestVisiblePixelSize = -1.0f;
S32                           TSShapeInstance::smNumSkipRenderDetails = 0;

Vector<QuatF>                 TSShapeInstance::smNodeCurrentRotations(__FILE__, __LINE__);
Vector<Point3F>               TSShapeInstance::smNodeCurrentTranslations(__FILE__, __LINE__);
Vector<F32>                   TSShapeInstance::smNodeCurrentUniformScales(__FILE__, __LINE__);
Vector<Point3F>               TSShapeInstance::smNodeCurrentAlignedScales(__FILE__, __LINE__);
Vector<TSScale>               TSShapeInstance::smNodeCurrentArbitraryScales(__FILE__, __LINE__);

Vector<TSThread*>             TSShapeInstance::smRotationThreads(__FILE__, __LINE__);
Vector<TSThread*>             TSShapeInstance::smTranslationThreads(__FILE__, __LINE__);
Vector<TSThread*>             TSShapeInstance::smScaleThreads(__FILE__, __LINE__);

//-------------------------------------------------------------------------------------
// constructors, destructors, initialization
//-------------------------------------------------------------------------------------

TSShapeInstance::TSShapeInstance( const Resource<TSShape> &shape, bool loadMaterials )
{
   VECTOR_SET_ASSOCIATION(mMeshObjects);
   VECTOR_SET_ASSOCIATION(mIflMaterialInstances);
   VECTOR_SET_ASSOCIATION(mNodeTransforms);
   VECTOR_SET_ASSOCIATION(mNodeReferenceRotations);
   VECTOR_SET_ASSOCIATION(mNodeReferenceTranslations);
   VECTOR_SET_ASSOCIATION(mNodeReferenceUniformScales);
   VECTOR_SET_ASSOCIATION(mNodeReferenceScaleFactors);
   VECTOR_SET_ASSOCIATION(mNodeReferenceArbitraryScaleRots);
   VECTOR_SET_ASSOCIATION(mThreadList);
   VECTOR_SET_ASSOCIATION(mTransitionThreads);

   mShapeResource = shape;
   mShape = mShapeResource;
   buildInstanceData( mShape, loadMaterials );
}

TSShapeInstance::TSShapeInstance( TSShape *shape, bool loadMaterials )
{
   VECTOR_SET_ASSOCIATION(mMeshObjects);
   VECTOR_SET_ASSOCIATION(mIflMaterialInstances);
   VECTOR_SET_ASSOCIATION(mNodeTransforms);
   VECTOR_SET_ASSOCIATION(mNodeReferenceRotations);
   VECTOR_SET_ASSOCIATION(mNodeReferenceTranslations);
   VECTOR_SET_ASSOCIATION(mNodeReferenceUniformScales);
   VECTOR_SET_ASSOCIATION(mNodeReferenceScaleFactors);
   VECTOR_SET_ASSOCIATION(mNodeReferenceArbitraryScaleRots);
   VECTOR_SET_ASSOCIATION(mThreadList);
   VECTOR_SET_ASSOCIATION(mTransitionThreads);

   mShapeResource = NULL;
   mShape = shape;
   buildInstanceData( mShape, loadMaterials );
}

TSShapeInstance::~TSShapeInstance()
{
   mMeshObjects.clear();

   while (mThreadList.size())
      destroyThread(mThreadList.last());

   setMaterialList(NULL);

   delete [] mDirtyFlags;
}

void TSShapeInstance::init()
{
   Con::addVariable("$pref::TS::detailAdjust", TypeF32, &smDetailAdjust);
   Con::addVariable("$pref::TS::skipLoadDLs", TypeS32, &TSShape::smNumSkipLoadDetails);
   Con::addVariable("$pref::TS::skipRenderDLs", TypeS32, &smNumSkipRenderDetails);
}

void TSShapeInstance::destroy()
{
}

void TSShapeInstance::buildInstanceData(TSShape * _shape, bool loadMaterials)
{
   S32 i;

   mShape = _shape;

   debrisRefCount = 0;

   mCurrentDetailLevel = 0;
   mCurrentIntraDetailLevel = 1.0f;

   // all triggers off at start
   mTriggerStates = 0;

   //
   mAlphaAlways = false;
   mAlphaAlwaysValue = 1.0f;

   // material list...
   mMaterialList = NULL;
   mOwnMaterialList = false;

   //
   mData = 0;
   mScaleCurrentlyAnimated = false;

   if(loadMaterials)
      setMaterialList(mShape->materialList);

   // set up node data
   S32 numNodes = mShape->nodes.size();
   mNodeTransforms.setSize(numNodes);

   // add objects to trees
   S32 numObjects = mShape->objects.size();
   mMeshObjects.setSize(numObjects);
   for (i=0; i<numObjects; i++)
   {
      const TSObject * obj = &mShape->objects[i];
      MeshObjectInstance * objInst = &mMeshObjects[i];

      // call objInst constructor
      constructInPlace(objInst);

      // hook up the object to it's node and transforms.
      objInst->mTransforms = &mNodeTransforms;
      objInst->nodeIndex = obj->nodeIndex;

      // set up list of meshes
      if (obj->numMeshes)
         objInst->meshList = &mShape->meshes[obj->startMeshIndex];
      else
         objInst->meshList = NULL;

      objInst->object = obj;
      objInst->forceHidden = false;
   }

   // construct ifl material objects

   if(loadMaterials)
   {
      for (i=0; i<mShape->iflMaterials.size(); i++)
      {
         mIflMaterialInstances.increment();
         mIflMaterialInstances.last().iflMaterial = &mShape->iflMaterials[i];
         mIflMaterialInstances.last().frame = -1;
      }
   }

   // set up subtree data
   S32 ss = mShape->subShapeFirstNode.size(); // we have this many subtrees
   mDirtyFlags = new U32[ss];

   mGroundThread = NULL;
   mCurrentDetailLevel = 0;

   animateSubtrees();

   // Construct billboards if not done already
   if ( loadMaterials && mShapeResource )
      mShape->setupBillboardDetails( mShapeResource.getPath().getFullPath() );
}

void TSShapeInstance::setMaterialList(TSMaterialList * ml)
{
   // get rid of old list
   if (mOwnMaterialList)
      delete mMaterialList;

   mMaterialList = ml;
   mOwnMaterialList = false;

   if ( mMaterialList )
   {
      const String shapePath = mShapeResource.getPath().getPath();

      ///@todo Review this - the limitation on files opened should be gone
      // read ifl materials if necessary -- this is here rather than in shape because we can't open 2 files at once :(
      if (mShape->materialList == mMaterialList)
         mShape->readIflMaterials( shapePath );

      mMaterialList->load( shapePath );
      mMaterialList->mapMaterials();
      initMaterialList();
   }
}

void TSShapeInstance::cloneMaterialList()
{
   if (mOwnMaterialList)
      return;

   mMaterialList = new TSMaterialList(mMaterialList);
   initMaterialList();

   mOwnMaterialList = true;
}

void TSShapeInstance::initMaterialList()
{
   mMaterialList->initMatInstances( MATMGR->getDefaultFeatures(), 
                                    mShape->getVertexFormat() );
}

void TSShapeInstance::reSkin( String newBaseName, String oldBaseName )
{
   if( newBaseName.isEmpty() )
      newBaseName = "base";
   if( oldBaseName.isEmpty() )
      oldBaseName = "base";

   const U32 oldBaseNameLength = oldBaseName.length();
   String oldBaseNamePlusDot;
   if( !oldBaseName.isEmpty() )
      oldBaseNamePlusDot = oldBaseName + String( "." );

   // Make our own copy of the materials list from the resource
   // if necessary.
   if (ownMaterialList() == false)
      cloneMaterialList();

   Torque::Path   shapePath = mShapeResource.getPath();
   const String   resourcePath = shapePath.getPath();

   // Cycle through the materials.
   TSMaterialList* pMatList = getMaterialList();
   const Vector<String> &materialNames = pMatList->getMaterialNameList();

   for (S32 j = 0; j < materialNames.size(); j++) 
   {
      // Get the name of this material.
      const String &pName = materialNames[j];
      if( !pName.isEmpty() )
      {
         // Try changing base.

         bool replacedRoot = false;
         if( !oldBaseName.isEmpty() && pName.compare( oldBaseNamePlusDot, oldBaseNameLength + 1, String::NoCase ) == 0 )
         {
            String matName = pName;
            String newPath = resourcePath + "/" + matName.replace( 0, oldBaseNameLength, newBaseName );
            replacedRoot = pMatList->setMaterial( j, newPath );
         }

         // If we did not change base, set the old material.

         if( !replacedRoot )
            pMatList->setMaterial( j, resourcePath + "/" + pName );
      }
   }

   // Initialize the material instances.

   initMaterialList();
}

//-------------------------------------------------------------------------------------
// Render & detail selection
//-------------------------------------------------------------------------------------

void TSShapeInstance::renderDebugNormals( F32 normalScalar, S32 dl )
{
   if ( dl < 0 )
      return;

   AssertFatal( dl >= 0 && dl < mShape->details.size(),
      "TSShapeInstance::renderDebugNormals() - Bad detail level!" );

   static GFXStateBlockRef sb;
   if ( sb.isNull() )
   {
      GFXStateBlockDesc desc;
      desc.setCullMode( GFXCullNone );
      desc.setZReadWrite( true );
      desc.zWriteEnable = false;
      desc.vertexColorEnable = true;

      sb = GFX->createStateBlock( desc );
   }
   GFX->setStateBlock( sb );

   const TSDetail *detail = &mShape->details[dl];
   const S32 ss = detail->subShapeNum;
   if ( ss < 0 )
      return;

   const S32 start = mShape->subShapeFirstObject[ss];
   const S32 end   = start + mShape->subShapeNumObjects[ss];

   for ( S32 i = start; i < end; i++ )
   {
      MeshObjectInstance *meshObj = &mMeshObjects[i];
      if ( !meshObj )
         continue;

      const MatrixF &meshMat = meshObj->getTransform();

      // Then go through each TSMesh...
      U32 m = 0;
      for( TSMesh *mesh = meshObj->getMesh(m); mesh != NULL; mesh = meshObj->getMesh(m++) )
      {
         // and pull out the list of normals.
         const U32 numNrms = mesh->mNumVerts;
         PrimBuild::begin( GFXLineList, 2 * numNrms );
         for ( U32 n = 0; n < numNrms; n++ )
         {
            Point3F norm = mesh->mVertexData[n].normal();
            Point3F vert = mesh->mVertexData[n].vert();

            meshMat.mulP( vert );
            meshMat.mulV( norm );

            // Then render them.
            PrimBuild::color4f( mFabs( norm.x ), mFabs( norm.y ), mFabs( norm.z ), 1.0f );
            PrimBuild::vertex3fv( vert );
            PrimBuild::vertex3fv( vert + (norm * normalScalar) );
         }

         PrimBuild::end();
      }
   }
}

void TSShapeInstance::renderDebugNodes()
{
   GFXDrawUtil *drawUtil = GFX->getDrawUtil();
   ColorI color( 255, 0, 0, 255 );

   GFXStateBlockDesc desc;
   desc.setBlend( false );
   desc.setZReadWrite( false, false );

   for ( U32 i = 0; i < mNodeTransforms.size(); i++ )
      drawUtil->drawTransform( desc, mNodeTransforms[i], Point3F::One, color );
}

void TSShapeInstance::listMeshes( const String &state ) const
{
   if ( state.equal( "All", String::NoCase ) )
   {
      for ( U32 i = 0; i < mMeshObjects.size(); i++ )
      {
         const MeshObjectInstance &mesh = mMeshObjects[i];
         Con::warnf( "meshidx %3d, %8s, %s", i, ( mesh.forceHidden ) ? "Hidden" : "Visible", mShape->getMeshName(i).c_str() );         
      }
   }
   else if ( state.equal( "Hidden", String::NoCase ) )
   {
      for ( U32 i = 0; i < mMeshObjects.size(); i++ )
      {
         const MeshObjectInstance &mesh = mMeshObjects[i];
         if ( mesh.forceHidden )
            Con::warnf( "meshidx %3d, %8s, %s", i, "Visible", mShape->getMeshName(i).c_str() );         
      }
   }
   else if ( state.equal( "Visible", String::NoCase ) )
   {
      for ( U32 i = 0; i < mMeshObjects.size(); i++ )
      {
         const MeshObjectInstance &mesh = mMeshObjects[i];
         if ( !mesh.forceHidden )
            Con::warnf( "meshidx %3d, %8s, %s", i, "Hidden", mShape->getMeshName(i).c_str() );         
      }
   }
   else
   {
      Con::warnf( "TSShapeInstance::listMeshes( %s ) - only All/Hidden/Visible are valid parameters." );
   }
}

void TSShapeInstance::render( const TSRenderState &rdata )
{
   if (mCurrentDetailLevel<0)
      return;

   PROFILE_SCOPE( TSShapeInstance_Render );

   // alphaIn:  we start to alpha-in next detail level when intraDL > 1-alphaIn-alphaOut
   //           (finishing when intraDL = 1-alphaOut)
   // alphaOut: start to alpha-out this detail level when intraDL > 1-alphaOut
   // NOTE:
   //   intraDL is at 1 when if shape were any closer to us we'd be at dl-1,
   //   intraDL is at 0 when if shape were any farther away we'd be at dl+1
   F32 alphaOut = mShape->alphaOut[mCurrentDetailLevel];
   F32 alphaIn  = mShape->alphaIn[mCurrentDetailLevel];
   F32 saveAA = mAlphaAlways ? mAlphaAlwaysValue : 1.0f;

   /// This first case is the single detail level render.
   if ( mCurrentIntraDetailLevel > alphaIn + alphaOut )
      render( rdata, mCurrentDetailLevel, mCurrentIntraDetailLevel );
   else if ( mCurrentIntraDetailLevel > alphaOut )
   {
      // draw this detail level w/ alpha=1 and next detail level w/
      // alpha=1-(intraDl-alphaOut)/alphaIn

      // first draw next detail level
      if ( mCurrentDetailLevel + 1 < mShape->details.size() && mShape->details[ mCurrentDetailLevel + 1 ].size > 0.0f )
      {
         setAlphaAlways( saveAA * ( alphaIn + alphaOut - mCurrentIntraDetailLevel ) / alphaIn );
         render( rdata, mCurrentDetailLevel + 1, 0.0f );
      }

      setAlphaAlways( saveAA );
      render( rdata, mCurrentDetailLevel, mCurrentIntraDetailLevel );
   }
   else
   {
      // draw next detail level w/ alpha=1 and this detail level w/
      // alpha = 1-intraDL/alphaOut

      // first draw next detail level
      if ( mCurrentDetailLevel + 1 < mShape->details.size() && mShape->details[ mCurrentDetailLevel + 1 ].size > 0.0f )
         render( rdata, mCurrentDetailLevel+1, 0.0f );

      setAlphaAlways( saveAA * mCurrentIntraDetailLevel / alphaOut );
      render( rdata, mCurrentDetailLevel, mCurrentIntraDetailLevel );
      setAlphaAlways( saveAA );
   }
}

bool TSShapeInstance::hasTranslucency()
{
   if(!mShape->details.size())
      return false;

   const TSDetail * detail = &mShape->details[0];
   S32 ss = detail->subShapeNum;

   return mShape->subShapeFirstTranslucentObject[ss] != mShape->subShapeFirstObject[ss] + mShape->subShapeNumObjects[ss];
}

bool TSShapeInstance::hasSolid()
{
   if(!mShape->details.size())
      return false;

   const TSDetail * detail = &mShape->details[0];
   S32 ss = detail->subShapeNum;

   return mShape->subShapeFirstTranslucentObject[ss] != mShape->subShapeFirstObject[ss];
}

void TSShapeInstance::setMeshForceHidden( const char *meshName, bool hidden )
{
   Vector<MeshObjectInstance>::iterator iter = mMeshObjects.begin();
   for ( ; iter != mMeshObjects.end(); iter++ )
   {
      S32 nameIndex = iter->object->nameIndex;
      const char *name = mShape->names[ nameIndex ];

      if ( dStrcmp( meshName, name ) == 0 )
      {
         iter->forceHidden = hidden;
         iter->visible = hidden ? 0.0f : 1.0f;
         return;
      }
   }
}

void TSShapeInstance::setMeshForceHidden( S32 meshIndex, bool hidden )
{
   AssertFatal( meshIndex > -1 && meshIndex < mMeshObjects.size(),
      "TSShapeInstance::setMeshForceHidden - Invalid index!" );
                  
   mMeshObjects[meshIndex].forceHidden = hidden;
   mMeshObjects[meshIndex].visible = hidden ? 0.0f : 1.0f;
}

void TSShapeInstance::render( const TSRenderState &rdata, S32 dl, F32 intraDL )
{
   AssertFatal( dl >= 0 && dl < mShape->details.size(),"TSShapeInstance::render" );

   S32 i;

   const TSDetail * detail = &mShape->details[dl];
   S32 ss = detail->subShapeNum;
   S32 od = detail->objectDetailNum;

   // if we're a billboard detail, draw it and exit
   if ( ss < 0 )
   {
      PROFILE_SCOPE( TSShapeInstance_RenderBillboards );
      
      if ( !rdata.isNoRenderTranslucent() )
         mShape->billboardDetails[ dl ]->render( rdata, mAlphaAlways ? mAlphaAlwaysValue : 1.0f );

      return;
   }

   PROFILE_START( TSShapeInstance_IFLMaterials );
   
   // set up animating ifl materials
   for (i=0; i<mIflMaterialInstances.size(); i++)
   {
      IflMaterialInstance  * iflMaterialInstance = &mIflMaterialInstances[i];
      const TSShape::IflMaterial * iflMaterial = iflMaterialInstance->iflMaterial;
      mMaterialList->remap(iflMaterial->materialSlot, iflMaterial->firstFrame + iflMaterialInstance->frame);
   }

   PROFILE_END(); // TSShapeInstance_IFLMaterials

   // run through the meshes   
   S32 start = rdata.isNoRenderNonTranslucent() ? mShape->subShapeFirstTranslucentObject[ss] : mShape->subShapeFirstObject[ss];
   S32 end   = rdata.isNoRenderTranslucent() ? mShape->subShapeFirstTranslucentObject[ss] : mShape->subShapeFirstObject[ss] + mShape->subShapeNumObjects[ss];
   for (i=start; i<end; i++)
   {
      // following line is handy for debugging, to see what part of the shape that it is rendering
      // const char *name = mShape->names[ mMeshObjects[i].object->nameIndex ];
      mMeshObjects[i].render( od, mMaterialList, rdata );
   }
}

void TSShapeInstance::setCurrentDetail( S32 dl, F32 intraDL )
{
   mCurrentDetailLevel = dl;
   mCurrentIntraDetailLevel = intraDL > 1.0f ? 1.0f : (intraDL < 0.0f ? 0.0f : intraDL);

   // restrict chosen detail level by cutoff value
   S32 cutoff = getMin( smNumSkipRenderDetails, mShape->mSmallestVisibleDL );
   if ( mCurrentDetailLevel>=0 && mCurrentDetailLevel<cutoff)
   {
      mCurrentDetailLevel = cutoff;
      mCurrentIntraDetailLevel = 1.0f;
   }
}

S32 TSShapeInstance::setDetailFromPosAndScale(  const SceneState *state,
                                                const Point3F &pos, 
                                                const Point3F &scale )
{
   VectorF camVector = pos - state->getDiffuseCameraPosition();
   F32 dist = getMax( camVector.len(), 0.01f );
   F32 invScale = ( 1.0f / getMax( getMax( scale.x, scale.y ), scale.z ) );

   return setDetailFromDistance( state, dist * invScale );
}

S32 TSShapeInstance::setDetailFromDistance( const SceneState *state, F32 scaledDistance )
{
   // Shortcut if the distance is really close or negative.
   if ( scaledDistance <= 0.0f )
   {
      mCurrentDetailLevel = getMin( 1, mShape->details.size() ) - 1;
      mCurrentIntraDetailLevel = 0.0f;
      return mCurrentDetailLevel;
   }
      
   // The pixel scale is used the linearly scale the lod
   // selection based on the viewport size.
   //
   // The original calculation from TGEA was...
   //
   // pixelScale = viewport.extent.x * 1.6f / 640.0f;
   //
   // Since we now work on the viewport height, assuming
   // 4:3 aspect ratio, we've changed the reference value
   // to 300 to be more compatible with legacy shapes.
   //
   const F32 pixelScale = state->getViewportExtent().y / 300.0f;

   // This is legacy DTS support for older "multires" based
   // meshes.  The original crossbow weapon uses this.
   //
   // If we have more than one detail level and the maxError
   // is non-negative then we do some sort of screen error 
   // metric for detail selection.
   //
   if (  mShape->mSmallestVisibleDL >= 0 && 
         mShape->details.first().maxError >= 0 )
   {
      // The pixel size of 1 meter at the input distance.
      F32 pixelRadius = state->projectRadius( scaledDistance, 1.0f ) * pixelScale;
      static const F32 smScreenError = 5.0f;
      return setDetailFromScreenError( smScreenError / pixelRadius );
   }

   F32 pixelRadius = state->projectRadius( scaledDistance, mShape->radius ) * pixelScale;
   F32 adjustedPR = pixelRadius * smDetailAdjust;

   if (  adjustedPR > smSmallestVisiblePixelSize && 
         adjustedPR <= mShape->mSmallestVisibleSize )
      adjustedPR = mShape->mSmallestVisibleSize + 0.01f;

   return setDetailFromPixelSize( adjustedPR );
}

S32 TSShapeInstance::setDetailFromPixelSize( F32 pixelSize )
{
   // check to see if not visible first...
   if ( pixelSize <= mShape->mSmallestVisibleSize )
   {
      // don't render...
      mCurrentDetailLevel=-1;
      mCurrentIntraDetailLevel = 0.0f;
      return -1;
   }

   // same detail level as last time?
   // only search for detail level if the current one isn't the right one already
   if (  mCurrentDetailLevel < 0 ||
        (mCurrentDetailLevel == 0 && pixelSize <= mShape->details[0].size) ||
        (mCurrentDetailLevel>0  && (pixelSize <= mShape->details[mCurrentDetailLevel].size || 
        pixelSize > mShape->details[mCurrentDetailLevel-1].size) ) )
   {
      // scan shape for highest detail size smaller than us...
      // shapes details are sorted from largest to smallest...
      // a detail of size <= 0 means it isn't a renderable detail level (utility detail)
      for (S32 i=0; i<mShape->details.size(); i++)
      {
         if ( pixelSize > mShape->details[i].size )
         {
            mCurrentDetailLevel = i;
            break;
         }
         if ( i + 1 >= mShape->details.size() || mShape->details[i+1].size < 0 )
         {
            // We've run out of details and haven't found anything?
            // Let's just grab this one.
            mCurrentDetailLevel = i;
            break;
         }
      }
   }

   F32 curSize = mShape->details[mCurrentDetailLevel].size;
   F32 nextSize = mCurrentDetailLevel == 0 ? 2.0f * curSize : mShape->details[mCurrentDetailLevel - 1].size;

   setCurrentDetail( mCurrentDetailLevel, 
                     nextSize-curSize>0.01f ? (pixelSize - curSize) / (nextSize - curSize) : 1.0f );

   return mCurrentDetailLevel;
}

S32 TSShapeInstance::setDetailFromScreenError( F32 errorTolerance )
{
   // note:  we use 10 time the average error as the metric...this is
   // more robust than the maxError...the factor of 10 is to put average error
   // on about the same scale as maxError.  The errorTOL is how much
   // error we are able to tolerate before going to a more detailed version of the
   // shape.  We look for a pair of details with errors bounding our errorTOL,
   // and then we select an interpolation parameter to tween betwen them.  Ok, so
   // this isn't exactly an error tolerance.  A tween value of 0 is the lower poly
   // model (higher detail number) and a value of 1 is the higher poly model (lower
   // detail number).

   // deal with degenerate case first...
   // if smallest detail corresponds to less than half tolerable error, then don't even draw
   F32 prevErr;
   if ( mShape->mSmallestVisibleDL < 0 )
      prevErr = 0.0f;
   else
      prevErr = 10.0f * mShape->details[mShape->mSmallestVisibleDL].averageError * 20.0f;
   if ( mShape->mSmallestVisibleDL < 0 || prevErr < errorTolerance )
   {
      // draw last detail
      mCurrentDetailLevel = mShape->mSmallestVisibleDL;
      mCurrentIntraDetailLevel = 0.0f;
      return mCurrentDetailLevel;
   }

   // this function is a little odd
   // the reason is that the detail numbers correspond to
   // when we stop using a given detail level...
   // we search the details from most error to least error
   // until we fit under the tolerance (errorTOL) and then
   // we use the next highest detail (higher error)
   for (S32 i = mShape->mSmallestVisibleDL; i >= 0; i-- )
   {
      F32 err0 = 10.0f * mShape->details[i].averageError;
      if ( err0 < errorTolerance )
      {
         // ok, stop here

         // intraDL = 1 corresponds to fully this detail
         // intraDL = 0 corresponds to the next lower (higher number) detail
         mCurrentDetailLevel = i;
         mCurrentIntraDetailLevel = 1.0f - (errorTolerance - err0) / (prevErr - err0);
         return mCurrentDetailLevel;
      }
      prevErr = err0;
   }

   // get here if we are drawing at DL==0
   mCurrentDetailLevel = 0;
   mCurrentIntraDetailLevel = 1.0f;
   return mCurrentDetailLevel;
}

//-------------------------------------------------------------------------------------
// Object (MeshObjectInstance & PluginObjectInstance) render methods
//-------------------------------------------------------------------------------------

void TSShapeInstance::ObjectInstance::render( S32, TSMaterialList *, const TSRenderState &rdata )
{
   AssertFatal(0,"TSShapeInstance::ObjectInstance::render:  no default render method.");
}

void TSShapeInstance::MeshObjectInstance::render(S32 objectDetail, TSMaterialList * materials, const TSRenderState &rdata)
{
   if ( visible <= 0.01f )
      return;

   TSMesh *mesh = getMesh(objectDetail);
   if ( !mesh )
      return;

   const MatrixF &transform = getTransform();

   if ( rdata.getCuller() )
   {
      Box3F box( mesh->getBounds() );
      transform.mul( box );
      if ( !rdata.getCuller()->intersects( box ) )
         return;
   }

   GFX->pushWorldMatrix();
   GFX->multWorld( transform );

   mesh->setFade( visible );

   // Pass a hint to the mesh that time has advanced and that the
   // skin is dirty and needs to be updated.  This should result
   // in the skin only updating once per frame in most cases.
   const U32 currTime = Sim::getCurrentTime();
   bool isSkinDirty = currTime != mLastTime;

   mesh->render(  materials, 
                  rdata, 
                  isSkinDirty,
                  *mTransforms, 
                  mVertexBuffer,
                  mPrimitiveBuffer );

   // Update the last render time.
   mLastTime = currTime;

   GFX->popWorldMatrix();
}

TSShapeInstance::MeshObjectInstance::MeshObjectInstance() 
   :  mLastTime( 0 )
{

}

void TSShapeInstance::prepCollision()
{
   PROFILE_SCOPE( TSShapeInstance_PrepCollision );

   // Iterate over all our meshes and call prepCollision on them...
   for(S32 i=0; i<mShape->meshes.size(); i++)
   {
      if(mShape->meshes[i])
         mShape->meshes[i]->prepOpcodeCollision();
   }
}

