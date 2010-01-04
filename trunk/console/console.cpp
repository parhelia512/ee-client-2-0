//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/platformTLS.h"
#include "platform/threads/thread.h"
#include "console/console.h"
#include "console/consoleInternal.h"
#include "console/consoleObject.h"
#include "console/consoleParser.h"
#include "core/stream/fileStream.h"
#include "console/ast.h"
#include "core/tAlgorithm.h"
#include "console/consoleTypes.h"
#include "console/telnetDebugger.h"
#include "console/simBase.h"
#include "console/compiler.h"
#include "console/stringStack.h"
#include "console/ICallMethod.h"
#include <stdarg.h>
#include "platform/threads/mutex.h"


extern StringStack STR;

ExprEvalState gEvalState;
StmtNode *gStatementList;
ConsoleConstructor *ConsoleConstructor::first = NULL;
bool gWarnUndefinedScriptVariables;

static char scratchBuffer[4096];

CON_DECLARE_PARSER(CMD);

static const char * prependDollar ( const char * name )
{
   if(name[0] != '$')
   {
      S32   len = dStrlen(name);
      AssertFatal(len < sizeof(scratchBuffer)-2, "CONSOLE: name too long");
      scratchBuffer[0] = '$';
      dMemcpy(scratchBuffer + 1, name, len + 1);
      name = scratchBuffer;
   }
   return name;
}

static const char * prependPercent ( const char * name )
{
   if(name[0] != '%')
   {
      S32   len = dStrlen(name);
      AssertFatal(len < sizeof(scratchBuffer)-2, "CONSOLE: name too long");
      scratchBuffer[0] = '%';
      dMemcpy(scratchBuffer + 1, name, len + 1);
      name = scratchBuffer;
   }
   return name;
}

//--------------------------------------
void ConsoleConstructor::init(const char *cName, const char *fName, const char *usg, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   mina = minArgs;
   maxa = maxArgs;
   funcName = fName;
   usage = usg;
   className = cName;
   sc = 0; fc = 0; vc = 0; bc = 0; ic = 0;
   callback = group = false;
   next = first;
   ns = false;
   first = this;
   toolOnly = isToolOnly;
}

void ConsoleConstructor::setup()
{
   for(ConsoleConstructor *walk = first; walk; walk = walk->next)
   {
#ifdef TORQUE_DEBUG
      walk->validate();
#endif

      if(walk->sc)
         Con::addCommand(walk->className, walk->funcName, walk->sc, walk->usage, walk->mina, walk->maxa, walk->toolOnly);
      else if(walk->ic)
         Con::addCommand(walk->className, walk->funcName, walk->ic, walk->usage, walk->mina, walk->maxa, walk->toolOnly);
      else if(walk->fc)
         Con::addCommand(walk->className, walk->funcName, walk->fc, walk->usage, walk->mina, walk->maxa, walk->toolOnly);
      else if(walk->vc)
         Con::addCommand(walk->className, walk->funcName, walk->vc, walk->usage, walk->mina, walk->maxa, walk->toolOnly);
      else if(walk->bc)
         Con::addCommand(walk->className, walk->funcName, walk->bc, walk->usage, walk->mina, walk->maxa, walk->toolOnly);
      else if(walk->group)
         Con::markCommandGroup(walk->className, walk->funcName, walk->usage);
      else if(walk->overload)
         Con::addOverload(walk->className, walk->funcName, walk->usage);
      else if(walk->callback)
         Con::noteScriptCallback(walk->className, walk->funcName, walk->usage);
      else if(walk->ns)
      {
         Namespace* ns = Namespace::find(StringTable->insert(walk->className));
         if( ns )
            ns->mUsage = walk->usage;
      }
      else
      {
         AssertISV(false, "Found a ConsoleConstructor with an indeterminate type!");
      }
   }
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, StringCallback sfunc, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   init(className, funcName, usage, minArgs, maxArgs, isToolOnly);
   sc = sfunc;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, IntCallback ifunc, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   init(className, funcName, usage, minArgs, maxArgs, isToolOnly);
   ic = ifunc;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, FloatCallback ffunc, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   init(className, funcName, usage, minArgs, maxArgs, isToolOnly);
   fc = ffunc;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, VoidCallback vfunc, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   init(className, funcName, usage, minArgs, maxArgs, isToolOnly);
   vc = vfunc;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, BoolCallback bfunc, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   init(className, funcName, usage, minArgs, maxArgs, isToolOnly);
   bc = bfunc;
}

ConsoleConstructor::ConsoleConstructor(const char* className, const char* groupName, const char* aUsage)
{
   init(className, groupName, usage, -1, -2);

   group = true;

   // Somewhere, the entry list is getting flipped, partially.
   // so we have to do tricks to deal with making sure usage
   // is properly populated.

   // This is probably redundant.
   static char * lastUsage = NULL;
   if(aUsage)
      lastUsage = (char *)aUsage;

   usage = lastUsage;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *callbackName, 
                                       const char *usage, bool isRequired)
{
   init(className, callbackName, usage, -2, -3);
   callback = true;
   ns = true;
}

