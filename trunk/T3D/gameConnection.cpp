//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/profiler.h"
#include "core/dnet.h"
#include "console/consoleTypes.h"
#include "console/simBase.h"
#include "core/stream/bitStream.h"
#include "sfx/sfxProfile.h"
#include "app/game.h"
#include "T3D/gameConnection.h"
#include "T3D/gameConnectionEvents.h"
#include "app/auth.h"
#include "T3D/gameProcess.h"
#include "core/util/safeDelete.h"
#include "T3D/camera.h"
#include "core/stream/fileStream.h"

//----------------------------------------------------------------------------
#define MAX_MOVE_PACKET_SENDS 4

#define ControlRequestTime 5000

const U32 GameConnection::CurrentProtocolVersion = 12;
const U32 GameConnection::MinRequiredProtocolVersion = 12;

//----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GameConnection);
S32 GameConnection::mLagThresholdMS = 0;
Signal<void(F32)> GameConnection::smFovUpdate;
Signal<void()>    GameConnection::smPlayingDemo;

#ifdef AFX_CAP_DATABLOCK_CACHE // AFX CODE BLOCK (db-cache) <<
StringTableEntry GameConnection::server_cache_filename = "";
StringTableEntry GameConnection::client_cache_filename = "";
bool GameConnection::server_cache_on = true;
bool GameConnection::client_cache_on = true;
#endif // AFX CODE BLOCK (db-cache) >>

//----------------------------------------------------------------------------
GameConnection::GameConnection()
{
	mRolloverObj = NULL;
	mPreSelectedObj = NULL;
	mSelectedObj = NULL;
	mChangedSelectedObj = false;
	mPreSelectTimestamp = 0;

   mLagging = false;
   mControlObject = NULL;
   mCameraObject = NULL;

   mMoveList.setConnection(this);

   mDataBlockModifiedKey = 0;
   mMaxDataBlockModifiedKey = 0;
   mAuthInfo = NULL;
   mControlForceMismatch = false;
   mConnectArgc = 0;
   for(U32 i = 0; i < MaxConnectArgs; i++)
      mConnectArgv[i] = 0;

   mJoinPassword = NULL;

   mMissionCRC = 0xffffffff;

   mDamageFlash = mWhiteOut = 0;

   mCameraPos = 0;
   mCameraSpeed = 10;

   mCameraFov = 90.f;
   mUpdateCameraFov = false;

   mAIControlled = false;

   mDisconnectReason[0] = 0;

   //blackout vars
   mBlackOut = 0.0f;
   mBlackOutTimeMS = 0;
   mBlackOutStartTimeMS = 0;
   mFadeToBlack = false;

   // first person
   mFirstPerson = false;
   mUpdateFirstPerson = false;
#ifdef AFX_CAP_DATABLOCK_CACHE // AFX CODE BLOCK (db-cache) <<
   client_db_stream = new InfiniteBitStream;
   server_cache_CRC = 0xffffffff;
#endif // AFX CODE BLOCK (db-cache) >>
}

GameConnection::~GameConnection()
{
   delete mAuthInfo;
   for(U32 i = 0; i < mConnectArgc; i++)
      dFree(mConnectArgv[i]);
   dFree(mJoinPassword);
#ifdef AFX_CAP_DATABLOCK_CACHE // AFX CODE BLOCK (db-cache) <<
   delete client_db_stream;
#endif // AFX CODE BLOCK (db-cache) >>
}

//----------------------------------------------------------------------------

bool GameConnection::canRemoteCreate()
{
   return true;
}

void GameConnection::setConnectArgs(U32 argc, const char **argv)
{
   if(argc > MaxConnectArgs)
      argc = MaxConnectArgs;
   mConnectArgc = argc;
   for(U32 i = 0; i < mConnectArgc; i++)
      mConnectArgv[i] = dStrdup(argv[i]);
}

void GameConnection::setJoinPassword(const char *password)
{
   mJoinPassword = dStrdup(password);
}

ConsoleMethod(GameConnection, setJoinPassword, void, 3, 3, "")
{
   object->setJoinPassword(argv[2]);
}

ConsoleMethod(GameConnection, setConnectArgs, void, 3, 17, "")
{
   object->setConnectArgs(argc - 2, argv + 2);
}

void GameConnection::onTimedOut()
{
   if(isConnectionToServer())
   {
      Con::printf("Connection to server timed out");
      Con::executef(this, "onConnectionTimedOut");
   }
   else
   {
      Con::printf("Client %d timed out.", getId());
      setDisconnectReason("TimedOut");
   }

}

void GameConnection::onConnectionEstablished(bool isInitiator)
{
   if(isInitiator)
   {
      setGhostFrom(false);
      setGhostTo(true);
      setSendingEvents(true);
      setTranslatesStrings(true);
      setIsConnectionToServer();
      mServerConnection = this;
      Con::printf("Connection established %d", getId());
      Con::executef(this, "onConnectionAccepted");
   }
   else
   {
      setGhostFrom(true);
      setGhostTo(false);
      setSendingEvents(true);
      setTranslatesStrings(true);
      Sim::getClientGroup()->addObject(this);
      mMoveList.init();

      const char *argv[MaxConnectArgs + 2];
      argv[0] = "onConnect";
      for(U32 i = 0; i < mConnectArgc; i++)
         argv[i + 2] = mConnectArgv[i];
      Con::execute(this, mConnectArgc + 2, argv);
   }
}

void GameConnection::onConnectTimedOut()
{
   Con::executef(this, "onConnectRequestTimedOut");
}

void GameConnection::onDisconnect(const char *reason)
{
   if(isConnectionToServer())
   {
      Con::printf("Connection with server lost.");
      Con::executef(this, "onConnectionDropped", reason);
      mMoveList.init();
   }
   else
   {
      Con::printf("Client %d disconnected.", getId());
      setDisconnectReason(reason);
   }
}

void GameConnection::onConnectionRejected(const char *reason)
{
   Con::executef(this, "onConnectRequestRejected", reason);
}

void GameConnection::handleStartupError(const char *errorString)
{
   Con::executef(this, "onConnectRequestRejected", errorString);
}

void GameConnection::writeConnectAccept(BitStream *stream)
{
   Parent::writeConnectAccept(stream);
   stream->write(getProtocolVersion());
}

bool GameConnection::readConnectAccept(BitStream *stream, const char **errorString)
{
   if(!Parent::readConnectAccept(stream, errorString))
      return false;

   U32 protocolVersion;
   stream->read(&protocolVersion);
   if(protocolVersion < MinRequiredProtocolVersion || protocolVersion > CurrentProtocolVersion)
   {
      *errorString = "CHR_PROTOCOL"; // this should never happen unless someone is faking us out.
      return false;
   }
   return true;
}

void GameConnection::writeConnectRequest(BitStream *stream)
{
   Parent::writeConnectRequest(stream);
   stream->writeString(GameString);
   stream->write(CurrentProtocolVersion);
   stream->write(MinRequiredProtocolVersion);
   stream->writeString(mJoinPassword);

   stream->write(mConnectArgc);
   for(U32 i = 0; i < mConnectArgc; i++)
      stream->writeString(mConnectArgv[i]);
}

