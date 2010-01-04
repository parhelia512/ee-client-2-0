//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include "core/stream/fileStream.h"
#include "core/util/journal/journal.h"
#include "core/util/safeDelete.h"
#include "console/console.h"

//-----------------------------------------------------------------------------

Journal::FuncDecl* Journal::_FunctionList;
Stream *Journal::mFile;
Journal::Mode Journal::_State;
U32 Journal::_Count;
bool Journal::_Dispatching = false;

Journal Journal::smInstance;

//-----------------------------------------------------------------------------
Journal::~Journal()
{
   if( mFile )
      Stop();
}

//-----------------------------------------------------------------------------

Journal::Functor* Journal::_create(Id id)
{
   for (FuncDecl* ptr = _FunctionList; ptr; ptr = ptr->next)
      if (ptr->id == id)
         return ptr->create();
   return 0;
}

Journal::Id Journal::_getFunctionId(VoidPtr ptr,VoidMethod method)
{
   for (FuncDecl* itr = _FunctionList; itr; itr = itr->next)
      if (itr->match(ptr,method))
         return itr->id;
   return 0;
}

void Journal::_removeFunctionId(VoidPtr ptr,VoidMethod method)
{
   FuncDecl ** itr = &_FunctionList;

   do 
   {
      if((*itr)->match(ptr, method))
      {
         // Unlink and break.
         FuncDecl* decl = *itr;
         idPool().free( decl->id );
         *itr = (*itr)->next;
         delete decl;
         return;
      }

      // Advance to next...
      itr = &((*itr)->next);
   }
   while(*itr);
}

void Journal::_start()
{
}

void Journal::_finish()
{
   if (_State == PlayState)
      --_Count;
   else {
      U32 pos = mFile->getPosition();
      mFile->setPosition(0);
      mFile->write(++_Count);
      mFile->setPosition(pos);
   }
}

void Journal::Record(const char * file)
{
   if (_State == StopState)
   {
      _Count = 0;
      mFile = new FileStream();

      if( ((FileStream*)mFile)->open(file, Torque::FS::File::Write) )
      {
         mFile->write(_Count);
         _State = RecordState;
      }
      else
      {
         AssertWarn(false,"Journal: Could not create journal file");
         Con::errorf("Journal: Could not create journal file '%s'", file);
      }
   }
}

void Journal::Play(const char * file)
{
   if (_State == StopState)
   {
      SAFE_DELETE(mFile);
      mFile = new FileStream();
      if( ((FileStream*)mFile)->open(file, Torque::FS::File::Read) )
      {
         mFile->read(&_Count);
         _State = PlayState;
      }
      else
      {
         AssertWarn(false,"Journal: Could not open journal file");
         Con::errorf("Journal: Could not open journal file '%s'", file);
      }
   }
}

void Journal::Stop()
{
   AssertFatal(mFile, "Journal::Stop - no file stream open!");

   SAFE_DELETE( mFile );
   _State = StopState;
}

bool Journal::PlayNext()
{
   if (_State == PlayState) {
      _start();
      Id id;

      mFile->read(&id);

      Functor* jrn = _create(id);
      AssertFatal(jrn,"Journal: Undefined function found in journal");
      jrn->read(mFile);
      _finish();

      _Dispatching = true;
      jrn->dispatch();
      _Dispatching = false;

      delete jrn;
      if (_Count)
         return true;
      Stop();

      //debugBreak();
   }
   return false;
}