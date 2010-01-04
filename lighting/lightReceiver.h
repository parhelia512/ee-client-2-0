//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _LIGHTRECEIVER_H_
#define _LIGHTRECEIVER_H_

#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _TORQUE_STRING_H_
#include "core/util/str.h"
#endif

struct LightReceiver
{
   LightReceiver();
   virtual ~LightReceiver();

   bool overrideOptions;
   bool receiveLMLighting;
   bool receiveSunLight;
   bool useAdaptiveSelfIllumination;
   bool useCustomAmbientLighting;
   bool customAmbientForSelfIllumination;
   ColorF customAmbientLighting;
   String lightGroupName;

protected:
};

#endif // _LIGHTRECEIVER_H_
