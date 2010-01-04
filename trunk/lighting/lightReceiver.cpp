//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/lightReceiver.h"

LightReceiver::LightReceiver()
{
   overrideOptions = true;
   receiveLMLighting = true;
   receiveSunLight = true;
   useAdaptiveSelfIllumination = false;
   useCustomAmbientLighting = false;
   customAmbientForSelfIllumination = false;
   customAmbientLighting = ColorF(0.0f, 0.0f, 0.0f); 
}

LightReceiver::~LightReceiver() 
{
}
