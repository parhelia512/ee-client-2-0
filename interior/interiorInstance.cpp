//-----------------------------------------------------------------------------
// Torque 3D Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "interior/interiorInstance.h"
#include "interior/interior.h"
//#include "interior/pathedInterior.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "core/stream/bitStream.h"
#include "gfx/bitmap/gBitmap.h"
#include "math/mathIO.h"
#include "gui/worldEditor/editor.h"
#include "interior/interiorResObjects.h"
//#include "T3D/trigger.h"
#include "sceneGraph/simPath.h"
#include "interior/forceField.h"
#include "lighting/lightManager.h"
#include "collision/convex.h"
#include "sfx/sfxProfile.h"
#include "sfx/sfxEnvironment.h"
#include "core/frameAllocator.h"
#include "sim/netConnection.h"
#include "platform/profiler.h"
#include "gui/3d/guiTSControl.h"
#include "math/mathUtils.h"
#include "renderInstance/renderPassManager.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsStatic.h"
#include "core/resourceManager.h"
#include "materials/materialManager.h"
#include "materials/materialFeatureTypes.h"

#ifdef TORQUE_COLLADA
#include "ts/collada/colladaUtils.h"
#endif

//--------------------------------------------------------------------------
//-------------------------------------- Local classes, data, and functions
//
const U32 csgMaxZoneSize = 256;
bool sgScopeBoolArray[256];

//--------------------------------------------------------------------------
//-------------------------------------- Static functions
//
IMPLEMENT_CO_NETOBJECT_V1(InteriorInstance);

//------------------------------------------------------------------------------
//-------------------------------------- InteriorInstance
//
bool InteriorInstance::smDontRestrictOutside = false;
F32  InteriorInstance::smDetailModification = 1.0f;


InteriorInstance::InteriorInstance()
{
   mAlarmState        = false;

   mInteriorFileName = NULL;
   mTypeMask = InteriorObjectType | StaticObjectType |
	   StaticRenderedObjectType | ShadowCasterObjectType;
   mNetFlags.set(Ghostable | ScopeAlways);

   mShowTerrainInside = false;
   mSmoothLighting = false;

   mSkinBase = StringTable->insert("base");
   mAudioProfile = 0;
   mAudioEnvironment = 0;
   mForcedDetailLevel = -1;

   mConvexList = new Convex;
   mCRC = 0;

   mPhysicsRep = NULL;
}

InteriorInstance::~InteriorInstance()
{
   delete mConvexList;
   mConvexList = NULL;

   // GFX2_RENDER_MERGE
   //for (U32 i = 0; i < mReflectPlanes.size(); i++)
   //   mReflectPlanes[i].clearTextures();
}


void InteriorInstance::init()
{
   // Does nothing for the moment
}


void InteriorInstance::destroy()
{
   // Also does nothing for the moment
}


//--------------------------------------------------------------------------
// Inspection
static SFXProfile * saveAudioProfile = 0;
static SFXEnvironment * saveAudioEnvironment = 0;
void InteriorInstance::inspectPreApply()
{
   saveAudioProfile = mAudioProfile;
   saveAudioEnvironment = mAudioEnvironment;
}

void InteriorInstance::inspectPostApply()
{
   if((mAudioProfile != saveAudioProfile) || (mAudioEnvironment != saveAudioEnvironment))
      setMaskBits(AudioMask);

   // Apply any transformations set in the editor
   Parent::inspectPostApply();

   // Update the Transform on Editor Apply.
   setMaskBits(TransformMask);
}

//--------------------------------------------------------------------------
//-------------------------------------- Console functionality
//
void InteriorInstance::initPersistFields()
{
   addGroup("Media");   
   addProtectedField("interiorFile",      TypeFilename,              Offset(mInteriorFileName, InteriorInstance), &setInteriorFile, &defaultProtectedGetFn, "");
   endGroup("Media");   

   addGroup("Audio");   
   addField("sfxProfile",      TypeSFXProfilePtr,       Offset(mAudioProfile, InteriorInstance));
   addField("sfxEnvironment",  TypeSFXEnvironmentPtr,   Offset(mAudioEnvironment, InteriorInstance));
   endGroup("Audio");   

   addGroup("Misc"); 
   addField("showTerrainInside", TypeBool,                  Offset(mShowTerrainInside, InteriorInstance));
   addField("smoothLighting", TypeBool,                  Offset(mSmoothLighting, InteriorInstance));
   endGroup("Misc");

   Parent::initPersistFields();
}

void InteriorInstance::consoleInit()
{
   //-------------------------------------- Class level variables
   Con::addVariable("pref::Interior::ShowEnvironmentMaps", TypeBool, &Interior::smRenderEnvironmentMaps);
   Con::addVariable("pref::Interior::VertexLighting", TypeBool, &Interior::smUseVertexLighting);
   Con::addVariable("pref::Interior::TexturedFog", TypeBool, &Interior::smUseTexturedFog);
   Con::addVariable("pref::Interior::lockArrays", TypeBool, &Interior::smLockArrays);

   Con::addVariable("pref::Interior::detailAdjust", TypeF32, &InteriorInstance::smDetailModification);

   // DEBUG ONLY!!!
#ifndef TORQUE_SHIPPING
   Con::addVariable("Interior::DontRestrictOutside", TypeBool, &smDontRestrictOutside);
#endif
}

//--------------------------------------------------------------------------
void InteriorInstance::renewOverlays()
{
/*
   StringTableEntry baseName = dStricmp(mSkinBase, "base") == 0 ? "blnd" : mSkinBase;

   for (U32 i = 0; i < mMaterialMaps.size(); i++) {
      MaterialList* pMatList = mMaterialMaps[i];

      for (U32 j = 0; j < pMatList->mMaterialNames.size(); j++) {
         const char* pName     = pMatList->mMaterialNames[j];
         const U32 len = dStrlen(pName);
         if (len < 6)
            continue;

         const char* possible = pName + (len - 5);
         if (dStricmp(".blnd", possible) == 0) {
            char newName[256];
            AssertFatal(len < 200, "InteriorInstance::renewOverlays: Error, len exceeds allowed name length");

            dStrncpy(newName, pName, possible - pName);
            newName[possible - pName] = '\0';
            dStrcat(newName, ".");
            dStrcat(newName, baseName);

            TextureHandle test = TextureHandle(newName, MeshTexture, false);
            if (test.getGLName() != 0) {
               pMatList->mMaterials[j] = test;
            } else {
               pMatList->mMaterials[j] = TextureHandle(pName, MeshTexture, false);
            }
         }
      }
   }
*/
}


