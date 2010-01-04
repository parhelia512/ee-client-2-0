//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef BYTESWAP
#define BYTESWAP(x, y) x = x ^ y; y = x ^ y; x = x ^y;
#endif //defined(BYTESWAP)