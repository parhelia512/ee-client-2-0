//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _ADVANCEDLIGHTINGFEATURES_H_
#define _ADVANCEDLIGHTINGFEATURES_H_

#ifndef _GFXENUMS_H_
#include "gfx/gfxEnums.h"
#endif


class AdvancedLightingFeatures
{
public:

   static void registerFeatures( const GFXFormat &prepassTargetFormat, const GFXFormat &lightInfoTargetFormat );
   static void unregisterFeatures();

private:

   static bool smFeaturesRegistered;
};

#endif // _ADVANCEDLIGHTINGFEATURES_H_
