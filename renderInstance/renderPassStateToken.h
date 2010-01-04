//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "renderInstance/renderBinManager.h"

class RenderPassStateBin;

class RenderPassStateToken : public SimObject
{
   typedef SimObject Parent;

public:
   DECLARE_CONOBJECT(RenderPassStateToken);

   static void initPersistFields();

   // These must be re-implemented, and will assert if called on the base class.
   // They just can't be pure-virtual, due to SimObject.
   virtual void process(SceneState *state, RenderPassStateBin *callingBin);
   virtual void reset();
   virtual void enable(bool enabled = true);
   virtual bool isEnabled() const;
};

//------------------------------------------------------------------------------

class RenderPassStateBin : public RenderBinManager
{
   typedef RenderBinManager Parent;

protected:
   SimObjectPtr<RenderPassStateToken> mStateToken;
   
public:
   DECLARE_CONOBJECT(RenderPassStateBin);
   static void initPersistFields();

   RenderPassStateBin();
   virtual ~RenderPassStateBin();

   AddInstResult addElement( RenderInst *inst );
   void render(SceneState *state);
   void clear();
   void sort();
};
