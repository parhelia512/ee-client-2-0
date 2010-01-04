//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GAMECONNECTION_H_
#define _GAMECONNECTION_H_

#ifndef _SIMBASE_H_
#include "console/simBase.h"
#endif
#ifndef _GAMEBASE_H_
#include "T3D/gameBase.h"
#endif
#ifndef _NETCONNECTION_H_
#include "sim/netConnection.h"
#endif
#ifndef _MOVEMANAGER_H_
#include "T3D/moveManager.h"
#endif
#ifndef _BITVECTOR_H_
#include "core/bitVector.h"
#endif
#ifndef _MOVELIST_H_
#include "T3D/moveList.h"
#endif

enum GameConnectionConstants
{
   MaxClients = 126,
   DataBlockQueueCount = 16
};

class SFXProfile;
class MatrixF;
class MatrixF;
class Point3F;
class MoveManager;
struct Move;
struct AuthInfo;

#define GameString TORQUE_APP_NAME
// AFX CODE BLOCK (db-cache) <<
//
// To disable datablock caching, remove or comment out the AFX_CAP_DATABLOCK_CACHE define below.
// Also, at a minimum, the following script preferences should be set to false:
//   $pref::Client::EnableDatablockCache = false; (in arcane.fx/client/defaults.cs)
//   $Pref::Server::EnableDatablockCache = false; (in arcane.fx/server/defaults.cs)
// Alternatively, all script code marked with "DATABLOCK CACHE CODE" can be removed or
// commented out.
//
#define AFX_CAP_DATABLOCK_CACHE
// AFX CODE BLOCK (db-cache) >>
const F32 MinCameraFov              = 1.f;      ///< min camera FOV
const F32 MaxCameraFov              = 179.f;    ///< max camera FOV

class GameConnection : public NetConnection
{
private:
   typedef NetConnection Parent;

   SimObjectPtr<GameBase> mControlObject;
   SimObjectPtr<GameBase> mCameraObject;
   U32 mDataBlockSequence;
   char mDisconnectReason[256];

   U32  mMissionCRC;             // crc of the current mission file from the server

private:
   U32 mLastControlRequestTime;
   S32 mDataBlockModifiedKey;
   S32 mMaxDataBlockModifiedKey;

   /// @name Client side first/third person
   /// @{

   ///
   bool  mFirstPerson;     ///< Are we currently first person or not.
   bool  mUpdateFirstPerson; ///< Set to notify client or server of first person change.
   bool  mUpdateCameraFov; ///< Set to notify server of camera FOV change.
   F32   mCameraFov;       ///< Current camera fov (in degrees).
   F32   mCameraPos;       ///< Current camera pos (0-1).
   F32   mCameraSpeed;     ///< Camera in/out speed.
   /// @}

public:

   /// @name Protocol Versions
   ///
   /// Protocol versions are used to indicated changes in network traffic.
   /// These could be changes in how any object transmits or processes
   /// network information. You can specify backwards compatibility by
   /// specifying a MinRequireProtocolVersion.  If the client
   /// protocol is >= this min value, the connection is accepted.
   ///
   /// Torque (V12) SDK 1.0 uses protocol  =  1
   ///
   /// Torque SDK 1.1 uses protocol = 2
   /// Torque SDK 1.4 uses protocol = 12
   /// @{
   static const U32 CurrentProtocolVersion;
   static const U32 MinRequiredProtocolVersion;
   /// @}

   /// Configuration
   enum Constants {
      BlockTypeMove = NetConnectionBlockTypeCount,
      GameConnectionBlockTypeCount,
      MaxConnectArgs = 16,
      DataBlocksDone = NumConnectionMessages,
      DataBlocksDownloadDone,
   };

   /// Set connection arguments; these are passed to the server when we connect.
   void setConnectArgs(U32 argc, const char **argv);

   /// Set the server password to use when we join.
   void setJoinPassword(const char *password);

   /// @name Event Handling
   /// @{

