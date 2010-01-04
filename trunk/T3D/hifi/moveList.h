//-----------------------------------------------------------------------------
// Torque Game Engine Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MOVELIST_H_
#define _MOVELIST_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _MOVEMANAGER_H_
#include "T3D/moveManager.h"
#endif
#ifndef _TVECTOR_H_
#include "core/tVector.h"
#endif

class BitStream;
class ResizeBitStream;
class NetObject;
class GameConnection;

class MoveList
{
   enum PrivateConstants 
   {
      MoveCountBits = 5,
      /// MaxMoveCount should not exceed the MoveManager's
      /// own maximum (MaxMoveQueueSize)
      MaxMoveCount = 30,
   };

public:

   MoveList();

   void init() { mTotalServerTicks = ServerTicksUninitialized; }

   void setConnection(GameConnection * connection) { mConnection = connection; }

   /// @name Move Packets
   /// Write/read move data to the packet.
   /// @{

   ///
   void ghostReadExtra(NetObject *,BitStream *, bool newGhost);
   void ghostWriteExtra(NetObject *,BitStream *) {}
   void ghostPreRead(NetObject *, bool newGhost);

   void clientWriteMovePacket(BitStream *bstream);
   void clientReadMovePacket(BitStream *);
   void serverWriteMovePacket(BitStream *);
   void serverReadMovePacket(BitStream *bstream);
   /// @}

   void writeDemoStartBlock(ResizeBitStream *stream);
   void readDemoStartBlock(BitStream *stream);

public: 
   // but not members of base class
   void           resetClientMoves() { mLastClientMove = mFirstMoveIndex; }
   void           resetCatchup() { mLastClientMove = mLastMoveAck; }

public:
   void           collectMove();
   void           pushMove(const Move &mv);

   virtual        U32 getMoveList(Move**,U32* numMoves);
   virtual bool   areMovesPending();
   virtual void   clearMoves(U32 count);

   void markControlDirty();
   bool isMismatch() { return mControlMismatch; }
   bool           isBacklogged();

   void           onAdvanceObjects() { if (mMoveList.size() > mLastSentMove-mFirstMoveIndex) mLastSentMove++; }

protected:
   bool           getNextMove(Move &curMove);
   void           resetMoveList();

protected:

   S32            getServerTicks(U32 serverTickNum);
   void           updateClientServerTickDiff(S32 & tickDiff);

   U32 mLastMoveAck;
   U32 mLastClientMove;
   U32 mFirstMoveIndex;
   U32 mLastSentMove;
   bool mControlMismatch;
   F32 mAvgMoveQueueSize;

   // server side move list management
   U32 mTargetMoveListSize;       // Target size of move buffer on server
   U32 mMaxMoveListSize;          // Max size move buffer allowed to grow to
   F32 mSmoothMoveAvg;            // Smoothing parameter for move list size running average
   F32 mMoveListSizeSlack;         // Amount above/below target size move list running average allowed to diverge

   // client side tracking of server ticks
   enum { TotalTicksBits=10, TotalTicksMask = (1<<TotalTicksBits)-1, ServerTicksUninitialized=0xFFFFFFFF };
   U32 mTotalServerTicks;

   GameConnection * mConnection;

   bool serverTicksInitialized() { return mTotalServerTicks!=ServerTicksUninitialized; }

   Vector<Move>     mMoveList;
};

#endif // _MOVELIST_H_
