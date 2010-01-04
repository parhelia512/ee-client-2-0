//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/frameAllocator.h"
#include "console/console.h"

U8*   FrameAllocator::smBuffer = NULL;
U32   FrameAllocator::smWaterMark = 0;
U32   FrameAllocator::smHighWaterMark = 0;

#ifdef TORQUE_DEBUG
U32   FrameAllocator::smMaxFrameAllocation = 0;

ConsoleFunction(getMaxFrameAllocation, S32, 1,1, "getMaxFrameAllocation();")
{
   return FrameAllocator::getMaxFrameAllocation();
}
#endif
