//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gfx/gfxOcclusionQuery.h"

String GFXOcclusionQuery::statusToString( OcclusionQueryStatus status )
{
   String outStr;
   if ( status == GFXOcclusionQuery::Unset )
      outStr = "Unset";
   else if ( status == GFXOcclusionQuery::Waiting )
      outStr = "Waiting";
   else if ( status == GFXOcclusionQuery::Occluded )
      outStr = "Occluded";
   else if ( status == GFXOcclusionQuery::NotOccluded )
      outStr = "Visible";
   else if ( status == GFXOcclusionQuery::Error )
      outStr = "Error";
   else
      outStr = "Unknown";

   return outStr;
}
