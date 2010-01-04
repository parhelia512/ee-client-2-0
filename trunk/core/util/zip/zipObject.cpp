//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/util/zip/zipObject.h"
#include "core/util/safeDelete.h"

//-----------------------------------------------------------------------------
// Constructor/Destructor
//-----------------------------------------------------------------------------

ZipObject::ZipObject()
{
   mZipArchive = NULL;
}

ZipObject::~ZipObject()
{
   closeArchive();
}

IMPLEMENT_CONOBJECT(ZipObject);

//-----------------------------------------------------------------------------
// Protected Methods
//-----------------------------------------------------------------------------

StreamObject *ZipObject::createStreamObject(Stream *stream)
{
   for(S32 i = 0;i < mStreamPool.size();++i)
   {
      StreamObject *so = mStreamPool[i];

      if(so == NULL)
      {
         // Reuse any free locations in the pool
         so = new StreamObject(stream);
         so->registerObject();
         mStreamPool[i] = so;
         return so;
      }
      
      if(so->getStream() == NULL)
      {
         // Existing unused stream, update it and return it
         so->setStream(stream);
         return so;
      }
   }

   // No free object found, create a new one
   StreamObject *so = new StreamObject(stream);
   so->registerObject();
   mStreamPool.push_back(so);

   return so;
}

//-----------------------------------------------------------------------------
// Public Methods
//-----------------------------------------------------------------------------

bool ZipObject::openArchive(const char *filename, Zip::ZipArchive::AccessMode mode /* = Read */)
{
   closeArchive();

   mZipArchive = new Zip::ZipArchive;
   if(mZipArchive->openArchive(filename, mode))
      return true;

   SAFE_DELETE(mZipArchive);
   return false;
}

void ZipObject::closeArchive()
{
   if(mZipArchive == NULL)
      return;

   for(S32 i = 0;i < mStreamPool.size();++i)
   {
      StreamObject *so = mStreamPool[i];
      if(so && so->getStream() != NULL)
         closeFile(so);
      
      SAFE_DELETE_OBJECT(mStreamPool[i]);
   }
   mStreamPool.clear();

   mZipArchive->closeArchive();
   SAFE_DELETE(mZipArchive);
}

//-----------------------------------------------------------------------------

StreamObject * ZipObject::openFileForRead(const char *filename)
{
   if(mZipArchive == NULL)
      return NULL;

   Stream * stream = mZipArchive->openFile(filename, Zip::ZipArchive::Read);
   if(stream != NULL)
      return createStreamObject(stream);

   return NULL;
}

StreamObject * ZipObject::openFileForWrite(const char *filename)
{
   if(mZipArchive == NULL)
      return NULL;

   Stream * stream = mZipArchive->openFile(filename, Zip::ZipArchive::Write);
   if(stream != NULL)
      return createStreamObject(stream);

   return NULL;
}

void ZipObject::closeFile(StreamObject *stream)
{
   if(mZipArchive == NULL)
      return;

#ifdef TORQUE_DEBUG
   bool found = false;
   for(S32 i = 0;i < mStreamPool.size();++i)
   {
      StreamObject *so = mStreamPool[i];
      if(so && so == stream)
      {
         found = true;
         break;
      }
   }

   AssertFatal(found, "ZipObject::closeFile() - Attempting to close stream not opened by this ZipObject");
#endif

   mZipArchive->closeFile(stream->getStream());
   stream->setStream(NULL);
}

//-----------------------------------------------------------------------------

bool ZipObject::addFile(const char *filename, const char *pathInZip, bool replace /* = true */)
{
   return mZipArchive->addFile(filename, pathInZip, replace);
}

bool ZipObject::extractFile(const char *pathInZip, const char *filename)
{
   return mZipArchive->extractFile(pathInZip, filename);
}

bool ZipObject::deleteFile(const char *filename)
{
   return mZipArchive->deleteFile(filename);
}

//-----------------------------------------------------------------------------

S32 ZipObject::getFileEntryCount()
{
   return mZipArchive ? mZipArchive->numEntries() : 0;
}

