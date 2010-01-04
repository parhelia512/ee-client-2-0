//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "sceneGraph/sceneState.h"
#include "console/consoleTypes.h"
#include "collision/boxConvex.h"

#include "core/stream/bitStream.h"
#include "math/mathIO.h"

#include "gfx/gfxTransformSaver.h"
#include "renderInstance/renderPassManager.h"
#include "gfx/gfxDrawUtil.h"

#include "T3D/trigger.h"

bool Trigger::smRenderTriggers = false;

//-----------------------------------------------------------------------------

ConsoleMethod( TriggerData, onEnterTrigger, void, 4, 4, "( Trigger t, SimObject intruder)")
{
   Trigger* trigger = NULL;
   if (Sim::findObject(argv[2], trigger) == false)
      return;

   // Do nothing with the trigger object id by default...
   SimGroup* pGroup = trigger->getGroup();
   for (SimGroup::iterator itr = pGroup->begin(); itr != pGroup->end(); itr++)
      Con::executef(*itr, "onTrigger", Con::getIntArg(trigger->getId()), "1");
}

ConsoleMethod( TriggerData, onLeaveTrigger, void, 4, 4, "( Trigger t, SimObject intruder)")
{
   Trigger* trigger = NULL;
   if (Sim::findObject(argv[2], trigger) == false)
      return;

   if (trigger->getNumTriggeringObjects() == 0) {
      SimGroup* pGroup = trigger->getGroup();
      for (SimGroup::iterator itr = pGroup->begin(); itr != pGroup->end(); itr++)
         Con::executef(*itr, "onTrigger", Con::getIntArg(trigger->getId()), "0");
   }
}

ConsoleMethod( TriggerData, onTickTrigger, void, 3, 3, "( Trigger t )")
{
   Trigger* trigger = NULL;
   if (Sim::findObject(argv[2], trigger) == false)
      return;

   // Do nothing with the trigger object id by default...
   SimGroup* pGroup = trigger->getGroup();
   for (SimGroup::iterator itr = pGroup->begin(); itr != pGroup->end(); itr++)
      Con::executef(*itr, "onTriggerTick", Con::getIntArg(trigger->getId()));
}

ConsoleMethod( Trigger, getNumObjects, S32, 2, 2, "")
{
   return object->getNumTriggeringObjects();
}

ConsoleMethod( Trigger, getObject, S32, 3, 3, "(int idx)")
{
   S32 index = dAtoi(argv[2]);

   if (index >= object->getNumTriggeringObjects() || index < 0)
      return -1;
   else
      return object->getObject(U32(index))->getId();
}

//----------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(TriggerData);

TriggerData::TriggerData()
{
   tickPeriodMS = 100;
   isClientSide = false;
}

bool TriggerData::onAdd()
{
   if (!Parent::onAdd())
      return false;

   return true;
}

void TriggerData::initPersistFields()
{
   addField("tickPeriodMS", TypeS32,  Offset(tickPeriodMS, TriggerData), "Time between calls to TriggerData::onTickTrigger().");
   addField("clientSide", TypeBool,   Offset(isClientSide, TriggerData), "Only trigger on clients.");

   Parent::initPersistFields();
}


//--------------------------------------------------------------------------
void TriggerData::packData(BitStream* stream)
{
   Parent::packData(stream);
   stream->write(tickPeriodMS);
   stream->write(isClientSide);
}

void TriggerData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);
   stream->read(&tickPeriodMS);
   stream->read(&isClientSide);
}


//--------------------------------------------------------------------------

IMPLEMENT_CO_NETOBJECT_V1(Trigger);

Trigger::Trigger()
{
   // Don't ghost by default.
   mNetFlags.set(Ghostable | ScopeAlways);

   mTypeMask |= TriggerObjectType;

   mObjScale.set(1, 1, 1);
   mObjToWorld.identity();
   mWorldToObj.identity();

   mDataBlock = NULL;

   mLastThink = 0;
   mCurrTick  = 0;

   mConvexList = new Convex;
}

