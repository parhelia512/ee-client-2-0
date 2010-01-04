//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/console.h"
#include "console/consoleInternal.h"
#include "console/ast.h"
#include "core/stream/fileStream.h"
#include "console/compiler.h"
#include "platform/event.h"
#include "platform/platformInput.h"
#include "torqueConfig.h"
#include "core/frameAllocator.h"

// Buffer for expanding script filenames.
static char sgScriptFilenameBuffer[1024];

//-------------------------------------- Helper Functions
static void forwardslash(char *str)
{
   while(*str)
   {
      if(*str == '\\')
         *str = '/';
      str++;
   }
}

//----------------------------------------------------------------
ConsoleFunctionGroupBegin( FileSystem, "Functions allowing you to search for files, read them, write them, and access their properties.");

static Vector<String>   sgFindFilesResults;
static U32              sgFindFilesPos = 0;

static S32 buildFileList(const char* pattern, bool recurse, bool multiMatch)
{
   static const String sSlash( "/" );

   sgFindFilesResults.clear();

   String sPattern(Torque::Path::CleanSeparators(pattern));
   if(sPattern.isEmpty())
   {
      Con::errorf("findFirstFile() requires a search pattern");
      return -1;
   }

   if(!Con::expandScriptFilename(sgScriptFilenameBuffer, sizeof(sgScriptFilenameBuffer), sPattern.c_str()))
   {
      Con::errorf("findFirstFile() given initial directory cannot be expanded: '%s'", pattern);
      return -1;
   }
   sPattern = String::ToString(sgScriptFilenameBuffer);

   String::SizeType slashPos = sPattern.find('/', 0, String::Right);
//    if(slashPos == String::NPos)
//    {
//       Con::errorf("findFirstFile() missing search directory or expression: '%s'", sPattern.c_str());
//       return -1;
//    }

   // Build the initial search path
   Torque::Path givenPath(Torque::Path::CompressPath(sPattern));
   givenPath.setFileName("*");
   givenPath.setExtension("*");

   if(givenPath.getPath().length() > 0 && givenPath.getPath().find('*', 0, String::Right) == givenPath.getPath().length()-1)
   {
      // Deal with legacy searches of the form '*/*.*'
      String suspectPath = givenPath.getPath();
      String::SizeType newLen = suspectPath.length()-1;
      if(newLen > 0 && suspectPath.find('/', 0, String::Right) == suspectPath.length()-2)
      {
         --newLen;
      }
      givenPath.setPath(suspectPath.substr(0, newLen));
   }

   Torque::FS::FileSystemRef fs = Torque::FS::GetFileSystem(givenPath);
   //Torque::Path path = fs->mapTo(givenPath);
   Torque::Path path = givenPath;
   
   // Make sure that we have a root so the correct file system can be determined when using zips
   if(givenPath.isRelative())
      path = Torque::Path::Join(Torque::FS::GetCwd(), '/', givenPath);
   
   path.setFileName(String::EmptyString);
   path.setExtension(String::EmptyString);
   if(!Torque::FS::IsDirectory(path))
   {
      Con::errorf("findFirstFile() invalid initial search directory: '%s'", path.getFullPath().c_str());
      return -1;
   }

   // Build the search expression
   const String expression(slashPos != String::NPos ? sPattern.substr(slashPos+1) : sPattern);
   if(expression.isEmpty())
   {
      Con::errorf("findFirstFile() requires a search expression: '%s'", sPattern.c_str());
      return -1;
   }

   S32 results = Torque::FS::FindByPattern(path, expression, recurse, sgFindFilesResults, multiMatch );
   if(givenPath.isRelative() && results > 0)
   {
      // Strip the CWD out of the returned paths
      // MakeRelativePath() returns incorrect results (it adds a leading ..) so doing this the dirty way
      const String cwd = Torque::FS::GetCwd().getFullPath();
      for(S32 i = 0;i < sgFindFilesResults.size();++i)
      {
         String str = sgFindFilesResults[i];
         if(str.compare(cwd, cwd.length(), String::NoCase) == 0)
            str = str.substr(cwd.length());
         sgFindFilesResults[i] = str;
      }
   }
   return results;
}