//--------------------------------------------------------------------------
void InteriorInstance::setSkinBase(const char* newBase)
{
   if (dStricmp(mSkinBase, newBase) == 0)
      return;

   mSkinBase = StringTable->insert(newBase);

   if (isServerObject())
      setMaskBits(SkinBaseMask);
   else
      renewOverlays();
}

#ifdef TORQUE_COLLADA
//--------------------------------------------------------------------------
void InteriorInstance::exportToCollada(bool bakeTransform)
{
   if (mInteriorRes->getNumDetailLevels() == 0)
   {
      Con::errorf("InteriorInstance::exportToCollada() called an InteriorInstance with no Interior");
      return;
   }

   // For now I am only worrying about the highest lod
   Interior* pInterior = mInteriorRes->getDetailLevel(0);

   if (!pInterior)
   {
      Con::errorf("InteriorInstance::exportToCollada() called an InteriorInstance with an invalid Interior");
      return;
   }

   // Get an optimized version of our mesh
   OptimizedPolyList interiorMesh;

   if (bakeTransform)
   {
      MatrixF mat = getTransform();
      Point3F scale = getScale();

      pInterior->buildExportPolyList(interiorMesh, &mat, &scale);
   }
   else
      pInterior->buildExportPolyList(interiorMesh);

   // Get our export path
   Torque::Path colladaFile = mInteriorRes.getPath();

   // Make sure to set our Collada extension
   colladaFile.setExtension("dae");

   // Use the InteriorInstance name if possible
   String meshName = getName();

   // Otherwise use the DIF's file name
   if (meshName.isEmpty())
      meshName = colladaFile.getFileName();

   // If we are baking the transform then append
   // a CRC version of the transform to the mesh/file name
   if (bakeTransform)
   {
      F32 trans[19];

      const MatrixF& mat = getTransform();
      const Point3F& scale = getScale();

      // Copy in the transform
      for (U32 i = 0; i < 4; i++)
      {
         for (U32 j = 0; j < 4; j++)
         {
            trans[i * 4 + j] = mat(i, j);
         }
      }

      // Copy in the scale
      trans[16] = scale.x;
      trans[17] = scale.y;
      trans[18] = scale.z;

      U32 crc = CRC::calculateCRC(trans, sizeof(F32) * 19);

      meshName += String::ToString("_%x", crc);
   }

   // Set the file name as the meshName
   colladaFile.setFileName(meshName);

   // Use a ColladaUtils function to do the actual export to a Collada file
   ColladaUtils::exportToCollada(colladaFile, interiorMesh, meshName);
}
#endif

//--------------------------------------------------------------------------
// from resManager.cc
extern U32 calculateCRC(void * buffer, S32 len, U32 crcVal );

bool InteriorInstance::onAdd()
{
   if(! loadInterior())
      return false;

   if(!Parent::onAdd())
      return false;

   addToScene();

   if ( gPhysicsPlugin )
      mPhysicsRep = gPhysicsPlugin->createStatic( this );

   return true;
}


void InteriorInstance::onRemove()
{
   SAFE_DELETE( mPhysicsRep );

   unloadInterior();

   removeFromScene();

   Parent::onRemove();
}

//-----------------------------------------------------------------------------

bool InteriorInstance::loadInterior()
{
   U32 i;

   // Load resource
   mInteriorRes = ResourceManager::get().load(mInteriorFileName);
   if (bool(mInteriorRes) == false) {
      Con::errorf(ConsoleLogEntry::General, "Unable to load interior: %s", mInteriorFileName);
      NetConnection::setLastError("Unable to load interior: %s", mInteriorFileName);
      return false;
   }
   if(isClientObject())
   {
      if(mCRC != mInteriorRes.getChecksum())
      {
         NetConnection::setLastError("Local interior file '%s' does not match version on server.", mInteriorFileName);
         return false;
      }
      for (i = 0; i < mInteriorRes->getNumDetailLevels(); i++) {
         // ok, if the material list load failed...
         // if this is a local connection, we'll assume that's ok
         // and just have white textures...
         // otherwise we want to return false.
         Interior* pInterior = mInteriorRes->getDetailLevel(i);
         if(!pInterior->prepForRendering(mInteriorRes.getPath().getFullPath().c_str()) )
         {
            if(!bool(mServerObject))
            {
               return false;
            }
         }
      }

      // copy planar reflect list from top detail level - for now
      Interior* pInterior = mInteriorRes->getDetailLevel(0);
      if( pInterior->mReflectPlanes.size() )
      {
         for ( i = 0; i < pInterior->mReflectPlanes.size(); i++ )
         {
            mPlaneReflectors.increment();
            PlaneReflector &plane = mPlaneReflectors.last();

            plane.refplane = pInterior->mReflectPlanes[i];
            plane.objectSpace = true;
            plane.registerReflector( this, &mReflectorDesc );
         }         
      }

   }
   else
      mCRC = mInteriorRes.getChecksum();

   // Ok, everything's groovy!  Let's cache our hashed filename for renderimage sorting...
   mInteriorFileHash = _StringTable::hashString(mInteriorFileName);

   // Setup bounding information
   mObjBox = mInteriorRes->getDetailLevel(0)->getBoundingBox();
   resetWorldBox();
   setRenderTransform(mObjToWorld);


   // Do any handle loading, etc. required.

   if (isClientObject()) {

      for (i = 0; i < mInteriorRes->getNumDetailLevels(); i++) {
         Interior* pInterior = mInteriorRes->getDetailLevel(i);

         // Force the lightmap manager to download textures if we're
         // running the mission editor.  Normally they are only
         // downloaded after the whole scene is lit.
         gInteriorLMManager.addInstance(pInterior->getLMHandle(), mLMHandle, this);
         if (gEditingMission)  {
            gInteriorLMManager.useBaseTextures(pInterior->getLMHandle(), mLMHandle);
            gInteriorLMManager.downloadGLTextures(pInterior->getLMHandle());
         }

         // Install material list
         //         mMaterialMaps.push_back(new MaterialList(pInterior->mMaterialList));
      }

      renewOverlays();
   } else {

   }

   setMaskBits(0xffffffff);
   return true;
}

void InteriorInstance::unloadInterior()
{
   mConvexList->nukeList();
   delete mConvexList;
   mConvexList = new Convex;

   if(isClientObject())
   {
      if(bool(mInteriorRes) && mLMHandle != 0xFFFFFFFF)
      {
         for(U32 i = 0; i < mInteriorRes->getNumDetailLevels(); i++)
         {
            Interior * pInterior = mInteriorRes->getDetailLevel(i);
            if (pInterior->getLMHandle() != 0xFFFFFFFF)
               gInteriorLMManager.removeInstance(pInterior->getLMHandle(), mLMHandle);
         }
      }
      
      if( mPlaneReflectors.size() )
      {
         for ( U32 i = 0; i < mPlaneReflectors.size(); i++ )
         {
            mPlaneReflectors[i].unregisterReflector();
         }         
         mPlaneReflectors.clear();
      }
   }
}

