//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GFXVERTEXTYPES_H_
#define _GFXVERTEXTYPES_H_

#ifndef _GFXVERTEXFORMAT_H_
#include "gfx/gfxVertexFormat.h"
#endif
#ifndef _GFXVERTEXCOLOR_H_
#include "gfx/gfxVertexColor.h"
#endif
#ifndef _MPOINT2_H_
#include "math/mPoint2.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif

// Disable warning 'structure was padded due to __declspec(align())'
// It's worth noting, though, that GPUs are heavily cache dependent and using
// 4-byte aligned vertex sizes is best.
#pragma warning(disable: 4324)

GFXDeclareVertexFormat( GFXVertexP )
{
   Point3F point;
};

GFXDeclareVertexFormat( GFXVertexPT )
{
   Point3F point;
   Point2F texCoord;
};

GFXDeclareVertexFormat( GFXVertexPTT )
{
   Point3F point;
   Point2F texCoord1;
   Point2F texCoord2;
};

GFXDeclareVertexFormat( GFXVertexPTTT )
{
   Point3F point;
   Point2F texCoord1;
   Point2F texCoord2;
   Point2F texCoord3;
};

GFXDeclareVertexFormat( GFXVertexPC )
{
   Point3F point;
   GFXVertexColor color;
};

GFXDeclareVertexFormat( GFXVertexPCN )
{
   Point3F point;
   Point3F normal;
   GFXVertexColor color;
};

GFXDeclareVertexFormat( GFXVertexPCT )
{
   Point3F point;
   GFXVertexColor color;
   Point2F texCoord;
};

GFXDeclareVertexFormat( GFXVertexPCTT )
{
   Point3F point;
   GFXVertexColor color;
   Point2F texCoord;
   Point2F texCoord2;
};

GFXDeclareVertexFormat( GFXVertexPN )
{
   Point3F point;
   Point3F normal;
};

GFXDeclareVertexFormat( GFXVertexPNT )
{
   Point3F point;
   Point3F normal;
   Point2F texCoord;
};

GFXDeclareVertexFormat( GFXVertexPNTT )
{
   Point3F point;
   Point3F normal;
   Point3F tangent;
   Point2F texCoord;
};

GFXDeclareVertexFormat( GFXVertexPNTBT )
{
   Point3F point;
   Point3F normal;
   Point3F tangent;
   Point3F binormal;
   Point2F texCoord;
};

/*

DEFINE_VERT( GFXVertexPCNT, 
            GFXVertexFlagXYZ | GFXVertexFlagNormal | GFXVertexFlagDiffuse | GFXVertexFlagTextureCount1 | GFXVertexFlagUV0)
{
   Point3F point;
   Point3F normal;
   GFXVertexColor color;
   Point2F texCoord;
};

DEFINE_VERT( GFXVertexPCNTT, 
            GFXVertexFlagXYZ | GFXVertexFlagNormal | GFXVertexFlagDiffuse | GFXVertexFlagTextureCount2 | GFXVertexFlagUV0 | GFXVertexFlagUV1)
{
   Point3F point;
   Point3F normal;
   GFXVertexColor color;
   Point2F texCoord[2];
};
*/

GFXDeclareVertexFormat( GFXVertexPNTTB )
{
   Point3F point;
   Point3F normal;
   Point3F T;
   Point3F B;
   Point2F texCoord;
   Point2F texCoord2;
};

/*
DEFINE_VERT( GFXVertexPNTB,
            GFXVertexFlagXYZ | GFXVertexFlagNormal | GFXVertexFlagTextureCount2 | 
            GFXVertexFlagUV0 | GFXVertexFlagUVW1 )
{
   Point3F point;
   Point3F normal;
   Point2F texCoord;
   Point3F binormal;
};
*/

#endif // _GFXVERTEXTYPES_H_
