//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/missionMarker.h"
#include "console/consoleTypes.h"
#include "core/color.h"

extern bool gEditingMission;
IMPLEMENT_CO_DATABLOCK_V1(MissionMarkerData);

//------------------------------------------------------------------------------
// Class: MissionMarker
//------------------------------------------------------------------------------
IMPLEMENT_CO_NETOBJECT_V1(MissionMarker);

MissionMarker::MissionMarker()
{
   mTypeMask |= StaticShapeObjectType | StaticObjectType;
   mDataBlock = 0;
   mAddedToScene = false;
   mNetFlags.set(Ghostable | ScopeAlways);
}

bool MissionMarker::onAdd()
{
   if(!Parent::onAdd() || !mDataBlock)
      return(false);

   if(gEditingMission)
   {
      addToScene();
      mAddedToScene = true;
   }

   return(true);
}

void MissionMarker::onRemove()
{
   if( mAddedToScene )
   {
      removeFromScene();
      mAddedToScene = false;
   }

   Parent::onRemove();
}

void MissionMarker::inspectPostApply()
{
   Parent::inspectPostApply();
   setMaskBits(PositionMask);
}

void MissionMarker::onEditorEnable()
{
   if(!mAddedToScene)
   {
      addToScene();
      mAddedToScene = true;
   }
}

void MissionMarker::onEditorDisable()
{
   if(mAddedToScene)
   {
      removeFromScene();
      mAddedToScene = false;
   }
}

bool MissionMarker::onNewDataBlock(GameBaseData * dptr)
{
   mDataBlock = dynamic_cast<MissionMarkerData*>(dptr);
   if(!mDataBlock || !Parent::onNewDataBlock(dptr))
      return(false);
   scriptOnNewDataBlock();
   return(true);
}

void MissionMarker::setTransform(const MatrixF& mat)
{
   Parent::setTransform(mat);
   setMaskBits(PositionMask);
}

U32 MissionMarker::packUpdate(NetConnection * con, U32 mask, BitStream * stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);
   if(stream->writeFlag(mask & PositionMask))
   {
      stream->writeAffineTransform(mObjToWorld);
      mathWrite(*stream, mObjScale);
   }

   return(retMask);
}

void MissionMarker::unpackUpdate(NetConnection * con, BitStream * stream)
{
   Parent::unpackUpdate(con, stream);
   if(stream->readFlag())
   {
      MatrixF mat;
      stream->readAffineTransform(&mat);
      Parent::setTransform(mat);

      Point3F scale;
      mathRead(*stream, &scale);
      setScale(scale);
   }
}

void MissionMarker::initPersistFields() {
   Parent::initPersistFields();
}

//------------------------------------------------------------------------------
// Class: WayPoint
//------------------------------------------------------------------------------
IMPLEMENT_CO_NETOBJECT_V1(WayPoint);

WayPointTeam::WayPointTeam()
{
   mTeamId = 0;
   mWayPoint = 0;
}

WayPoint::WayPoint()
{
   mName = StringTable->insert("");
}

void WayPoint::setHidden(bool hidden)
{
   if(isServerObject())
      setMaskBits(UpdateHiddenMask);
   mHidden = hidden;
}

bool WayPoint::onAdd()
{
   if(!Parent::onAdd())
      return(false);

   //
   if(isClientObject())
      Sim::getWayPointSet()->addObject(this);
   else
   {
      mTeam.mWayPoint = this;
      setMaskBits(UpdateNameMask|UpdateTeamMask);
   }

   return(true);
}

void WayPoint::inspectPostApply()
{
   Parent::inspectPostApply();
   if(!mName || !mName[0])
      mName = StringTable->insert("");
   setMaskBits(UpdateNameMask|UpdateTeamMask);
}

U32 WayPoint::packUpdate(NetConnection * con, U32 mask, BitStream * stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);
   if(stream->writeFlag(mask & UpdateNameMask))
      stream->writeString(mName);
   if(stream->writeFlag(mask & UpdateTeamMask))
      stream->write(mTeam.mTeamId);
   if(stream->writeFlag(mask & UpdateHiddenMask))
      stream->writeFlag(mHidden);
   return(retMask);
}