//--------------------------------------------------------------------------
bool InteriorInstance::onSceneAdd(SceneGraph* pGraph)
{
   AssertFatal(mInteriorRes, "Error, should not have been added to the scene if there's no interior!");

   if (Parent::onSceneAdd(pGraph) == false)
      return false;

   U32 maxNumZones = 0;

   for (U32 i = 0; i < mInteriorRes->getNumDetailLevels(); i++)
   {
      if (mInteriorRes->getDetailLevel(i)->mZones.size() > maxNumZones)
         maxNumZones = mInteriorRes->getDetailLevel(i)->mZones.size();
   }

   if (maxNumZones > 1)
   {
      AssertWarn(getNumCurrZones() == 1, "There should be one and only one zone for an interior that manages zones");
      mSceneManager->registerZones(this, (maxNumZones - 1));
   }

   return true;
}


//--------------------------------------------------------------------------
void InteriorInstance::onSceneRemove()
{
   AssertFatal(mInteriorRes, "Error, should not have been added to the scene if there's no interior!");

   if (isManagingZones())
      mSceneManager->unregisterZones(this);

   Parent::onSceneRemove();
}


//--------------------------------------------------------------------------
bool InteriorInstance::getOverlappingZones(SceneObject* obj,
                                           U32*         zones,
                                           U32*         numZones)
{
   MatrixF xForm(true);
   Point3F invScale(1.0f / getScale().x,
                    1.0f / getScale().y,
                    1.0f / getScale().z);
   xForm.scale(invScale);
   xForm.mul(getWorldTransform());
   xForm.mul(obj->getTransform());
   xForm.scale(obj->getScale());

   U32 waterMark = FrameAllocator::getWaterMark();

   U16* zoneVector = (U16*)FrameAllocator::alloc(mInteriorRes->getDetailLevel(0)->mZones.size() * sizeof(U16));
   U32 numRetZones = 0;

   bool outsideToo = mInteriorRes->getDetailLevel(0)->scanZones(obj->getObjBox(),
                                                                xForm,
                                                                zoneVector,
                                                                &numRetZones);
   if (numRetZones > SceneObject::MaxObjectZones) {
      Con::warnf(ConsoleLogEntry::General, "Too many zones returned for query on %s.  Returning first %d",
                 mInteriorFileName, SceneObject::MaxObjectZones);
   }

   for (U32 i = 0; i < getMin(numRetZones, U32(SceneObject::MaxObjectZones)); i++)
      zones[i] = zoneVector[i] + mZoneRangeStart - 1;
   *numZones = numRetZones;

   FrameAllocator::setWaterMark(waterMark);

   return outsideToo;
}


//--------------------------------------------------------------------------
U32 InteriorInstance::getPointZone(const Point3F& p)
{
   AssertFatal(mInteriorRes, "Error, no interior!");

   Point3F osPoint = p;
   mWorldToObj.mulP(osPoint);
   osPoint.convolveInverse(mObjScale);

   S32 zone = mInteriorRes->getDetailLevel(0)->getZoneForPoint(osPoint);

   // If we're in solid (-1) or outside, we need to return 0
   if (zone == -1 || zone == 0)
      return 0;

   return (zone-1) + mZoneRangeStart;
}

// does a hack check to determine how much a point is 'inside'.. should have
// portals prebuilt with the transfer energy to each other portal in the zone
// from the neighboring zone.. these values can be used to determine the factor
// from within an individual zone.. also, each zone could be marked with
// average material property for eax environment audio
// ~0: outside -> 1: inside
bool InteriorInstance::getPointInsideScale(const Point3F & pos, F32 * pScale)
{
   AssertFatal(mInteriorRes, "InteriorInstance::getPointInsideScale: no interior");

   Interior * interior = mInteriorRes->getDetailLevel(0);

   Point3F p = pos;
   mWorldToObj.mulP(p);
   p.convolveInverse(mObjScale);

   U32 zoneIndex = interior->getZoneForPoint(p);
   if(zoneIndex == -1)  // solid?
   {
      *pScale = 1.f;
      return(true);
   }
   else if(zoneIndex == 0) // outside?
   {
      *pScale = 0.f;
      return(true);
   }

   U32 waterMark = FrameAllocator::getWaterMark();
   const Interior::Portal** portals = (const Interior::Portal**)FrameAllocator::alloc(256 * sizeof(const Interior::Portal*));
   U32 numPortals = 0;

   Interior::Zone & zone = interior->mZones[zoneIndex];

   U32 i;
   for(i = 0; i < zone.portalCount; i++)
   {
      const Interior::Portal & portal = interior->mPortals[interior->mZonePortalList[zone.portalStart + i]];
      if(portal.zoneBack == 0 || portal.zoneFront == 0) {
         AssertFatal(numPortals < 256, "Error, overflow in temporary portal buffer!");
         portals[numPortals++] = &portal;
      }
   }

   // inside?
   if(numPortals == 0)
   {
      *pScale = 1.f;

      FrameAllocator::setWaterMark(waterMark);
      return(true);
   }

   Point3F* portalCenters = (Point3F*)FrameAllocator::alloc(numPortals * sizeof(Point3F));
   U32 numPortalCenters = 0;

   // scale using the distances to the portals in this zone...
   for(i = 0; i < numPortals; i++)
   {
      const Interior::Portal * portal = portals[i];
      if(!portal->triFanCount)
         continue;

      Point3F center(0, 0, 0);
      for(U32 j = 0; j < portal->triFanCount; j++)
      {
         const Interior::TriFan & fan = interior->mWindingIndices[portal->triFanStart + j];
         U32 numPoints = fan.windingCount;

         if(!numPoints)
            continue;

         for(U32 k = 0; k < numPoints; k++)
         {
            const Point3F & a = interior->mPoints[interior->mWindings[fan.windingStart + k]].point;
            center += a;
         }

         center /= (F32)numPoints;
         portalCenters[numPortalCenters++] = center;
      }
   }

   // 'magic' check here...
   F32 magic = Con::getFloatVariable("Interior::insideDistanceFalloff", 10.f);

   F32 val = 0.f;
   for(i = 0; i < numPortalCenters; i++)
      val += 1.f - mClampF(Point3F(portalCenters[i] - p).len() / magic, 0.f, 1.f);

   *pScale = 1.f - mClampF(val, 0.f, 1.f);

   FrameAllocator::setWaterMark(waterMark);
   return(true);
}