String ZipObject::getFileEntry(S32 idx)
{
   if(mZipArchive == NULL)
      return "";

   const Zip::CentralDir &dir = (*mZipArchive)[idx];
   char buffer[1024];
   int chars = dSprintf(buffer, sizeof(buffer), "%s\t%d\t%d\t%d\t%08x",
            dir.mFilename.c_str(), dir.mUncompressedSize, dir.mCompressedSize,
            dir.mCompressMethod, dir.mCRC32);
   if (chars < sizeof(buffer))
      buffer[chars] = 0;

   return String(buffer);
}

//-----------------------------------------------------------------------------
// Console Methods
//-----------------------------------------------------------------------------

static const struct
{
   const char *strMode;
   Zip::ZipArchive::AccessMode mode;
} gModeMap[]=
{
   { "read", Zip::ZipArchive::Read },
   { "write", Zip::ZipArchive::Write },
   { "readwrite", Zip::ZipArchive::ReadWrite },
   { NULL, (Zip::ZipArchive::AccessMode)0 }
};

//-----------------------------------------------------------------------------

ConsoleMethod(ZipObject, openArchive, bool, 3, 4, "(filename, [accessMode = Read]) Open a zip file")
{
   Zip::ZipArchive::AccessMode mode = Zip::ZipArchive::Read;

   if(argc > 3)
   {
      for(S32 i = 0;gModeMap[i].strMode;++i)
      {
         if(dStricmp(gModeMap[i].strMode, argv[3]) == 0)
         {
            mode = gModeMap[i].mode;
            break;
         }
      }
   }

   char buf[512];
   Con::expandScriptFilename(buf, sizeof(buf), argv[2]);

   return object->openArchive(buf, mode);
}

ConsoleMethod(ZipObject, closeArchive, void, 2, 2, "() Close a zip file")
{
   object->closeArchive();
}

//-----------------------------------------------------------------------------

ConsoleMethod(ZipObject, openFileForRead, S32, 3, 3, "(filename) Open a file within the zip for reading")
{
   StreamObject *stream = object->openFileForRead(argv[2]);
   return stream ? stream->getId() : 0;
}

ConsoleMethod(ZipObject, openFileForWrite, S32, 3, 3, "(filename) Open a file within the zip for reading")
{
   StreamObject *stream = object->openFileForWrite(argv[2]);
   return stream ? stream->getId() : 0;
}

ConsoleMethod(ZipObject, closeFile, void, 3, 3, "(stream) Close a file within the zip")
{
   StreamObject *stream = dynamic_cast<StreamObject *>(Sim::findObject(argv[2]));
   if(stream == NULL)
   {
      Con::errorf("ZipObject::closeFile - Invalid stream specified");
      return;
   }

   object->closeFile(stream);
}

//-----------------------------------------------------------------------------

ConsoleMethod(ZipObject, addFile, bool, 4, 5, "(filename, pathInZip[, replace = true]) Add a file to the zip")
{
   // Left this line commented out as it was useful when i had a problem
   //  with the zip's i was creating. [2/21/2007 justind]
   // [tom, 2/21/2007] To is now a warnf() for better visual separation in the console
  // Con::errorf("zipAdd: %s", argv[2]);
   //Con::warnf("    To: %s", argv[3]);

   return object->addFile(argv[2], argv[3], argc > 4 ? dAtob(argv[4]) : true);
}

ConsoleMethod(ZipObject, extractFile, bool, 4, 4, "(pathInZip, filename) Extract a file from the zip")
{
   return object->extractFile(argv[2], argv[3]);
}

ConsoleMethod(ZipObject, deleteFile, bool, 3, 3, "(pathInZip) Delete a file from the zip")
{
   return object->deleteFile(argv[2]);
}

//-----------------------------------------------------------------------------

ConsoleMethod(ZipObject, getFileEntryCount, S32, 2, 2, "() Get number of files in the zip")
{
   return object->getFileEntryCount();
}

ConsoleMethod(ZipObject, getFileEntry, const char *, 3, 3, "(index) Get file entry. Returns tab separated string containing filename, uncompressed size, compressed size, compression method and CRC32")
{
   return object->getFileEntry(dAtoi(argv[2]));
}
