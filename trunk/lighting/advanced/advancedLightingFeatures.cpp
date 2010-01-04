//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "lighting/advanced/advancedLightingFeatures.h"

#include "shaderGen/featureMgr.h"
#include "gfx/gfxStringEnumTranslate.h"
#include "materials/materialParameters.h"
#include "materials/materialFeatureTypes.h"
#include "gfx/gfxDevice.h"
#include "core/util/safeDelete.h"

#ifndef TORQUE_OS_MAC
#  include "lighting/advanced/hlsl/gBufferConditionerHLSL.h"
#  include "lighting/advanced/hlsl/advancedLightingFeaturesHLSL.h"
#else
#  include "lighting/advanced/glsl/gBufferConditionerGLSL.h"
#  include "lighting/advanced/glsl/advancedLightingFeaturesGLSL.h"
#endif



bool AdvancedLightingFeatures::smFeaturesRegistered = false;

void AdvancedLightingFeatures::registerFeatures( const GFXFormat &prepassTargetFormat, const GFXFormat &lightInfoTargetFormat )
{
   AssertFatal( !smFeaturesRegistered, "AdvancedLightingFeatures::registerFeatures() - Features already registered. Bad!" );

   // If we ever need this...
   TORQUE_UNUSED(lightInfoTargetFormat);

   if(GFX->getAdapterType() == OpenGL)
   {
#ifdef TORQUE_OS_MAC
      FEATUREMGR->registerFeature(MFT_PrePassConditioner, new GBufferConditionerGLSL( prepassTargetFormat ));
      FEATUREMGR->registerFeature(MFT_RTLighting, new DeferredRTLightingFeatGLSL());
      FEATUREMGR->registerFeature(MFT_NormalMap, new DeferredBumpFeatGLSL());
      FEATUREMGR->registerFeature(MFT_PixSpecular, new DeferredPixelSpecularGLSL());
      FEATUREMGR->registerFeature(MFT_MinnaertShading, new DeferredMinnaertGLSL());
      FEATUREMGR->registerFeature(MFT_SubSurface, new DeferredSubSurfaceGLSL());
#endif
   }
   else
   {
#ifndef TORQUE_OS_MAC
      FEATUREMGR->registerFeature(MFT_PrePassConditioner, 
         new GBufferConditionerHLSL( prepassTargetFormat, GBufferConditionerHLSL::ViewSpace ));
      FEATUREMGR->registerFeature(MFT_RTLighting, new DeferredRTLightingFeatHLSL());
      FEATUREMGR->registerFeature(MFT_NormalMap, new DeferredBumpFeatHLSL());
      FEATUREMGR->registerFeature(MFT_PixSpecular, new DeferredPixelSpecularHLSL());
      FEATUREMGR->registerFeature(MFT_MinnaertShading, new DeferredMinnaertHLSL());
      FEATUREMGR->registerFeature(MFT_SubSurface, new DeferredSubSurfaceHLSL());
#endif
   }

   smFeaturesRegistered = true;
}

void AdvancedLightingFeatures::unregisterFeatures()
{
   FEATUREMGR->unregisterFeature(MFT_PrePassConditioner);
   FEATUREMGR->unregisterFeature(MFT_RTLighting);
   FEATUREMGR->unregisterFeature(MFT_NormalMap);
   FEATUREMGR->unregisterFeature(MFT_PixSpecular);
   FEATUREMGR->unregisterFeature(MFT_MinnaertShading);
   FEATUREMGR->unregisterFeature(MFT_SubSurface);

   smFeaturesRegistered = false;
}
