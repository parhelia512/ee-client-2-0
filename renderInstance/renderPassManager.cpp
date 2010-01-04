//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "renderInstance/renderPassManager.h"

#include "materials/sceneData.h"
#include "materials/matInstance.h"
#include "materials/customMaterialDefinition.h"
#include "materials/materialManager.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneObject.h"
#include "gfx/primBuilder.h"
#include "platform/profiler.h"
#include "renderInstance/renderBinManager.h"
#include "renderInstance/renderObjectMgr.h"
#include "renderInstance/renderMeshMgr.h"
#include "renderInstance/renderTranslucentMgr.h"
#include "renderInstance/renderGlowMgr.h"
#include "renderInstance/renderTerrainMgr.h"
#include "core/util/safeDelete.h"
#include "math/util/matrixSet.h"

//const String IRenderable3D::InterfaceName("Render3D");
const F32 RenderPassManager::PROCESSADD_NONE = -1e30f;
const F32 RenderPassManager::PROCESSADD_NORMAL = 0.5f;

const RenderInstType RenderPassManager::RIT_Interior("Interior");
const RenderInstType RenderPassManager::RIT_Mesh("Mesh");
const RenderInstType RenderPassManager::RIT_Shadow("Shadow");
const RenderInstType RenderPassManager::RIT_Sky("Sky");
const RenderInstType RenderPassManager::RIT_Terrain("Terrain");
const RenderInstType RenderPassManager::RIT_Object("Object");      
const RenderInstType RenderPassManager::RIT_ObjectTranslucent("ObjectTranslucent");
const RenderInstType RenderPassManager::RIT_Decal("Decal");
const RenderInstType RenderPassManager::RIT_Water("Water");
const RenderInstType RenderPassManager::RIT_Foliage("Foliage");
const RenderInstType RenderPassManager::RIT_Translucent("Translucent");
const RenderInstType RenderPassManager::RIT_Begin("Begin");
const RenderInstType RenderPassManager::RIT_Custom("Custom");
const RenderInstType RenderPassManager::RIT_Particle("Particle");
const RenderInstType RenderPassManager::RIT_Occluder("Occluder");


//*****************************************************************************
// RenderInstance
//*****************************************************************************

void RenderInst::clear()
{
   dMemset( this, 0, sizeof(RenderInst) );
}

void MeshRenderInst::clear()
{
   dMemset( this, 0, sizeof(MeshRenderInst) );
   visibility = 1.0f;
}

void ParticleRenderInst::clear()
{
   dMemset( this, 0, sizeof(ParticleRenderInst) );
}

void ObjectRenderInst::clear()
{
   userData = NULL;

   dMemset( this, 0, sizeof( ObjectRenderInst ) );

   // The memset here is kinda wrong... it clears the
   // state initialized by the delegate constructor.
   //
   // This fixes it... but we probably need to have a
   // real constructor for RenderInsts.
   renderDelegate.clear();
}

void OccluderRenderInst::clear()
{
   dMemset( this, 0, sizeof(OccluderRenderInst) );
}

//*****************************************************************************
// Render Instance Manager
//*****************************************************************************
IMPLEMENT_CONOBJECT(RenderPassManager);


RenderPassManager::RenderBinEventSignal& RenderPassManager::getRenderBinSignal()
{
   static RenderBinEventSignal theSignal;
   return theSignal;
}

void RenderPassManager::initPersistFields()
{
}

RenderPassManager::RenderPassManager()
{   
   mSceneManager = NULL;
   VECTOR_SET_ASSOCIATION( mRenderBins );
   VECTOR_SET_ASSOCIATION( mAddBins );

#ifndef TORQUE_SHIPPING
   mAddInstCount = 0;
   mAddInstPassCount = 0;
   VECTOR_SET_ASSOCIATION( mAddBinInstCounts );
#endif

   mMatrixSet = reinterpret_cast<MatrixSet *>(dAligned_malloc(sizeof(MatrixSet), 16));
   constructInPlace(mMatrixSet);
}

RenderPassManager::~RenderPassManager()
{
   dAligned_free(mMatrixSet);

   // Any bins left need to be deleted.
   for ( U32 i=0; i<mRenderBins.size(); i++ )
   {
      RenderBinManager *bin = mRenderBins[i];

      // Clear the parent first, so that RenderBinManager::onRemove()
      // won't call removeManager() and break this loop.
      bin->setParentManager( NULL );
      bin->deleteObject();
   }
}