void ConsoleConstructor::validate()
{
#ifdef TORQUE_DEBUG
   // Don't do the following check if we're not a method/func.
   if(this->group)
      return;

   // In debug, walk the list and make sure this isn't a duplicate.
   for(ConsoleConstructor *walk = first; walk; walk = walk->next)
   {
      // Skip mismatching func/method names.
      if(dStricmp(walk->funcName, this->funcName))
         continue;

      // Don't compare functions with methods or vice versa.
      if(bool(this->className) != bool(walk->className))
         continue;

      // Skip mismatching classnames, if they're present.
      if(this->className && walk->className && dStricmp(walk->className, this->className))
         continue;

      // If we encounter ourselves, stop searching; this prevents duplicate
      // firing of the assert, instead only firing for the latter encountered
      // entry.
      if(this == walk)
         break;

      // Match!
      if(this->className)
      {
         AssertISV(false, avar("ConsoleConstructor::setup - ConsoleMethod '%s::%s' collides with another of the same name.", this->className, this->funcName));
      }
      else
      {
         AssertISV(false, avar("ConsoleConstructor::setup - ConsoleFunction '%s' collides with another of the same name.", this->funcName));
      }
   }
#endif
}

// We comment out the implementation of the Con namespace when doxygenizing because
// otherwise Doxygen decides to ignore our docs in console.h
#ifndef DOXYGENIZING

