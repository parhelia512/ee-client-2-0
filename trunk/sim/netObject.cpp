//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/simBase.h"
#include "core/dnet.h"
#include "sim/netConnection.h"
#include "sim/netObject.h"
#include "console/consoleTypes.h"
#include "add/RPGPack/RPGUtils.h"

IMPLEMENT_CONOBJECT(NetObject);

extern bool						gIsDedicated;
//----------------------------------------------------------------------------
NetObject *NetObject::mDirtyList = NULL;

NetObject::NetObject()
{
	// netFlags will clear itself to 0
	mNetIndex = U32(-1);
   mFirstObjectRef = NULL;
   mPrevDirtyList = NULL;
   mNextDirtyList = NULL;
   mDirtyMaskBits = 0;
}

NetObject::~NetObject()
{
   if(mDirtyMaskBits)
   {
      if(mPrevDirtyList)
         mPrevDirtyList->mNextDirtyList = mNextDirtyList;
      else
         mDirtyList = mNextDirtyList;
      if(mNextDirtyList)
         mNextDirtyList->mPrevDirtyList = mPrevDirtyList;
   }
}

String NetObject::describeSelf()
{
   String desc = Parent::describeSelf();

   if( isClientObject() )
      desc += "|net: client";
   else
      desc += "|net: server";

   return desc;
}

void NetObject::setMaskBits(U32 orMask)
{
   AssertFatal(orMask != 0, "Invalid net mask bits set.");
   AssertFatal(mDirtyMaskBits == 0 || (mPrevDirtyList != NULL || mNextDirtyList != NULL || mDirtyList == this), "Invalid dirty list state.");
   if(!mDirtyMaskBits)
   {
      AssertFatal(mNextDirtyList == NULL && mPrevDirtyList == NULL, "Object with zero mask already in list.");
      if(mDirtyList)
      {
         mNextDirtyList = mDirtyList;
         mDirtyList->mPrevDirtyList = this;
      }
      mDirtyList = this;
   }
   mDirtyMaskBits |= orMask;
   AssertFatal(mDirtyMaskBits == 0 || (mPrevDirtyList != NULL || mNextDirtyList != NULL || mDirtyList == this), "Invalid dirty list state.");
}

void NetObject::clearMaskBits(U32 orMask)
{
   if(isDeleted())
      return;
   if(mDirtyMaskBits)
   {
      mDirtyMaskBits &= ~orMask;
      if(!mDirtyMaskBits)
      {
         if(mPrevDirtyList)
            mPrevDirtyList->mNextDirtyList = mNextDirtyList;
         else
            mDirtyList = mNextDirtyList;
         if(mNextDirtyList)
            mNextDirtyList->mPrevDirtyList = mPrevDirtyList;
         mNextDirtyList = mPrevDirtyList = NULL;
      }
   }

   for(GhostInfo *walk = mFirstObjectRef; walk; walk = walk->nextObjectRef)
   {
      if(walk->updateMask && walk->updateMask == orMask)
      {
         walk->updateMask = 0;
         walk->connection->ghostPushToZero(walk);
      }
      else
         walk->updateMask &= ~orMask;
   }
}

void NetObject::collapseDirtyList()
{
#ifdef TORQUE_DEBUG
   Vector<NetObject *> tempV;
   for(NetObject *t = mDirtyList; t; t = t->mNextDirtyList)
      tempV.push_back(t);
#endif

   for(NetObject *obj = mDirtyList; obj; )
   {
      NetObject *next = obj->mNextDirtyList;
      U32 dirtyMask = obj->mDirtyMaskBits;

      obj->mNextDirtyList = NULL;
      obj->mPrevDirtyList = NULL;
      obj->mDirtyMaskBits = 0;

      if(!obj->isDeleted() && dirtyMask)
      {
         for(GhostInfo *walk = obj->mFirstObjectRef; walk; walk = walk->nextObjectRef)
         {
            U32 orMask = obj->filterMaskBits(dirtyMask,walk->connection);
            if(!walk->updateMask && orMask)
            {
               walk->updateMask = orMask;
               walk->connection->ghostPushNonZero(walk);
            }
            else
               walk->updateMask |= orMask;
         }
      }
      obj = next;
   }
   mDirtyList = NULL;
#ifdef TORQUE_DEBUG
   for(U32 i = 0; i < tempV.size(); i++)
   {
      AssertFatal(tempV[i]->mNextDirtyList == NULL && tempV[i]->mPrevDirtyList == NULL && tempV[i]->mDirtyMaskBits == 0, "Error in collapse");
   }
#endif
}

//-----------------------------------------------------------------------------