Trigger::~Trigger()
{
   delete mConvexList;
   mConvexList = NULL;
}

bool Trigger::castRay(const Point3F &start, const Point3F &end, RayInfo* info)
{
   // Collide against bounding box
   F32 st,et,fst = 0,fet = 1;
   F32 *bmin = &mObjBox.minExtents.x;
   F32 *bmax = &mObjBox.maxExtents.x;
   F32 const *si = &start.x;
   F32 const *ei = &end.x;

   for (int i = 0; i < 3; i++)
   {
      if (*si < *ei)
      {
         if (*si > *bmax || *ei < *bmin)
            return false;
         F32 di = *ei - *si;
         st = (*si < *bmin)? (*bmin - *si) / di: 0;
         et = (*ei > *bmax)? (*bmax - *si) / di: 1;
      }
      else
      {
         if (*ei > *bmax || *si < *bmin)
            return false;
         F32 di = *ei - *si;
         st = (*si > *bmax)? (*bmax - *si) / di: 0;
         et = (*ei < *bmin)? (*bmin - *si) / di: 1;
      }
      if (st > fst) fst = st;
      if (et < fet) fet = et;
      if (fet < fst)
         return false;
      bmin++; bmax++;
      si++; ei++;
   }

   info->normal = start - end;
   info->normal.normalizeSafe();
   getTransform().mulV( info->normal );

   info->t = fst;
   info->object = this;
   info->point.interpolate(start,end,fst);
   info->material = 0;
   return true;
}


//--------------------------------------------------------------------------
/* Console polyhedron data type exporter
   The polyhedron type is really a quadrilateral and consists of a corner
   point follow by three vectors representing the edges extending from the
   corner.
*/
ConsoleType( TriggerPolyhedron, TypeTriggerPolyhedron, Polyhedron )


ConsoleGetType( TypeTriggerPolyhedron )
{
   U32 i;
   Polyhedron* pPoly = reinterpret_cast<Polyhedron*>(dptr);

   // First point is corner, need to find the three vectors...`
   Point3F origin = pPoly->pointList[0];
   U32 currVec = 0;
   Point3F vecs[3];
   for (i = 0; i < pPoly->edgeList.size(); i++) {
      const U32 *vertex = pPoly->edgeList[i].vertex;
      if (vertex[0] == 0)
         vecs[currVec++] = pPoly->pointList[vertex[1]] - origin;
      else
         if (vertex[1] == 0)
            vecs[currVec++] = pPoly->pointList[vertex[0]] - origin;
   }
   AssertFatal(currVec == 3, "Internal error: Bad trigger polyhedron");

   // Build output string.
   char* retBuf = Con::getReturnBuffer(1024);
   dSprintf(retBuf, 1023, "%7.7f %7.7f %7.7f %7.7f %7.7f %7.7f %7.7f %7.7f %7.7f %7.7f %7.7f %7.7f",
            origin.x, origin.y, origin.z,
            vecs[0].x, vecs[0].y, vecs[0].z,
            vecs[2].x, vecs[2].y, vecs[2].z,
            vecs[1].x, vecs[1].y, vecs[1].z);

   return retBuf;
}

