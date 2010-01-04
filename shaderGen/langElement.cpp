//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/strings/stringFunctions.h"
#include "core/util/str.h"

#include "langElement.h"

//**************************************************************************
// Language element
//**************************************************************************
Vector<LangElement*> LangElement::elementList( __FILE__, __LINE__ );

//--------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------
LangElement::LangElement()
{
   elementList.push_back( this );

   static U32 tempNum = 0;
   dSprintf( (char*)name, sizeof(name), "tempName%d", tempNum++ );
}

//--------------------------------------------------------------------------
// Find element of specified name
//--------------------------------------------------------------------------
LangElement * LangElement::find( const char *name )
{
   for( U32 i=0; i<elementList.size(); i++ )
   {
      if( !dStrcmp( (char*)elementList[i]->name, name ) )
      {
         return elementList[i];
      }
   }
   
   return NULL;
}

//--------------------------------------------------------------------------
// Delete existing elements
//--------------------------------------------------------------------------
void LangElement::deleteElements()
{
   for( U32 i=0; i<elementList.size(); i++ )
   {
      delete elementList[i];
   }
   
   elementList.setSize( 0 );

}

//--------------------------------------------------------------------------
// Set name
//--------------------------------------------------------------------------
void LangElement::setName(const char *newName )
{
   dStrcpy( (char*)name, newName );
}

//**************************************************************************
// Variable
//**************************************************************************
U32 Var::texUnitCount = 0;

Var::Var()
{
   dStrcpy( (char*)type, "float4" );
   structName[0] = '\0';
   uniform = false;
   vertData = false;
   connector = false;
   sampler = false;
   mapsToSampler = false;
   texCoordNum = 0;
   constSortPos = cspUninit;
   arraySize = 1;
}

Var::Var( const char *inName, const char *inType )
{
   structName[0] = '\0';
   uniform = false;
   vertData = false;
   connector = false;
   sampler = false;
   mapsToSampler = false;
   texCoordNum = 0;
   constSortPos = cspUninit;
   arraySize = 1;

   setName( inName );
   setType( inType );
}

void Var::setUniform(const String& constType, const String& constName, ConstantSortPosition sortPos)
{ 
   uniform = true;
   setType(constType.c_str());
   setName(constName.c_str());   
   constSortPos = cspPass;      
}

//--------------------------------------------------------------------------
// Set struct name
//--------------------------------------------------------------------------
void Var::setStructName(const char *newName )
{
   dStrcpy( (char*)structName, newName );
}

//--------------------------------------------------------------------------
// Set connect name
//--------------------------------------------------------------------------
void Var::setConnectName(const char *newName )
{
   dStrcpy( (char*)connectName, newName );
}

//--------------------------------------------------------------------------
// Set type
//--------------------------------------------------------------------------
void Var::setType(const char *newType )
{
   dStrcpy( (char*)type, newType );
}

//--------------------------------------------------------------------------
// print
//--------------------------------------------------------------------------
void Var::print( Stream &stream )
{
   if( structName[0] != '\0' )
   {
      stream.write( dStrlen((char*)structName), structName );
      stream.write( 1, "." );
   }

   stream.write( dStrlen((char*)name), name );
}

//--------------------------------------------------------------------------
// Get next available texture unit number
//--------------------------------------------------------------------------
U32 Var::getTexUnitNum(U32 numElements)
{
   U32 ret = texUnitCount;
   texUnitCount += numElements;
   return ret;
}

//--------------------------------------------------------------------------
// Reset
//--------------------------------------------------------------------------
void Var::reset()
{
   texUnitCount = 0;
}

//**************************************************************************
// Multi line statement
//**************************************************************************
void MultiLine::addStatement( LangElement *elem )
{
   AssertFatal( elem, "Attempting to add empty statement" );

   mStatementList.push_back( elem );
}

//--------------------------------------------------------------------------
// Print
//--------------------------------------------------------------------------
void MultiLine::print( Stream &stream )
{
   for( U32 i=0; i<mStatementList.size(); i++ )
   {
      mStatementList[i]->print( stream );
   }
} 