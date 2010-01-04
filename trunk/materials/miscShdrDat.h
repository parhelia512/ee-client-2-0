//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _MISCSHDRDAT_H_
#define _MISCSHDRDAT_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif

//**************************************************************************
// This file is an attempt to keep certain classes from having to know about
//   the ShaderGen class
//**************************************************************************


//-----------------------------------------------------------------------
// Enums
//-----------------------------------------------------------------------
enum RegisterType
{
   RT_POSITION = 0,
   RT_NORMAL,
   RT_BINORMAL,
   RT_TANGENT,
   RT_COLOR,
   RT_TEXCOORD,
};

enum Components
{
   C_VERT_STRUCT = 0,
   C_CONNECTOR,
   C_VERT_MAIN,
   C_PIX_MAIN,
};

#endif // _MISCSHDRDAT_H_