/* Console polyhedron data type loader
   The polyhedron type is really a quadrilateral and consists of an corner
   point follow by three vectors representing the edges extending from the
   corner.
*/
ConsoleSetType( TypeTriggerPolyhedron )
{
   if (argc != 1) {
      Con::printf("(TypeTriggerPolyhedron) multiple args not supported for polyhedra");
      return;
   }

   Point3F origin;
   Point3F vecs[3];

   U32 numArgs = dSscanf(argv[0], "%g %g %g %g %g %g %g %g %g %g %g %g",
                         &origin.x, &origin.y, &origin.z,
                         &vecs[0].x, &vecs[0].y, &vecs[0].z,
                         &vecs[1].x, &vecs[1].y, &vecs[1].z,
                         &vecs[2].x, &vecs[2].y, &vecs[2].z);
   if (numArgs != 12) {
      Con::printf("Bad polyhedron!");
      return;
   }

   Polyhedron* pPoly = reinterpret_cast<Polyhedron*>(dptr);

   pPoly->pointList.setSize(8);
   pPoly->pointList[0] = origin;
   pPoly->pointList[1] = origin + vecs[0];
   pPoly->pointList[2] = origin + vecs[1];
   pPoly->pointList[3] = origin + vecs[2];
   pPoly->pointList[4] = origin + vecs[0] + vecs[1];
   pPoly->pointList[5] = origin + vecs[0] + vecs[2];
   pPoly->pointList[6] = origin + vecs[1] + vecs[2];
   pPoly->pointList[7] = origin + vecs[0] + vecs[1] + vecs[2];

   Point3F normal;
   pPoly->planeList.setSize(6);

   mCross(vecs[2], vecs[0], &normal);
   pPoly->planeList[0].set(origin, normal);
   mCross(vecs[0], vecs[1], &normal);
   pPoly->planeList[1].set(origin, normal);
   mCross(vecs[1], vecs[2], &normal);
   pPoly->planeList[2].set(origin, normal);
   mCross(vecs[1], vecs[0], &normal);
   pPoly->planeList[3].set(pPoly->pointList[7], normal);
   mCross(vecs[2], vecs[1], &normal);
   pPoly->planeList[4].set(pPoly->pointList[7], normal);
   mCross(vecs[0], vecs[2], &normal);
   pPoly->planeList[5].set(pPoly->pointList[7], normal);

   pPoly->edgeList.setSize(12);
   pPoly->edgeList[0].vertex[0]  = 0; pPoly->edgeList[0].vertex[1]  = 1; pPoly->edgeList[0].face[0]  = 0; pPoly->edgeList[0].face[1]  = 1;
   pPoly->edgeList[1].vertex[0]  = 1; pPoly->edgeList[1].vertex[1]  = 5; pPoly->edgeList[1].face[0]  = 0; pPoly->edgeList[1].face[1]  = 4;
   pPoly->edgeList[2].vertex[0]  = 5; pPoly->edgeList[2].vertex[1]  = 3; pPoly->edgeList[2].face[0]  = 0; pPoly->edgeList[2].face[1]  = 3;
   pPoly->edgeList[3].vertex[0]  = 3; pPoly->edgeList[3].vertex[1]  = 0; pPoly->edgeList[3].face[0]  = 0; pPoly->edgeList[3].face[1]  = 2;
   pPoly->edgeList[4].vertex[0]  = 3; pPoly->edgeList[4].vertex[1]  = 6; pPoly->edgeList[4].face[0]  = 3; pPoly->edgeList[4].face[1]  = 2;
   pPoly->edgeList[5].vertex[0]  = 6; pPoly->edgeList[5].vertex[1]  = 2; pPoly->edgeList[5].face[0]  = 2; pPoly->edgeList[5].face[1]  = 5;
   pPoly->edgeList[6].vertex[0]  = 2; pPoly->edgeList[6].vertex[1]  = 0; pPoly->edgeList[6].face[0]  = 2; pPoly->edgeList[6].face[1]  = 1;
   pPoly->edgeList[7].vertex[0]  = 1; pPoly->edgeList[7].vertex[1]  = 4; pPoly->edgeList[7].face[0]  = 4; pPoly->edgeList[7].face[1]  = 1;
   pPoly->edgeList[8].vertex[0]  = 4; pPoly->edgeList[8].vertex[1]  = 2; pPoly->edgeList[8].face[0]  = 1; pPoly->edgeList[8].face[1]  = 5;
   pPoly->edgeList[9].vertex[0]  = 4; pPoly->edgeList[9].vertex[1]  = 7; pPoly->edgeList[9].face[0]  = 4; pPoly->edgeList[9].face[1]  = 5;
   pPoly->edgeList[10].vertex[0] = 5; pPoly->edgeList[10].vertex[1] = 7; pPoly->edgeList[10].face[0] = 3; pPoly->edgeList[10].face[1] = 4;
   pPoly->edgeList[11].vertex[0] = 7; pPoly->edgeList[11].vertex[1] = 6; pPoly->edgeList[11].face[0] = 3; pPoly->edgeList[11].face[1] = 5;
}