void WayPoint::unpackUpdate(NetConnection * con, BitStream * stream)
{
   Parent::unpackUpdate(con, stream);
   if(stream->readFlag())
      mName = stream->readSTString(true);
   if(stream->readFlag())
      stream->read(&mTeam.mTeamId);
   if(stream->readFlag())
      mHidden = stream->readFlag();
}

//-----------------------------------------------------------------------------
// TypeWayPointTeam
//-----------------------------------------------------------------------------
ConsoleType( WayPointTeam, TypeWayPointTeam, WayPointTeam )

ConsoleGetType( TypeWayPointTeam )
{
   char * buf = Con::getReturnBuffer(32);
   dSprintf(buf, 32, "%d", ((WayPointTeam*)dptr)->mTeamId);
   return(buf);
}

ConsoleSetType( TypeWayPointTeam )
{
   WayPointTeam * pTeam = (WayPointTeam*)dptr;
   pTeam->mTeamId = dAtoi(argv[0]);

   if(pTeam->mWayPoint && pTeam->mWayPoint->isServerObject())
      pTeam->mWayPoint->setMaskBits(WayPoint::UpdateTeamMask);
}

void WayPoint::initPersistFields()
{
   addGroup("Misc");	
   addField("name", TypeCaseString, Offset(mName, WayPoint));
   addField("team", TypeWayPointTeam, Offset(mTeam, WayPoint));
   endGroup("Misc");
   
   Parent::initPersistFields();
}

//------------------------------------------------------------------------------
// Class: SpawnSphere
//------------------------------------------------------------------------------
IMPLEMENT_CO_NETOBJECT_V1(SpawnSphere);

Sphere SpawnSphere::smSphere(Sphere::Octahedron);

SpawnSphere::SpawnSphere()
{
   mAutoSpawn = false;

   mRadius = 100.f;
   mSphereWeight = 100.f;
   mIndoorWeight = 100.f;
   mOutdoorWeight = 100.f;
}

bool SpawnSphere::onAdd()
{
   if(!Parent::onAdd())
      return(false);

   if(!isClientObject())
      setMaskBits(UpdateSphereMask);

   if (!isGhost())
   {
      Con::executef(this, "onAdd",scriptThis());

      if (mAutoSpawn)
         spawnObject();
   }

   return true;
}

SimObject* SpawnSphere::spawnObject(String additionalProps)
{
   SimObject* spawnObject = Sim::spawnObject(mSpawnClass, mSpawnDataBlock, mSpawnName,
                                             mSpawnProperties + " " + additionalProps, mSpawnScript);

   // If we have a spawnObject add it to the MissionCleanup group
   if (spawnObject)
   {
      SimObject* cleanup = Sim::findObject("MissionCleanup");

      if (cleanup)
      {
         SimGroup* missionCleanup = dynamic_cast<SimGroup*>(cleanup);

         missionCleanup->addObject(spawnObject);
      }
   }

   return spawnObject;
}

void SpawnSphere::inspectPostApply()
{
   Parent::inspectPostApply();
   setMaskBits(UpdateSphereMask);
}

U32 SpawnSphere::packUpdate(NetConnection * con, U32 mask, BitStream * stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   //
   if(stream->writeFlag(mask & UpdateSphereMask))
   {
      stream->writeFlag(mAutoSpawn);

      stream->write(mSpawnClass);
      stream->write(mSpawnDataBlock);
      stream->write(mSpawnName);
      stream->write(mSpawnProperties);
      stream->write(mSpawnScript);

      stream->write(mRadius);
      stream->write(mSphereWeight);
      stream->write(mIndoorWeight);
      stream->write(mOutdoorWeight);
   }
   return(retMask);
}

void SpawnSphere::unpackUpdate(NetConnection * con, BitStream * stream)
{
   Parent::unpackUpdate(con, stream);
   if(stream->readFlag())
   {
      mAutoSpawn = stream->readFlag();

      stream->read(&mSpawnClass);
      stream->read(&mSpawnDataBlock);
      stream->read(&mSpawnName);
      stream->read(&mSpawnProperties);
      stream->read(&mSpawnScript);

      stream->read(&mRadius);
      stream->read(&mSphereWeight);
      stream->read(&mIndoorWeight);
      stream->read(&mOutdoorWeight);
   }
}