namespace Con
{

static Vector<ConsumerCallback> gConsumers(__FILE__, __LINE__);
static Vector< String > sInstantGroupStack( __FILE__, __LINE__ );
static DataChunker consoleLogChunker;
static Vector<ConsoleLogEntry> consoleLog(__FILE__, __LINE__);
static bool consoleLogLocked;
static bool logBufferEnabled=true;
static S32 printLevel = 10;
static FileStream consoleLogFile;
static const char *defLogFileName = "console.log";
static S32 consoleLogMode = 0;
static bool active = false;
static bool newLogFile;
static const char *logFileName;

static const int MaxCompletionBufferSize = 4096;
static char completionBuffer[MaxCompletionBufferSize];
static char tabBuffer[MaxCompletionBufferSize] = {0};
static SimObjectPtr<SimObject> tabObject;
static U32 completionBaseStart;
static U32 completionBaseLen;

String gInstantGroup;
Con::ConsoleInputEvent smConsoleInput;

/// Current script file name and root, these are registered as
/// console variables.
/// @{

///
StringTableEntry gCurrentFile;
StringTableEntry gCurrentRoot;
/// @}

bool alwaysUseDebugOutput = true;
bool useTimestamp = false;

ConsoleFunctionGroupBegin( Clipboard, "Miscellaneous functions to control the clipboard and clear the console.");

ConsoleFunction( cls, void, 1, 1, "Clear the screen.")
{
   if(consoleLogLocked)
      return;
   consoleLogChunker.freeBlocks();
   consoleLog.setSize(0);
};

ConsoleFunction( getClipboard, const char*, 1, 1, "Get text from the clipboard.")
{
	return Platform::getClipboard();
};

ConsoleFunction( setClipboard, bool, 2, 2, "(string text)"
               "Set the system clipboard.")
{
	return Platform::setClipboard(argv[1]);
};

ConsoleFunctionGroupEnd( Clipboard );


void postConsoleInput( RawData data );

void init()
{
   AssertFatal(active == false, "Con::init should only be called once.");

   // Set up general init values.
   active                        = true;
   logFileName                   = NULL;
   newLogFile                    = true;
   gWarnUndefinedScriptVariables = false;

   // Initialize subsystems.
   Namespace::init();
   ConsoleConstructor::setup();

   // Set up the parser(s)
   CON_ADD_PARSER(CMD,   "cs",   true);   // TorqueScript

   // Variables
   setVariable("Con::prompt", "% ");
   addVariable("Con::logBufferEnabled", TypeBool, &logBufferEnabled);
   addVariable("Con::printLevel", TypeS32, &printLevel);
   addVariable("Con::warnUndefinedVariables", TypeBool, &gWarnUndefinedScriptVariables);
   addVariable( "instantGroup", TypeRealString, &gInstantGroup );

   // Current script file name and root
   Con::addVariable( "Con::File", TypeString, &gCurrentFile );
   Con::addVariable( "Con::Root", TypeString, &gCurrentRoot );

   // alwaysUseDebugOutput determines whether to send output to the platform's 
   // "debug" system.  see winConsole for an example.  
   // in ship builds we don't expose this variable to script
   // and we set it to false by default (don't want to provide more information
   // to potential hackers).  platform code should also ifdef out the code that 
   // pays attention to this in ship builds (see winConsole.cpp) 
   // note that enabling this can slow down your game 
   // if you are running from the debugger and printing a lot of console messages.
#ifndef TORQUE_SHIPPING
   addVariable("pref::Console::alwaysUseDebugOutput", TypeBool, &alwaysUseDebugOutput);
#else
   alwaysUseDebugOutput = false;
#endif

   // controls whether a timestamp is prepended to every console message
   addVariable("$pref::Console::useTimestamp", TypeBool, &useTimestamp);

   // Setup the console types.
   ConsoleBaseType::initialize();

   // And finally, the ACR...
   AbstractClassRep::initialize();

   // Plug us into the journaled console input signal.
   smConsoleInput.notify(postConsoleInput);
}

//--------------------------------------

void shutdown()
{
   AssertFatal(active == true, "Con::shutdown should only be called once.");
   active = false;

   smConsoleInput.remove(postConsoleInput);

   consoleLogFile.close();
   Namespace::shutdown();
   AbstractClassRep::shutdown();
   Compiler::freeConsoleParserList();
}

bool isActive()
{
   return active;
}

bool isMainThread()
{
#ifdef TORQUE_MULTITHREAD
   return ThreadManager::isMainThread();
#else
   // If we're single threaded we're always in the main thread.
   return true;
#endif
}

//--------------------------------------

void getLockLog(ConsoleLogEntry *&log, U32 &size)
{
   consoleLogLocked = true;
   log = consoleLog.address();
   size = consoleLog.size();
}

void unlockLog()
{
   consoleLogLocked = false;
}

U32 tabComplete(char* inputBuffer, U32 cursorPos, U32 maxResultLength, bool forwardTab)
{
   // Check for null input.
   if (!inputBuffer[0]) 
   {
      return cursorPos;
   }

   // Cap the max result length.
   if (maxResultLength > MaxCompletionBufferSize) 
   {
      maxResultLength = MaxCompletionBufferSize;
   }

   // See if this is the same partial text as last checked.
   if (dStrcmp(tabBuffer, inputBuffer)) 
   {
      // If not...
      // Save it for checking next time.
      dStrcpy(tabBuffer, inputBuffer);
      // Scan backward from the cursor position to find the base to complete from.
      S32 p = cursorPos;
      while ((p > 0) && (inputBuffer[p - 1] != ' ') && (inputBuffer[p - 1] != '.') && (inputBuffer[p - 1] != '('))
      {
         p--;
      }
      completionBaseStart = p;
      completionBaseLen = cursorPos - p;
      // Is this function being invoked on an object?
      if (inputBuffer[p - 1] == '.') 
      {
         // If so...
         if (p <= 1) 
         {
            // Bail if no object identifier.
            return cursorPos;
         }

         // Find the object identifier.
         S32 objLast = --p;
         while ((p > 0) && (inputBuffer[p - 1] != ' ') && (inputBuffer[p - 1] != '(')) 
         {
            p--;
         }

         if (objLast == p) 
         {
            // Bail if no object identifier.
            return cursorPos;
         }

         // Look up the object identifier.
         dStrncpy(completionBuffer, inputBuffer + p, objLast - p);
         completionBuffer[objLast - p] = 0;
         tabObject = Sim::findObject(completionBuffer);
         if (tabObject == NULL) 
         {
            // Bail if not found.
            return cursorPos;
         }
      }
      else 
      {
         // Not invoked on an object; we'll use the global namespace.
         tabObject = 0;
      }
   }

   // Chop off the input text at the cursor position.
   inputBuffer[cursorPos] = 0;

   // Try to find a completion in the appropriate namespace.
   const char *newText;

   if (tabObject != 0)
   {
      newText = tabObject->tabComplete(inputBuffer + completionBaseStart, completionBaseLen, forwardTab);
   }
   else 
   {
      // In the global namespace, we can complete on global vars as well as functions.
      if (inputBuffer[completionBaseStart] == '$')
      {
         newText = gEvalState.globalVars.tabComplete(inputBuffer + completionBaseStart, completionBaseLen, forwardTab);
      }
      else 
      {
         newText = Namespace::global()->tabComplete(inputBuffer + completionBaseStart, completionBaseLen, forwardTab);
      }
   }

   if (newText) 
   {
      // If we got something, append it to the input text.
      S32 len = dStrlen(newText);
      if (len + completionBaseStart > maxResultLength)
      {
         len = maxResultLength - completionBaseStart;
      }
      dStrncpy(inputBuffer + completionBaseStart, newText, len);
      inputBuffer[completionBaseStart + len] = 0;
      // And set the cursor after it.
      cursorPos = completionBaseStart + len;
   }

   // Save the modified input buffer for checking next time.
   dStrcpy(tabBuffer, inputBuffer);

   // Return the new (maybe) cursor position.
   return cursorPos;
}

//------------------------------------------------------------------------------
static void log(const char *string)
{
   // Bail if we ain't logging.
   if (!consoleLogMode) 
   {
      return;
   }

   // In mode 1, we open, append, close on each log write.
   if ((consoleLogMode & 0x3) == 1) 
   {
      consoleLogFile.open(defLogFileName, Torque::FS::File::ReadWrite);
   }

   // Write to the log if its status is hunky-dory.
   if ((consoleLogFile.getStatus() == Stream::Ok) || (consoleLogFile.getStatus() == Stream::EOS)) 
   {
      consoleLogFile.setPosition(consoleLogFile.getStreamSize());
      // If this is the first write...
      if (newLogFile) 
      {
         // Make a header.
         Platform::LocalTime lt;
         Platform::getLocalTime(lt);
         char buffer[128];
         dSprintf(buffer, sizeof(buffer), "//-------------------------- %d/%d/%d -- %02d:%02d:%02d -----\r\n",
               lt.month + 1,
               lt.monthday,
               lt.year + 1900,
               lt.hour,
               lt.min,
               lt.sec);
         consoleLogFile.write(dStrlen(buffer), buffer);
         newLogFile = false;
         if (consoleLogMode & 0x4) 
         {
            // Dump anything that has been printed to the console so far.
            consoleLogMode -= 0x4;
            U32 size, line;
            ConsoleLogEntry *log;
            getLockLog(log, size);
            for (line = 0; line < size; line++) 
            {
               consoleLogFile.write(dStrlen(log[line].mString), log[line].mString);
               consoleLogFile.write(2, "\r\n");
            }
            unlockLog();
         }
      }
      // Now write what we came here to write.
      consoleLogFile.write(dStrlen(string), string);
      consoleLogFile.write(2, "\r\n");
   }

   if ((consoleLogMode & 0x3) == 1) 
   {
      consoleLogFile.close();
   }
}

//------------------------------------------------------------------------------

static void _printf(ConsoleLogEntry::Level level, ConsoleLogEntry::Type type, const char* fmt, va_list argptr)
{
   if (!active)
	   return;
   Con::active = false; 

   char buffer[4096];
   U32 offset = 0;
   if(gEvalState.traceOn && gEvalState.stack.size())
   {
      offset = gEvalState.stack.size() * 3;
      for(U32 i = 0; i < offset; i++)
         buffer[i] = ' ';
   }

   if (useTimestamp)
   {
      static U32 startTime = Platform::getRealMilliseconds();
      U32 curTime = Platform::getRealMilliseconds() - startTime;
      offset += dSprintf(buffer + offset, sizeof(buffer) - offset, "[+%4d.%03d]", U32(curTime * 0.001), curTime % 1000);
   }
   dVsprintf(buffer + offset, sizeof(buffer) - offset, fmt, argptr);

   for(S32 i = 0; i < gConsumers.size(); i++)
      gConsumers[i](level, buffer);

   if(logBufferEnabled || consoleLogMode)
   {
      char *pos = buffer;
      while(*pos)
      {
         if(*pos == '\t')
            *pos = '^';
         pos++;
      }
      pos = buffer;

      for(;;)
      {
         char *eofPos = dStrchr(pos, '\n');
         if(eofPos)
            *eofPos = 0;

         log(pos);
         if(logBufferEnabled && !consoleLogLocked)
         {
            ConsoleLogEntry entry;
            entry.mLevel  = level;
            entry.mType   = type;
#ifndef TORQUE_SHIPPING // this is equivalent to a memory leak, turn it off in ship build            
            entry.mString = (const char *)consoleLogChunker.alloc(dStrlen(pos) + 1);
            dStrcpy(const_cast<char*>(entry.mString), pos);
            
            // This prevents infinite recursion if the console itself needs to
            // re-allocate memory to accommodate the new console log entry, and 
            // LOG_PAGE_ALLOCS is defined. It is kind of a dirty hack, but the
            // uses for LOG_PAGE_ALLOCS are limited, and it is not worth writing
            // a lot of special case code to support this situation. -patw
            const bool save = Con::active;
            Con::active = false;
            consoleLog.push_back(entry);
            Con::active = save;
#endif
         }
         if(!eofPos)
            break;
         pos = eofPos + 1;
      }
   }

   Con::active = true;
}

//------------------------------------------------------------------------------
void printf(const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   _printf(ConsoleLogEntry::Normal, ConsoleLogEntry::General, fmt, argptr);
   va_end(argptr);
}

void warnf(ConsoleLogEntry::Type type, const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   _printf(ConsoleLogEntry::Warning, type, fmt, argptr);
   va_end(argptr);
}

void errorf(ConsoleLogEntry::Type type, const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   _printf(ConsoleLogEntry::Error, type, fmt, argptr);
   va_end(argptr);
}

void warnf(const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   _printf(ConsoleLogEntry::Warning, ConsoleLogEntry::General, fmt, argptr);
   va_end(argptr);
}

void errorf(const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   _printf(ConsoleLogEntry::Error, ConsoleLogEntry::General, fmt, argptr);
   va_end(argptr);
}

//---------------------------------------------------------------------------

void setVariable(const char *name, const char *value)
{
   // get the field info from the object..
   if(name[0] != '$' && dStrchr(name, '.') && !isFunction(name))
   {
      S32 len = dStrlen(name);
      AssertFatal(len < sizeof(scratchBuffer)-1, "Sim::getVariable - name too long");
      dMemcpy(scratchBuffer, name, len+1);

      char * token = dStrtok(scratchBuffer, ".");
      SimObject * obj = Sim::findObject(token);
      if(!obj)
         return;

      token = dStrtok(0, ".\0");
      if(!token)
         return;

      while(token != NULL)
      {
         const char * val = obj->getDataField(StringTable->insert(token), 0);
         if(!val)
            return;

         char *fieldToken = token;
         token = dStrtok(0, ".\0");
         if(token)
         {
            obj = Sim::findObject(token);
            if(!obj)
               return;
         }
         else
         {
            obj->setDataField(StringTable->insert(fieldToken), 0, value);
         }
      }
   }

   name = prependDollar(name);
   gEvalState.globalVars.setVariable(StringTable->insert(name), value);
}

void setLocalVariable(const char *name, const char *value)
{
   name = prependPercent(name);
   gEvalState.stack.last()->setVariable(StringTable->insert(name), value);
}

void setBoolVariable(const char *varName, bool value)
{
   setVariable(varName, value ? "1" : "0");
}

void setIntVariable(const char *varName, S32 value)
{
   char scratchBuffer[32];
   dSprintf(scratchBuffer, sizeof(scratchBuffer), "%d", value);
   setVariable(varName, scratchBuffer);
}

void setFloatVariable(const char *varName, F32 value)
{
   char scratchBuffer[32];
   dSprintf(scratchBuffer, sizeof(scratchBuffer), "%g", value);
   setVariable(varName, scratchBuffer);
}

//---------------------------------------------------------------------------
void addConsumer(ConsumerCallback consumer)
{
   gConsumers.push_back(consumer);
}

// dhc - found this empty -- trying what I think is a reasonable impl.
void removeConsumer(ConsumerCallback consumer)
{
   for(S32 i = 0; i < gConsumers.size(); i++)
   {
      if (gConsumers[i] == consumer)
      {
         // remove it from the list.
         gConsumers.erase(i);
         break;
      }
   }
}

void stripColorChars(char* line)
{
   char* c = line;
   char cp = *c;
   while (cp) 
   {
      if (cp < 18) 
      {
         // Could be a color control character; let's take a closer look.
         if ((cp != 8) && (cp != 9) && (cp != 10) && (cp != 13)) 
         {
            // Yep... copy following chars forward over this.
            char* cprime = c;
            char cpp;
            do 
            {
               cpp = *++cprime;
               *(cprime - 1) = cpp;
            } 
            while (cpp);
            // Back up 1 so we'll check this position again post-copy.
            c--;
         }
      }
      cp = *++c;
   }
}

const char *getVariable(const char *name)
{
   // get the field info from the object..
   if(name[0] != '$' && dStrchr(name, '.') && !isFunction(name))
   {
      S32 len = dStrlen(name);
      AssertFatal(len < sizeof(scratchBuffer)-1, "Sim::getVariable - name too long");
      dMemcpy(scratchBuffer, name, len+1);

      char * token = dStrtok(scratchBuffer, ".");
      SimObject * obj = Sim::findObject(token);
      if(!obj)
         return("");

      token = dStrtok(0, ".\0");
      if(!token)
         return("");

      while(token != NULL)
      {
         const char * val = obj->getDataField(StringTable->insert(token), 0);
         if(!val)
            return("");

         token = dStrtok(0, ".\0");
         if(token)
         {
            obj = Sim::findObject(token);
            if(!obj)
               return("");
         }
         else
            return(val);
      }
   }

   name = prependDollar(name);
   return gEvalState.globalVars.getVariable(StringTable->insert(name));
}

const char *getLocalVariable(const char *name)
{
   name = prependPercent(name);

   return gEvalState.stack.last()->getVariable(StringTable->insert(name));
}

bool getBoolVariable(const char *varName, bool def)
{
   const char *value = getVariable(varName);
   return *value ? dAtob(value) : def;
}

S32 getIntVariable(const char *varName, S32 def)
{
   const char *value = getVariable(varName);
   return *value ? dAtoi(value) : def;
}

F32 getFloatVariable(const char *varName, F32 def)
{
   const char *value = getVariable(varName);
   return *value ? dAtof(value) : def;
}

//---------------------------------------------------------------------------

bool addVariable(const char *name, S32 t, void *dp)
{
   gEvalState.globalVars.addVariable(name, t, dp);
   return true;
}

bool removeVariable(const char *name)
{
   name = StringTable->lookup(prependDollar(name));
   return name!=0 && gEvalState.globalVars.removeVariable(name);
}

//---------------------------------------------------------------------------

void addCommand(const char *nsName, const char *name,StringCallback cb, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void addCommand(const char *nsName, const char *name,VoidCallback cb, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void addCommand(const char *nsName, const char *name,IntCallback cb, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void addCommand(const char *nsName, const char *name,FloatCallback cb, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void addCommand(const char *nsName, const char *name,BoolCallback cb, const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void noteScriptCallback(const char *className, const char *funcName, const char *usage)
{
   Namespace *ns = lookupNamespace(className);
   ns->addScriptCallback(StringTable->insert(funcName), usage);
}

void markCommandGroup(const char * nsName, const char *name, const char* usage)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->markGroup(name,usage);
}

void beginCommandGroup(const char * nsName, const char *name, const char* usage)
{
   markCommandGroup(nsName, name, usage);
}

void endCommandGroup(const char * nsName, const char *name)
{
   markCommandGroup(nsName, name, NULL);
}

void addOverload(const char * nsName, const char * name, const char * altUsage)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addOverload(name,altUsage);
}

void addCommand(const char *name,StringCallback cb,const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace::global()->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void addCommand(const char *name,VoidCallback cb,const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace::global()->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void addCommand(const char *name,IntCallback cb,const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace::global()->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void addCommand(const char *name,FloatCallback cb,const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace::global()->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

void addCommand(const char *name,BoolCallback cb,const char *usage, S32 minArgs, S32 maxArgs, bool isToolOnly /* = false */)
{
   Namespace::global()->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs, isToolOnly);
}

const char *evaluate(const char* string, bool echo, const char *fileName)
{
   if (echo)
      Con::printf("%s%s", getVariable( "$Con::Prompt" ), string);

   if(fileName)
      fileName = StringTable->insert(fileName);

   CodeBlock *newCodeBlock = new CodeBlock();
   return newCodeBlock->compileExec(fileName, string, false, fileName ? -1 : 0);
}

//------------------------------------------------------------------------------
const char *evaluatef(const char* string, ...)
{
   char buffer[4096];
   va_list args;
   va_start(args, string);
   dVsprintf(buffer, sizeof(buffer), string, args);
   CodeBlock *newCodeBlock = new CodeBlock();
   return newCodeBlock->compileExec(NULL, buffer, false, 0);
}

const char *execute(S32 argc, const char *argv[])
{
#ifdef TORQUE_MULTITHREAD
   if(isMainThread())
   {
#endif
      Namespace::Entry *ent;
      StringTableEntry funcName = StringTable->insert(argv[0]);
      ent = Namespace::global()->lookup(funcName);

      if(!ent)
      {
         warnf(ConsoleLogEntry::Script, "%s: Unknown command.", argv[0]);

         // Clean up arg buffers, if any.
         STR.clearFunctionOffset();
         return "";
      }
      return ent->execute(argc, argv, &gEvalState);
#ifdef TORQUE_MULTITHREAD
   }
   else
   {
      SimConsoleThreadExecCallback cb;
      SimConsoleThreadExecEvent *evt = new SimConsoleThreadExecEvent(argc, argv, false, &cb);
      Sim::postEvent(Sim::getRootGroup(), evt, Sim::getCurrentTime());
      
      return cb.waitForResult();
   }
#endif
}

//------------------------------------------------------------------------------
const char *execute(SimObject *object, S32 argc, const char *argv[], bool thisCallOnly)
{
   static char idBuf[16];
   if(argc < 2)
      return "";

   // [neo, 10/05/2007 - #3010]
   // Make sure we don't get recursive calls, respect the flag!   
   // Should we be calling handlesMethod() first?
   if( !thisCallOnly )
   {
      ICallMethod *com = dynamic_cast<ICallMethod *>(object);
      if(com)
         com->callMethodArgList(argc, argv, false);
   }

   if(object->getNamespace())
   {
      dSprintf(idBuf, sizeof(idBuf), "%d", object->getId());
      argv[1] = idBuf;

      StringTableEntry funcName = StringTable->insert(argv[0]);
      Namespace::Entry *ent = object->getNamespace()->lookup(funcName);

      if(ent == NULL)
      {
         //warnf(ConsoleLogEntry::Script, "%s: undefined for object '%s' - id %d", funcName, object->getName(), object->getId());

         // Clean up arg buffers, if any.
         STR.clearFunctionOffset();
         return "";
      }

      // Twiddle %this argument
      const char *oldArg1 = argv[1];
      dSprintf(idBuf, sizeof(idBuf), "%d", object->getId());
      argv[1] = idBuf;

      SimObject *save = gEvalState.thisObject;
      gEvalState.thisObject = object;
      const char *ret = ent->execute(argc, argv, &gEvalState);
      gEvalState.thisObject = save;

      // Twiddle it back
      argv[1] = oldArg1;

      return ret;
   }
   warnf(ConsoleLogEntry::Script, "Con::execute - %d has no namespace: %s", object->getId(), argv[0]);
   return "";
}

#define B( a ) const char* a = NULL
#define A const char*
inline const char*_executef(SimObject *obj, S32 checkArgc, S32 argc, 
                            A a, B(b), B(c), B(d), B(e), B(f), B(g), B(h), B(i), B(j), B(k))
{
#undef A
#undef B
   const U32 maxArg = 12;
   AssertWarn(checkArgc == argc, "Incorrect arg count passed to Con::executef(SimObject*)");
   AssertFatal(argc <= maxArg - 1, "Too many args passed to Con::_executef(SimObject*). Please update the function to handle more.");
   const char* argv[maxArg];
   argv[0] = a;
   argv[1] = a;
   argv[2] = b;
   argv[3] = c;
   argv[4] = d;
   argv[5] = e;
   argv[6] = f;
   argv[7] = g;
   argv[8] = h;
   argv[9] = i;
   argv[10] = j;
   argv[11] = k;
   return execute(obj, argc+1, argv);
}

#define A const char*
#define OBJ SimObject* obj
const char *executef(OBJ, A a)                                    { return _executef(obj, 1, 1, a); }
const char *executef(OBJ, A a, A b)                               { return _executef(obj, 2, 2, a, b); }
const char *executef(OBJ, A a, A b, A c)                          { return _executef(obj, 3, 3, a, b, c); }
const char *executef(OBJ, A a, A b, A c, A d)                     { return _executef(obj, 4, 4, a, b, c, d); }
const char *executef(OBJ, A a, A b, A c, A d, A e)                { return _executef(obj, 5, 5, a, b, c, d, e); }
const char *executef(OBJ, A a, A b, A c, A d, A e, A f)           { return _executef(obj, 6, 6, a, b, c, d, e, f); }
const char *executef(OBJ, A a, A b, A c, A d, A e, A f, A g)      { return _executef(obj, 7, 7, a, b, c, d, e, f, g); }
const char *executef(OBJ, A a, A b, A c, A d, A e, A f, A g, A h) { return _executef(obj, 8, 8, a, b, c, d, e, f, g, h); }
const char *executef(OBJ, A a, A b, A c, A d, A e, A f, A g, A h, A i) { return _executef(obj, 9, 9, a, b, c, d, e, f, g, h, i); }
const char *executef(OBJ, A a, A b, A c, A d, A e, A f, A g, A h, A i, A j) { return _executef(obj,10,10, a, b, c, d, e, f, g, h, i, j); }
const char *executef(OBJ, A a, A b, A c, A d, A e, A f, A g, A h, A i, A j, A k) { return _executef(obj,11,11, a, b, c, d, e, f, g, h, i, j, k); }
#undef A

//------------------------------------------------------------------------------
#define B( a ) const char* a = NULL
#define A const char*
inline const char*_executef(S32 checkArgc, S32 argc, A a, B(b), B(c), B(d), B(e), B(f), B(g), B(h), B(i), B(j))
{
#undef A
#undef B
   const U32 maxArg = 10;
   AssertFatal(checkArgc == argc, "Incorrect arg count passed to Con::executef()");
   AssertFatal(argc <= maxArg, "Too many args passed to Con::_executef(). Please update the function to handle more.");
   const char* argv[maxArg];
   argv[0] = a;
   argv[1] = b;
   argv[2] = c;
   argv[3] = d;
   argv[4] = e;
   argv[5] = f;
   argv[6] = g;
   argv[7] = h;
   argv[8] = i;
   argv[9] = j;
   return execute(argc, argv);
}
   
#define A const char*
const char *executef(A a)                                    { return _executef(1, 1, a); }
const char *executef(A a, A b)                               { return _executef(2, 2, a, b); }
const char *executef(A a, A b, A c)                          { return _executef(3, 3, a, b, c); }
const char *executef(A a, A b, A c, A d)                     { return _executef(4, 4, a, b, c, d); }
const char *executef(A a, A b, A c, A d, A e)                { return _executef(5, 5, a, b, c, d, e); }
const char *executef(A a, A b, A c, A d, A e, A f)           { return _executef(6, 6, a, b, c, d, e, f); }
const char *executef(A a, A b, A c, A d, A e, A f, A g)      { return _executef(7, 7, a, b, c, d, e, f, g); }
const char *executef(A a, A b, A c, A d, A e, A f, A g, A h) { return _executef(8, 8, a, b, c, d, e, f, g, h); }
const char *executef(A a, A b, A c, A d, A e, A f, A g, A h, A i) { return _executef(9, 9, a, b, c, d, e, f, g, h, i); }
const char *executef(A a, A b, A c, A d, A e, A f, A g, A h, A i, A j) { return _executef(10,10,a, b, c, d, e, f, g, h, i, j); }
#undef A


//------------------------------------------------------------------------------
bool isFunction(const char *fn)
{
   const char *string = StringTable->lookup(fn);
   if(!string)
      return false;
   else
      return Namespace::global()->lookup(string) != NULL;
}

//------------------------------------------------------------------------------

void setLogMode(S32 newMode)
{
   if ((newMode & 0x3) != (consoleLogMode & 0x3)) {
      if (newMode && !consoleLogMode) {
         // Enabling logging when it was previously disabled.
         newLogFile = true;
      }
      if ((consoleLogMode & 0x3) == 2) {
         // Changing away from mode 2, must close logfile.
         consoleLogFile.close();
      }
      else if ((newMode & 0x3) == 2) {
#ifdef _XBOX
         // Xbox is not going to support logging to a file. Use the OutputDebugStr
         // log consumer
         Platform::debugBreak();
#endif
         // Starting mode 2, must open logfile.
         consoleLogFile.open(defLogFileName, Torque::FS::File::Write);
      }
      consoleLogMode = newMode;
   }
}

Namespace *lookupNamespace(const char *ns)
{
   if(!ns)
      return Namespace::global();
   return Namespace::find(StringTable->insert(ns));
}

bool linkNamespaces(const char *parent, const char *child)
{
   Namespace *pns = lookupNamespace(parent);
   Namespace *cns = lookupNamespace(child);
   if(pns && cns)
      return cns->classLinkTo(pns);
   return false;
}

bool unlinkNamespaces(const char *parent, const char *child)
{
   Namespace *pns = lookupNamespace(parent);
   Namespace *cns = lookupNamespace(child);

   if(pns == cns)
   {
      Con::warnf("Con::unlinkNamespaces - trying to unlink '%s' from itself, aborting.", parent);
      return false;
   }

   if(pns && cns)
      return cns->unlinkClass(pns);

   return false;
}

bool classLinkNamespaces(Namespace *parent, Namespace *child)
{
   if(parent && child)
      return child->classLinkTo(parent);
   return false;
}

void setData(S32 type, void *dptr, S32 index, S32 argc, const char **argv, const EnumTable *tbl, BitSet32 flag)
{
   ConsoleBaseType *cbt = ConsoleBaseType::getType(type);
   AssertFatal(cbt, "Con::setData - could not resolve type ID!");
   cbt->setData((void *) (((const char *)dptr) + index * cbt->getTypeSize()),argc, argv, tbl, flag);
}

const char *getData(S32 type, void *dptr, S32 index, const EnumTable *tbl, BitSet32 flag)
{
   ConsoleBaseType *cbt = ConsoleBaseType::getType(type);
   AssertFatal(cbt, "Con::getData - could not resolve type ID!");
   return cbt->getData((void *) (((const char *)dptr) + index * cbt->getTypeSize()), tbl, flag);
}

const char *getFormattedData(S32 type, const char *data, const EnumTable *tbl, BitSet32 flag)
{
   ConsoleBaseType *cbt = ConsoleBaseType::getType( type );
   AssertFatal(cbt, "Con::getData - could not resolve type ID!");

   // Datablock types are just a datablock 
   // name and don't ever need formatting.
   if ( cbt->isDatablock() )
      return data;

   bool currWarn = gWarnUndefinedScriptVariables;
   gWarnUndefinedScriptVariables = false;

   const char* globalValue = Con::getVariable(data);

   gWarnUndefinedScriptVariables = currWarn;

   if (dStrlen(globalValue) > 0)
      return globalValue;

   void* variable = cbt->getNativeVariable();

   if (variable)
   {
      Con::setData(type, variable, 0, 1, &data, tbl, flag);
      const char* formattedVal = Con::getData(type, variable, 0, tbl, flag);

      char* returnBuffer = Con::getReturnBuffer(2048);
      dSprintf(returnBuffer, 2048, "%s\0", formattedVal );

      cbt->deleteNativeVariable(variable);

      return returnBuffer;
   }
   else
      return data;
}

//------------------------------------------------------------------------------

bool isCurrentScriptToolScript()
{
   // With a player build we ALWAYS return false
#ifndef TORQUE_TOOLS
   return false;
#else
   const StringTableEntry cbFullPath = CodeBlock::getCurrentCodeBlockFullPath();
   if(cbFullPath == NULL)
      return false;
   const StringTableEntry exePath = Platform::getMainDotCsDir();

   return dStrnicmp(exePath, cbFullPath, dStrlen(exePath)) == 0;
#endif
}

//------------------------------------------------------------------------------

StringTableEntry getModNameFromPath(const char *path)
{
   if(path == NULL || *path == 0)
      return NULL;

   char buf[1024];
   buf[0] = 0;

   if(path[0] == '/' || path[1] == ':')
   {
      // It's an absolute path
      const StringTableEntry exePath = Platform::getMainDotCsDir();
      U32 len = dStrlen(exePath);
      if(dStrnicmp(exePath, path, len) == 0)
      {
         const char *ptr = path + len + 1;
         const char *slash = dStrchr(ptr, '/');
         if(slash)
         {
            dStrncpy(buf, ptr, slash - ptr);
            buf[slash - ptr] = 0;
         }
         else
            return NULL;
      }
      else
         return NULL;
   }
   else
   {
      const char *slash = dStrchr(path, '/');
      if(slash)
      {
         dStrncpy(buf, path, slash - path);
         buf[slash - path] = 0;
      }
      else
         return NULL;
   }

   return StringTable->insert(buf);
}

void postConsoleInput( RawData data )
{
   // Schedule this to happen at the next time event.
   char *argv[2];
   argv[0] = "eval";
   argv[1] = ( char* ) data.data;
   Sim::postCurrentEvent(Sim::getRootGroup(), new SimConsoleEvent(2, const_cast<const char**>(argv), false));
}

//------------------------------------------------------------------------------

void pushInstantGroup( String name )
{
   sInstantGroupStack.push_back( gInstantGroup );
   gInstantGroup = name;
}

void popInstantGroup()
{
   if( sInstantGroupStack.empty() )
      gInstantGroup = String::EmptyString;
   else
   {
      gInstantGroup = sInstantGroupStack.last();
      sInstantGroupStack.pop_back();
   }
}

} // end of Console namespace

EnumTable::EnumTable(S32 sSize, const Enums *sTable, S32 sFirstFlag)
{
   size = sSize;
   table = sTable;
   if (sFirstFlag == -1)
   {
      firstFlag = size;
      mask = -1;
   }
   else
   {
      U32 max = 0;
      S32 i = 0;
      for (i=0; i< size; i++)
      {
         if (table[i].index == sFirstFlag)
            break;
         if (table[i].index > max)
            max = table[i].index;
      }
      firstFlag = i;
      mask = getNextPow2(max) - 1;
   }
}

#endif