   virtual void onTimedOut();
   virtual void onConnectTimedOut();
   virtual void onDisconnect(const char *reason);
   virtual void onConnectionRejected(const char *reason);
   virtual void onConnectionEstablished(bool isInitiator);
   virtual void handleStartupError(const char *errorString);
   /// @}

   /// @name Packet I/O
   /// @{

   virtual void writeConnectRequest(BitStream *stream);
   virtual bool readConnectRequest(BitStream *stream, const char **errorString);
   virtual void writeConnectAccept(BitStream *stream);
   virtual bool readConnectAccept(BitStream *stream, const char **errorString);
   /// @}

   bool canRemoteCreate();

private:
   /// @name Connection State
   /// This data is set with setConnectArgs() and setJoinPassword(), and
   /// sent across the wire when we connect.
   /// @{

   U32      mConnectArgc;
   char *mConnectArgv[MaxConnectArgs];
   char *mJoinPassword;
   /// @}

protected:
   struct GamePacketNotify : public NetConnection::PacketNotify
   {
      S32 cameraFov;
      GamePacketNotify();
   };
   PacketNotify *allocNotify();

   bool mControlForceMismatch;

   Vector<SimDataBlock *> mDataBlockLoadList;

public:
   MoveList    mMoveList;
protected:
   bool        mAIControlled;
   AuthInfo *  mAuthInfo;

   static S32  mLagThresholdMS;
   S32         mLastPacketTime;
   bool        mLagging;

   /// @name Flashing
   ////
   /// Note, these variables are not networked, they are for the local connection only.
   /// @{
   F32 mDamageFlash;
   F32 mWhiteOut;

   F32   mBlackOut;
   S32   mBlackOutTimeMS;
   S32   mBlackOutStartTimeMS;
   bool  mFadeToBlack;

   /// @}

   /// @name Packet I/O
   /// @{

   void readPacket      (BitStream *bstream);
   void writePacket     (BitStream *bstream, PacketNotify *note);
   void packetReceived  (PacketNotify *note);
   void packetDropped   (PacketNotify *note);
   void connectionError (const char *errorString);

   void writeDemoStartBlock   (ResizeBitStream *stream);
   bool readDemoStartBlock    (BitStream *stream);
   void handleRecordedBlock   (U32 type, U32 size, void *data);
   /// @}
   void ghostWriteExtra(NetObject *,BitStream *);
   void ghostReadExtra(NetObject *,BitStream *, bool newGhost);
   void ghostPreRead(NetObject *, bool newGhost);
   
public:

   DECLARE_CONOBJECT(GameConnection);
   void handleConnectionMessage(U32 message, U32 sequence, U32 ghostCount);
   void preloadDataBlock(SimDataBlock *block);
   void fileDownloadSegmentComplete();
   void preloadNextDataBlock(bool hadNew);
   static void consoleInit();

   void setDisconnectReason(const char *reason);
   GameConnection();
   ~GameConnection();

   U32 getDataBlockSequence() { return mDataBlockSequence; }
   void setDataBlockSequence(U32 seq) { mDataBlockSequence = seq; }

   bool onAdd();
   void onRemove();

   static GameConnection *getConnectionToServer() 
   { 
      return dynamic_cast<GameConnection*>((NetConnection *) mServerConnection); 
   }
   
   static GameConnection *getLocalClientConnection() 
   { 
      return dynamic_cast<GameConnection*>((NetConnection *) mLocalClientConnection); 
   }

   /// @name Control object
   /// @{

   ///
   void setControlObject(GameBase *);
   GameBase* getControlObject() {  return  mControlObject; }
   
   void setCameraObject(GameBase *);
   GameBase* getCameraObject();
   
   bool getControlCameraTransform(F32 dt,MatrixF* mat);
   bool getControlCameraVelocity(Point3F *vel);

   bool getControlCameraFov(F32 *fov);
   bool setControlCameraFov(F32 fov);
   bool isValidControlCameraFov(F32 fov);
   