//-----------------------------------------------------------------------------
void Trigger::consoleInit()
{
   Con::addVariable( "$Trigger::renderTriggers", TypeBool, &smRenderTriggers );
}

void Trigger::initPersistFields()
{
   addField("polyhedron", TypeTriggerPolyhedron, Offset(mTriggerPolyhedron, Trigger),
      "The polyhedron type is really a quadrilateral and consists of a corner"
      "point followed by three vectors representing the edges extending from the corner." );

   addProtectedField("enterCommand", TypeCommand, Offset(mEnterCommand, Trigger), &setEnterCmd, &defaultProtectedGetFn,
      "The command to execute when an object enters this trigger. Object id stored in %obj. Maximum 1023 characters." );
   addProtectedField("leaveCommand", TypeCommand, Offset(mLeaveCommand, Trigger), &setLeaveCmd, &defaultProtectedGetFn,
      "The command to execute when an object leaves this trigger. Object id stored in %obj. Maximum 1023 characters." );
   addProtectedField("tickCommand", TypeCommand, Offset(mTickCommand, Trigger), &setTickCmd, &defaultProtectedGetFn,
      "The command to execute while an object is inside this trigger. Maximum 1023 characters." );

   Parent::initPersistFields();
}

bool Trigger::setEnterCmd(void *obj, const char*)
{
   static_cast<Trigger*>(obj)->setMaskBits(EnterCmdMask);
   return true; // to update the actual field
}

bool Trigger::setLeaveCmd(void *obj, const char*)
{
   static_cast<Trigger*>(obj)->setMaskBits(LeaveCmdMask);
   return true; // to update the actual field
}

bool Trigger::setTickCmd(void *obj, const char*)
{
   static_cast<Trigger*>(obj)->setMaskBits(TickCmdMask);
   return true; // to update the actual field
}

//--------------------------------------------------------------------------

bool Trigger::onAdd()
{
   if(!Parent::onAdd())
      return false;

   Con::executef(this, "onAdd", Con::getIntArg(getId()));

   Polyhedron temp = mTriggerPolyhedron;
   setTriggerPolyhedron(temp);

   addToScene();

   if (isServerObject())
      scriptOnAdd();
      
   return true;
}

void Trigger::onRemove()
{
   Con::executef(this, "onRemove", Con::getIntArg(getId()));

   mConvexList->nukeList();

   removeFromScene();
   Parent::onRemove();
}

bool Trigger::onNewDataBlock(GameBaseData* dptr)
{
   mDataBlock = dynamic_cast<TriggerData*>(dptr);
   if (!mDataBlock || !Parent::onNewDataBlock(dptr))
      return false;

   scriptOnNewDataBlock();
   return true;
}

void Trigger::onDeleteNotify(SimObject* obj)
{
   GameBase* pScene = dynamic_cast<GameBase*>(obj);
   if (pScene != NULL && mDataBlock != NULL)
   {
      for (U32 i = 0; i < mObjects.size(); i++)
      {
         if (pScene == mObjects[i])
         {
            mObjects.erase(i);
            Con::executef(mDataBlock, "onLeaveTrigger", scriptThis(), Con::getIntArg(pScene->getId()));
            break;
         }
      }
   }

   Parent::onDeleteNotify(obj);
}

void Trigger::inspectPostApply()
{
   setTriggerPolyhedron(mTriggerPolyhedron);
   setMaskBits(PolyMask);
   Parent::inspectPostApply();
}

//--------------------------------------------------------------------------

void Trigger::buildConvex(const Box3F& box, Convex* convex)
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


//------------------------------------------------------------------------------