bool GameConnection::readConnectRequest(BitStream *stream, const char **errorString)
{
   if(!Parent::readConnectRequest(stream, errorString))
      return false;
   U32 currentProtocol, minProtocol;
   char gameString[256];
   stream->readString(gameString);
   if(dStrcmp(gameString, GameString))
   {
      *errorString = "CHR_GAME";
      return false;
   }

   stream->read(&currentProtocol);
   stream->read(&minProtocol);

   char joinPassword[256];
   stream->readString(joinPassword);

   if(currentProtocol < MinRequiredProtocolVersion)
   {
      *errorString = "CHR_PROTOCOL_LESS";
      return false;
   }
   if(minProtocol > CurrentProtocolVersion)
   {
      *errorString = "CHR_PROTOCOL_GREATER";
      return false;
   }
   setProtocolVersion(currentProtocol < CurrentProtocolVersion ? currentProtocol : CurrentProtocolVersion);

   const char *serverPassword = Con::getVariable("Pref::Server::Password");
   if(serverPassword[0])
   {
      if(dStrcmp(joinPassword, serverPassword))
      {
         *errorString = "CHR_PASSWORD";
         return false;
      }
   }

   stream->read(&mConnectArgc);
   if(mConnectArgc > MaxConnectArgs)
   {
      *errorString = "CR_INVALID_ARGS";
      return false;
   }
   const char *connectArgv[MaxConnectArgs + 3];
   for(U32 i = 0; i < mConnectArgc; i++)
   {
      char argString[256];
      stream->readString(argString);
      mConnectArgv[i] = dStrdup(argString);
      connectArgv[i + 3] = mConnectArgv[i];
   }
   connectArgv[0] = "onConnectRequest";
   char buffer[256];
   Net::addressToString(getNetAddress(), buffer);
   connectArgv[2] = buffer;

   const char *ret = Con::execute(this, mConnectArgc + 3, connectArgv);
   if(ret[0])
   {
      *errorString = ret;
      return false;
   }
   return true;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void GameConnection::connectionError(const char *errorString)
{
   if(isConnectionToServer())
   {
      Con::printf("Connection error: %s.", errorString);
      Con::executef(this, "onConnectionError", errorString);
   }
   else
   {
      Con::printf("Client %d packet error: %s.", getId(), errorString);
      setDisconnectReason("Packet Error.");
   }
   deleteObject();
}


void GameConnection::setAuthInfo(const AuthInfo *info)
{
   mAuthInfo = new AuthInfo;
   *mAuthInfo = *info;
}

const AuthInfo *GameConnection::getAuthInfo()
{
   return mAuthInfo;
}


//----------------------------------------------------------------------------

void GameConnection::setControlObject(GameBase *obj)
{
   if(mControlObject == obj)
      return;

   if(mControlObject && mControlObject != mCameraObject)
      mControlObject->setControllingClient(0);

   if(obj)
   {
      // Nothing else is permitted to control this object.
      if (GameBase* coo = obj->getControllingObject())
         coo->setControlObject(0);
      if (GameConnection *con = obj->getControllingClient())
      {
         if(this != con)
         {
            // was it controlled via camera or control
            if(con->getControlObject() == obj)
               con->setControlObject(0);
            else
               con->setCameraObject(0);
         }
      }

      // We are now the controlling client of this object.
      obj->setControllingClient(this);
   }

   // Okay, set our control object.
   mControlObject = obj;
   mControlForceMismatch = true;

   if(mCameraObject.isNull())
      setScopeObject(mControlObject);
}

void GameConnection::setCameraObject(GameBase *obj)
{
   if(mCameraObject == obj)
      return;

   if(mCameraObject && mCameraObject != mControlObject)
      mCameraObject->setControllingClient(0);

   if(obj)
   {
      // nothing else is permitted to control this object
      if(GameBase *coo = obj->getControllingObject())
         coo->setControlObject(0);

      if(GameConnection *con = obj->getControllingClient())
      {
         if(this != con)
         {
            // was it controlled via camera or control
            if(con->getControlObject() == obj)
               con->setControlObject(0);
            else
               con->setCameraObject(0);
         }
      }

      // we are now the controlling client of this object
      obj->setControllingClient(this);
   }

   // Okay, set our camera object.
   mCameraObject = obj;

   if(mCameraObject.isNull())
      setScopeObject(mControlObject);
   else
   {
      setScopeObject(mCameraObject);

      // if this is a client then set the fov and active image
      if(isConnectionToServer())
      {
         F32 fov = mCameraObject->getDefaultCameraFov();
         //GameSetCameraFov(fov);
         smFovUpdate.trigger(fov);
      }
   }
}

GameBase* GameConnection::getCameraObject()
{
   // If there is no camera object, or if we're first person, return
   // the control object.
   if( !mControlObject.isNull() && (mCameraObject.isNull() || mFirstPerson))
      return mControlObject;
   return mCameraObject;
}

static S32 sChaseQueueSize = 0;
static MatrixF* sChaseQueue = 0;
static S32 sChaseQueueHead = 0;
static S32 sChaseQueueTail = 0;

bool GameConnection::getControlCameraTransform(F32 dt, MatrixF* mat)
{
   GameBase* obj = getCameraObject();
   if(!obj)
      return false;

   GameBase* cObj = obj;
   while((cObj = cObj->getControllingObject()) != 0)
   {
      if(cObj->useObjsEyePoint())
         obj = cObj;
   }

   if (dt) 
   {
      if (mFirstPerson || obj->onlyFirstPerson()) 
      {
         if (mCameraPos > 0)
            if ((mCameraPos -= mCameraSpeed * dt) <= 0)
               mCameraPos = 0;
      }
      else 
      {
         if (mCameraPos < 1)
            if ((mCameraPos += mCameraSpeed * dt) > 1)
               mCameraPos = 1;
      }
   }

   if (!sChaseQueueSize || mFirstPerson || obj->onlyFirstPerson())
      obj->getCameraTransform(&mCameraPos,mat);
   else 
   {
      MatrixF& hm = sChaseQueue[sChaseQueueHead];
      MatrixF& tm = sChaseQueue[sChaseQueueTail];
      obj->getCameraTransform(&mCameraPos,&hm);
      *mat = tm;
      if (dt) 
      {
         if ((sChaseQueueHead += 1) >= sChaseQueueSize)
            sChaseQueueHead = 0;
         if (sChaseQueueHead == sChaseQueueTail)
            if ((sChaseQueueTail += 1) >= sChaseQueueSize)
               sChaseQueueTail = 0;
      }
   }
   return true;
}

bool GameConnection::getControlCameraFov(F32 * fov)
{
   //find the last control object in the chain (client->player->turret->whatever...)
   GameBase *obj = getCameraObject();
   GameBase *cObj = NULL;
   while (obj)
   {
      cObj = obj;
      obj = obj->getControlObject();
   }
   if (cObj)
   {
      *fov = cObj->getCameraFov();
      return(true);
   }

   return(false);
}

bool GameConnection::isValidControlCameraFov(F32 fov)
{
   //find the last control object in the chain (client->player->turret->whatever...)
   GameBase *obj = getCameraObject();
   GameBase *cObj = NULL;
   while (obj)
   {
      cObj = obj;
      obj = obj->getControlObject();
   }

   return cObj ? cObj->isValidCameraFov(fov) : NULL;
}

bool GameConnection::setControlCameraFov(F32 fov)
{
   //find the last control object in the chain (client->player->turret->whatever...)
   GameBase *obj = getCameraObject();
   GameBase *cObj = NULL;
   while (obj)
   {
      cObj = obj;
      obj = obj->getControlObject();
   }
   if (cObj)
   {
      // allow shapebase to clamp fov to its datablock values
      cObj->setCameraFov(mClampF(fov, MinCameraFov, MaxCameraFov));
      fov = cObj->getCameraFov();

      // server fov of client has 1degree resolution
      if(S32(fov) != S32(mCameraFov))
         mUpdateCameraFov = true;

      mCameraFov = fov;
      return(true);
   }
   return(false);
}

bool GameConnection::getControlCameraVelocity(Point3F *vel)
{
   if (GameBase* obj = getCameraObject()) {
      *vel = obj->getVelocity();
      return true;
   }
   return false;
}

bool GameConnection::isControlObjectRotDampedCamera()
{
   if (Camera* cam = dynamic_cast<Camera*>(getCameraObject())) {
      if(cam->isRotationDamped())
         return true;
   }
   return false;
}

void GameConnection::setFirstPerson(bool firstPerson)
{
   mFirstPerson = firstPerson;
   mUpdateFirstPerson = true;
}



//----------------------------------------------------------------------------

bool GameConnection::onAdd()
{
   if (!Parent::onAdd())
      return false;

   return true;
}

void GameConnection::onRemove()
{
   if(isNetworkConnection())
   {
      sendDisconnectPacket(mDisconnectReason);
   }
   else if (isLocalConnection() && isConnectionToServer())
   {
      // we're a client-side but local connection
      // delete the server side of the connection on our local server so that it updates 
      // clientgroup and what not (this is so that we can disconnect from a local server
      // without needing to destroy and recreate the server before we can connect to it 
      // again)
      getRemoteConnection()->deleteObject();
      setRemoteConnectionObject(NULL);
   }
   if(!isConnectionToServer())
      Con::executef(this, "onDrop", mDisconnectReason);

   if (mControlObject)
      mControlObject->setControllingClient(0);
   Parent::onRemove();
}

void GameConnection::setDisconnectReason(const char *str)
{
   dStrncpy(mDisconnectReason, str, sizeof(mDisconnectReason) - 1);
   mDisconnectReason[sizeof(mDisconnectReason) - 1] = 0;
}

//----------------------------------------------------------------------------

void GameConnection::handleRecordedBlock(U32 type, U32 size, void *data)
{
   switch(type)
   {
      case BlockTypeMove:
         mMoveList.pushMove(*((Move *) data));
         if(isRecording()) // put it back into the stream
            recordBlock(type, size, data);
         break;
      default:
         Parent::handleRecordedBlock(type, size, data);
         break;
   }
}

void GameConnection::writeDemoStartBlock(ResizeBitStream *stream)
{
   // write all the data blocks to the stream:

   for(SimObjectId i = DataBlockObjectIdFirst; i <= DataBlockObjectIdLast; i++)
   {
      SimDataBlock *data;
      if(Sim::findObject(i, data))
      {
         stream->writeFlag(true);
         SimDataBlockEvent evt(data);
         evt.pack(this, stream);
         stream->validate();
      }
   }
   stream->writeFlag(false);
   stream->write(mFirstPerson);
   stream->write(mCameraPos);
   stream->write(mCameraSpeed);

   stream->writeString(Con::getVariable("$Client::MissionFile"));

   mMoveList.writeDemoStartBlock(stream);

   // dump all the "demo" vars associated with this connection:
   SimFieldDictionaryIterator itr(getFieldDictionary());

   SimFieldDictionary::Entry *entry;
   while((entry = *itr) != NULL)
   {
      if(!dStrnicmp(entry->slotName, "demo", 4))
      {
         stream->writeFlag(true);
         stream->writeString(entry->slotName + 4);
         stream->writeString(entry->value);
         stream->validate();
      }
      ++itr;
   }
   stream->writeFlag(false);
   Parent::writeDemoStartBlock(stream);

   stream->validate();

   // dump out the control object ghost id
   S32 idx = mControlObject ? getGhostIndex(mControlObject) : -1;
   stream->write(idx);
   if(mControlObject)
   {
#ifdef TORQUE_NET_STATS
      U32 beginPos = stream->getBitPosition();
#endif
      mControlObject->writePacketData(this, stream);
#ifdef TORQUE_NET_STATS
      mControlObject->getClassRep()->updateNetStatWriteData( stream->getBitPosition() - beginPos);
#endif
   }
   idx = mCameraObject ? getGhostIndex(mCameraObject) : -1;
   stream->write(idx);
   if(mCameraObject && mCameraObject != mControlObject)
   {
#ifdef TORQUE_NET_STATS
      U32 beginPos = stream->getBitPosition();
#endif

      mCameraObject->writePacketData(this, stream);

#ifdef TORQUE_NET_STATS
      mCameraObject->getClassRep()->updateNetStatWriteData( stream->getBitPosition() - beginPos);
#endif
   }
   mLastControlRequestTime = Platform::getVirtualMilliseconds();
}

bool GameConnection::readDemoStartBlock(BitStream *stream)
{
   while(stream->readFlag())
   {
      SimDataBlockEvent evt;
      evt.unpack(this, stream);
      evt.process(this);
   }

   while(mDataBlockLoadList.size())
   {
      preloadNextDataBlock(false);
      if(mErrorBuffer.isNotEmpty())
         return false;
   }

   stream->read(&mFirstPerson);
   stream->read(&mCameraPos);
   stream->read(&mCameraSpeed);

   char buf[256];
   stream->readString(buf);
   Con::setVariable("$Client::MissionFile",buf);

   mMoveList.readDemoStartBlock(stream);

   // read in all the demo vars associated with this recording
   // they are all tagged on to the object and start with the
   // string "demo"

   while(stream->readFlag())
   {
      StringTableEntry slotName = StringTable->insert("demo");
      char array[256];
      char value[256];
      stream->readString(array);
      stream->readString(value);
      setDataField(slotName, array, value);
   }
   bool ret = Parent::readDemoStartBlock(stream);
   // grab the control object
   S32 idx;
   stream->read(&idx);

   GameBase * obj = 0;
   if(idx != -1)
   {
      obj = dynamic_cast<GameBase*>(resolveGhost(idx));
      setControlObject(obj);
      obj->readPacketData(this, stream);
   }

   // Get the camera object, and read it in if it's different
   S32 idx2;
   stream->read(&idx2);
   obj = 0;
   if(idx2 != -1 && idx2 != idx)
   {
      obj = dynamic_cast<GameBase*>(resolveGhost(idx2));
      setCameraObject(obj);
      obj->readPacketData(this, stream);
   }
   return ret;
}

void GameConnection::demoPlaybackComplete()
{
   static const char *demoPlaybackArgv[1] = { "demoPlaybackComplete" };
   Sim::postCurrentEvent(Sim::getRootGroup(), new SimConsoleEvent(1, demoPlaybackArgv, false));
   Parent::demoPlaybackComplete();
}

void GameConnection::ghostPreRead(NetObject * nobj, bool newGhost)
{
   Parent::ghostPreRead( nobj, newGhost );

   mMoveList.ghostPreRead(nobj,newGhost);
}

void GameConnection::ghostReadExtra(NetObject * nobj, BitStream * bstream, bool newGhost)
{
   Parent::ghostReadExtra( nobj, bstream, newGhost );

   mMoveList.ghostReadExtra(nobj, bstream, newGhost);
   }

void GameConnection::ghostWriteExtra(NetObject * nobj, BitStream * bstream)
{
   Parent::ghostWriteExtra( nobj, bstream);

   mMoveList.ghostWriteExtra(nobj, bstream);
}

//----------------------------------------------------------------------------

void GameConnection::readPacket(BitStream *bstream)
{
   bstream->clearStringBuffer();
   bstream->clearCompressionPoint();

   if (isConnectionToServer())
   {
      mMoveList.clientReadMovePacket(bstream);

      mDamageFlash = 0;
      mWhiteOut = 0;
      if(bstream->readFlag())
      {
         if(bstream->readFlag())
            mDamageFlash = bstream->readFloat(7);
         if(bstream->readFlag())
            mWhiteOut = bstream->readFloat(7) * 1.5;
      }

      if (bstream->readFlag())
      {
         if(bstream->readFlag())
         {
            // the control object is dirty...so we get an update:
            bool callScript = false;
            if(mControlObject.isNull())
               callScript = true;

            S32 gIndex = bstream->readInt(NetConnection::GhostIdBitSize);
            GameBase* obj = dynamic_cast<GameBase*>(resolveGhost(gIndex));
            if (mControlObject != obj)
               setControlObject(obj);
#ifdef TORQUE_NET_STATS
            U32 beginSize = bstream->getBitPosition();
#endif
            obj->readPacketData(this, bstream);
#ifdef TORQUE_NET_STATS
            obj->getClassRep()->updateNetStatReadData(bstream->getBitPosition() - beginSize);
#endif

            // let move list know that control object is dirty
            mMoveList.markControlDirty();

            if(callScript)
               Con::executef(this, "initialControlSet");
         }
         else
         {
            // read out the compression point
            Point3F pos;
            bstream->read(&pos.x);
            bstream->read(&pos.y);
            bstream->read(&pos.z);
            bstream->setCompressionPoint(pos);
         }
      }

      if (bstream->readFlag())
      {
         S32 gIndex = bstream->readInt(NetConnection::GhostIdBitSize);
         GameBase* obj = dynamic_cast<GameBase*>(resolveGhost(gIndex));
         setCameraObject(obj);
         obj->readPacketData(this, bstream);
      }
      else
         setCameraObject(0);

      // server changed first person
      if(bstream->readFlag())
      {
         setFirstPerson(bstream->readFlag());
         mUpdateFirstPerson = false;
      }

      // server forcing a fov change?
      if(bstream->readFlag())
      {
         S32 fov = bstream->readInt(8);
         setControlCameraFov((F32)fov);

         // don't bother telling the server if we were able to set the fov
         F32 setFov;
         if(getControlCameraFov(&setFov) && (S32(setFov) == fov))
            mUpdateCameraFov = false;

         // update the games fov info
         smFovUpdate.trigger((F32)fov);
      }
   }
   else
   {
      mMoveList.serverReadMovePacket(bstream);

      mCameraPos = bstream->readFlag() ? 1.0f : 0.0f;
      if (bstream->readFlag())
         mControlForceMismatch = true;

      // client changed first person
      if(bstream->readFlag())
      {
         setFirstPerson(bstream->readFlag());
         mUpdateFirstPerson = false;
      }

      // check fov change.. 1degree granularity on server
      if(bstream->readFlag())
      {
         S32 fov = mClamp(bstream->readInt(8), S32(MinCameraFov), S32(MaxCameraFov));
         setControlCameraFov((F32)fov);

         // may need to force client back to a valid fov
         F32 setFov;
         if(getControlCameraFov(&setFov) && (S32(setFov) == fov))
            mUpdateCameraFov = false;
      }
   }

   Parent::readPacket(bstream);
   bstream->clearCompressionPoint();
   bstream->clearStringBuffer();
   if (isConnectionToServer())
   {
      PROFILE_START(ClientCatchup);
      gClientProcessList.clientCatchup(this);
      PROFILE_END();
   }
}

void GameConnection::writePacket(BitStream *bstream, PacketNotify *note)
{
   bstream->clearCompressionPoint();
   bstream->clearStringBuffer();
   GamePacketNotify *gnote = (GamePacketNotify *) note;

   U32 startPos = bstream->getBitPosition();
   if (isConnectionToServer())
   {
      mMoveList.clientWriteMovePacket(bstream);

      bstream->writeFlag(mCameraPos == 1);

      // if we're recording, we want to make sure that we get periodic updates of the
      // control object "just in case" - ie if the math copro is different between the
      // recording machine (SIMD vs FPU), we get periodic corrections

      bool forceUpdate = false;
      if(isRecording())
      {
         U32 currentTime = Platform::getVirtualMilliseconds();
         if(currentTime - mLastControlRequestTime > ControlRequestTime)
         {
            mLastControlRequestTime = currentTime;
            forceUpdate=true;;
         }
      }
      bstream->writeFlag(forceUpdate);

      // first person changed?
      if(bstream->writeFlag(mUpdateFirstPerson)) 
      {
         bstream->writeFlag(mFirstPerson);
         mUpdateFirstPerson = false;
      }

      // camera fov changed? (server fov resolution is 1 degree)
      if(bstream->writeFlag(mUpdateCameraFov))
      {
         bstream->writeInt(mClamp(S32(mCameraFov), S32(MinCameraFov), S32(MaxCameraFov)), 8);
         mUpdateCameraFov = false;
      }
      DEBUG_LOG(("PKLOG %d CLIENTMOVES: %d", getId(), bstream->getCurPos() - startPos));
   }
   else
   {
      mMoveList.serverWriteMovePacket(bstream);

      // get the ghost index of the control object, and write out
      // all the damage flash & white out

      S32 gIndex = -1;
      if (!mControlObject.isNull())
      {
         gIndex = getGhostIndex(mControlObject);

         F32 flash = mControlObject->getDamageFlash();
         F32 whiteOut = mControlObject->getWhiteOut();
         if(bstream->writeFlag(flash != 0 || whiteOut != 0))
         {
            if(bstream->writeFlag(flash != 0))
               bstream->writeFloat(flash, 7);
            if(bstream->writeFlag(whiteOut != 0))
               bstream->writeFloat(whiteOut/1.5, 7);
         }
      }
      else
         bstream->writeFlag(false);

      if (bstream->writeFlag(gIndex != -1))
      {
         // assume that the control object will write in a compression point
         if(bstream->writeFlag(mMoveList.isMismatch() || mControlForceMismatch))
         {
#ifdef TORQUE_DEBUG_NET
            if (mMoveList.isMismatch())
               Con::printf("packetDataChecksum disagree!");
            else
               Con::printf("packetDataChecksum disagree! (force)");
#endif

            bstream->writeInt(gIndex, NetConnection::GhostIdBitSize);
#ifdef TORQUE_NET_STATS
            U32 beginSize = bstream->getBitPosition();
#endif
            mControlObject->writePacketData(this, bstream);
#ifdef TORQUE_NET_STATS
            mControlObject->getClassRep()->updateNetStatWriteData(bstream->getBitPosition() - beginSize);
#endif
            mControlForceMismatch = false;
         }
         else
         {
            // we'll have to use the control object's position as the compression point
            // should make this lower res for better space usage:
            Point3F coPos = mControlObject->getPosition();
            bstream->write(coPos.x);
            bstream->write(coPos.y);
            bstream->write(coPos.z);
            bstream->setCompressionPoint(coPos);
         }
      }
      DEBUG_LOG(("PKLOG %d CONTROLOBJECTSTATE: %d", getId(), bstream->getCurPos() - startPos));
      startPos = bstream->getBitPosition();

      if (!mCameraObject.isNull() && mCameraObject != mControlObject)
      {
         gIndex = getGhostIndex(mCameraObject);
         if (bstream->writeFlag(gIndex != -1))
         {
            bstream->writeInt(gIndex, NetConnection::GhostIdBitSize);
            mCameraObject->writePacketData(this, bstream);
         }
      }
      else
         bstream->writeFlag( false );

      // first person changed?
      if(bstream->writeFlag(mUpdateFirstPerson)) 
      {
         bstream->writeFlag(mFirstPerson);
         mUpdateFirstPerson = false;
      }

      // server forcing client fov?
      gnote->cameraFov = -1;
      if(bstream->writeFlag(mUpdateCameraFov))
      {
         gnote->cameraFov = mClamp(S32(mCameraFov), S32(MinCameraFov), S32(MaxCameraFov));
         bstream->writeInt(gnote->cameraFov, 8);
         mUpdateCameraFov = false;
      }
      DEBUG_LOG(("PKLOG %d PINGCAMSTATE: %d", getId(), bstream->getCurPos() - startPos));
   }

   Parent::writePacket(bstream, note);
   bstream->clearCompressionPoint();
   bstream->clearStringBuffer();
}


void GameConnection::detectLag()
{
   //see if we're lagging...
   S32 curTime = Sim::getCurrentTime();
   if (curTime - mLastPacketTime > mLagThresholdMS)
   {
      if (!mLagging)
      {
         mLagging = true;
         Con::executef(this, "setLagIcon", "true");
      }
   }
   else if (mLagging)
   {
      mLagging = false;
      Con::executef(this, "setLagIcon", "false");
   }
}

GameConnection::GamePacketNotify::GamePacketNotify()
{
   // need to fill in empty notifes for demo start block
   cameraFov = 0;
}

NetConnection::PacketNotify *GameConnection::allocNotify()
{
   return new GamePacketNotify;
}

void GameConnection::packetReceived(PacketNotify *note)
{
   //record the time so we can tell if we're lagging...
   mLastPacketTime = Sim::getCurrentTime();

   // If we wanted to do something special, we grab our note like this:
   //GamePacketNotify *gnote = (GamePacketNotify *) note;

   Parent::packetReceived(note);
}

void GameConnection::packetDropped(PacketNotify *note)
{
   Parent::packetDropped(note);
   GamePacketNotify *gnote = (GamePacketNotify *) note;
   if(gnote->cameraFov != -1)
      mUpdateCameraFov = true;
}

//----------------------------------------------------------------------------

void GameConnection::play2D(SFXProfile* profile)
{
   postNetEvent(new Sim2DAudioEvent(profile));
}

void GameConnection::play3D(SFXProfile* profile, const MatrixF *transform)
{
   if ( !transform )
      play2D(profile);

   else if ( !mControlObject )
      postNetEvent(new Sim3DAudioEvent(profile,transform));

   else
   {
      // TODO: Maybe improve this to account for the duration
      // of the sound effect and if the control object can get
      // into hearing range within time?

      // Only post the event if it's within audible range
      // of the control object.
      Point3F ear,pos;
      transform->getColumn(3,&pos);
      mControlObject->getTransform().getColumn(3,&ear);
      if ((ear - pos).len() < profile->getDescription()->mMaxDistance)
         postNetEvent(new Sim3DAudioEvent(profile,transform));
   } 
}

void GameConnection::doneScopingScene()
{
   // Could add special post-scene scoping here, such as scoping
   // objects not visible to the camera, but visible to sensors.
}

void GameConnection::preloadDataBlock(SimDataBlock *db)
{
   mDataBlockLoadList.push_back(db);
   if(mDataBlockLoadList.size() == 1)
      preloadNextDataBlock(false);
}

void GameConnection::fileDownloadSegmentComplete()
{
   // this is called when a the file list has finished processing...
   // at this point we can try again to add the object
   // subclasses can override this to do, for example, datablock redos.
   if(mDataBlockLoadList.size())
      preloadNextDataBlock(mNumDownloadedFiles != 0);
   Parent::fileDownloadSegmentComplete();
}

void GameConnection::preloadNextDataBlock(bool hadNewFiles)
{
   if(!mDataBlockLoadList.size())
      return;
   while(mDataBlockLoadList.size())
   {
      // only check for new files if this is the first load, or if new
      // files were downloaded from the server.
//       if(hadNewFiles)
//          gResourceManager->setMissingFileLogging(true);
//       gResourceManager->clearMissingFileList();
      SimDataBlock *object = mDataBlockLoadList[0];
      if(!object)
      {
         // a null object is used to signify that the last ghost in the list is down
         mDataBlockLoadList.pop_front();
         AssertFatal(mDataBlockLoadList.size() == 0, "Error! Datablock save list should be empty!");
         sendConnectionMessage(DataBlocksDownloadDone, mDataBlockSequence);
#ifdef AFX_CAP_DATABLOCK_CACHE // AFX CODE BLOCK (db-cache) <<
		 // This should be the last of the datablocks. An argument of false
		 // indicates that this is a client save.
		 if (clientCacheEnabled())
			 saveDatablockCache(false);
#endif // AFX CODE BLOCK (db-cache) >>
//          gResourceManager->setMissingFileLogging(false);
         return;
      }
      mFilesWereDownloaded = hadNewFiles;
      if(!object->preload(false, mErrorBuffer))
      {
         mFilesWereDownloaded = false;
         // make sure there's an error message if necessary
         if(mErrorBuffer.isEmpty())
            setLastError("Invalid packet. (object preload)");

         // if there were no new files, make sure the error message
         // is the one from the last time we tried to add this object
         if(!hadNewFiles)
         {
            mErrorBuffer = mLastFileErrorBuffer;
//             gResourceManager->setMissingFileLogging(false);
            return;
         }

         // object failed to load, let's see if it had any missing files
         if(isLocalConnection() /*|| !gResourceManager->getMissingFileList(mMissingFileList)*/)
         {
            // no missing files, must be an error
            // connection will automagically delete the ghost always list
            // when this error is reported.
//             gResourceManager->setMissingFileLogging(false);
            return;
         }

         // ok, copy the error buffer out to a scratch pad for now
         mLastFileErrorBuffer = mErrorBuffer;
         mErrorBuffer = String();

         // request the missing files...
         mNumDownloadedFiles = 0;
         sendNextFileDownloadRequest();
         break;
      }
      mFilesWereDownloaded = false;
//       gResourceManager->setMissingFileLogging(false);
      mDataBlockLoadList.pop_front();
      hadNewFiles = true;
   }
}


//----------------------------------------------------------------------------
//localconnection only blackout functions
void GameConnection::setBlackOut(bool fadeToBlack, S32 timeMS)
{
   mFadeToBlack = fadeToBlack;
   mBlackOutStartTimeMS = Sim::getCurrentTime();
   mBlackOutTimeMS = timeMS;

   //if timeMS <= 0 set the value instantly
   if (mBlackOutTimeMS <= 0)
      mBlackOut = (mFadeToBlack ? 1.0f : 0.0f);
}

F32 GameConnection::getBlackOut()
{
   S32 curTime = Sim::getCurrentTime();

   //see if we're in the middle of a black out
   if (curTime < mBlackOutStartTimeMS + mBlackOutTimeMS)
   {
      S32 elapsedTime = curTime - mBlackOutStartTimeMS;
      F32 timePercent = F32(elapsedTime) / F32(mBlackOutTimeMS);
      mBlackOut = (mFadeToBlack ? timePercent : 1.0f - timePercent);
   }
   else
      mBlackOut = (mFadeToBlack ? 1.0f : 0.0f);

   //return the blackout time
   return mBlackOut;
}

void GameConnection::handleConnectionMessage(U32 message, U32 sequence, U32 ghostCount)
{
   if(isConnectionToServer())
   {
      if(message == DataBlocksDone)
      {
         mDataBlockLoadList.push_back(NULL);
         mDataBlockSequence = sequence;
         if(mDataBlockLoadList.size() == 1)
            preloadNextDataBlock(true);
      }
   }
   else
   {
      if(message == DataBlocksDownloadDone)
      {
         if(getDataBlockSequence() == sequence)
            Con::executef(this, "onDataBlocksDone", Con::getIntArg(getDataBlockSequence()));
      }
   }
   Parent::handleConnectionMessage(message, sequence, ghostCount);
}

//----------------------------------------------------------------------------

ConsoleMethod( GameConnection, transmitDataBlocks, void, 3, 3, "(int sequence)")
{
    // Set the datablock sequence.
    object->setDataBlockSequence(dAtoi(argv[2]));

    // Store a pointer to the datablock group.
    SimDataBlockGroup* pGroup = Sim::getDataBlockGroup();

    // Determine the size of the datablock group.
    const U32 iCount = pGroup->size();

	// If this is the local client...
#ifdef AFX_CAP_DATABLOCK_CACHE // AFX CODE BLOCK (db-cache) <<
	if (GameConnection::getLocalClientConnection() == object && !GameConnection::serverCacheEnabled())
#else
	if (GameConnection::getLocalClientConnection() == object)
#endif // AFX CODE BLOCK (db-cache) >>
    {
        // Set up a pointer to the datablock.
        SimDataBlock* pDataBlock = 0;

        // Iterate through all the datablocks...
        for (U32 i = 0; i < iCount; i++)
        {
            // Get a pointer to the datablock in question...
            pDataBlock = (SimDataBlock*)(*pGroup)[i];

            // Set the client's new modified key.
            object->setMaxDataBlockModifiedKey(pDataBlock->getModifiedKey());

            // Set up a buffer for the datablock send.
            U8 iBuffer[16384];
            BitStream mStream(iBuffer, 16384);

            // Pack the datablock stream.
            pDataBlock->packData(&mStream);

            // Set the stream position back to zero.
            mStream.setPosition(0);

            // Unpack the datablock stream.
            pDataBlock->unpackData(&mStream);

            // Call the console function to set the number of blocks to be sent.
            Con::executef("onDataBlockObjectReceived", Con::getIntArg(i), Con::getIntArg(iCount));

            // Preload the datablock on the dummy client.
            pDataBlock->preload(false, NetConnection::getErrorBuffer());
        }

        // Get the last datablock (if any)...
        if (pDataBlock)
        {
            // Ensure the datablock modified key is set.
            object->setDataBlockModifiedKey(object->getMaxDataBlockModifiedKey());

            // Ensure that the client knows that the datablock send is done...
            object->sendConnectionMessage(GameConnection::DataBlocksDone, object->getDataBlockSequence());
        }
    } 
    else
    {
        // Otherwise, store the current datablock modified key.
        const S32 iKey = object->getDataBlockModifiedKey();

        // Iterate through the datablock group...
        U32 i = 0;
        for (; i < iCount; i++)
        {
            // If the datablock's modified key has already been set, break out of the loop...
            if (((SimDataBlock*)(*pGroup)[i])->getModifiedKey() > iKey)
            {
                break;
            }
        }

        // If this is the last datablock in the group...
        if (i == iCount)
        {
            // Ensure that the client knows that the datablock send is done...
            object->sendConnectionMessage(GameConnection::DataBlocksDone, object->getDataBlockSequence());

            // Then exit out since nothing else needs to be done.
            return;
        }

        // Set the maximum datablock modified key value.
        object->setMaxDataBlockModifiedKey(iKey);

        // Get the minimum number of datablocks...
        const U32 iMax = getMin(i + DataBlockQueueCount, iCount);

        // Iterate through the remaining datablocks...
        for (;i < iMax; i++)
        {
            // Get a pointer to the datablock in question...
            SimDataBlock* pDataBlock = (SimDataBlock*)(*pGroup)[i];

            // Post the datablock event to the client.
            object->postNetEvent(new SimDataBlockEvent(pDataBlock, i, iCount, object->getDataBlockSequence()));
        }
    }
}

ConsoleMethod( GameConnection, activateGhosting, void, 2, 2, "")
{
   object->activateGhosting();
}

ConsoleMethod( GameConnection, resetGhosting, void, 2, 2, "")
{
   object->resetGhosting();
}

ConsoleMethod( GameConnection, setControlObject, bool, 3, 3, "(ShapeBase object)")
{
   GameBase *gb;
   if(!Sim::findObject(argv[2], gb))
      return false;

   object->setControlObject(gb);
   return true;
}

ConsoleMethod( GameConnection, getControlObject, S32, 2, 2, "")
{
   TORQUE_UNUSED(argv);
   SimObject* cp = object->getControlObject();
   return cp? cp->getId(): 0;
}

ConsoleMethod( GameConnection, isAIControlled, bool, 2, 2, "")
{
   return object->isAIControlled();
}

ConsoleMethod( GameConnection, isControlObjectRotDampedCamera, bool, 2, 2, "")
{
   return object->isControlObjectRotDampedCamera();
}

ConsoleMethod( GameConnection, play2D, bool, 3, 3, "(SFXProfile ap)")
{
   SFXProfile *profile;
   if(!Sim::findObject(argv[2], profile))
      return false;
   object->play2D(profile);
   return true;
}

ConsoleMethod( GameConnection, play3D, bool, 4, 4, "(SFXProfile ap, Transform pos)")
{
   SFXProfile *profile;
   if(!Sim::findObject(argv[2], profile))
      return false;

   Point3F pos(0,0,0);
   AngAxisF aa;
   aa.axis.set(0,0,1);
   aa.angle = 0;
   dSscanf(argv[3],"%g %g %g %g %g %g %g",
      &pos.x,&pos.y,&pos.z,&aa.axis.x,&aa.axis.y,&aa.axis.z,&aa.angle);
   MatrixF mat;
   aa.setMatrix(&mat);
   mat.setColumn(3,pos);

   object->play3D(profile,&mat);
   return true;
}

ConsoleMethod( GameConnection, chaseCam, bool, 3, 3, "(int size)")
{
   S32 size = dAtoi(argv[2]);
   if (size != sChaseQueueSize) 
   {
      SAFE_DELETE_ARRAY(sChaseQueue);

      sChaseQueueSize = size;
      sChaseQueueHead = sChaseQueueTail = 0;

      if (size) 
      {
         sChaseQueue = new MatrixF[size];
         return true;
      }
   }
   return false;
}

ConsoleMethod( GameConnection, setControlCameraFov, void, 3, 3, "(int newFOV)"
              "Set new FOV in degrees.")
{
   object->setControlCameraFov(dAtof(argv[2]));
}

ConsoleMethod( GameConnection, getControlCameraFov, F32, 2, 2, "")
{
   F32 fov = 0.0f;
   if(!object->getControlCameraFov(&fov))
      return(0.0f);
   return(fov);
}

ConsoleMethod( GameConnection, setBlackOut, void, 4, 4, "(bool doFade, int timeMS)")
{
   object->setBlackOut(dAtob(argv[2]), dAtoi(argv[3]));
}

ConsoleMethod( GameConnection, setMissionCRC, void, 3, 3, "(int CRC)")
{
   if(object->isConnectionToServer())
      return;

   object->postNetEvent(new SetMissionCRCEvent(dAtoi(argv[2])));
}

ConsoleMethod( GameConnection, delete, void, 2, 3, "(string reason=NULL) Disconnect a client; reason is sent as part of the disconnect packet.")
{
   if (argc == 3)
      object->setDisconnectReason(argv[2]);
   object->deleteObject();
}


//--------------------------------------------------------------------------
void GameConnection::consoleInit()
{
   Con::addVariable("Pref::Net::LagThreshold", TypeS32, &mLagThresholdMS);
   // Con::addVariable("specialFog", TypeBool, &SceneGraph::useSpecial);
#ifdef AFX_CAP_DATABLOCK_CACHE // AFX CODE BLOCK (db-cache) <<
   Con::addVariable("$Pref::Server::DatablockCacheFilename",  TypeString,   &server_cache_filename);
   Con::addVariable("$pref::Client::DatablockCacheFilename",  TypeString,   &client_cache_filename);
   Con::addVariable("$Pref::Server::EnableDatablockCache",    TypeBool,     &server_cache_on);
   Con::addVariable("$pref::Client::EnableDatablockCache",    TypeBool,     &client_cache_on);
#endif // AFX CODE BLOCK (db-cache) >>
}

ConsoleMethod(GameConnection, startRecording, void, 3, 3, "(string fileName)records the network connection to a demo file.")
{
   char fileName[1024];
   Con::expandScriptFilename(fileName, sizeof(fileName), argv[2]);
   object->startDemoRecord(fileName);
}

ConsoleMethod(GameConnection, stopRecording, void, 2, 2, "()stops the demo recording.")
{
   object->stopRecording();
}

ConsoleMethod(GameConnection, playDemo, bool, 3, 3, "(string demoFileName)plays a previously recorded demo.")
{
   char filename[1024];
   Con::expandScriptFilename(filename, sizeof(filename), argv[2]);

   // Note that calling onConnectionEstablished will change the values in argv!
   object->onConnectionEstablished(true);
   object->setEstablished();

   if(!object->replayDemoRecord(filename))
   {
      Con::printf("Unable to open demo file %s.", filename);
      object->deleteObject();
      return false;
   }

   // After demo has loaded, execute the scene re-light the scene
   //SceneLighting::lightScene(0, 0);
   GameConnection::smPlayingDemo.trigger();

   return true;
}

ConsoleMethod(GameConnection, isDemoPlaying, bool, 2, 2, "isDemoPlaying();")
{
   TORQUE_UNUSED(argc);
   TORQUE_UNUSED(argv);
   return object->isPlayingBack();
}

ConsoleMethod(GameConnection, isDemoRecording, bool, 2, 2, "()")
{
   TORQUE_UNUSED(argc);
   TORQUE_UNUSED(argv);
   return object->isRecording();
}

ConsoleMethod( GameConnection, listClassIDs, void, 2, 2, "() List all of the "
              "classes that this connection knows about, and what their IDs "
              "are. Useful for debugging network problems.")
{
   Con::printf("--------------- Class ID Listing ----------------");
   Con::printf(" id    |   name");

   for(AbstractClassRep *rep = AbstractClassRep::getClassList();
      rep;
      rep = rep->getNextClass())
   {
      ConsoleObject *obj = rep->create();
      if(obj && rep->getClassId(object->getNetClassGroup()) >= 0)
         Con::printf("%7.7d| %s", rep->getClassId(object->getNetClassGroup()), rep->getClassName());
      delete obj;
   }
}

ConsoleStaticMethod(GameConnection, getServerConnection, S32, 1, 1, "() Get the server connection if any.")
{
   if(GameConnection::getConnectionToServer())
      return GameConnection::getConnectionToServer()->getId();
   else
   {
      Con::errorf("GameConnection::getServerConnection - no connection available.");
      return -1;
   }
}

ConsoleMethod(GameConnection, setCameraObject, S32, 3, 3, "")
{
   NetObject *obj;
   if(!Sim::findObject(argv[2], obj))
      return false;

   object->setCameraObject(dynamic_cast<GameBase*>(obj));
   return true;
}

ConsoleMethod(GameConnection, getCameraObject, S32, 2, 2, "")
{
   SimObject *obj = dynamic_cast<SimObject*>(object->getCameraObject());
   return obj ? obj->getId() : 0;
}

ConsoleMethod(GameConnection, clearCameraObject, void, 2, 2, "")
{
   object->setCameraObject(NULL);
}

ConsoleMethod(GameConnection, isFirstPerson, bool, 2, 2, "() True if this connection is in first person mode.")
{
   // Note: Transition to first person occurs over time via mCameraPos, so this
   // won't immediately return true after a set.
   return object->isFirstPerson();
}

ConsoleMethod(GameConnection, setFirstPerson, void, 3, 3, "(bool firstPerson) Sets this connection into or out of first person mode.")
{
   object->setFirstPerson(dAtob(argv[2]));
}

#ifdef AFX_CAP_DATABLOCK_CACHE // AFX CODE BLOCK (db-cache) <<

void GameConnection::tempDisableStringBuffering(BitStream* bs) const 
{ 
	//bs->setStringBuffer(0); 
}

void GameConnection::restoreStringBuffering(BitStream* bs) const 
{ 
	//bs->setStringBuffer(curr_stringBuf); 
}              

// rewind to stream postion and then move raw bytes into client_db_stream
// for caching purposes.
void GameConnection::repackClientDatablock(BitStream* bstream, S32 start_pos)
{
	static U8 bit_buffer[Net::MaxPacketDataSize];

	if (!clientCacheEnabled() || !client_db_stream)
		return;

	S32 cur_pos = bstream->getCurPos();
	S32 n_bits = cur_pos - start_pos;
	if (n_bits <= 0)
		return;

	bstream->setCurPos(start_pos);
	bstream->readBits(n_bits, bit_buffer);
	bstream->setCurPos(cur_pos);

	//S32 start_pos2 = client_db_stream->getCurPos();
	client_db_stream->writeBits(n_bits, bit_buffer);
}

#define CLIENT_CACHE_VERSION_CODE 4724110

void GameConnection::saveDatablockCache(bool on_server)
{
	InfiniteBitStream bit_stream;
	BitStream* bstream = 0;

	if (on_server)
	{
		SimDataBlockGroup *g = Sim::getDataBlockGroup();

		// find the first one we haven't sent:
		U32 i, groupCount = g->size();
		S32 key = this->getDataBlockModifiedKey();
		for (i = 0; i < groupCount; i++)
			if (((SimDataBlock*)(*g)[i])->getModifiedKey() > key)
				break;

		// nothing to save
		if (i == groupCount) 
			return;

		bstream = &bit_stream;

		for (;i < groupCount; i++) 
		{
			SimDataBlock* obj = (SimDataBlock*)(*g)[i];
			GameConnection* gc = this;
			NetConnection* conn = this;
			SimObjectId id = obj->getId();

			if (bstream->writeFlag(gc->getDataBlockModifiedKey() < obj->getModifiedKey()))        // A - flag
			{
				if (obj->getModifiedKey() > gc->getMaxDataBlockModifiedKey())
					gc->setMaxDataBlockModifiedKey(obj->getModifiedKey());

				bstream->writeInt(id - DataBlockObjectIdFirst,DataBlockObjectIdBitSize);            // B - int

				S32 classId = obj->getClassId(conn->getNetClassGroup());
				bstream->writeClassId(classId, NetClassTypeDataBlock, conn->getNetClassGroup());    // C - id
				bstream->writeInt(i, DataBlockObjectIdBitSize);                                     // D - int
				bstream->writeInt(groupCount, DataBlockObjectIdBitSize + 1);                        // E - int
				obj->packData(bstream);
			}
		}
	}
	else
	{
		bstream = client_db_stream;
	}

	if (bstream->getPosition() <= 0)
		return;

	// zero out any leftover bits short of an even byte count
	U32 n_leftover_bits = (bstream->getPosition()*8) - bstream->getCurPos();
	if (n_leftover_bits >= 0 && n_leftover_bits <= 8)
	{
		// note - an unusual problem regarding setCurPos() results when there 
		// are no leftover bytes. Adding a buffer byte in this case avoids the problem.
		if (n_leftover_bits == 0)
			n_leftover_bits = 8;
		U8 bzero = 0;
		bstream->writeBits(n_leftover_bits, &bzero);
	}

	// this is where we actually save the file
	const char* filename = (on_server) ? server_cache_filename : client_cache_filename;
	if (filename && filename[0] != '\0')
	{
		FileStream f_stream;
		if(!f_stream.open( filename, Torque::FS::File::Write ))
		//if(!ResourceManager::get().openFileForWrite(f_stream,))
		{
			Con::printf("Failed to open file '%s'.", filename);
			return;
		}

		U32 save_sz = bstream->getPosition();

		if (!on_server)
		{
			f_stream.write((U32)CLIENT_CACHE_VERSION_CODE);
			f_stream.write(save_sz);
			f_stream.write(server_cache_CRC);
			f_stream.write((U32)CLIENT_CACHE_VERSION_CODE);
		}

		f_stream.write(save_sz, bstream->getBuffer());

		// zero out any leftover bytes short of a 4-byte multiple
		while ((save_sz % 4) != 0)
		{
			f_stream.write((U8)0);
			save_sz++;
		}

		f_stream.close();
	}

	if (!on_server)
		client_db_stream->clear();
}

static bool afx_saved_db_cache = false;
static U32 afx_saved_db_cache_CRC = 0xffffffff;

void GameConnection::resetDatablockCache()
{
	afx_saved_db_cache = false;
	afx_saved_db_cache_CRC = 0xffffffff;
}

ConsoleFunction(resetDatablockCache, void, 1, 1, "resetDatablockCache()")
{
	GameConnection::resetDatablockCache();
}

ConsoleFunction(isDatablockCacheSaved, bool, 1, 1, "resetDatablockCache()")
{
	return afx_saved_db_cache;
}

ConsoleFunction(getDatablockCacheCRC, S32, 1, 1, "getDatablockCacheCRC()")
{
	return (S32)afx_saved_db_cache_CRC;
}

ConsoleFunction(extractDatablockCacheCRC, S32, 2, 2, "extractDatablockCacheCRC(filename)")
{
	FileStream f_stream;
	if(!f_stream.open(argv[1], Torque::FS::File::Read))
	{
		Con::errorf("Failed to open file '%s'.", argv[1]);
		return -1;
	}

	U32 stream_sz = f_stream.getStreamSize();
	if (stream_sz < 4*32)
	{
		Con::errorf("File '%s' is not a valid datablock cache.", argv[1]);
		f_stream.close();
		return -1;
	}

	U32 pre_code; f_stream.read(&pre_code);
	U32 save_sz; f_stream.read(&save_sz);
	U32 crc_code; f_stream.read(&crc_code);
	U32 post_code; f_stream.read(&post_code);

	f_stream.close();

	if (pre_code != post_code)
	{
		Con::errorf("File '%s' is not a valid datablock cache.", argv[1]);
		return -1;
	}

	if (pre_code != (U32)CLIENT_CACHE_VERSION_CODE)
	{
		Con::errorf("Version of datablock cache file '%s' does not match version of running software.", argv[1]);
		return -1;
	}

	return (S32)crc_code;
}

ConsoleFunction(setDatablockCacheCRC, void, 2, 2, "setDatablockCacheCRC(crc)")
{
	GameConnection *conn = GameConnection::getConnectionToServer();
	if(!conn)
		return;

	U32 crc_u = (U32)dAtoi(argv[1]);
	conn->setServerCacheCRC(crc_u);
}

ConsoleMethod( GameConnection, saveDatablockCache, void, 2, 2, "saveDatablockCache()")
{
	if (GameConnection::serverCacheEnabled() && !afx_saved_db_cache)
	{
		// Save the datablocks to a cache file. An argument
		// of true indicates that this is a server save.
		object->saveDatablockCache(true);
		afx_saved_db_cache = true;
		afx_saved_db_cache_CRC = 0xffffffff;

		const char* filename = object->serverCacheFilename();
		if (filename && filename[0] != '\0')
		{
			U32 crcVal;
			FileStream f_stream;
			if(f_stream.open(filename, Torque::FS::File::Read))
			{
				afx_saved_db_cache_CRC = CRC::calculateCRCStream(&f_stream,crcVal);
			}
			else
				Con::errorf("saveDatablockCache() failed to get CRC for file '%s'.", filename);\
		}
	}
}

ConsoleMethod( GameConnection, loadDatablockCache, void, 2, 2, "loadDatablockCache()")
{
	if (GameConnection::clientCacheEnabled())
	{
		object->loadDatablockCache();
	}
}

ConsoleMethod( GameConnection, loadDatablockCache_Begin, bool, 2, 2, "loadDatablockCache_Begin()")
{
	if (GameConnection::clientCacheEnabled())
	{
		return object->loadDatablockCache_Begin();
	}

	return false;
}

ConsoleMethod( GameConnection, loadDatablockCache_Continue, bool, 2, 2, "loadDatablockCache_Continue()")
{
	if (GameConnection::clientCacheEnabled())
	{
		return object->loadDatablockCache_Continue();
	}

	return false;
}

static char*        afx_db_load_buf = 0;
static U32          afx_db_load_buf_sz = 0;
static BitStream*   afx_db_load_bstream = 0;

void GameConnection::loadDatablockCache()
{
	if (!loadDatablockCache_Begin())
		return;

	while (loadDatablockCache_Continue())
		;
}

bool GameConnection::loadDatablockCache_Begin()
{
	if (!client_cache_filename || client_cache_filename[0] == '\0')
	{
		Con::errorf("No filename was specified for the client datablock cache.");
		return false;
	}

	// open cache file
	FileStream f_stream;
	if(!f_stream.open(client_cache_filename, Torque::FS::File::Read))
	{
		Con::errorf("Failed to open file '%s'.", client_cache_filename);
		return false;
	}

	// get file size
	U32 stream_sz = f_stream.getStreamSize();
	if (stream_sz <= 4*4)
	{
		Con::errorf("File '%s' is too small to be a valid datablock cache.", client_cache_filename);
		f_stream.close();
		return false;
	}

	// load header data
	U32 pre_code; f_stream.read(&pre_code);
	U32 save_sz; f_stream.read(&save_sz);
	U32 crc_code; f_stream.read(&crc_code);
	U32 post_code; f_stream.read(&post_code);

	// validate header info 
	if (pre_code != post_code)
	{
		Con::errorf("File '%s' is not a valid datablock cache.", client_cache_filename);
		f_stream.close();
		return false;
	}
	if (pre_code != (U32)CLIENT_CACHE_VERSION_CODE)
	{
		Con::errorf("Version of datablock cache file '%s' does not match version of running software.", client_cache_filename);
		f_stream.close();
		return false;
	}

	// allocated the in-memory buffer
	afx_db_load_buf_sz = stream_sz - (4*4);
	afx_db_load_buf = new char[afx_db_load_buf_sz];

	// load data from file into memory
	if (!f_stream.read(stream_sz, afx_db_load_buf))
	{
		Con::errorf("Failed to read data from file '%s'.", client_cache_filename);
		f_stream.close();
		delete [] afx_db_load_buf;
		afx_db_load_buf = 0;
		afx_db_load_buf_sz = 0;
		return false;
	}

	// close file
	f_stream.close();

	// At this point we have the whole cache in memory

	// create a bitstream from the in-memory buffer
	afx_db_load_bstream = new BitStream(afx_db_load_buf, afx_db_load_buf_sz);

	return true;
}

bool GameConnection::loadDatablockCache_Continue()
{
	if (!afx_db_load_bstream)
		return false;

	// prevent repacking of datablocks during load
	BitStream* save_client_db_stream = client_db_stream;
	client_db_stream = 0;

	bool all_finished = false;

	// loop through at most 16 datablocks
	BitStream *bstream = afx_db_load_bstream;
	for (S32 i = 0; i < 16; i++)
	{
		S32 save_pos = bstream->getCurPos();
		if (!bstream->readFlag())
		{
			all_finished = true;
			break;
		}
		bstream->setCurPos(save_pos);
		SimDataBlockEvent evt;
		evt.unpack(this, bstream);
		evt.process(this);
	}

	client_db_stream = save_client_db_stream;

	if (all_finished)
	{
		delete afx_db_load_bstream;
		afx_db_load_bstream = 0;
		delete [] afx_db_load_buf;
		afx_db_load_buf = 0;
		afx_db_load_buf_sz = 0;
		return false;
	}

	return true;
}

#endif // AFX CODE BLOCK (db-cache) >>

ConsoleMethod(GameConnection, setSelectedObj, bool, 3, 4, "(object, [propagate_to_client])")
{
	SceneObject* pending_selection;
	if (!Sim::findObject(argv[2], pending_selection))
		return false;

	bool propagate_to_client = (argc > 3) ? dAtob(argv[3]) : false;
	object->setSelectedObj(pending_selection, propagate_to_client);

	return true;
}

ConsoleMethod(GameConnection, getSelectedObj, S32, 2, 2, "()")
{
	SimObject* selected = object->getSelectedObj();
	return (selected) ? selected->getId(): -1;
}

ConsoleMethod(GameConnection, clearSelectedObj, void, 2, 3, "([propagate_to_client])")
{
	bool propagate_to_client = (argc > 2) ? dAtob(argv[2]) : false;
	object->setSelectedObj(NULL, propagate_to_client);
}

ConsoleMethod(GameConnection, setPreSelectedObjFromRollover, void, 2, 2, "()")
{
	object->setPreSelectedObjFromRollover();
}

ConsoleMethod(GameConnection, clearPreSelectedObj, void, 2, 2, "()")
{
	object->clearPreSelectedObj();
}

ConsoleMethod(GameConnection, setSelectedObjFromPreSelected, void, 2, 2, "()")
{
	object->setSelectedObjFromPreSelected();
}


void GameConnection::setSelectedObj(SceneObject* so, bool propagate_to_client) 
{ 
	if (!isConnectionToServer())
	{
		return;
	}

	// clear previously selected object
	if (mSelectedObj)
	{
		mSelectedObj->setSelectionFlags(mSelectedObj->getSelectionFlags() & ~SceneObject::SELECTED);
		clearNotify(mSelectedObj);
		Con::executef(this, "onObjectDeselected", mSelectedObj->scriptThis());
	}

	// save new selection
	mSelectedObj = so; 

	// mark selected object
	if (mSelectedObj)
	{
		mSelectedObj->setSelectionFlags(mSelectedObj->getSelectionFlags() | SceneObject::SELECTED);
		deleteNotify(mSelectedObj);
	}

	// mark selection dirty
	//mChangedSelectedObj = true; 

	// notify appropriate script of the change
	if (mSelectedObj)
		Con::executef(this, "onObjectSelected", mSelectedObj->scriptThis());
}

void GameConnection::setRolloverObj(SceneObject* so) 
{ 
	// save new selection
	mRolloverObj = so;  

	// notify appropriate script of the change
	Con::executef(this, "onObjectRollover", (mRolloverObj) ? mRolloverObj->scriptThis() : "");
}

void GameConnection::setPreSelectedObjFromRollover()
{
	mPreSelectedObj = mRolloverObj;
	mPreSelectTimestamp = Platform::getRealMilliseconds();
}

void GameConnection::clearPreSelectedObj()
{
	mPreSelectedObj = 0;
	mPreSelectTimestamp = 0;
}

void GameConnection::setSelectedObjFromPreSelected()
{
	U32 now = Platform::getRealMilliseconds();
	if (now - mPreSelectTimestamp < 1000)
		setSelectedObj(mPreSelectedObj);
	mPreSelectedObj = 0;
}

void GameConnection::onDeleteNotify(SimObject* obj)
{
	if (obj == mSelectedObj)
		setSelectedObj(NULL);

	Parent::onDeleteNotify(obj);
}