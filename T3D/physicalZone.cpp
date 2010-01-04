//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/physicalZone.h"
#include "core/stream/bitStream.h"
#include "collision/boxConvex.h"
#include "collision/clippedPolyList.h"
#include "console/consoleTypes.h"
#include "math/mathIO.h"
#include "sceneGraph/sceneState.h"
#include "T3D/trigger.h"
#include "gfx/gfxTransformSaver.h"
#include "renderInstance/renderPassManager.h"
#include "gfx/gfxDrawUtil.h"

IMPLEMENT_CO_NETOBJECT_V1(PhysicalZone);

bool PhysicalZone::smRenderPZones = false;

ConsoleMethod(PhysicalZone, activate, void, 2, 2, "Activate the physical zone's effects.")
{
   if (object->isClientObject())
      return;

   object->activate();
}

ConsoleMethod(PhysicalZone, deactivate, void, 2, 2, "Deactivate the physical zone's effects.")
{
   if (object->isClientObject())
      return;

   object->deactivate();
}


//--------------------------------------------------------------------------
//--------------------------------------
//
PhysicalZone::PhysicalZone()
{
   mNetFlags.set(Ghostable | ScopeAlways);

   mTypeMask |= PhysicalZoneObjectType;

   mVelocityMod = 1.0f;
   mGravityMod  = 1.0f;
   mAppliedForce.set(0, 0, 0);

   mConvexList = new Convex;
   mActive = true;
}

PhysicalZone::~PhysicalZone()
{
   delete mConvexList;
   mConvexList = NULL;
}

//--------------------------------------------------------------------------
void PhysicalZone::consoleInit()
{
   Con::addVariable( "$PhysicalZone::renderZones", TypeBool, &smRenderPZones );
}

void PhysicalZone::initPersistFields()
{
   addGroup("Misc");
   addField("velocityMod",  TypeF32,               Offset(mVelocityMod,  PhysicalZone));
   addField("gravityMod",   TypeF32,               Offset(mGravityMod,   PhysicalZone));
   addField("appliedForce", TypePoint3F,           Offset(mAppliedForce, PhysicalZone));
   addField("polyhedron",   TypeTriggerPolyhedron, Offset(mPolyhedron,   PhysicalZone),
      "The polyhedron type is really a quadrilateral and consists of a corner"
      "point followed by three vectors representing the edges extending from the corner." );
   endGroup("Misc");

   Parent::initPersistFields();
}

//--------------------------------------------------------------------------
bool PhysicalZone::onAdd()
{
   if(!Parent::onAdd())
      return false;

   if (mVelocityMod < -40.0f || mVelocityMod > 40.0f) {
      Con::errorf("PhysicalZone: velocity mod out of range.  [-40, 40]");
      mVelocityMod = mVelocityMod < -40.0f ? -40.0f : 40.0f;
   }
   if (mGravityMod < -40.0f || mGravityMod > 40.0f) {
      Con::errorf("PhysicalZone: GravityMod out of range.  [-40, 40]");
      mGravityMod = mGravityMod < -40.0f ? -40.0f : 40.0f;
   }
   static const char* coordString[] = { "x", "y", "z" };
   F32* p = mAppliedForce;
   for (U32 i = 0; i < 3; i++) {
      if (p[i] < -40000.0f || p[i] > 40000.0f) {
         Con::errorf("PhysicalZone: applied force: %s out of range.  [-40000, 40000]", coordString[i]);
         p[i] = p[i] < -40000.0f ? -40000.0f : 40000.0f;
      }
   }

   Polyhedron temp = mPolyhedron;
   setPolyhedron(temp);

   addToScene();

   return true;
}


void PhysicalZone::onRemove()
{
   mConvexList->nukeList();

   removeFromScene();
   Parent::onRemove();
}

void PhysicalZone::inspectPostApply()
{
   setPolyhedron(mPolyhedron);
   Parent::inspectPostApply();
}

//------------------------------------------------------------------------------
void PhysicalZone::setTransform(const MatrixF & mat)
{
   Parent::setTransform(mat);

      MatrixF base(true);
      base.scale(Point3F(1.0/mObjScale.x,
                         1.0/mObjScale.y,
                         1.0/mObjScale.z));
      base.mul(mWorldToObj);
      mClippedList.setBaseTransform(base);

   if (isServerObject())
      setMaskBits(InitialUpdateMask);
}