ConsoleFunction(findFirstFile, const char *, 2, 3, "(string pattern [, bool recurse]) Returns the first file in the directory system matching the given pattern.")
{
   bool recurse = true;
   if(argc == 3)
   {
      recurse = dAtob(argv[2]);
   }

   S32 numResults = buildFileList(argv[1], recurse, false);

   // For Debugging
   //for ( S32 i = 0; i < sgFindFilesResults.size(); i++ )
   //   Con::printf( " [%i] [%s]", i, sgFindFilesResults[i].c_str() );

   sgFindFilesPos = 1;

   if(numResults < 0)
   {
      Con::errorf("findFirstFile() search directory not found: '%s'", argv[1]);
      return String();
   }

   return numResults ? sgFindFilesResults[0] : String();
}

ConsoleFunction(findNextFile, const char*, 1, 2, "([string pattern]) Returns the next file matching a search begun in findFirstFile.")
{
   if ( sgFindFilesPos + 1 > sgFindFilesResults.size() )
      return String();

   return sgFindFilesResults[sgFindFilesPos++];
}

ConsoleFunction(getFileCount, S32, 2, 3, "(string pattern [, bool recurse]) Returns the number of files in the directory tree that match the given pattern")
{
   bool recurse = true;
   if(argc == 3)
   {
      recurse = dAtob(argv[2]);
   }

   S32 numResults = buildFileList(argv[1], recurse, false);

   if(numResults < 0)
   {
      return 0;
   }

   return numResults;
}

ConsoleFunction(findFirstFileMultiExpr, const char *, 2, 3, "(string pattern [, bool recurse]) Returns the first file in the directory system matching the given pattern.")
{
   bool recurse = true;
   if(argc == 3)
   {
      recurse = dAtob(argv[2]);
   }

   S32 numResults = buildFileList(argv[1], recurse, true);

   // For Debugging
   //for ( S32 i = 0; i < sgFindFilesResults.size(); i++ )
   //   Con::printf( " [%i] [%s]", i, sgFindFilesResults[i].c_str() );

   sgFindFilesPos = 1;

   if(numResults < 0)
   {
      Con::errorf("findFirstFile() search directory not found: '%s'", argv[1]);
      return String();
   }

   return numResults ? sgFindFilesResults[0] : String();
}

ConsoleFunction(findNextFileMultiExpr, const char*, 1, 2, "([string pattern]) Returns the next file matching a search begun in findFirstFile.")
{
   if ( sgFindFilesPos + 1 > sgFindFilesResults.size() )
      return String();

   return sgFindFilesResults[sgFindFilesPos++];
}

ConsoleFunction(getFileCountMultiExpr, S32, 2, 3, "(string pattern [, bool recurse]) Returns the number of files in the directory tree that match the given pattern")
{
   bool recurse = true;
   if(argc == 3)
   {
      recurse = dAtob(argv[2]);
   }

   S32 numResults = buildFileList(argv[1], recurse, true);

   if(numResults < 0)
   {
      return 0;
   }

   return numResults;
}

ConsoleFunction(getFileCRC, S32, 2, 2, "getFileCRC(filename)")
{
   TORQUE_UNUSED(argc);
   String filename(Torque::Path::CleanSeparators(argv[1]));
   Con::expandScriptFilename(sgScriptFilenameBuffer, sizeof(sgScriptFilenameBuffer), filename.c_str());

   Torque::Path givenPath(Torque::Path::CompressPath(sgScriptFilenameBuffer));
   Torque::FS::FileNodeRef fileRef = Torque::FS::GetFileNode( givenPath );

   if ( fileRef == NULL )
   {
      Con::errorf("getFileCRC() - could not access file: [%s]", givenPath.getFullPath().c_str() );
      return -1;
   }

   return fileRef->getChecksum();
}