//--------------------------------------------------------------------------
// renderObject - this function is called pretty much only for debug rendering
//--------------------------------------------------------------------------
void InteriorInstance::renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance* overrideMat )
{
#ifndef TORQUE_SHIPPING
   if (Interior::smRenderMode == 0)
      return;

   if (overrideMat)
      return;

   if(gEditingMission && isHidden())
      return;

   U32 detailLevel = 0;
   detailLevel = calcDetailLevel(state, state->getCameraPosition());

   Interior* pInterior = mInteriorRes->getDetailLevel( detailLevel );

   if (!pInterior)
      return;

   PROFILE_START( IRO_DebugRender );

   GFX->pushWorldMatrix();

   // setup world matrix - for fixed function
   MatrixF world = GFX->getWorldMatrix();
   world.mul( getRenderTransform() );
   world.scale( getScale() );
   GFX->setWorldMatrix( world );

   // setup world matrix - for shaders
   MatrixF proj = GFX->getProjectionMatrix();
   proj.mul(world);

   SceneGraphData sgData;

   sgData = pInterior->setupSceneGraphInfo( this, state );
   ZoneVisDeterminer zoneVis = pInterior->setupZoneVis( this, state );
   pInterior->debugRender( zoneVis, sgData, this, proj );

   GFX->popWorldMatrix();

   PROFILE_END();
#endif
}


//--------------------------------------------------------------------------
bool InteriorInstance::scopeObject(const Point3F&        rootPosition,
                                   const F32             /*rootDistance*/,
                                   bool*                 zoneScopeState)
{
   AssertFatal(isManagingZones(), "Error, should be a zone manager if we are called on to scope the scene!");
   if (bool(mInteriorRes) == false)
      return false;

   Interior* pInterior = getDetailLevel(0);
   AssertFatal(pInterior->mZones.size() <= csgMaxZoneSize, "Error, too many zones!  Increase max");
   bool* pInteriorScopingState = sgScopeBoolArray;
   dMemset(pInteriorScopingState, 0, sizeof(bool) * pInterior->mZones.size());

   // First, let's transform the point into the interior's space
   Point3F interiorRoot = rootPosition;
   getWorldTransform().mulP(interiorRoot);
   interiorRoot.convolveInverse(getScale());

   S32 realStartZone = getPointZone(rootPosition);
   if (realStartZone != 0)
      realStartZone = realStartZone - mZoneRangeStart + 1;

   bool continueOut = pInterior->scopeZones(realStartZone,
                                            interiorRoot,
                                            pInteriorScopingState);

   // Copy pInteriorScopingState to zoneScopeState
   for (S32 i = 1; i < pInterior->mZones.size(); i++)
      zoneScopeState[i + mZoneRangeStart - 1] = pInteriorScopingState[i];

   return continueOut;
}


//--------------------------------------------------------------------------
U32 InteriorInstance::calcDetailLevel(SceneState* state, const Point3F& wsPoint)
{
   AssertFatal(mInteriorRes, "Error, should not try to calculate the deatil level without a resource to work with!");
   AssertFatal(getNumCurrZones() > 0, "Error, must belong to a zone for this to work");

   if (smDetailModification < 0.3f)
      smDetailModification = 0.3f;
   if (smDetailModification > 1.0f)
      smDetailModification = 1.0f;

   // Early out for simple interiors
   if (mInteriorRes->getNumDetailLevels() == 1)
      return 0;

   if((mForcedDetailLevel >= 0) && (mForcedDetailLevel < mInteriorRes->getNumDetailLevels()))
      return(mForcedDetailLevel);

   Point3F osPoint = wsPoint;
   mRenderWorldToObj.mulP(osPoint);
   osPoint.convolveInverse(mObjScale);

   // First, see if the point is in the object space bounding box of the highest detail
   //  If it is, then the detail level is zero.
   if (mObjBox.isContained(osPoint))
      return 0;

   // Otherwise, we're going to have to do some ugly trickery to get the projection.
   //  I've stolen the worldToScreenScale from dglMatrix, we'll have to calculate the
   //  projection of the bounding sphere of the lowest detail level.
   //  worldToScreenScale = (near * view.extent.x) / (right - left)
   RectI viewport;
   F64   frustum[4] = { 1e10, -1e10, 1e10, -1e10 };

   bool init = false;
   SceneObjectRef* pWalk = mZoneRefHead;
   AssertFatal(pWalk != NULL, "Error, object must exist in at least one zone to call this!");
   while (pWalk) {
      const SceneState::ZoneState& rState = state->getZoneState(pWalk->zone);
      if (rState.render == true) 
      {
         // frustum

         const F32 left = rState.frustum.getNearLeft();
         const F32 right = rState.frustum.getNearRight();
         const F32 bottom = rState.frustum.getNearBottom();
         const F32 top = rState.frustum.getNearTop();

         if (left < frustum[0])     frustum[0] = left;
         if (right > frustum[1])    frustum[1] = right;
         if (bottom < frustum[2])   frustum[2] = bottom;
         if (top > frustum[3])      frustum[3] = top;

         // viewport
         if (init == false)
            viewport = rState.viewport;
         else
            viewport.unionRects(rState.viewport);

         init = true;
      }
      pWalk = pWalk->nextInObj;
   }
   AssertFatal(init, "Error, at least one zone must be rendered here!");

   F32 worldToScreenScale   = (state->getNearPlane() * viewport.extent.x) / (frustum[1] - frustum[0]);
   const SphereF& lowSphere = mInteriorRes->getDetailLevel(mInteriorRes->getNumDetailLevels() - 1)->mBoundingSphere;
   F32 dist                 = (lowSphere.center - osPoint).len();
   F32 projRadius           = (lowSphere.radius / dist) * worldToScreenScale;

   // Scale the projRadius based on the objects maximum scale axis
   projRadius *= getMax(mFabs(mObjScale.x), getMax(mFabs(mObjScale.y), mFabs(mObjScale.z)));

   // Multiply based on detail preference...
   projRadius *= smDetailModification;

   // Ok, now we have the projected radius, we need to search through the interiors to
   //  find the largest interior that will support this projection.
   U32 final = mInteriorRes->getNumDetailLevels() - 1;
   for (U32 i = 0; i< mInteriorRes->getNumDetailLevels() - 1; i++) {
      Interior* pDetail = mInteriorRes->getDetailLevel(i);

      if (pDetail->mMinPixels < projRadius) {
         final = i;
         break;
      }
   }

   // Ok, that's it.
   return final;
}