void SpawnSphere::initPersistFields()
{
   addGroup("Spawn");	
   addField("spawnClass",        TypeRealString,      Offset(mSpawnClass,      SpawnSphere));
   addField("spawnDatablock",    TypeRealString,      Offset(mSpawnDataBlock,  SpawnSphere));
   addField("spawnProperties",   TypeRealString,      Offset(mSpawnProperties, SpawnSphere));
   addField("spawnScript",       TypeCommand,         Offset(mSpawnScript,     SpawnSphere), "Command to execute when spawning an object. New object id is stored in $SpawnObject.  Max 255 characters." );
   addField("autoSpawn",         TypeBool,            Offset(mAutoSpawn,       SpawnSphere));
   endGroup("Spawn");

   addGroup("Dimensions");	
   addField("radius",            TypeF32, Offset(mRadius, SpawnSphere));
   endGroup("Dimensions");	

   addGroup("Weight");	
   addField("sphereWeight",      TypeF32, Offset(mSphereWeight, SpawnSphere));
   addField("indoorWeight",      TypeF32, Offset(mIndoorWeight, SpawnSphere));
   addField("outdoorWeight",     TypeF32, Offset(mOutdoorWeight, SpawnSphere));
   endGroup("Weight");

   Parent::initPersistFields();
}

ConsoleMethod(SpawnSphere, spawnObject, S32, 2, 3, "([string additionalProps]) Spawns the object based \
                                                       on the SpawnSphere's class, datablock, properties, \
                                                       and script settings. Allows you to pass in extra properties.")
{
   String additionalProps;

   if (argc == 3)
      additionalProps = String(argv[2]);

   SimObject* obj = object->spawnObject(additionalProps);

   if (obj)
      return obj->getId();

   return -1;
}


//------------------------------------------------------------------------------
// Class: CameraBookmark
//------------------------------------------------------------------------------
IMPLEMENT_CO_NETOBJECT_V1(CameraBookmark);

CameraBookmark::CameraBookmark()
{
   mName = StringTable->insert("");
}

bool CameraBookmark::onAdd()
{
   if(!Parent::onAdd())
      return(false);

   //
   if(!isClientObject())
   {
      setMaskBits(UpdateNameMask);
   }

   if( isServerObject() && isMethod("onAdd") )
      Con::executef( this, "onAdd" );

   return(true);
}

void CameraBookmark::onRemove()
{
   if( isServerObject() && isMethod("onRemove") )
      Con::executef( this, "onRemove" );

   Parent::onRemove();
}

void CameraBookmark::onGroupAdd()
{
   if( isServerObject() && isMethod("onGroupAdd") )
      Con::executef( this, "onGroupAdd" );
}

void CameraBookmark::onGroupRemove()
{
   if( isServerObject() && isMethod("onGroupRemove") )
      Con::executef( this, "onGroupRemove" );
}

void CameraBookmark::inspectPostApply()
{
   Parent::inspectPostApply();
   if(!mName || !mName[0])
      mName = StringTable->insert("");
   setMaskBits(UpdateNameMask);

   if( isMethod("onInspectPostApply") )
      Con::executef( this, "onInspectPostApply" );
}

U32 CameraBookmark::packUpdate(NetConnection * con, U32 mask, BitStream * stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);
   if(stream->writeFlag(mask & UpdateNameMask))
      stream->writeString(mName);
   return(retMask);
}

void CameraBookmark::unpackUpdate(NetConnection * con, BitStream * stream)
{
   Parent::unpackUpdate(con, stream);
   if(stream->readFlag())
      mName = stream->readSTString(true);
}

void CameraBookmark::initPersistFields()
{
   //addGroup("Misc");	
   //addField("name", TypeCaseString, Offset(mName, CameraBookmark));
   //endGroup("Misc");

   Parent::initPersistFields();

   removeField("nameTag"); // From GameBase
}