void Trigger::setTransform(const MatrixF & mat)
{
   Parent::setTransform(mat);

   if (isServerObject()) {
      MatrixF base(true);
      base.scale(Point3F(1.0/mObjScale.x,
                         1.0/mObjScale.y,
                         1.0/mObjScale.z));
      base.mul(mWorldToObj);
      mClippedList.setBaseTransform(base);

      setMaskBits(TransformMask | ScaleMask);
   }
}

bool Trigger::prepRenderImage( SceneState *state,
                               const U32 stateKey,
                               const U32 startZone,
                               const bool modifyBaseState )
{
   if ( isLastState(state, stateKey) )
      return false;

   // only render if selected or render flag is set
   if ( !smRenderTriggers && !isSelected() )
      return false;

   setLastState( state, stateKey );

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if (state->isObjectRendered(this))
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &Trigger::renderObject );
      ri->type = RenderPassManager::RIT_Object;
      ri->defaultKey = 0;
      ri->defaultKey2 = 0;
      state->getRenderPass()->addInst( ri );
   }

   return false;
}

void Trigger::renderObject( ObjectRenderInst *ri,
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
   drawer->drawPolyhedron( desc, mTriggerPolyhedron, ColorI( 255, 192, 0, 45 ) );
}

void Trigger::setTriggerPolyhedron(const Polyhedron& rPolyhedron)
{
   mTriggerPolyhedron = rPolyhedron;

   if (mTriggerPolyhedron.pointList.size() != 0) {
      mObjBox.minExtents.set(1e10, 1e10, 1e10);
      mObjBox.maxExtents.set(-1e10, -1e10, -1e10);
      for (U32 i = 0; i < mTriggerPolyhedron.pointList.size(); i++) {
         mObjBox.minExtents.setMin(mTriggerPolyhedron.pointList[i]);
         mObjBox.maxExtents.setMax(mTriggerPolyhedron.pointList[i]);
      }
   } else {
      mObjBox.minExtents.set(-0.5, -0.5, -0.5);
      mObjBox.maxExtents.set( 0.5,  0.5,  0.5);
   }

   MatrixF xform = getTransform();
   setTransform(xform);

   mClippedList.clear();
   mClippedList.mPlaneList = mTriggerPolyhedron.planeList;
//   for (U32 i = 0; i < mClippedList.mPlaneList.size(); i++)
//      mClippedList.mPlaneList[i].neg();

   MatrixF base(true);
   base.scale(Point3F(1.0/mObjScale.x,
                      1.0/mObjScale.y,
                      1.0/mObjScale.z));
   base.mul(mWorldToObj);

   mClippedList.setBaseTransform(base);
}


//--------------------------------------------------------------------------

bool Trigger::testObject(GameBase* enter)
{
   if (mTriggerPolyhedron.pointList.size() == 0)
      return false;

   mClippedList.clear();

   SphereF sphere;
   sphere.center = (mWorldBox.minExtents + mWorldBox.maxExtents) * 0.5;
   VectorF bv = mWorldBox.maxExtents - sphere.center;
   sphere.radius = bv.len();

   enter->buildPolyList(&mClippedList, mWorldBox, sphere);
   return mClippedList.isEmpty() == false;
}


void Trigger::potentialEnterObject(GameBase* enter)
{
   if( (!mDataBlock || mDataBlock->isClientSide) && isServerObject() )
      return;
   if( (mDataBlock && !mDataBlock->isClientSide) && isGhost() )
      return;

   for (U32 i = 0; i < mObjects.size(); i++) {
      if (mObjects[i] == enter)
         return;
   }

   if (testObject(enter) == true) {
      mObjects.push_back(enter);
      deleteNotify(enter);

      if(!mEnterCommand.isEmpty())
      {
         String command = String("%obj = ") + enter->scriptThis() + ";" + mEnterCommand;
         Con::evaluate(command.c_str());
      }

      if( mDataBlock )
         Con::executef(mDataBlock, "onEnterTrigger", scriptThis(), enter->scriptThis());
   }
}