ConsoleMethod(NetObject,scopeToClient,void,3,3,"(NetConnection %client)"
              "Cause the NetObject to be forced as scoped on the specified NetConnection.")
{
   TORQUE_UNUSED(argc);
   NetConnection *conn;
   if(!Sim::findObject(argv[2], conn))
   {
      Con::errorf(ConsoleLogEntry::General, "NetObject::scopeToClient: Couldn't find connection %s", argv[2]);
      return;
   }
   conn->objectLocalScopeAlways(object);
}

ConsoleMethod(NetObject,clearScopeToClient,void,3,3,"clearScopeToClient(%client)"
              "Undo the effects of a scopeToClient() call.")
{
   TORQUE_UNUSED(argc);
   NetConnection *conn;
   if(!Sim::findObject(argv[2], conn))
   {
      Con::errorf(ConsoleLogEntry::General, "NetObject::clearScopeToClient: Couldn't find connection %s", argv[2]);
      return;
   }
   conn->objectLocalClearAlways(object);
}

ConsoleMethod(NetObject,setScopeAlways,void,2,2,"Always scope this object on all connections.")
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   object->setScopeAlways();
}

void NetObject::setScopeAlways()
{
   if(mNetFlags.test(Ghostable) && !mNetFlags.test(IsGhost))
   {
      mNetFlags.set(ScopeAlways);

      // if it's a ghost always object, add it to the ghost always set
      // for ClientReps created later.

      Sim::getGhostAlwaysSet()->addObject(this);

      // add it to all Connections that already exist.

      SimGroup *clientGroup = Sim::getClientGroup();
      SimGroup::iterator i;
      for(i = clientGroup->begin(); i != clientGroup->end(); i++)
      {
         NetConnection *con = (NetConnection *) (*i);
         if(con->isGhosting())
            con->objectInScope(this);
      }
   }
}

void NetObject::clearScopeAlways()
{
   if(!mNetFlags.test(IsGhost))
   {
      mNetFlags.clear(ScopeAlways);
      Sim::getGhostAlwaysSet()->removeObject(this);

      // Un ghost this object from all the connections
      while(mFirstObjectRef)
         mFirstObjectRef->connection->detachObject(mFirstObjectRef);
   }
}

bool NetObject::onAdd()
{
	if (!Parent::onAdd())
		return false;
	if ( ClientOnlyNetObject::isClientOnly(&(typeid(this))) )
	{
		if (gIsDedicated)
			return false;
		else
			mNetFlags = IsGhost;
	}

   if(mNetFlags.test(ScopeAlways))
      setScopeAlways();

   return true;
}

void NetObject::onRemove()
{
   while(mFirstObjectRef)
      mFirstObjectRef->connection->detachObject(mFirstObjectRef);

   Parent::onRemove();
}

//-----------------------------------------------------------------------------

F32 NetObject::getUpdatePriority(CameraScopeQuery*, U32, S32 updateSkips)
{
   return F32(updateSkips) * 0.1;
}

U32 NetObject::packUpdate(NetConnection* conn, U32 mask, BitStream* stream)
{
   return 0;
}

void NetObject::unpackUpdate(NetConnection*, BitStream*)
{
}

void NetObject::onCameraScopeQuery(NetConnection *cr, CameraScopeQuery* /*camInfo*/)
{
   // default behavior -
   // ghost everything that is ghostable

   for (SimSetIterator obj(Sim::getRootGroup()); *obj; ++obj)
   {
		NetObject* nobj = dynamic_cast<NetObject*>(*obj);
		if (nobj)
		{
			AssertFatal(!nobj->mNetFlags.test(NetObject::Ghostable) || !nobj->mNetFlags.test(NetObject::IsGhost),
			   "NetObject::onCameraScopeQuery: object marked both ghostable and as ghost");

			// Some objects don't ever want to be ghosted
			if (!nobj->mNetFlags.test(NetObject::Ghostable))
				continue;
         if (!nobj->mNetFlags.test(NetObject::ScopeAlways))
         {
            // it's in scope...
            cr->objectInScope(nobj);
         }
      }
   }
}

//-----------------------------------------------------------------------------

void NetObject::initPersistFields()
{
   Parent::initPersistFields();
}

ConsoleMethod( NetObject, getGhostID, S32, 2, 2, "")
{
   return object->getNetIndex();
}

ConsoleMethod( NetObject, getClientObject, S32, 2, 2, "Short-Circuit-Netorking: this is only valid for a local-client / singleplayer situation." )
{
   NetObject *obj = object->getClientObject();
   if ( obj )
      return obj->getId();

   return NULL;
}

ConsoleMethod( NetObject, getServerObject, S32, 2, 2, "Short-Circuit-Netorking: this is only valid for a local-client / singleplayer situation." )
{
   NetObject *obj = object->getServerObject();
   if ( obj )
      return obj->getId();

   return NULL;
}