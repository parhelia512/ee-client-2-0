//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/stream/streamObject.h"

//-----------------------------------------------------------------------------
// Constructor/Destructor
//-----------------------------------------------------------------------------

StreamObject::StreamObject()
{
   mStream = NULL;
}

StreamObject::StreamObject(Stream *stream)
{
   mStream = stream;
}

StreamObject::~StreamObject()
{
}

IMPLEMENT_CONOBJECT(StreamObject);

//-----------------------------------------------------------------------------

bool StreamObject::onAdd()
{
   if(mStream == NULL)
   {
      Con::errorf("StreamObject::onAdd - StreamObject can not be instantiated from script.");
      return false;
   }
   return Parent::onAdd();
}

//-----------------------------------------------------------------------------
// Public Methods
//-----------------------------------------------------------------------------

const char * StreamObject::getStatus()
{
   if(mStream == NULL)
      return "";

   switch(mStream->getStatus())
   {
      case Stream::Ok:
         return "Ok";
      case Stream::IOError:
         return "IOError";
      case Stream::EOS:
         return "EOS";
      case Stream::IllegalCall:
         return "IllegalCall";
      case Stream::Closed:
         return "Closed";
      case Stream::UnknownError:
         return "UnknownError";

      default:
         return "Invalid";
   }
}

//-----------------------------------------------------------------------------

const char * StreamObject::readLine()
{
   if(mStream == NULL)
      return NULL;

   char *buffer = Con::getReturnBuffer(256);
   mStream->readLine((U8 *)buffer, 256);
   return buffer;
}

const char * StreamObject::readString()
{
   if(mStream == NULL)
      return NULL;

   char *buffer = Con::getReturnBuffer(256);
   mStream->readString(buffer);
   return buffer;
}

const char *StreamObject::readLongString(U32 maxStringLen)
{
   if(mStream == NULL)
      return NULL;

   char *buffer = Con::getReturnBuffer(maxStringLen + 1);
   mStream->readLongString(maxStringLen, buffer);
   return buffer;
}

//-----------------------------------------------------------------------------
// Console Methods
//-----------------------------------------------------------------------------

ConsoleMethod(StreamObject, getStatus, const char *, 2, 2, "()")
{
   return object->getStatus();
}

ConsoleMethod(StreamObject, isEOS, bool, 2, 2, "() Test for end of stream")
{
   return object->isEOS();
}

ConsoleMethod(StreamObject, isEOF, bool, 2, 2, "() Test for end of stream")
{
   return object->isEOS();
}

//-----------------------------------------------------------------------------

ConsoleMethod(StreamObject, getPosition, S32, 2, 2, "()")
{
   return object->getPosition();
}

ConsoleMethod(StreamObject, setPosition, bool, 3, 3, "(newPosition)")
{
   return object->setPosition(dAtoi(argv[2]));
}

ConsoleMethod(StreamObject, getStreamSize, S32, 2, 2, "()")
{
   return object->getStreamSize();
}

//-----------------------------------------------------------------------------

ConsoleMethod(StreamObject, readLine, const char *, 2, 2, "()")
{
   const char *str = object->readLine();
   return str ? str : "";
}

ConsoleMethod(StreamObject, writeLine, void, 3, 3, "(line)")
{
   object->writeLine((U8 *)argv[2]);
}

//-----------------------------------------------------------------------------

ConsoleMethod(StreamObject, readSTString, const char *, 2, 3, "([caseSensitive = false])")
{
   const char *str = object->readSTString(argc > 2 ? dAtob(argv[2]) : false);
   return str ? str : "";
}

ConsoleMethod(StreamObject, readString, const char *, 2, 2, "()")
{
   const char *str = object->readString();
   return str ? str : "";
}

ConsoleMethod(StreamObject, readLongString, const char *, 3, 3, "(maxLength)")
{
   const char *str = object->readLongString(dAtoi(argv[2]));
   return str ? str : "";
}

ConsoleMethod(StreamObject, writeLongString, void, 4, 4, "(maxLength, string)")
{
   object->writeLongString(dAtoi(argv[2]), argv[3]);
}

ConsoleMethod(StreamObject, writeString, void, 3, 4, "(string, [maxLength = 255])")
{
   object->writeString(argv[2], argc > 3 ? dAtoi(argv[3]) : 255);
}

//-----------------------------------------------------------------------------

ConsoleMethod(StreamObject, copyFrom, bool, 3, 3, "(StreamObject other)")
{
   StreamObject *other = dynamic_cast<StreamObject *>(Sim::findObject(argv[2]));
   if(other == NULL)
      return false;

   return object->copyFrom(other);
}
