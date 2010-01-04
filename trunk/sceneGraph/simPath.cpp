//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gfx/gfxDevice.h"
#include "gfx/gfxVertexBuffer.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/gfxTransformSaver.h"
#include "sceneGraph/simPath.h"
#include "console/consoleTypes.h"
#include "sceneGraph/pathManager.h"
#include "sceneGraph/sceneState.h"
#include "math/mathIO.h"
#include "core/stream/bitStream.h"
#include "renderInstance/renderPassManager.h"

extern bool gEditingMission;

//--------------------------------------------------------------------------
//-------------------------------------- Console functions and cmp funcs
//
ConsoleFunction( pathOnMissionLoadDone, void, 1, 1, "Load all path information from interiors.")
{
   // Need to load subobjects for all loaded interiors...
   SimGroup* pMissionGroup = dynamic_cast<SimGroup*>(Sim::findObject("MissionGroup"));
   AssertFatal(pMissionGroup != NULL, "Error, mission done loading and no mission group?");

   U32 currStart = 0;
   U32 currEnd   = 1;
   Vector<SimGroup*> groups;
   groups.push_back(pMissionGroup);

   while (true) {
      for (U32 i = currStart; i < currEnd; i++) {
         for (SimGroup::iterator itr = groups[i]->begin(); itr != groups[i]->end(); itr++) {
            if (dynamic_cast<SimGroup*>(*itr) != NULL)
               groups.push_back(static_cast<SimGroup*>(*itr));
         }
      }

      if (groups.size() == currEnd) {
         break;
      } else {
         currStart = currEnd;
         currEnd   = groups.size();
      }
   }

   for (U32 i = 0; i < groups.size(); i++) {
      SimPath::Path* pPath = dynamic_cast<SimPath::Path*>(groups[i]);
      if (pPath)
         pPath->updatePath();
   }
}

S32 FN_CDECL cmpPathObject(const void* p1, const void* p2)
{
   SimObject* o1 = *((SimObject**)p1);
   SimObject* o2 = *((SimObject**)p2);

   Marker* m1 = dynamic_cast<Marker*>(o1);
   Marker* m2 = dynamic_cast<Marker*>(o2);

   if (m1 == NULL && m2 == NULL)
      return 0;
   else if (m1 != NULL && m2 == NULL)
      return 1;
   else if (m1 == NULL && m2 != NULL)
      return -1;
   else {
      // Both markers...
      return S32(m1->mSeqNum) - S32(m2->mSeqNum);
   }
}

namespace SimPath
{

//--------------------------------------------------------------------------
//-------------------------------------- Implementation
//
IMPLEMENT_CONOBJECT(Path);

Path::Path()
{
   mPathIndex = NoPathIndex;
   mIsLooping = true;
}

Path::~Path()
{
   //
}

//--------------------------------------------------------------------------
void Path::initPersistFields()
{
   addField("isLooping",   TypeBool, Offset(mIsLooping,   Path));

   Parent::initPersistFields();
   //
}



//--------------------------------------------------------------------------
bool Path::onAdd()
{
   if(!Parent::onAdd())
      return false;

   return true;
}


void Path::onRemove()
{
   //

   Parent::onRemove();
}



//--------------------------------------------------------------------------
/// Sort the markers objects into sequence order
void Path::sortMarkers()
{
   dQsort(objectList.address(), objectList.size(), sizeof(SimObject*), cmpPathObject);
}

void Path::updatePath()
{
   // If we need to, allocate a path index from the manager
   if (mPathIndex == NoPathIndex)
      mPathIndex = gServerPathManager->allocatePathId();

   sortMarkers();

   Vector<Point3F> positions;
   Vector<QuatF>   rotations;
   Vector<U32>     times;
   Vector<U32>     smoothingTypes;

   for (iterator itr = begin(); itr != end(); itr++)
   {
      Marker* pMarker = dynamic_cast<Marker*>(*itr);
      if (pMarker != NULL)
      {
         Point3F pos;
         pMarker->getTransform().getColumn(3, &pos);
         positions.push_back(pos);

         QuatF rot;
         rot.set(pMarker->getTransform());
         rotations.push_back(rot);

         times.push_back(pMarker->mMSToNext);
         smoothingTypes.push_back(pMarker->mSmoothingType);
      }
   }

   // DMMTODO: Looping paths.
   gServerPathManager->updatePath(mPathIndex, positions, rotations, times, smoothingTypes);
}

void Path::addObject(SimObject* obj)
{
   Parent::addObject(obj);

   if (mPathIndex != NoPathIndex) {
      // If we're already finished, and this object is a marker, then we need to
      //  update our path information...
      if (dynamic_cast<Marker*>(obj) != NULL)
         updatePath();
   }
}

void Path::removeObject(SimObject* obj)
{
   bool recalc = dynamic_cast<Marker*>(obj) != NULL;

   Parent::removeObject(obj);

   if (mPathIndex != NoPathIndex && recalc == true)
      updatePath();
}

ConsoleMethod(Path, getPathId, S32, 2, 2, "getPathId();")
{
   Path *path = static_cast<Path *>(object);
   return path->getPathIndex();
}

} // Namespace

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

