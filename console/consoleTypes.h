//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _CONSOLETYPES_H_
#define _CONSOLETYPES_H_

#ifndef _DYNAMIC_CONSOLETYPES_H_
#include "console/dynamicTypes.h"
#endif
#ifndef _MATHTYPES_H_
#include "math/mathTypes.h"
#endif

#ifndef Offset
#if defined(TORQUE_COMPILER_GCC) && (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
#define Offset(m,T) ((int)(&((T *)1)->m) - 1)
#else
#define Offset(x, cls) ((dsize_t)((const char *)&(((cls *)0)->x)-(const char *)0))
#endif
#endif

class ColorI;
class ColorF;
class GFXShader;
class GFXCubemap;
class CustomMaterial;
class ProjectileData;
class ParticleEmitterData;

// Define Core Console Types
DefineConsoleType( TypeBool, bool )
DefineConsoleType( TypeBoolVector, Vector<bool>)
DefineConsoleType( TypeS8,  S8 )
DefineConsoleType( TypeS32, S32 )
DefineConsoleType( TypeBitMask32, S32 )
DefineConsoleType( TypeS32Vector, Vector<S32> )
DefineConsoleType( TypeF32, F32 )
DefineConsoleType( TypeF32Vector, Vector<F32> )
DefineConsoleType( TypeString, const char * )
DefineConsoleType( TypeCaseString, const char * )
DefineConsoleType( TypeRealString, String )
DefineConsoleType( TypeCommand, String )
DefineConsoleType( TypeFilename, const char * )
DefineConsoleType( TypeStringFilename, String )

/// TypeImageFilename is equivalent to TypeStringFilename in its usage,
/// it exists for the benefit of GuiInspector, which will provide a custom
/// InspectorField for this type that can display a texture preview.
DefineConsoleType( TypeImageFilename, String )

/// TypeMaterialName is equivalent to TypeRealString in its usage,
/// it exists for the benefit of GuiInspector, which will provide a 
/// custom InspectorField for this type.
DefineConsoleType( TypeMaterialName, String )

/// TypeCubemapName is equivalent to TypeRealString in its usage,
/// but the Inspector will provide a drop-down list of CubemapData objects.
DefineConsoleType( TypeCubemapName, String )

DefineConsoleType( TypeEnum, S32 )
DefineConsoleType( TypeModifiedEnum, S32 )
DefineConsoleType( TypeFlag, S32 )
DefineConsoleType( TypeColorI, ColorI )
DefineConsoleType( TypeColorF, ColorF )
DefineConsoleType( TypeSimObjectPtr, SimObject* )
DefineConsoleType( TypeSimObjectName, SimObject* )
DefineConsoleType( TypeShader, GFXShader * )
DefineConsoleType( TypeCustomMaterial, CustomMaterial * )
DefineConsoleType( TypeProjectileDataPtr, ProjectileData* )
DefineConsoleType( TypeParticleEmitterDataPtr, ParticleEmitterData* );

/// Special field type for SimObject::objectName
DefineConsoleType( TypeName, const char* )

#endif