//--------------------------------------------------------------------------
bool InteriorInstance::prepRenderImage(SceneState* state,   const U32 stateKey,
                                       const U32 startZone, const bool modifyBaseState)
{
   if (isLastState(state, stateKey))
      return false;

   if(gEditingMission && isHidden())
      return false;

   PROFILE_START(InteriorPrepRenderImage);

   setLastState(state, stateKey);

   U32 realStartZone;
   if (startZone != 0xFFFFFFFF) {
      AssertFatal(startZone != 0, "Hm.  This really shouldn't happen.  Should only get inside zones here");
      AssertFatal(isManagingZones(), "Must be managing zones if we're here...");

      realStartZone = startZone - mZoneRangeStart + 1;
   } else {
      realStartZone = getPointZone(state->getCameraPosition());
      if (realStartZone != 0)
         realStartZone = realStartZone - mZoneRangeStart + 1;
   }

   if (modifyBaseState == false) 
   {
      // Regular query.  We only return a render zone if our parent zone is rendered.
      //  Otherwise, we always render
      if (state->isObjectRendered(this) == false)
      {
         PROFILE_END();
         return false;
      }
   } 
   else 
   {
      if (mShowTerrainInside == true)
         state->enableTerrainOverride();
   }

   U32 detailLevel = 0;
   if (startZone == 0xFFFFFFFF)
      detailLevel = calcDetailLevel(state, state->getCameraPosition());


   U32 baseZoneForPrep = getCurrZone(0);
   bool multipleZones = false;
   if (getNumCurrZones() > 1) {
      U32 numRenderedZones = 0;
      baseZoneForPrep = 0xFFFFFFFF;
      for (U32 i = 0; i < getNumCurrZones(); i++) {
         if (state->getZoneState(getCurrZone(i)).render == true) {
            numRenderedZones++;
            if (baseZoneForPrep == 0xFFFFFFFF)
               baseZoneForPrep = getCurrZone(i);
         }
      }

      if (numRenderedZones > 1)
         multipleZones = true;
   }

   bool continueOut = mInteriorRes->getDetailLevel(0)->prepRender(state,
                                                                  baseZoneForPrep,
                                                                  realStartZone, mZoneRangeStart,
                                                                  mRenderObjToWorld, mObjScale,
                                                                  modifyBaseState & !smDontRestrictOutside,
                                                                  smDontRestrictOutside | multipleZones,
                                                                  state->isInvertedCull() );
   if (smDontRestrictOutside)
      continueOut = true;


   // need to delay the batching because zone information is not complete until
   // the entire scene tree is built.
   SceneState::InteriorListElem elem;
   elem.obj = this;
   elem.stateKey = stateKey;
   elem.startZone = 0xFFFFFFFF;
   elem.detailLevel = detailLevel;
   elem.worldXform = state->getRenderPass()->allocSharedXform(RenderPassManager::View);

   state->insertInterior( elem );
   

   PROFILE_END();
   return continueOut;
}


//--------------------------------------------------------------------------
bool InteriorInstance::castRay(const Point3F& s, const Point3F& e, RayInfo* info)
{
   info->object = this;
   return mInteriorRes->getDetailLevel(0)->castRay(s, e, info);
}

//------------------------------------------------------------------------------
void InteriorInstance::setTransform(const MatrixF & mat)
{
   Parent::setTransform(mat);

   // Since the interior is a static object, it's render transform changes 1 to 1
   //  with it's collision transform
   setRenderTransform(mat);

   if (isServerObject())
      setMaskBits(TransformMask);
}


//------------------------------------------------------------------------------
bool InteriorInstance::buildPolyList(AbstractPolyList* list, const Box3F& wsBox, const SphereF&)
{
   if (bool(mInteriorRes) == false)
      return false;

   // Setup collision state data
   list->setTransform(&getTransform(), getScale());
   list->setObject(this);

   return mInteriorRes->getDetailLevel(0)->buildPolyList(list, wsBox, mWorldToObj, getScale());
}



void InteriorInstance::buildConvex(const Box3F& box, Convex* convex)
{
   if (bool(mInteriorRes) == false)
      return;

   mConvexList->collectGarbage();

   Box3F realBox = box;
   mWorldToObj.mul(realBox);
   realBox.minExtents.convolveInverse(mObjScale);
   realBox.maxExtents.convolveInverse(mObjScale);

   if (realBox.isOverlapped(getObjBox()) == false)
      return;

   U32 waterMark = FrameAllocator::getWaterMark();

   if ((convex->getObject()->getType() & VehicleObjectType) &&
       mInteriorRes->getDetailLevel(0)->mVehicleConvexHulls.size() > 0)
   {
      // Can never have more hulls than there are hulls in the interior...
      U16* hulls = (U16*)FrameAllocator::alloc(mInteriorRes->getDetailLevel(0)->mVehicleConvexHulls.size() * sizeof(U16));
      U32 numHulls = 0;

      Interior* pInterior = mInteriorRes->getDetailLevel(0);
      if (pInterior->getIntersectingVehicleHulls(realBox, hulls, &numHulls) == false) {
         FrameAllocator::setWaterMark(waterMark);
         return;
      }

      for (U32 i = 0; i < numHulls; i++) {
         // See if this hull exists in the working set already...
         Convex* cc = 0;
         CollisionWorkingList& wl = convex->getWorkingList();
         for (CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext) {
            if (itr->mConvex->getType() == InteriorConvexType &&
                (static_cast<InteriorConvex*>(itr->mConvex)->getObject() == this &&
                 static_cast<InteriorConvex*>(itr->mConvex)->hullId    == -S32(hulls[i] + 1))) {
               cc = itr->mConvex;
               break;
            }
         }
         if (cc)
            continue;

         // Create a new convex.
         InteriorConvex* cp = new InteriorConvex;
         mConvexList->registerObject(cp);
         convex->addToWorkingList(cp);
         cp->mObject   = this;
         cp->pInterior = pInterior;
         cp->hullId    = -S32(hulls[i] + 1);
         cp->box.minExtents.x = pInterior->mVehicleConvexHulls[hulls[i]].minX;
         cp->box.minExtents.y = pInterior->mVehicleConvexHulls[hulls[i]].minY;
         cp->box.minExtents.z = pInterior->mVehicleConvexHulls[hulls[i]].minZ;
         cp->box.maxExtents.x = pInterior->mVehicleConvexHulls[hulls[i]].maxX;
         cp->box.maxExtents.y = pInterior->mVehicleConvexHulls[hulls[i]].maxY;
         cp->box.maxExtents.z = pInterior->mVehicleConvexHulls[hulls[i]].maxZ;
      }
   }
   else
   {
      // Can never have more hulls than there are hulls in the interior...
      U16* hulls = (U16*)FrameAllocator::alloc(mInteriorRes->getDetailLevel(0)->mConvexHulls.size() * sizeof(U16));
      U32 numHulls = 0;

      Interior* pInterior = mInteriorRes->getDetailLevel(0);
      if (pInterior->getIntersectingHulls(realBox, hulls, &numHulls) == false) {
         FrameAllocator::setWaterMark(waterMark);
         return;
      }

      for (U32 i = 0; i < numHulls; i++) {
         // See if this hull exists in the working set already...
         Convex* cc = 0;
         CollisionWorkingList& wl = convex->getWorkingList();
         for (CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext) {
            if (itr->mConvex->getType() == InteriorConvexType &&
                (static_cast<InteriorConvex*>(itr->mConvex)->getObject() == this &&
                 static_cast<InteriorConvex*>(itr->mConvex)->hullId    == hulls[i])) {
               cc = itr->mConvex;
               break;
            }
         }
         if (cc)
            continue;

         // Create a new convex.
         InteriorConvex* cp = new InteriorConvex;
         mConvexList->registerObject(cp);
         convex->addToWorkingList(cp);
         cp->mObject   = this;
         cp->pInterior = pInterior;
         cp->hullId    = hulls[i];
         cp->box.minExtents.x = pInterior->mConvexHulls[hulls[i]].minX;
         cp->box.minExtents.y = pInterior->mConvexHulls[hulls[i]].minY;
         cp->box.minExtents.z = pInterior->mConvexHulls[hulls[i]].minZ;
         cp->box.maxExtents.x = pInterior->mConvexHulls[hulls[i]].maxX;
         cp->box.maxExtents.y = pInterior->mConvexHulls[hulls[i]].maxY;
         cp->box.maxExtents.z = pInterior->mConvexHulls[hulls[i]].maxZ;
      }
   }
   FrameAllocator::setWaterMark(waterMark);
}