ConsoleFunction(isFile, bool, 2, 2, "isFile(fileName)")
{
   TORQUE_UNUSED(argc);
   String filename(Torque::Path::CleanSeparators(argv[1]));
   Con::expandScriptFilename(sgScriptFilenameBuffer, sizeof(sgScriptFilenameBuffer), filename.c_str());

   Torque::Path givenPath(Torque::Path::CompressPath(sgScriptFilenameBuffer));
   return Torque::FS::IsFile(givenPath);
}

ConsoleFunction( IsDirectory, bool, 2, 2, "( string: directory of form \"foo/bar\", do not include trailing /, case insensitive, directory must have files in it if you expect the directory to be in a zip )" )
{
   TORQUE_UNUSED(argc);
   String dir(Torque::Path::CleanSeparators(argv[1]));
   Con::expandScriptFilename(sgScriptFilenameBuffer, sizeof(sgScriptFilenameBuffer), dir.c_str());

   Torque::Path givenPath(Torque::Path::CompressPath(sgScriptFilenameBuffer));
   return Torque::FS::IsDirectory( givenPath );
}

ConsoleFunction(isWriteableFileName, bool, 2, 2, "isWriteableFileName(fileName)")
{
   TORQUE_UNUSED(argc);
   String filename(Torque::Path::CleanSeparators(argv[1]));
   Con::expandScriptFilename(sgScriptFilenameBuffer, sizeof(sgScriptFilenameBuffer), filename.c_str());

   Torque::Path givenPath(Torque::Path::CompressPath(sgScriptFilenameBuffer));
   Torque::FS::FileSystemRef fs = Torque::FS::GetFileSystem(givenPath);
   Torque::Path path = fs->mapTo(givenPath);

   return !Torque::FS::IsReadOnly(path);
}

ConsoleFunction(startFileChangeNotifications, void, 1, 1, "startFileChangeNotifications() - start watching resources for file changes" )
{
   Torque::FS::StartFileChangeNotifications();
}

ConsoleFunction(stopFileChangeNotifications, void, 1, 1, "stopFileChangeNotifications() - stop watching resources for file changes" )
{
   Torque::FS::StopFileChangeNotifications();
}


ConsoleFunction(getDirectoryList, const char*, 2, 3, "getDirectoryList(%path, %depth)")
{
   // Grab the full path.
   char path[1024];
   Platform::makeFullPathName(dStrcmp(argv[1], "/") == 0 ? "" : argv[1], path, sizeof(path));

   //dSprintf(path, 511, "%s/%s", Platform::getWorkingDirectory(), argv[1]);

   // Append a trailing backslash if it's not present already.
   if (path[dStrlen(path) - 1] != '/')
   {
      S32 pos = dStrlen(path);
      path[pos] = '/';
      path[pos + 1] = '\0';
   }

   // Grab the depth to search.
   S32 depth = 0;
   if (argc > 2)
      depth = dAtoi(argv[2]);

   // Dump the directories.
   Vector<StringTableEntry> directories;
   Platform::dumpDirectories(path, directories, depth, true);

   if( directories.empty() )
      return "";

   // Grab the required buffer length.
   S32 length = 0;

   for (S32 i = 0; i < directories.size(); i++)
      length += dStrlen(directories[i]) + 1;

   // Get a return buffer.
   char* buffer = Con::getReturnBuffer(length);
   char* p = buffer;

   // Copy the directory names to the buffer.
   for (S32 i = 0; i < directories.size(); i++)
   {
      dStrcpy(p, directories[i]);
      p += dStrlen(directories[i]);
      // Tab separated.
      p[0] = '\t';
      p++;
   }
   p--;
   p[0] = '\0';

   return buffer;
}

ConsoleFunction(fileSize, S32, 2, 2, "fileSize(fileName) returns filesize or -1 if no file")
{
   TORQUE_UNUSED(argc);
   Con::expandScriptFilename(sgScriptFilenameBuffer, sizeof(sgScriptFilenameBuffer), argv[1]);
   return Platform::getFileSize( sgScriptFilenameBuffer );
}