bool PhysicalZone::prepRenderImage( SceneState *state,
                                    const U32 stateKey,
                                    const U32 startZone,
                                    const bool modifyBaseState )
{
   if ( isLastState(state, stateKey) )
      return false;

   // only render if selected or render flag is set
   if ( !smRenderPZones && !isSelected() )
      return false;

   setLastState( state, stateKey );

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if (state->isObjectRendered(this))
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &PhysicalZone::renderObject );
      ri->type = RenderPassManager::RIT_Object;
      ri->defaultKey = 0;
      ri->defaultKey2 = 0;
      state->getRenderPass()->addInst( ri );
   }

   return false;
}


void PhysicalZone::renderObject( ObjectRenderInst *ri,
                                 SceneState *state,
                                 BaseMatInstance *overrideMat )
{
   if (overrideMat)
      return;

   GFXStateBlockDesc desc;
   desc.setZReadWrite( true, false );
   desc.setBlend( true );
   desc.setCullMode( GFXCullNone );

   GFXTransformSaver saver;

   MatrixF mat = getRenderTransform();
   mat.scale( getScale() );

   GFX->multWorld( mat );

   GFXDrawUtil *drawer = GFX->getDrawUtil();
   drawer->drawPolyhedron( desc, mPolyhedron, ColorI( 0, 255, 0, 45 ) );
}

//--------------------------------------------------------------------------
U32 PhysicalZone::packUpdate(NetConnection* con, U32 mask, BitStream* stream)
{
   U32 i;
   U32 retMask = Parent::packUpdate(con, mask, stream);

   if (stream->writeFlag((mask & InitialUpdateMask) != 0)) {
      // Note that we don't really care about efficiency here, since this is an
      //  edit-only ghost...
      mathWrite(*stream, mObjToWorld);
      mathWrite(*stream, mObjScale);

      // Write the polyhedron
      stream->write(mPolyhedron.pointList.size());
      for (i = 0; i < mPolyhedron.pointList.size(); i++)
         mathWrite(*stream, mPolyhedron.pointList[i]);

      stream->write(mPolyhedron.planeList.size());
      for (i = 0; i < mPolyhedron.planeList.size(); i++)
         mathWrite(*stream, mPolyhedron.planeList[i]);

      stream->write(mPolyhedron.edgeList.size());
      for (i = 0; i < mPolyhedron.edgeList.size(); i++) {
         const Polyhedron::Edge& rEdge = mPolyhedron.edgeList[i];

         stream->write(rEdge.face[0]);
         stream->write(rEdge.face[1]);
         stream->write(rEdge.vertex[0]);
         stream->write(rEdge.vertex[1]);
      }

      stream->write(mVelocityMod);
      stream->write(mGravityMod);
      mathWrite(*stream, mAppliedForce);
      stream->writeFlag(mActive);
   } else {
      stream->writeFlag(mActive);
   }

   return retMask;
}

void PhysicalZone::unpackUpdate(NetConnection* con, BitStream* stream)
{
   Parent::unpackUpdate(con, stream);

   if (stream->readFlag()) {
      U32 i, size;
      MatrixF temp;
      Point3F tempScale;
      Polyhedron tempPH;

      // Transform
      mathRead(*stream, &temp);
      mathRead(*stream, &tempScale);

      // Read the polyhedron
      stream->read(&size);
      tempPH.pointList.setSize(size);
      for (i = 0; i < tempPH.pointList.size(); i++)
         mathRead(*stream, &tempPH.pointList[i]);

      stream->read(&size);
      tempPH.planeList.setSize(size);
      for (i = 0; i < tempPH.planeList.size(); i++)
         mathRead(*stream, &tempPH.planeList[i]);

      stream->read(&size);
      tempPH.edgeList.setSize(size);
      for (i = 0; i < tempPH.edgeList.size(); i++) {
         Polyhedron::Edge& rEdge = tempPH.edgeList[i];

         stream->read(&rEdge.face[0]);
         stream->read(&rEdge.face[1]);
         stream->read(&rEdge.vertex[0]);
         stream->read(&rEdge.vertex[1]);
      }

      stream->read(&mVelocityMod);
      stream->read(&mGravityMod);
      mathRead(*stream, &mAppliedForce);

      setPolyhedron(tempPH);
      setScale(tempScale);
      setTransform(temp);
      mActive = stream->readFlag();
   } else {
      mActive = stream->readFlag();
   }
}


