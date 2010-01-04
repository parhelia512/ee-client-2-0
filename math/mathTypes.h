//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MATHTYPES_H_
#define _MATHTYPES_H_

#ifndef _DYNAMIC_CONSOLETYPES_H_
#include "console/dynamicTypes.h"
#endif

void RegisterMathFunctions(void);

class Point2I;
class Point2F;
class Point3F;
class Point4F;
class RectI;
class RectF;
class MatrixF;
class Box3F;

DefineConsoleType( TypePoint2I, Point2I )
DefineConsoleType( TypePoint2F, Point2F )
DefineConsoleType( TypePoint3F, Point3F )
DefineConsoleType( TypePoint4F, Point4F )
DefineConsoleType( TypeRectI, RectI )
DefineConsoleType( TypeRectF, RectF )
DefineConsoleType( TypeMatrixPosition, MatrixF)
DefineConsoleType( TypeMatrixRotation, MatrixF )
DefineConsoleType( TypeBox3F, Box3F )


#endif
