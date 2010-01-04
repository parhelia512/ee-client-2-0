//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _APP_GAMEPROCESS_H_
#define _APP_GAMEPROCESS_H_

#include "platform/platform.h"
#include "sim/processList.h"

class GameBase;
class GameConnection;
struct Move;

//----------------------------------------------------------------------------

/// List to keep track of GameBases to process.
class ClientProcessList : public ProcessList
{
   typedef ProcessList Parent;

protected:
   
   void onTickObject(ProcessObject *);
   void advanceObjects();
   void onAdvanceObjects();
   bool doBacklogged(SimTime timeDelta);
   GameBase * getGameBase(ProcessObject * obj);

   
public:
   ClientProcessList();
   
   void addObject(ProcessObject * obj);

   bool advanceTime(SimTime timeDelta);

   /// @}
   // after update from server, catch back up to where we were
   void clientCatchup(GameConnection*);
};

class ServerProcessList : public ProcessList
{
   typedef ProcessList Parent;

protected:

   void onTickObject(ProcessObject *);
   void advanceObjects();
   GameBase * getGameBase(ProcessObject * obj);

public:
   ServerProcessList();
   
   void addObject(ProcessObject * obj);
};

extern ClientProcessList gClientProcessList;
extern ServerProcessList gServerProcessList;

#endif