GFXStateBlockRef Marker::smStateBlock;
GFXVertexBufferHandle<GFXVertexPC> Marker::smVertexBuffer;
GFXPrimitiveBufferHandle Marker::smPrimitiveBuffer;

static Point3F wedgePoints[4] = {
   Point3F(-1, -1,  0),
   Point3F( 0,  1,  0),
   Point3F( 1, -1,  0),
   Point3F( 0,-.75, .5),
};

void Marker::initGFXResources()
{
   if(smVertexBuffer != NULL)
      return;
      
   GFXStateBlockDesc d;
   d.cullDefined = true;
   d.cullMode = GFXCullNone;
   
   smStateBlock = GFX->createStateBlock(d);
   
   smVertexBuffer.set(GFX, 4, GFXBufferTypeStatic);
   GFXVertexPC* verts = smVertexBuffer.lock();
   verts[0].point = wedgePoints[0] * 0.25f;
   verts[1].point = wedgePoints[1] * 0.25f;
   verts[2].point = wedgePoints[2] * 0.25f;
   verts[3].point = wedgePoints[3] * 0.25f;
   verts[0].color = verts[1].color = verts[2].color = verts[3].color = GFXVertexColor(ColorI(0, 255, 0, 255));
   smVertexBuffer.unlock();
   
   smPrimitiveBuffer.set(GFX, 24, 12, GFXBufferTypeStatic);
   U16* prims;
   smPrimitiveBuffer.lock(&prims);
   prims[0] = 0;
   prims[1] = 3;
   prims[2] = 3;
   prims[3] = 1;
   prims[4] = 1;
   prims[5] = 0;
   
   prims[6] = 3;
   prims[7] = 1;
   prims[8] = 1;
   prims[9] = 2;
   prims[10] = 2;
   prims[11] = 3;
   
   prims[12] = 0;
   prims[13] = 3;
   prims[14] = 3;
   prims[15] = 2;
   prims[16] = 2;
   prims[17] = 0;
   
   prims[18] = 0;
   prims[19] = 2;
   prims[20] = 2;
   prims[21] = 1;
   prims[22] = 1;
   prims[23] = 0;
   smPrimitiveBuffer.unlock();
}

IMPLEMENT_CO_NETOBJECT_V1(Marker);
Marker::Marker()
{
   // Not ghostable unless we're editing...
   mNetFlags.clear(Ghostable);

   mTypeMask = MarkerObjectType;

   mSeqNum   = 0;
   mSmoothingType = SmoothingTypeLinear;
   mMSToNext = 1000;
   mSmoothingType = SmoothingTypeSpline;
   mKnotType = KnotTypeNormal;
}

Marker::~Marker()
{
   //
}

