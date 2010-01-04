//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/staticShape.h"

class ScopeAlwaysShape : public StaticShape
{
      typedef StaticShape Parent;

   public:
      ScopeAlwaysShape();
      static void initPersistFields();
      DECLARE_CONOBJECT(ScopeAlwaysShape);
};

ScopeAlwaysShape::ScopeAlwaysShape()
{
   mNetFlags.set(Ghostable|ScopeAlways);
   mTypeMask |= ShadowCasterObjectType;
}

void ScopeAlwaysShape::initPersistFields()
{
   Parent::initPersistFields();
}

IMPLEMENT_CO_NETOBJECT_V1(ScopeAlwaysShape);