//------------------------------------------------------------------------------
U32 InteriorInstance::packUpdate(NetConnection* c, U32 mask, BitStream* stream)
{
   U32 retMask = Parent::packUpdate(c, mask, stream);

   if (stream->writeFlag((mask & InitMask) != 0)) {
      // Initial update, write the whole kit and kaboodle
      stream->write(mCRC);

      stream->writeString(mInteriorFileName);
      stream->writeFlag(mShowTerrainInside);
      stream->writeFlag(mSmoothLighting);

      // Write the transform (do _not_ use writeAffineTransform.  Since this is a static
      //  object, the transform must be RIGHT THE *&)*$&^ ON or it will goof up the
      //  syncronization between the client and the server.
      mathWrite(*stream, mObjToWorld);
      mathWrite(*stream, mObjScale);

      // Write the alarm state
      stream->writeFlag(mAlarmState);

      // Write the skinbase
      stream->writeString(mSkinBase);

      // audio profile
      if(stream->writeFlag(mAudioProfile))
         stream->writeRangedU32(mAudioProfile->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);

      // audio environment:
      if(stream->writeFlag(mAudioEnvironment))
         stream->writeRangedU32(mAudioEnvironment->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);

   }
   else
   {
      if (stream->writeFlag((mask & TransformMask) != 0)) {
         mathWrite(*stream, mObjToWorld);
         mathWrite(*stream, mObjScale);
      }

      stream->writeFlag(mAlarmState);


      if (stream->writeFlag(mask & SkinBaseMask))
         stream->writeString(mSkinBase);

      // audio update:
      if(stream->writeFlag(mask & AudioMask))
      {
         // profile:
         if(stream->writeFlag(mAudioProfile))
            stream->writeRangedU32(mAudioProfile->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);

         // environment:
         if(stream->writeFlag(mAudioEnvironment))
            stream->writeRangedU32(mAudioEnvironment->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);
      }
   }

   return retMask;
}


//------------------------------------------------------------------------------
void InteriorInstance::unpackUpdate(NetConnection* c, BitStream* stream)
{
   Parent::unpackUpdate(c, stream);

   MatrixF temp;
   Point3F tempScale;

   if (stream->readFlag()) {
      bool isNewUpdate(mInteriorRes);

      if(isNewUpdate)
         unloadInterior();

      // Initial Update
      // CRC
      stream->read(&mCRC);

      // File
      mInteriorFileName = stream->readSTString();

      // Terrain flag
      mShowTerrainInside = stream->readFlag();

      //Smooth lighting flag
      mSmoothLighting = stream->readFlag();

      // Transform
      mathRead(*stream, &temp);
      mathRead(*stream, &tempScale);
      setScale(tempScale);
      setTransform(temp);

      // Alarm state: Note that we handle this ourselves on the initial update
      //  so that the state is always full on or full off...
      mAlarmState = stream->readFlag();

      mSkinBase = stream->readSTString();

      // audio profile:
      if(stream->readFlag())
      {
         U32 profileId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
         mAudioProfile = dynamic_cast<SFXProfile*>(Sim::findObject(profileId));
      }
      else
         mAudioProfile = 0;

      // audio environment:
      if(stream->readFlag())
      {
         U32 profileId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
         mAudioEnvironment = dynamic_cast<SFXEnvironment*>( Sim::findObject( profileId ) );
      }
      else
         mAudioEnvironment = 0;

      if(isNewUpdate)
      {
         if(! loadInterior())
            Con::errorf("InteriorInstance::unpackUpdate - Unable to load new interior");
      }
   }
   else
   {
      // Normal update
      if (stream->readFlag()) {
         mathRead(*stream, &temp);
         mathRead(*stream, &tempScale);
         setScale(tempScale);
         setTransform(temp);
      }

      setAlarmMode(stream->readFlag());


      if (stream->readFlag()) {
         mSkinBase = stream->readSTString();
         renewOverlays();
      }

      // audio update:
      if(stream->readFlag())
      {
         // profile:
         if(stream->readFlag())
         {
            U32 profileId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
            mAudioProfile = dynamic_cast<SFXProfile*>( Sim::findObject( profileId ) );
         }
         else
            mAudioProfile = 0;

         // environment:
         if(stream->readFlag())
         {
            U32 profileId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
            mAudioEnvironment = dynamic_cast<SFXEnvironment*>( Sim::findObject( profileId ) );
         }
         else
            mAudioEnvironment = 0;
      }
   }
}


//------------------------------------------------------------------------------
Interior* InteriorInstance::getDetailLevel(const U32 level)
{
   return mInteriorRes->getDetailLevel(level);
}

U32 InteriorInstance::getNumDetailLevels()
{
   return mInteriorRes->getNumDetailLevels();
}