void RenderPassManager::_insertSort(Vector<RenderBinManager*>& list, RenderBinManager* mgr, bool renderOrder)
{
   U32 i;
   for (i = 0; i < list.size(); i++)
   {
      bool renderCompare = mgr->getRenderOrder() < list[i]->getRenderOrder();
      bool processAddCompare = mgr->getProcessAddOrder() < list[i]->getProcessAddOrder();
      if ((renderOrder && renderCompare) || (!renderOrder && processAddCompare))      
      {
         list.insert(i);
         list[i] = mgr;
         return;
      }
   }

   list.push_back(mgr);
}

void RenderPassManager::addManager(RenderBinManager* mgr)
{
   if ( !mgr->isProperlyAdded() )
      mgr->registerObject();

   AssertFatal( mgr->getParentManager() == NULL, "RenderPassManager::addManager() - Bin is still part of another pass manager!" );
   mgr->setParentManager(this);

   _insertSort(mRenderBins, mgr, true);

   if ( mgr->getProcessAddOrder() != PROCESSADD_NONE )
      _insertSort(mAddBins, mgr, false);
}

void RenderPassManager::removeManager(RenderBinManager* mgr)
{
   AssertFatal( mgr->getParentManager() == this, "RenderPassManager::removeManager() - We do not own this bin!" );

   mRenderBins.remove( mgr );
   mAddBins.remove( mgr );
   mgr->setParentManager( NULL );
}

RenderBinManager* RenderPassManager::getManager(S32 i) const
{
   if (i >= 0 && i < mRenderBins.size())
      return mRenderBins[i];
   else
      return NULL;
}

#ifndef TORQUE_SHIPPING
void RenderPassManager::resetCounters()
{
   mAddInstCount = 0;
   mAddInstPassCount = 0;

   mAddBinInstCounts.setSize( mRenderBins.size() );
   dMemset( mAddBinInstCounts.address(), 0, mAddBinInstCounts.size() * sizeof( U32 ) );

   // TODO: We need to expose the counts again.  The difficulty 
   // is two fold... 
   //
   // First we need to track bins and not RITs... you can have 
   // the same RIT in multiple bins, so we miss some if we did that.
   //
   // Second we have multiple RenderPassManagers, so we need to
   // either combine the results of all passes, show all the bins
   // and passes, or just one pass at a time.
}
#endif

void RenderPassManager::addInst( RenderInst *inst )
{
   AssertFatal(inst != NULL, "doh, null instance");

   PROFILE_SCOPE(SceneRenderPassManager_addInst);

   #ifndef TORQUE_SHIPPING
      mAddInstCount++;
   #endif
   
   // See what managers want to look at this instance.
   bool bHandled = false;
#ifndef TORQUE_SHIPPING 
   U32 i = 0;
#endif
   for (Vector<RenderBinManager *>::iterator itr = mAddBins.begin();
      itr != mAddBins.end(); itr++)
   {
      RenderBinManager *curBin = *itr;
      AssertFatal(curBin, "Empty render bin slot!");

      // We may want to pass if the inst has been handled already to addElement?
      RenderBinManager::AddInstResult result = curBin->addElement(inst);

      #ifndef TORQUE_SHIPPING   
         // Log some rendering stats
         if ( result != RenderBinManager::arSkipped  )
         {
            mAddInstPassCount++;
            if ( i < mAddBinInstCounts.size() )
               mAddBinInstCounts[ i ]++;
         }
         i++;
      #endif

     switch (result)
      {
         case RenderBinManager::arAdded :
            bHandled = true;
            break;
         case RenderBinManager::arStop :
            bHandled = true;
            continue;
            break;
         default :
            // handle warning
            break;
      }
   }
   //AssertFatal(bHandled, "Instance without a render manager!");
}

void RenderPassManager::sort()
{
   PROFILE_SCOPE( RenderPassManager_Sort );

   for (Vector<RenderBinManager *>::iterator itr = mRenderBins.begin();
      itr != mRenderBins.end(); itr++)
   {
      AssertFatal(*itr, "Render manager invalid!");
      (*itr)->sort();
   }
}

void RenderPassManager::clear()
{
   PROFILE_SCOPE( RenderPassManager_Clear );

   mChunker.clear();

   for (Vector<RenderBinManager *>::iterator itr = mRenderBins.begin();
      itr != mRenderBins.end(); itr++)
   {
      AssertFatal(*itr, "Invalid render manager!");
      (*itr)->clear();
   }
}