   // Used by editor
   bool isControlObjectRotDampedCamera();

   void setFirstPerson(bool firstPerson);
   
   /// @}

   void detectLag();

   /// @name Datablock management
   /// @{

   S32  getDataBlockModifiedKey     ()  { return mDataBlockModifiedKey; }
   void setDataBlockModifiedKey     (S32 key)  { mDataBlockModifiedKey = key; }
   S32  getMaxDataBlockModifiedKey  ()  { return mMaxDataBlockModifiedKey; }
   void setMaxDataBlockModifiedKey  (S32 key)  { mMaxDataBlockModifiedKey = key; }
   /// @}

   /// @name Fade control
   /// @{

   F32 getDamageFlash() { return mDamageFlash; }
   F32 getWhiteOut() { return mWhiteOut; }

   void setBlackOut(bool fadeToBlack, S32 timeMS);
   F32  getBlackOut();
   /// @}

   /// @name Authentication
   ///
   /// This is remnant code from Tribes 2.
   /// @{

   void            setAuthInfo(const AuthInfo *info);
   const AuthInfo *getAuthInfo();
   /// @}

   /// @name Sound
   /// @{

   void play2D(SFXProfile *profile);
   void play3D(SFXProfile *profile, const MatrixF *transform);
   /// @}

   /// @name Misc.
   /// @{

   bool isFirstPerson()  { return mCameraPos == 0; }
   bool isAIControlled() { return mAIControlled; }

   void doneScopingScene();
   void demoPlaybackComplete();

   void setMissionCRC(U32 crc)           { mMissionCRC = crc; }
   U32  getMissionCRC()           { return(mMissionCRC); }
   /// @}

   static Signal<void(F32)> smFovUpdate;
   static Signal<void()> smPlayingDemo;
#ifdef AFX_CAP_DATABLOCK_CACHE // AFX CODE BLOCK (db-cache) <<
  private:
	  static StringTableEntry  server_cache_filename;
	  static StringTableEntry  client_cache_filename;
	  static bool   server_cache_on;
	  static bool   client_cache_on;
	  BitStream*    client_db_stream;
	  //char*         curr_stringBuf;
	  U32           server_cache_CRC;
  public:
	  void          repackClientDatablock(BitStream*, S32 start_pos);
	  void          saveDatablockCache(bool on_server);
	  void          loadDatablockCache();
	  bool          loadDatablockCache_Begin();
	  bool          loadDatablockCache_Continue();
	  void          tempDisableStringBuffering(BitStream* bs) const;
	  void          restoreStringBuffering(BitStream* bs) const;
	  void          setServerCacheCRC(U32 crc) { server_cache_CRC = crc; }

	  static void   resetDatablockCache();
	  static bool   serverCacheEnabled() { return server_cache_on; }
	  static bool   clientCacheEnabled() { return client_cache_on; }
	  static const char* serverCacheFilename() { return server_cache_filename; }
	  static const char* clientCacheFilename() { return client_cache_filename; }
#endif // AFX CODE BLOCK (db-cache) >>
		private:   
			SimObjectPtr<SceneObject> mRolloverObj;  
			SimObjectPtr<SceneObject> mPreSelectedObj;  
			SimObjectPtr<SceneObject> mSelectedObj;  
			bool          mChangedSelectedObj;
			U32           mPreSelectTimestamp;
  protected:
	  virtual void  onDeleteNotify(SimObject*);
  public:   
	  void          setRolloverObj(SceneObject*);   
	  SceneObject*  getRolloverObj() { return  mRolloverObj; }   
	  void          setSelectedObj(SceneObject*, bool propagate_to_client=false);   
	  SceneObject*  getSelectedObj() { return  mSelectedObj; }  
	  void          setPreSelectedObjFromRollover();
	  void          clearPreSelectedObj();
	  void          setSelectedObjFromPreSelected();
};

#endif