void Trigger::processTick(const Move* move)
{
   Parent::processTick(move);

   if (!mDataBlock)
      return;
   if (mDataBlock->isClientSide && isServerObject())
      return;
   if (!mDataBlock->isClientSide && isClientObject())
      return;

   //
   if (mObjects.size() == 0)
      return;

   if (mLastThink + mDataBlock->tickPeriodMS < mCurrTick) {
      mCurrTick  = 0;
      mLastThink = 0;

      for (S32 i = S32(mObjects.size() - 1); i >= 0; i--) {
         if (testObject(mObjects[i]) == false) {
            GameBase* remove = mObjects[i];
            mObjects.erase(i);
            clearNotify(remove);
            if(!mLeaveCommand.isEmpty())
            {
               String command = String("%obj = ") + remove->scriptThis() + ";" + mLeaveCommand;
               Con::evaluate(command.c_str());
            }
            Con::executef(mDataBlock, "onLeaveTrigger", scriptThis(), remove->scriptThis());
         }
      }

      if (!mTickCommand.isEmpty())
         Con::evaluate(mTickCommand.c_str());

      if (mObjects.size() != 0)
         Con::executef(mDataBlock, "onTickTrigger", scriptThis());
   } else {
      mCurrTick += TickMs;
   }
}

//--------------------------------------------------------------------------

U32 Trigger::packUpdate(NetConnection* con, U32 mask, BitStream* stream)
{
   U32 i;
   U32 retMask = Parent::packUpdate(con, mask, stream);

   if( stream->writeFlag( mask & TransformMask ) )
   {
      stream->writeAffineTransform(mObjToWorld);
   }

   // Write the polyhedron
   if( stream->writeFlag( mask & PolyMask ) )
   {
      stream->write(mTriggerPolyhedron.pointList.size());
      for (i = 0; i < mTriggerPolyhedron.pointList.size(); i++)
         mathWrite(*stream, mTriggerPolyhedron.pointList[i]);

      stream->write(mTriggerPolyhedron.planeList.size());
      for (i = 0; i < mTriggerPolyhedron.planeList.size(); i++)
         mathWrite(*stream, mTriggerPolyhedron.planeList[i]);

      stream->write(mTriggerPolyhedron.edgeList.size());
      for (i = 0; i < mTriggerPolyhedron.edgeList.size(); i++) {
         const Polyhedron::Edge& rEdge = mTriggerPolyhedron.edgeList[i];

         stream->write(rEdge.face[0]);
         stream->write(rEdge.face[1]);
         stream->write(rEdge.vertex[0]);
         stream->write(rEdge.vertex[1]);
      }
   }

   if( stream->writeFlag( mask & EnterCmdMask ) )
      stream->writeLongString(CMD_SIZE-1, mEnterCommand.c_str());
   if( stream->writeFlag( mask & LeaveCmdMask ) )
      stream->writeLongString(CMD_SIZE-1, mLeaveCommand.c_str());
   if( stream->writeFlag( mask & TickCmdMask ) )
      stream->writeLongString(CMD_SIZE-1, mTickCommand.c_str());

   return retMask;
}

void Trigger::unpackUpdate(NetConnection* con, BitStream* stream)
{
   Parent::unpackUpdate(con, stream);

   U32 i, size;

   // Transform
   if( stream->readFlag() )
   {
      MatrixF temp;
      stream->readAffineTransform(&temp);
      setTransform(temp);
   }

   // Read the polyhedron
   if( stream->readFlag() )
   {
      Polyhedron tempPH;
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
      setTriggerPolyhedron(tempPH);
   }

   if( stream->readFlag() )
   {
      char buf[CMD_SIZE];
      stream->readLongString(CMD_SIZE-1, buf);
      mEnterCommand = buf;
   }
   if( stream->readFlag() )
   {
      char buf[CMD_SIZE];
      stream->readLongString(CMD_SIZE-1, buf);
      mLeaveCommand = buf;
   }
   if( stream->readFlag() )
   {
      char buf[CMD_SIZE];
      stream->readLongString(CMD_SIZE-1, buf);
      mTickCommand = buf;
   }
}
