//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _CONSOLE_CONSOLECALLBACK_H_
#define _CONSOLE_CONSOLECALLBACK_H_

#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/frameAllocator.h"
#include "core/stringTable.h"

/// Matching implement for DECLARE_CONSOLE_CALLBACK.
#define IMPLEMENT_CONSOLE_CALLBACK(theClass, theType, name, args, argNames, requiredFlag, usageString) \
   theType theClass::name args \
{ \
   ScriptCallbackHelper cbh; \
   cbh.setCallback(#name, this); \
   cbh.storeArgs argNames; \
   cbh.issueCallback(); \
   theType res; dMemset(&res, 0, sizeof(res)); castConsoleTypeFromString(res, cbh._result); \
   return res; \
} \
   ConsoleConstructor cs_##theClass##_##retType##_##name(#theClass, #name, #theType " " #name #args " - " usageString, requiredFlag);

/// Helper class to interface with the console for script callbacks.
///
/// @see IMPLEMENT_CONSOLE_CALLBACK, DECLARE_CONSOLE_CALLBACK
struct ScriptCallbackHelper
{
   StringTableEntry _cbName;
   SimObject *_self;
   S32 _argc; 

   /// Matches up to storeArgs.
   const static S32 csmMaxArguments=10;

   const char *_argv[csmMaxArguments+2];
   const char *_result;
   FrameAllocatorMarker _famAlloc;

   ScriptCallbackHelper()
   {
      _argc = 0;
   }

   ~ScriptCallbackHelper()
   {

   }

   inline void setCallback(const char * name, SimObject *obj)
   {
      _cbName = StringTable->insert(name);
      _self = obj;
      _argc = _self != 0 ? 2 : 1;
      _argv[0] = name;
   }

   template<class T> void processArg(T &arg)
   {
      // Convert to string and add to list of args.
      const char *tmpRes = castConsoleTypeToString(arg);
      char *tmp = (char *)_famAlloc.alloc(dStrlen(tmpRes) + 1);
      dStrcpy(tmp, tmpRes);
      _argv[_argc++] = tmp;
   }

   void issueCallback()
   {
      if(_self)
         _result = Con::execute(_self, _argc, _argv);
      else
         _result = Con::execute(_argc, _argv);
   }

   template<class A> void storeArgs(A &a) { processArg(a); }
   template<class A, class B> void storeArgs(A &a, B &b) { processArg(a); processArg(b); }

   template<class A, class B, class C> 
      void storeArgs(A &a, B &b, C &c) 
   { processArg(a); processArg(b); processArg(c); }

   template<class A, class B, class C, class D>
      void storeArgs(A &a, B &b, C &c, D &d) 
   { processArg(a); processArg(b); processArg(c); processArg(d); }

   template<class A, class B, class C, class D, class E>
      void storeArgs(A &a, B &b, C &c, D &d, E&e) 
   { processArg(a); processArg(b); processArg(c); processArg(d); processArg(e); }

   template<class A, class B, class C, class D, class E, class F>
      void storeArgs(A &a, B &b, C &c, D &d, E&e, F&f) 
   { processArg(a); processArg(b); processArg(c); processArg(d); processArg(e); processArg(f); }

   template<class A, class B, class C, class D, class E, class F, class G>
      void storeArgs(A &a, B &b, C &c, D &d, E&e, F&f, G&g)
   { processArg(a); processArg(b); processArg(c); processArg(d); processArg(e); processArg(f); }

   template<class A, class B, class C, class D, class E, class F, class G, class H>
      void storeArgs(A &a, B &b, C &c, D &d, E&e, F&f, G&g, H&h) 
   { processArg(a); processArg(b); processArg(c); processArg(d); processArg(e); processArg(f); processArg(g); }

   template<class A, class B, class C, class D, class E, class F, class G, class H, class I>
      void storeArgs(A &a, B &b, C &c, D &d, E&e, F&f, G&g, H&h, I&i)
   { processArg(a); processArg(b); processArg(c); processArg(d); processArg(e); processArg(f); processArg(g); processArg(h); }

   template<class A, class B, class C, class D, class E, class F, class G, class H, class I, class J>
      void storeArgs(A &a, B &b, C &c, D &d, E&e, F&f, G&g, H&h, I&i, J&j)
   { processArg(a); processArg(b); processArg(c); processArg(d); processArg(e); processArg(f); processArg(g); processArg(h); processArg(i); }

};


#endif