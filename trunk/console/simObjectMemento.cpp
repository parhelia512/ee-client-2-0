//-----------------------------------------------------------------------------
// Copyright (C) Sickhead Games, LLC
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/simObjectMemento.h"

#include "console/simObject.h"
#include "console/simDatablock.h"
#include "core/stream/memStream.h"


SimObjectMemento::SimObjectMemento()
   : mState( NULL ),
		mIsDatablock( false )
{
}

SimObjectMemento::~SimObjectMemento()
{
   dFree( mState );
}

void SimObjectMemento::save( SimObject *object )
{
   // Cleanup any existing state data.
   dFree( mState );
   mObjectName = String::EmptyString;

	// Use a stream to save the state.
   MemStream stream( 256 );

	SimDataBlock * db = dynamic_cast<SimDataBlock*>(object);
	if( !db )
		stream.write( sizeof( "return " ) - 1, "return " );
	else
		mIsDatablock = true;
	
   object->write( stream, 0, SimObject::NoName );
   stream.write( (UTF8)0 );

   // Steal the data away from the stream.
   mState = (UTF8*)stream.takeBuffer();
   mObjectName = object->getName();
}

SimObject *SimObjectMemento::restore() const
{
   // Make sure we have data to restore.
   if ( !mState )
      return NULL;

	String uniqueName = Sim::getUniqueName( mObjectName );

   // TODO: We could potentially make this faster by
   // caching the CodeBlock generated from the string
	SimObject *object;
	if( !mIsDatablock )
	{
		const UTF8* result = Con::evaluate( mState );
		if ( !result )
			return NULL;

		U32 objectId = dAtoi( result );
		object = Sim::findObject( objectId );
	}
	else
	{
		Con::evaluate( mState );
		if( mObjectName == String::EmptyString )
			return NULL;

		object = Sim::findObject( mObjectName );
	}
	
	
   

   if ( !object )
      return NULL;

   object->assignName( uniqueName );

   return object;
}