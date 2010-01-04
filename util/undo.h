//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _UNDO_H_
#define _UNDO_H_

#include "console/simBase.h"
#include "core/util/tVector.h"

class UndoManager;

///
class UndoAction : public SimObject
{
   friend class UndoManager;

protected:
   // The manager this was added to.
   UndoManager* mUndoManager;

public:

   /// A brief description of the action, for display in menus and the like.
   // not private because we're exposing it to the console.
   String mActionName;

   // Required in all ConsoleObject subclasses.
   typedef SimObject Parent;
   DECLARE_CONOBJECT(UndoAction);
   static void initPersistFields();
   
   /// Create a new action, asigning it a name for display in menus et cetera.
   UndoAction(const UTF8 *actionName = " ");
   virtual ~UndoAction();

   /// Implement these methods to perform your specific undo & redo tasks. 
   virtual void undo() { };
   virtual void redo() { };
   
   /// Adds the action to the undo stack of the default UndoManager, or the provided manager.
   void addToManager(UndoManager* theMan = NULL);
};

///
class UndoManager : public SimObject
{
private:
   /// Default number of undo & redo levels.
   const static U32 kDefaultNumLevels = 100;

   /// The stacks of undo & redo actions. They will be capped at size mNumLevels.
   Vector<UndoAction*> mUndoStack;
   Vector<UndoAction*> mRedoStack;
   
   /// Deletes all the UndoActions in a stack, then clears it.
   void clearStack(Vector<UndoAction*> &stack);
   /// Clamps a Vector to mNumLevels entries.
   void clampStack(Vector<UndoAction*> &stack);

   /// Run the removal logic on the action.
   void doRemove( UndoAction* action, bool noDelete );
   
public:
   /// Number of undo & redo levels.
   // not private because we're exposing it to the console.
   U32 mNumLevels;

   // Required in all ConsoleObject subclasses.
   typedef SimObject Parent;
   DECLARE_CONOBJECT(UndoManager);
   static void initPersistFields();

   /// Constructor. If levels = 0, we use the default number of undo levels.
   UndoManager(U32 levels = kDefaultNumLevels);
   /// Destructor. deletes and clears the undo & redo stacks.
   ~UndoManager();
   /// Accessor to the default undo manager singleton. Creates one if needed.
   static UndoManager& getDefaultManager();
   
   /// Undo last action, and put it on the redo stack.
   void undo();
   /// Redo the last action, and put it on the undo stack.
   void redo();
   
   /// Clears the undo and redo stacks.
   void clearAll();

   /// Returns the printable name of the top actions on the undo & redo stacks.
   const char* getNextUndoName();
   const char* getNextRedoName();

   S32 getUndoCount();
   S32 getRedoCount();

   const char* getUndoName(S32 index);
   const char* getRedoName(S32 index);

   UndoAction* getUndoAction(S32 index);
   UndoAction* getRedoAction(S32 index);

   /// Add an action to the top of the undo stack, and clear the redo stack.
   void addAction(UndoAction* action);
   void removeAction(UndoAction* action, bool noDelete = false);
};


/// Script Undo Action Creation
/// 
/// Undo actions can be created in script like this:
/// 
/// ...
/// %undo = new UndoScriptAction() { class = SampleUndo; actionName = "Sample Undo"; };
/// %undo.addToManager(UndoManager);
/// ...
/// 
/// function SampleUndo::undo()
/// {
///    ...
/// }
/// 
/// function SampleUndo::redo()
/// {
///    ...
/// }
/// 
class UndoScriptAction : public UndoAction
{
public:
   typedef UndoAction Parent;

   UndoScriptAction() : UndoAction()
   {
      mNSLinkMask = LinkSuperClassName | LinkClassName;
   };

   virtual void undo() { Con::executef(this, "undo"); };
   virtual void redo() { Con::executef(this, "redo"); }

   virtual bool onAdd()
   {
      // Let Parent Do Work.
      if(!Parent::onAdd())
         return false;


      // Notify Script.
      if(isMethod("onAdd"))
         Con::executef(this, "onAdd");

      // Return Success.
      return true;
   };

   virtual void onRemove()
   {
      if (mUndoManager)
         mUndoManager->removeAction((UndoAction*)this, true);

      // notify script
      if(isMethod("onRemove"))
         Con::executef(this, "onRemove");

      Parent::onRemove();
   }

   DECLARE_CONOBJECT(UndoScriptAction);
};

#endif // _UNDO_H_