void RenderPassManager::render(SceneState * state)
{
   PROFILE_SCOPE( RenderPassManager_Render );

   GFX->pushWorldMatrix();
   MatrixF proj = GFX->getProjectionMatrix();

   
   for (Vector<RenderBinManager *>::iterator itr = mRenderBins.begin();
      itr != mRenderBins.end(); itr++)
   {
      RenderBinManager *curBin = *itr;
      AssertFatal(curBin, "Invalid render manager!");
      getRenderBinSignal().trigger(curBin, state, true);
      curBin->render(state);
      getRenderBinSignal().trigger(curBin, state, false);
   }

   GFX->popWorldMatrix();
   GFX->setProjectionMatrix( proj );
      
   // Restore a clean state for subsequent rendering.
   GFX->disableShaders();
   for(S32 i = 0; i < GFX->getNumSamplers(); ++i)
      GFX->setTexture(i, NULL);
}

void RenderPassManager::renderPass(SceneState * state)
{
   PROFILE_SCOPE( RenderPassManager_RenderPass );
   sort();
   render(state);
   clear();
}

GFXTextureObject *RenderPassManager::getDepthTargetTexture()
{
   // If this is OpenGL, or something else has set the depth buffer, return the pointer
   if( mDepthBuff.isValid() )
   {
      // If this is OpenGL, make sure the depth target matches up
      // with the active render target.  Otherwise recreate.
      
      if( GFX->getAdapterType() == OpenGL )
      {
         GFXTarget* activeRT = GFX->getActiveRenderTarget();
         AssertFatal( activeRT, "Must be an active render target to call 'getDepthTargetTexture'" );
         
         Point2I activeRTSize = activeRT->getSize();
         if( mDepthBuff.getWidth() == activeRTSize.x &&
             mDepthBuff.getHeight() == activeRTSize.y )
            return mDepthBuff.getPointer();
      }
      else
         return mDepthBuff.getPointer();
   }

   if(GFX->getAdapterType() == OpenGL)
   {
      AssertFatal(GFX->getActiveRenderTarget(), "Must be an active render target to call 'getDepthTargetTexture'");

      const Point2I rtSize = GFX->getActiveRenderTarget()->getSize();
      mDepthBuff.set(rtSize.x, rtSize.y, GFXFormatD24S8, 
         &GFXDefaultZTargetProfile, avar("%s() - mDepthBuff (line %d)", __FUNCTION__, __LINE__));
      return mDepthBuff.getPointer();
   }

   // Default return value
   return GFXTextureTarget::sDefaultDepthStencil;
}

void RenderPassManager::setDepthTargetTexture( GFXTextureObject *zTarget )
{
   mDepthBuff = zTarget;
}

const MatrixF* RenderPassManager::allocSharedXform( SharedTransformType stt )
{
   AssertFatal(stt == View || stt == Projection, "Bad shared transform type");

   // Enable this to simulate non-shared transform performance
   //#define SIMULATE_NON_SHARED_TRANSFORMS
#ifdef SIMULATE_NON_SHARED_TRANSFORMS
   return allocUniqueXform(stt == View ? mMatrixSet->getWorldToCamera() : mMatrixSet->getCameraToScreen());
#else
   return &(stt == View ? mMatrixSet->getWorldToCamera() : mMatrixSet->getCameraToScreen());
#endif
}

void RenderPassManager::assignSharedXform( SharedTransformType stt, const MatrixF &xfm )
{
   AssertFatal(stt == View || stt == Projection, "Bad shared transform type");
   if(stt == View)
      mMatrixSet->setSceneView(xfm);
   else
      mMatrixSet->setSceneProjection(xfm);
}

// Script interface

ConsoleMethod(RenderPassManager, getManagerCount, S32, 2, 2, 
   "Returns the total number of bin managers." )
{
   TORQUE_UNUSED(argc);
   TORQUE_UNUSED(argv);
   
   return (S32) object->getManagerCount();
}

ConsoleMethod( RenderPassManager, getManager, S32, 3, 3, 
   "Get the manager at index." )
{
   TORQUE_UNUSED(argc);

   S32 objectIndex = dAtoi(argv[2]);
   if(objectIndex < 0 || objectIndex >= S32(object->getManagerCount()))
   {
      Con::printf("Set::getObject index out of range.");
      return -1;
   }
   return object->getManager(objectIndex)->getId();
}

ConsoleMethod( RenderPassManager, addManager, void, 3, 3, 
   "Add a manager." )
{
   TORQUE_UNUSED(argc);

   RenderBinManager* m;   
   if (Sim::findObject<RenderBinManager>(argv[2], m))
      object->addManager(m);
   else
      Con::errorf("Object %s does not exist or is not a RenderBinManager", argv[2]);
}

ConsoleMethod( RenderPassManager, removeManager, void, 3, 3, 
   "Removes a manager by name." )
{
   TORQUE_UNUSED(argc);

   RenderBinManager* m;
   if (Sim::findObject<RenderBinManager>(argv[2], m))
      object->removeManager(m);
   else
      Con::errorf("Object %s does not exist or is not a RenderBinManager", argv[2]);
}