ConsoleFunction( fileModifiedTime, const char*, 2, 2, "fileModifiedTime( string fileName )\n"
   "Returns a platform specific formatted string with the last modified time for the file." )
{
   Con::expandScriptFilename(sgScriptFilenameBuffer, sizeof(sgScriptFilenameBuffer), argv[1]);

   FileTime ft = {0};
   Platform::getFileTimes( sgScriptFilenameBuffer, NULL, &ft );

   Platform::LocalTime lt = {0};
   Platform::fileToLocalTime( ft, &lt );   
   
   String fileStr = Platform::localTimeToString( lt );
   
   char *buffer = Con::getReturnBuffer( fileStr.size() );
   dStrcpy( buffer, fileStr );   
   
   return buffer;
}

ConsoleFunction( fileCreatedTime, const char*, 2, 2, "fileCreatedTime( string fileName )\n"
   "Returns a platform specific formatted string with the creation time for the file." )
{
   Con::expandScriptFilename( sgScriptFilenameBuffer, sizeof(sgScriptFilenameBuffer), argv[1] );

   FileTime ft = {0};
   Platform::getFileTimes( sgScriptFilenameBuffer, &ft, NULL );

   Platform::LocalTime lt = {0};
   Platform::fileToLocalTime( ft, &lt );   

   String fileStr = Platform::localTimeToString( lt );

   char *buffer = Con::getReturnBuffer( fileStr.size() );
   dStrcpy( buffer, fileStr );  

   return buffer;
}

ConsoleFunction(fileDelete, bool, 2,2, "fileDelete('path')")
{
   static char fileName[1024];
   static char sandboxFileName[1024];

   Con::expandScriptFilename( fileName, sizeof( fileName ), argv[1] );
   Platform::makeFullPathName(fileName, sandboxFileName, sizeof(sandboxFileName));

   return dFileDelete(sandboxFileName);
}


//----------------------------------------------------------------

ConsoleFunction(fileExt, const char *, 2, 2, "fileExt(fileName)")
{
   TORQUE_UNUSED(argc);
   const char *ret = dStrrchr(argv[1], '.');
   if(ret)
      return ret;
   return "";
}

ConsoleFunction(fileBase, const char *, 2, 2, "fileBase(fileName)")
{

   S32 pathLen = dStrlen( argv[1] );
   FrameTemp<char> szPathCopy( pathLen + 1);

   dStrcpy( szPathCopy, argv[1] );
   forwardslash( szPathCopy );

   TORQUE_UNUSED(argc);
   const char *path = dStrrchr(szPathCopy, '/');
   if(!path)
      path = szPathCopy;
   else
      path++;
   char *ret = Con::getReturnBuffer(dStrlen(path) + 1);
   dStrcpy(ret, path);
   char *ext = dStrrchr(ret, '.');
   if(ext)
      *ext = 0;
   return ret;
}

ConsoleFunction(fileName, const char *, 2, 2, "fileName(filePathName)")
{
   S32 pathLen = dStrlen( argv[1] );
   FrameTemp<char> szPathCopy( pathLen + 1);

   dStrcpy( szPathCopy, argv[1] );
   forwardslash( szPathCopy );

   TORQUE_UNUSED(argc);
   const char *name = dStrrchr(szPathCopy, '/');
   if(!name)
      name = szPathCopy;
   else
      name++;
   char *ret = Con::getReturnBuffer(dStrlen(name));
   dStrcpy(ret, name);
   return ret;
}

ConsoleFunction(filePath, const char *, 2, 2, "filePath(fileName)")
{
   S32 pathLen = dStrlen( argv[1] );
   FrameTemp<char> szPathCopy( pathLen + 1);

   dStrcpy( szPathCopy, argv[1] );
   forwardslash( szPathCopy );

   TORQUE_UNUSED(argc);
   const char *path = dStrrchr(szPathCopy, '/');
   if(!path)
      return "";
   U32 len = path - (char*)szPathCopy;
   char *ret = Con::getReturnBuffer(len + 1);
   dStrncpy(ret, szPathCopy, len);
   ret[len] = 0;
   return ret;
}

