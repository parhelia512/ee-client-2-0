//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/fileObject.h"

IMPLEMENT_CONOBJECT(FileObject);

bool FileObject::isEOF()
{
   return mCurPos == mBufferSize;
}

FileObject::FileObject()
{
   mFileBuffer = NULL;
   mBufferSize = 0;
   mCurPos = 0;
   stream = NULL;
}

FileObject::~FileObject()
{
   SAFE_DELETE_ARRAY(mFileBuffer);
   SAFE_DELETE(stream);
}

void FileObject::close()
{
   SAFE_DELETE(stream);
   SAFE_DELETE_ARRAY(mFileBuffer);
   mFileBuffer = NULL;
   mBufferSize = mCurPos = 0;
}

bool FileObject::openForWrite(const char *fileName, const bool append)
{
   char buffer[1024];
   Con::expandScriptFilename( buffer, sizeof( buffer ), fileName );

   close();

   if((stream = FileStream::createAndOpen( fileName, append ? Torque::FS::File::WriteAppend : Torque::FS::File::Write )) == NULL)
      return false;

   stream->setPosition( stream->getStreamSize() );
   return( true );
}

bool FileObject::openForRead(const char* /*fileName*/)
{
   AssertFatal(false, "Error, not yet implemented!");
   return false;
}

bool FileObject::readMemory(const char *fileName)
{
   char buffer[1024];
   Con::expandScriptFilename( buffer, sizeof( buffer ), fileName );

   close();

   void *data = NULL;
   U32 dataSize = 0;
   Torque::FS::ReadFile(buffer, data, dataSize, true);
   if(data == NULL)
      return false;

   mBufferSize = dataSize;
   mFileBuffer = (U8 *)data;
   mCurPos = 0;

   return true;
}

const U8 *FileObject::readLine()
{
   if(!mFileBuffer)
      return (U8 *) "";

   U32 tokPos = mCurPos;

   for(;;)
   {
      if(mCurPos == mBufferSize)
         break;

      if(mFileBuffer[mCurPos] == '\r')
      {
         mFileBuffer[mCurPos++] = 0;
         if(mFileBuffer[mCurPos] == '\n')
            mCurPos++;
         break;
      }

      if(mFileBuffer[mCurPos] == '\n')
      {
         mFileBuffer[mCurPos++] = 0;
         break;
      }

      mCurPos++;
   }

   return mFileBuffer + tokPos;
}

void FileObject::peekLine( U8* line, S32 length )
{
   if(!mFileBuffer)
   {
      line[0] = '\0';
      return;
   }

   // Copy the line into the buffer. We can't do this like readLine because
   // we can't modify the file buffer.
   S32 i = 0;
   U32 tokPos = mCurPos;
   while( ( tokPos != mBufferSize ) && ( mFileBuffer[tokPos] != '\r' ) && ( mFileBuffer[tokPos] != '\n' ) && ( i < ( length - 1 ) ) )
      line[i++] = mFileBuffer[tokPos++];

   line[i++] = '\0';

   //if( i == length )
      //Con::warnf( "FileObject::peekLine - The line contents could not fit in the buffer (size %d). Truncating.", length );
}

void FileObject::writeLine(const U8 *line)
{
   stream->write(dStrlen((const char *) line), line);
   stream->write(2, "\r\n");
}

void FileObject::writeObject( SimObject* object, const U8* objectPrepend )
{
   if( objectPrepend == NULL )
      stream->write(2, "\r\n");
   else
      stream->write(dStrlen((const char *) objectPrepend), objectPrepend );
   object->write( *stream, 0 );
}

ConsoleMethod( FileObject, openForRead, bool, 3, 3, "(string filename)")
{
   return object->readMemory(argv[2]);
}

ConsoleMethod( FileObject, openForWrite, bool, 3, 3, "(string filename)")
{
   return object->openForWrite(argv[2]);
}

ConsoleMethod( FileObject, openForAppend, bool, 3, 3, "(string filename)")
{
   return object->openForWrite(argv[2], true);
}

ConsoleMethod( FileObject, isEOF, bool, 2, 2, "Are we at the end of the file?")
{
   return object->isEOF();
}

ConsoleMethod( FileObject, readLine, const char*, 2, 2, "Read a line from the file.")
{
   return (const char *) object->readLine();
}

ConsoleMethod( FileObject, peekLine, const char*, 2, 2, "Read a line from the file without moving the stream position.")
{
   char *line = Con::getReturnBuffer( 512 );
   object->peekLine( (U8*)line, 512 );
   return line;
}

ConsoleMethod( FileObject, writeLine, void, 3, 3, "(string text)"
              "Write a line to the file, if it was opened for writing.")
{
   object->writeLine((const U8 *) argv[2]);
}

ConsoleMethod( FileObject, close, void, 2, 2, "Close the file.")
{
   object->close();
}

ConsoleMethod( FileObject, writeObject, void, 3, 4, "FileObject.writeObject(SimObject, object prepend)" )
{
   SimObject* obj = Sim::findObject( argv[2] );
   if( !obj )
   {
      Con::printf("FileObject::writeObject - Invalid Object!");
      return;
   }

   char *objName = NULL;
   if( argc == 4 )
      objName = (char*)argv[3];

   object->writeObject( obj, (const U8*)objName );
}

