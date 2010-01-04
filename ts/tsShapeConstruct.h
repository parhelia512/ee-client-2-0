//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TSSHAPECONSTRUCT_H_
#define _TSSHAPECONSTRUCT_H_

#ifndef __RESOURCE_H__
#include "core/resource.h"
#endif
#ifndef _TSSHAPE_H_
#include "ts/tsShape.h"
#endif
#ifndef _SIMBASE_H_
#include "console/simBase.h"
#endif
#ifndef _COLLADA_UTILS_H_
#include "ts/collada/colladaUtils.h"
#endif

/// This class allows an artist to export their animations for the model
/// into the .dsq format.  This class in particular matches up the model
/// with the .dsqs to create a nice animated model.
class TSShapeConstructor : public SimObject
{
   typedef SimObject Parent;

public:
   struct ChangeSet
   {
      enum eCommandType
      {
         CmdAddNode,
         CmdRemoveNode,
         CmdRenameNode,
         CmdSetNodeTransform,
         CmdSetNodeParent,

         CmdAddSequence,
         CmdRemoveSequence,
         CmdRenameSequence,
         CmdSetSequenceCyclic,
         CmdSetSequenceBlend,
         CmdSetSequencePriority,
         CmdSetSequenceGroundSpeed,

         CmdAddTrigger,
         CmdRemoveTrigger,

         CmdInvalid
      };

      struct Command
      {
         eCommandType      type;       // Command type
         StringTableEntry  name;       // Command name
         String            argv[8];    // Command arguments
         S32               argc;       // Number of arguments
         Command() : type(CmdInvalid), name(0), argc(0) { }
      };

      Vector<Command>   mCommands;

      eCommandType getCmdType(const char* name);
      void clear() { mCommands.clear(); }
      bool empty() { return mCommands.empty(); }

      void add(const char* cmdName, S32 argc, const char* argv[]);
      static void optimize(const ChangeSet& input, ChangeSet& output);
      void write(Stream& stream);
   };

   // This structure stores information about the sequence that is not retained
   // by TSShape.
   struct SequenceData
   {
      String from;         // The source sequence (ie. a DSQ file)
      S32 start;           // The first frame in the source sequence
      S32 end;             // The last frame in the source sequence
      S32 total;           // The total number of frames in the source sequence
      String blendSeq;     // The blend reference sequence
      S32 blendFrame;      // The blend reference frame
      SequenceData() : from("\t"), start(0), end(-1), blendFrame(0) { }
   };

   static const int MaxLegacySequences = 127;

protected:
   FileName          mShapePath;
   Vector<FileName>  mSequences;
   ChangeSet         mChangeSet;

   static bool addSequenceFromField( void* obj, const char* data );
   
   static void       _onTSShapeLoaded( Resource< TSShape >& shape );
   static void       _onTSShapeUnloaded( const Torque::Path& path, TSShape* shape );
   
   static ResourceRegisterPostLoadSignal< TSShape > _smAutoLoad;
   static ResourceRegisterUnloadSignal< TSShape > _smAutoUnload;
   
   virtual void      _onLoad( TSShape* shape );
   virtual void      _onUnload();

public:

   TSShape*                mShape;        // Edited shape; NULL while not loaded; not a Resource<TSShape> as we don't want it to prevent from unloading.
   Vector<SequenceData>    mSeqData;
   ColladaUtils::ImportOptions   mOptions;

public:

   TSShapeConstructor();
   ~TSShapeConstructor();

   DECLARE_CONOBJECT(TSShapeConstructor);
   static void initPersistFields();
   static TSShapeConstructor* findShapeConstructor(const FileName& path);

   bool onAdd();

   void onScriptChanged(const Torque::Path& path);

   bool writeField(StringTableEntry fieldname, const char *value);
   void write(Stream& stream, U32 tabStop, U32 flags);

   void addToChangeSet(const char* name, S32 argc, const char* argv[])
   {
      mChangeSet.add(name, argc, argv);
   }

   void popFromChangeSet(S32 count)
   {
      mChangeSet.mCommands.decrement(count);
   }

   TSShape* getShape() const { return mShape; }
};

#endif
