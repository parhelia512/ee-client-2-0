//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

//
// This PhysX implementation for Torque was originally based on
// the "PhysX in TGEA" resource written by Shannon Scarvaci.
//
// http://www.garagegames.com/index.php?sec=mg&mod=resource&page=view&qid=12711
//

#ifndef _PHYSX_H_
#define _PHYSX_H_

/*
#ifndef _TORQUE_TYPES_H_
#	include "platform/types.h"
#endif
*/

#if defined(TORQUE_OS_MAC) && !defined(__APPLE__)
   #define __APPLE__
#elif defined(TORQUE_OS_LINUX) && !defined(LINUX)
   #define LINUX
#elif defined(TORQUE_OS_WIN32) && !defined(WIN32)
   #define WIN32
#endif

#ifndef NX_PHYSICS_NXPHYSICS
#include <NxPhysics.h>
#endif
#ifndef NX_FOUNDATION_NXSTREAM
#include <NxStream.h>
#endif
#ifndef NX_COOKING_H
#include <NxCooking.h>
#endif
#ifndef NX_FOUNDATION_NXUSEROUTPUTSTREAM
#include <NxUserOutputStream.h>
#endif

#pragma comment(lib, "PhysXLoader.lib")
#pragma comment(lib, "NxCooking.lib")

/// The single global physx sdk object for this process.
extern NxPhysicsSDK *gPhysicsSDK;

enum PxGroups
{
   PxGroup_Default,
   PxGroup_ClientOnly,
};

#endif // _PHYSX_H_