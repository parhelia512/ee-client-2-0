//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/materialFeatureData.h"


MaterialFeatureData::MaterialFeatureData()
{
}

MaterialFeatureData::MaterialFeatureData( const MaterialFeatureData &data )
   :  features( data.features ),
      materialFeatures( data.materialFeatures )
{
}

MaterialFeatureData::MaterialFeatureData( const FeatureSet &handle )
   :  features( handle )
{
}

void MaterialFeatureData::clear()
{
   features.clear();
   materialFeatures.clear();
}