//--------------------------------------------------------------------------
void PhysicalZone::setPolyhedron(const Polyhedron& rPolyhedron)
{
   mPolyhedron = rPolyhedron;

   if (mPolyhedron.pointList.size() != 0) {
      mObjBox.minExtents.set(1e10, 1e10, 1e10);
      mObjBox.maxExtents.set(-1e10, -1e10, -1e10);
      for (U32 i = 0; i < mPolyhedron.pointList.size(); i++) {
         mObjBox.minExtents.setMin(mPolyhedron.pointList[i]);
         mObjBox.maxExtents.setMax(mPolyhedron.pointList[i]);
      }
   } else {
      mObjBox.minExtents.set(-0.5, -0.5, -0.5);
      mObjBox.maxExtents.set( 0.5,  0.5,  0.5);
   }

   MatrixF xform = getTransform();
   setTransform(xform);

   mClippedList.clear();
   mClippedList.mPlaneList = mPolyhedron.planeList;

   MatrixF base(true);
   base.scale(Point3F(1.0/mObjScale.x,
                      1.0/mObjScale.y,
                      1.0/mObjScale.z));
   base.mul(mWorldToObj);

   mClippedList.setBaseTransform(base);
}


//--------------------------------------------------------------------------
void PhysicalZone::buildConvex(const Box3F& box, Convex* convex)
{
   // These should really come out of a pool
   mConvexList->collectGarbage();

   Box3F realBox = box;
   mWorldToObj.mul(realBox);
   realBox.minExtents.convolveInverse(mObjScale);
   realBox.maxExtents.convolveInverse(mObjScale);

   if (realBox.isOverlapped(getObjBox()) == false)
      return;

   // Just return a box convex for the entire shape...
   Convex* cc = 0;
   CollisionWorkingList& wl = convex->getWorkingList();
   for (CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext) {
      if (itr->mConvex->getType() == BoxConvexType &&
          itr->mConvex->getObject() == this) {
         cc = itr->mConvex;
         break;
      }
   }
   if (cc)
      return;

   // Create a new convex.
   BoxConvex* cp = new BoxConvex;
   mConvexList->registerObject(cp);
   convex->addToWorkingList(cp);
   cp->init(this);

   mObjBox.getCenter(&cp->mCenter);
   cp->mSize.x = mObjBox.len_x() / 2.0f;
   cp->mSize.y = mObjBox.len_y() / 2.0f;
   cp->mSize.z = mObjBox.len_z() / 2.0f;
}

bool PhysicalZone::testObject(SceneObject* enter)
{
   // TODO: This doesn't look like it's testing against the polyhedron at
   // all.  And whats the point of building a convex if no collision methods
   // are implemented?

   if (mPolyhedron.pointList.size() == 0)
      return false;

   mClippedList.clear();

   SphereF sphere;
   sphere.center = (mWorldBox.minExtents + mWorldBox.maxExtents) * 0.5;
   VectorF bv = mWorldBox.maxExtents - sphere.center;
   sphere.radius = bv.len();

   enter->buildPolyList(&mClippedList, mWorldBox, sphere);
   return mClippedList.isEmpty() == false;
}

bool PhysicalZone::testBox( const Box3F &box ) const
{
   return mWorldBox.isOverlapped( box );
}

void PhysicalZone::activate()
{
   AssertFatal(isServerObject(), "Client objects not allowed in ForceFieldInstance::open()");

   if (mActive != true)
      setMaskBits(ActiveMask);
   mActive = true;
}

void PhysicalZone::deactivate()
{
   AssertFatal(isServerObject(), "Client objects not allowed in ForceFieldInstance::close()");

   if (mActive != false)
      setMaskBits(ActiveMask);
   mActive = false;
}