//--------------------------------------------------------------------------
static EnumTable::Enums markerEnums[] =
{
   { Marker::SmoothingTypeSpline , "Spline" },
   { Marker::SmoothingTypeLinear , "Linear" },
   //{ Marker::SmoothingTypeAccelerate , "Accelerate" },
};
static EnumTable markerSmoothingTable(2, &markerEnums[0]);

static EnumTable::Enums knotEnums[] =
{
   { Marker::KnotTypeNormal ,       "Normal" },
   { Marker::KnotTypePositionOnly,  "Position Only" },
   { Marker::KnotTypeKink,          "Kink" },
};
static EnumTable markerKnotTable(3, &knotEnums[0]);


void Marker::initPersistFields()
{
   addField("seqNum",   TypeS32, Offset(mSeqNum,   Marker));
   addField("type", TypeEnum, Offset(mKnotType, Marker), 1, &markerKnotTable);
   addField("msToNext", TypeS32, Offset(mMSToNext, Marker));
   addField("smoothingType", TypeEnum, Offset(mSmoothingType, Marker), 1, &markerSmoothingTable);
   endGroup("Misc");

   Parent::initPersistFields();
}

//--------------------------------------------------------------------------
bool Marker::onAdd()
{
   if(!Parent::onAdd())
      return false;

   mObjBox = Box3F(Point3F(-.25, -.25, -.25), Point3F(.25, .25, .25));
   resetWorldBox();

   if(gEditingMission)
      onEditorEnable();

   return true;
}


void Marker::onRemove()
{
   if(gEditingMission)
      onEditorDisable();

   Parent::onRemove();

   smVertexBuffer = NULL;
   smPrimitiveBuffer = NULL;
}

void Marker::onGroupAdd()
{
   mSeqNum = getGroup()->size();
}


/// Enable scoping so we can see this thing on the client.
void Marker::onEditorEnable()
{
   mNetFlags.set(Ghostable);
   setScopeAlways();
   addToScene();
}

/// Disable scoping so we can see this thing on the client
void Marker::onEditorDisable()
{
   removeFromScene();
   mNetFlags.clear(Ghostable);
   clearScopeAlways();
}


/// Tell our parent that this Path has been modified
void Marker::inspectPostApply()
{
   SimPath::Path *path = dynamic_cast<SimPath::Path*>(getGroup());
   if (path)
      path->updatePath();
}


//--------------------------------------------------------------------------
bool Marker::prepRenderImage(SceneState* state, const U32 stateKey,
                             const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
   if (isLastState(state, stateKey))
      return false;
   setLastState(state, stateKey);

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if (state->isObjectRendered(this)) {

      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &Marker::renderObject );
      ri->type = RenderPassManager::RIT_Object;
      state->getRenderPass()->addInst(ri);
   }

   return false;
}


void Marker::renderObject(ObjectRenderInst *ri, SceneState *state, BaseMatInstance* overrideMat)
{
   initGFXResources();
   
   for(U32 i = 0; i < GFX->getNumSamplers(); i++)
      GFX->setTexture(i, NULL);
   GFXTransformSaver saver;
   MatrixF mat = getRenderTransform();
   mat.scale(mObjScale);
   GFX->multWorld(mat);
   
   GFX->setStateBlock(smStateBlock);
   GFX->setVertexBuffer(smVertexBuffer);
   GFX->setPrimitiveBuffer(smPrimitiveBuffer);
   GFX->drawIndexedPrimitive(GFXLineList, 0, 0, 4, 0, 12);
}


//--------------------------------------------------------------------------
U32 Marker::packUpdate(NetConnection* con, U32 mask, BitStream* stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   // Note that we don't really care about efficiency here, since this is an
   //  edit-only ghost...
   stream->writeAffineTransform(mObjToWorld);

   return retMask;
}

void Marker::unpackUpdate(NetConnection* con, BitStream* stream)
{
   Parent::unpackUpdate(con, stream);

   // Transform
   MatrixF otow;
   stream->readAffineTransform(&otow);

   setTransform(otow);
}