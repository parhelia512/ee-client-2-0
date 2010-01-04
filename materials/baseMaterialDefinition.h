//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _BASEMATERIALDEFINITION_H_
#define _BASEMATERIALDEFINITION_H_

#ifndef _SIMOBJECT_H_
#include "console/simObject.h"
#endif

class BaseMatInstance;

class BaseMaterialDefinition : public SimObject
{
   typedef SimObject Parent;
public:
   virtual BaseMatInstance* createMatInstance() = 0;
   virtual bool isIFL() const = 0;
   virtual bool isTranslucent() const = 0;
   virtual bool isDoubleSided() const = 0;
   virtual bool isLightmapped() const = 0;
   virtual bool castsShadows() const = 0;
};

#endif