//--------------------------------------------------------------------------
//-------------------------------------- Alarm functionality
//
void InteriorInstance::setAlarmMode(const bool alarm)
{
   if (mInteriorRes->getDetailLevel(0)->mHasAlarmState == false)
      return;

   if (mAlarmState == alarm)
      return;

   mAlarmState = alarm;
   if (isServerObject())
   {
      setMaskBits(AlarmMask);
   }
   else
   {
      // DMMTODO: Invalidate current light state
   }
}



void InteriorInstance::createTriggerTransform(const InteriorResTrigger* trigger, MatrixF* transform)
{
   Point3F offset;
   MatrixF xform = getTransform();
   xform.getColumn(3, &offset);

   Point3F triggerOffset = trigger->mOffset;
   triggerOffset.convolve(mObjScale);
   getTransform().mulV(triggerOffset);
   offset += triggerOffset;
   xform.setColumn(3, offset);

   *transform = xform;
}


bool InteriorInstance::readLightmaps(GBitmap**** lightmaps)
{
   AssertFatal(mInteriorRes, "Error, no interior loaded!");
   AssertFatal(lightmaps, "Error, no lightmaps or numdetails result pointers");
   AssertFatal(*lightmaps == NULL, "Error, already have a pointer in the lightmaps result field!");

   // Load resource
   FileStream* pStream;
   if((pStream = FileStream::createAndOpen( mInteriorFileName, Torque::FS::File::Read )) == NULL)
   {
      Con::errorf(ConsoleLogEntry::General, "Unable to load interior: %s", mInteriorFileName);
      return false;
   }

   InteriorResource* pResource = new InteriorResource;
   bool success = pResource->read(*pStream);
   delete pStream;

   if (success == false)
   {
      delete pResource;
      return false;
   }
   AssertFatal(pResource->getNumDetailLevels() == mInteriorRes->getNumDetailLevels(),
               "Mismatched detail levels!");

   *lightmaps  = new GBitmap**[mInteriorRes->getNumDetailLevels()];

   for (U32 i = 0; i < pResource->getNumDetailLevels(); i++)
   {
      Interior* pInterior = pResource->getDetailLevel(i);
      (*lightmaps)[i] = new GBitmap*[pInterior->mLightmaps.size()];
      for (U32 j = 0; j < pInterior->mLightmaps.size(); j++)
      {
         ((*lightmaps)[i])[j] = pInterior->mLightmaps[j];
         pInterior->mLightmaps[j] = NULL;
      }
      pInterior->mLightmaps.clear();
   }

   delete pResource;
   return true;
}

S32 InteriorInstance::getSurfaceZone(U32 surfaceindex, Interior *detail)
{
	AssertFatal(((surfaceindex >= 0) && (surfaceindex < detail->surfaceZones.size())), "Bad surface index!");
	S32 zone = detail->surfaceZones[surfaceindex];
	if(zone > -1)
		return zone + mZoneRangeStart;
	return getCurrZone(0);
}

//-----------------------------------------------------------------------------
// Protected Field Accessors
//-----------------------------------------------------------------------------

bool InteriorInstance::setInteriorFile(void* obj, const char* data)
{
   if(data == NULL)
      return true;

   InteriorInstance *inst = static_cast<InteriorInstance *>(obj);

   if(inst->isProperlyAdded())
      inst->unloadInterior();

   inst->mInteriorFileName = StringTable->insert(data);

   if(inst->isProperlyAdded())
   {
      if(! inst->loadInterior())
         Con::errorf("InteriorInstance::setInteriorFile - Unable to load new interior");
   }

   return false;
}

//-----------------------------------------------------------------------------
// Console Functions / Methods
//-----------------------------------------------------------------------------

ConsoleFunctionGroupBegin(Interiors, "");

#ifndef TORQUE_SHIPPING
ConsoleFunction( setInteriorRenderMode, void, 2, 2, "(int modeNum)")
{
   S32 mode = dAtoi(argv[1]);
   if (mode < 0 || mode > Interior::ShowDetailLevel)
      mode = 0;

   Interior::smRenderMode = mode;
}

ConsoleFunction( setInteriorFocusedDebug, void, 2, 2, "(bool enable)")
{
   if (dAtob(argv[1])) {
      Interior::smFocusedDebug = true;
   } else {
      Interior::smFocusedDebug = false;
   }
}

#endif

ConsoleFunction( isPointInside, bool, 2, 4, "(Point3F pos) or (float x, float y, float z)")
{
   static bool lastValue = false;

   if(!(argc == 2 || argc == 4))
   {
      Con::errorf(ConsoleLogEntry::General, "cIsPointInside: invalid parameters");
      return(lastValue);
   }

   Point3F pos;
   if(argc == 2)
      dSscanf(argv[1], "%g %g %g", &pos.x, &pos.y, &pos.z);
   else
   {
      pos.x = dAtof(argv[1]);
      pos.y = dAtof(argv[2]);
      pos.z = dAtof(argv[3]);
   }

   RayInfo collision;
   if(gClientContainer.castRay(pos, Point3F(pos.x, pos.y, pos.z - 2000.f), InteriorObjectType, &collision))
   {
      if(collision.face == -1)
         Con::errorf(ConsoleLogEntry::General, "cIsPointInside: failed to find hit face on interior");
      else
      {
         InteriorInstance * interior = dynamic_cast<InteriorInstance *>(collision.object);
         if(interior)
            lastValue = !interior->getDetailLevel(0)->isSurfaceOutsideVisible(collision.face);
         else
            Con::errorf(ConsoleLogEntry::General, "cIsPointInside: invalid interior on collision");
      }
   }

   return(lastValue);
}


ConsoleFunctionGroupEnd(Interiors);

#ifdef TORQUE_COLLADA
ConsoleMethod( InteriorInstance, exportToCollada, void, 2, 3, "([bool bakeTransform] exports the Interior to a Collada file)")
{
   if (argc == 3)
      object->exportToCollada(dAtob(argv[2]));
   else
      object->exportToCollada();
}
#endif

ConsoleMethod( InteriorInstance, setAlarmMode, void, 3, 3, "(string mode) Mode is 'On' or 'Off'")
{
   AssertFatal(dynamic_cast<InteriorInstance*>(object) != NULL,
      "Error, how did a non-interior get here?");

   bool alarm;
   if (dStricmp(argv[2], "On") == 0)
      alarm = true;
   else
      alarm = false;

   InteriorInstance* interior = static_cast<InteriorInstance*>(object);
   if (interior->isClientObject()) {
      Con::errorf(ConsoleLogEntry::General, "InteriorInstance: client objects may not receive console commands.  Ignored");
      return;
   }

   interior->setAlarmMode(alarm);
}