ConsoleFunction(getWorkingDirectory, const char *, 1, 1, "alias to getCurrentDirectory()")
{
   return Platform::getCurrentDirectory();
}

//-----------------------------------------------------------------------------

// [tom, 5/1/2007] I changed these to be ordinary console functions as they
// are just string processing functions. They are needed by the 3D tools which
// are not currently built with TORQUE_TOOLS defined.

ConsoleFunction(makeFullPath, const char *, 2, 3, "(string path, [string currentWorkingDir])")
{
   char *buf = Con::getReturnBuffer(512);
   Platform::makeFullPathName(argv[1], buf, 512, argc > 2 ? argv[2] : NULL);
   return buf;
}

ConsoleFunction(makeRelativePath, const char *, 3, 3, "(string path, string to)")
{
   return Platform::makeRelativePathName(argv[1], argv[2]);
}

ConsoleFunction(pathConcat, const char *, 3, 0, "(string path, string file1, [... fileN])")
{
   char *buf = Con::getReturnBuffer(1024);
   char pathBuf[1024];
   dStrcpy(buf, argv[1]);

   // CodeReview [tom, 5/1/2007] I don't think this will work as expected with multiple file names

   for(S32 i = 2;i < argc;++i)
   {
      Platform::makeFullPathName(argv[i], pathBuf, 1024, buf);
      dStrcpy(buf, pathBuf);
   }
   return buf;
}

//-----------------------------------------------------------------------------

ConsoleFunction(getExecutableName, const char *, 1, 1, "getExecutableName()")
{
   return Platform::getExecutableName();
}

ConsoleFunction(getMainDotCsDir, const char *, 1, 1, "getExecutableName()")
{
   return Platform::getMainDotCsDir();
}

//-----------------------------------------------------------------------------
// Tools Only Functions
//-----------------------------------------------------------------------------

#ifdef TORQUE_TOOLS

ConsoleToolFunction(openFolder, void, 2 ,2,"openFolder(%path);")
{
   Platform::openFolder( argv[1] );
}

ConsoleToolFunction(openFile, void, 2,2,"openFile(%path);")
{
   Platform::openFile( argv[1] );
}

ConsoleToolFunction(pathCopy, bool, 3, 4, "pathCopy(fromFile, toFile [, nooverwrite = true])")
{
   bool nooverwrite = true;

   if( argc > 3 )
      nooverwrite = dAtob( argv[3] );

   static char fromFile[1024];
   static char toFile[1024];

   static char qualifiedFromFile[1024];
   static char qualifiedToFile[1024];

   Con::expandScriptFilename( fromFile, sizeof( fromFile ), argv[1] );
   Con::expandScriptFilename( toFile, sizeof( toFile ), argv[2] );

   Platform::makeFullPathName(fromFile, qualifiedFromFile, sizeof(qualifiedFromFile));
   Platform::makeFullPathName(toFile, qualifiedToFile, sizeof(qualifiedToFile));

   return dPathCopy( qualifiedFromFile, qualifiedToFile, nooverwrite );
}

ConsoleToolFunction(getCurrentDirectory, const char *, 1, 1, "getCurrentDirectory()")
{
   return Platform::getCurrentDirectory();
}

ConsoleToolFunction( setCurrentDirectory, bool, 2, 2, "setCurrentDirectory(absolutePathName)" )
{
   return Platform::setCurrentDirectory( StringTable->insert( argv[1] ) );

}

ConsoleToolFunction( createPath, bool, 2,2, "createPath(\"file name or path name\");  creates the path or path to the file name")
{
   static char pathName[1024];

   Con::expandScriptFilename( pathName, sizeof( pathName ), argv[1] );

   return Platform::createPath( pathName );
}

#endif // TORQUE_TOOLS

//-----------------------------------------------------------------------------

ConsoleFunctionGroupEnd( FileSystem );
