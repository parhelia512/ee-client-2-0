//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gfx/gfxVertexTypes.h"


GFXImplementVertexFormat( GFXVertexP )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
}

GFXImplementVertexFormat( GFXVertexPT )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
}

GFXImplementVertexFormat( GFXVertexPTT )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 1 );
}

GFXImplementVertexFormat( GFXVertexPTTT )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 1 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 2 );
}

GFXImplementVertexFormat( GFXVertexPC )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::COLOR, GFXDeclType_Color );
}

GFXImplementVertexFormat( GFXVertexPCN )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::COLOR, GFXDeclType_Color );
}

GFXImplementVertexFormat( GFXVertexPCT )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::COLOR, GFXDeclType_Color );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
}

GFXImplementVertexFormat( GFXVertexPCTT )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::COLOR, GFXDeclType_Color );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 1 );
}

GFXImplementVertexFormat( GFXVertexPN )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
}

GFXImplementVertexFormat( GFXVertexPNT )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
}

GFXImplementVertexFormat( GFXVertexPNTT )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::TANGENT, GFXDeclType_Float3 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
}

GFXImplementVertexFormat( GFXVertexPNTBT )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::TANGENT, GFXDeclType_Float3 );
   addElement( GFXSemantic::BINORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
}

GFXImplementVertexFormat( GFXVertexPNTTB )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::TANGENT, GFXDeclType_Float3 );
   addElement( GFXSemantic::BINORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 1 );
}