ConsoleMethod( InteriorInstance, setSkinBase, void, 3, 3, "(string basename)")
{
   AssertFatal(dynamic_cast<InteriorInstance*>(object) != NULL,
      "Error, how did a non-interior get here?");

   InteriorInstance* interior = static_cast<InteriorInstance*>(object);
   if (interior->isClientObject()) {
      Con::errorf(ConsoleLogEntry::General, "InteriorInstance: client objects may not receive console commands.  Ignored");
      return;
   }

   interior->setSkinBase(argv[2]);
}

ConsoleMethod( InteriorInstance, getNumDetailLevels, S32, 2, 2, "")
{
   InteriorInstance * instance = static_cast<InteriorInstance*>(object);
   return(instance->getNumDetailLevels());
}

ConsoleMethod( InteriorInstance, setDetailLevel, void, 3, 3, "(int level)")
{
   InteriorInstance * instance = static_cast<InteriorInstance*>(object);
   if(instance->isServerObject())
   {
      NetConnection * toServer = NetConnection::getConnectionToServer();
      NetConnection * toClient = NetConnection::getLocalClientConnection();
      if(!toClient || !toServer)
         return;

      S32 index = toClient->getGhostIndex(instance);
      if(index == -1)
         return;

      InteriorInstance * clientInstance = dynamic_cast<InteriorInstance*>(toServer->resolveGhost(index));
      if(clientInstance)
         clientInstance->setDetailLevel(dAtoi(argv[2]));
   }
   else
      instance->setDetailLevel(dAtoi(argv[2]));
}

//------------------------------------------------------------------------
//These functions are duplicated in tsStatic, shapeBase, and interiorInstance.
//They each function a little differently; but achieve the same purpose of gathering
//target names/counts without polluting simObject.

ConsoleMethod( InteriorInstance, getTargetName, const char*, 4, 4, "(detailLevel, targetNum)")
{
	S32 detailLevel = dAtoi(argv[2]);
	S32 idx = dAtoi(argv[3]);

	Interior* obj = object->getDetailLevel(detailLevel);

	if(obj)
		return obj->getTargetName(idx);

	return "";
}

ConsoleMethod( InteriorInstance, getTargetCount, S32, 3, 3, "(detailLevel)")
{
	S32 detailLevel = dAtoi(argv[2]);

	Interior* obj = object->getDetailLevel(detailLevel);
	if(obj)
		return obj->getTargetCount();

	return -1;
}

// This method is able to change materials per map to with others. The material that is being replaced is being mapped to
// unmapped_mat as a part of this transition
ConsoleMethod( InteriorInstance, changeMaterial, void, 5, 5, "(mapTo, fromMaterial, ToMaterial)")
{
	// simple parsing through the interiors detail levels looking for the correct mapto.
	// break when we find the correct detail level to depend on.
	U32 level = -1;
	for( U32 i = 0; i < object->getNumDetailLevels(); i++ )
	{
		for( U32 j = 0; j < object->getDetailLevel(i)->getTargetCount(); j++ )
		{
			if( object->getDetailLevel(i)->getTargetName(j).compare( argv[2] ) == 0 )
			{
				level = i;
				break;
			}
		}

		if( level != -1 )
			break;
	}
	
	// kind of lame to do the same check after potentially breaking out early. needs to be done because
	// we dont want to end up with the wrong detail level
	if( level == -1  )
		return;
	
	// initilize server/client versions
	Interior *serverObj = object->getDetailLevel( level );

	InteriorInstance * instanceClientObj = dynamic_cast< InteriorInstance* > ( object->getClientObject() );
	Interior *clientObj = instanceClientObj->getDetailLevel( level );

	if(serverObj)
	{
		// Lets get ready to switch out materials
		Material *oldMat = dynamic_cast<Material*>(Sim::findObject(argv[3]));
		Material *newMat = dynamic_cast<Material*>(Sim::findObject(argv[4]));

		// if no valid new material, theres no reason for doing this
		if( !newMat )
			return;
		
		// Lets remap the old material off, so as to let room for our current material room to claim its spot
		if( oldMat )
			oldMat->mMapTo = String("unmapped_mat");

		newMat->mMapTo = argv[2];
		
		// Map the material in the in the matmgr
		MATMGR->mapMaterial( argv[2], argv[4] );
		
		U32 i = 0;
		// Replace instances with the new material being traded in. Lets make sure that we only
		// target the specific targets per inst. This technically is only done here for interiors for 
		// safe keeping. The remapping that truly matters most (for on the fly changes) are done in the node lists
		for (; i < serverObj->mMaterialList->getMaterialNameList().size(); i++)
		{
			if( String(argv[2]) == serverObj->mMaterialList->getMaterialName(i))
			{
				delete [] clientObj->mMaterialList->mMatInstList[i];
				clientObj->mMaterialList->mMatInstList[i] = newMat->createMatInstance();

				delete [] serverObj->mMaterialList->mMatInstList[i];
				serverObj->mMaterialList->mMatInstList[i] = newMat->createMatInstance();
				break;
			}
		}

		// Finishing the safekeeping
		const GFXVertexFormat *flags = getGFXVertexFormat<GFXVertexPNTTB>();
		FeatureSet features = MATMGR->getDefaultFeatures();
		clientObj->mMaterialList->getMaterialInst(i)->init( features, flags );
		serverObj->mMaterialList->getMaterialInst(i)->init( features, flags );

		// These loops are referenced in interior.cpp's initMatInstances
		// Made a couple of alterations to tailor specifically towards one changing one instance
		for( U32 i=0; i<clientObj->getNumZones(); i++ )
		{
			for( U32 j=0; j<clientObj->mZoneRNList[i].renderNodeList.size(); j++ )
			{
				BaseMatInstance *matInst = clientObj->mZoneRNList[i].renderNodeList[j].matInst;
				Material* refMat = dynamic_cast<Material*>(matInst->getMaterial());

				if(refMat == oldMat)
				{
					clientObj->mZoneRNList[i].renderNodeList[j].matInst = newMat->createMatInstance();
					clientObj->mZoneRNList[i].renderNodeList[j].matInst->init(MATMGR->getDefaultFeatures(), getGFXVertexFormat<GFXVertexPNTTB>());
					
					//if ( pMat )
						//mHasTranslucentMaterials |= pMat->mTranslucent && !pMat->mTranslucentZWrite;
				}
			}
		}
		
		// Lets reset the clientObj settings in order to accomadate the new material
		clientObj->fillSurfaceTexMats();
		clientObj->createZoneVBs();
		clientObj->cloneMatInstances();
		clientObj->createReflectPlanes();
		clientObj->initMatInstances();
	}
}

ConsoleMethod( InteriorInstance, getModelFile, const char *, 2, 2, "getModelFile( String )")
{
	return object->getInteriorFileName();
}