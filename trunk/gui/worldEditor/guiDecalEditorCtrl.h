//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIDECALEDITORCTRL_H_
#define _GUIDECALEDITORCTRL_H_

#ifndef _EDITTSCTRL_H_
#include "gui/worldEditor/editTSCtrl.h"
#endif
#ifndef _UNDO_H_
#include "util/undo.h"
#endif
#ifndef _DECALINSTANCE_H_
#include "T3D/decal/decalInstance.h"
#endif

class GameBase;
class Gizmo;
struct RayInfo;
class DecalInstance;
class DecalData;

class GuiDecalEditorCtrl : public EditTSCtrl
{
   typedef EditTSCtrl Parent;

   public:

      GuiDecalEditorCtrl();
      ~GuiDecalEditorCtrl();

      DECLARE_CONOBJECT(GuiDecalEditorCtrl);

      // SimObject
      bool onAdd();
      static void initPersistFields();      
      void onEditorDisable();

      // GuiControl
      virtual bool onWake();
      virtual void onSleep();
      virtual void onRender(Point2I offset, const RectI &updateRect);

      // EditTSCtrl      
      void get3DCursor( GuiCursor *&cursor, bool &visible, const Gui3DMouseEvent &event_ );
      void on3DMouseDown(const Gui3DMouseEvent & event);
      void on3DMouseUp(const Gui3DMouseEvent & event);
      void on3DMouseMove(const Gui3DMouseEvent & event);
      void on3DMouseDragged(const Gui3DMouseEvent & event);
      void on3DMouseEnter(const Gui3DMouseEvent & event);
      void on3DMouseLeave(const Gui3DMouseEvent & event);
      void on3DRightMouseDown(const Gui3DMouseEvent & event);
      void on3DRightMouseUp(const Gui3DMouseEvent & event);
      void updateGuiInfo();      
      void renderScene(const RectI & updateRect);

      /// Find clicked point on "static collision" objects.
      bool getRayInfo( const Gui3DMouseEvent &event, RayInfo *rInfo );
      
      void selectDecal( DecalInstance *inst );
      void deleteSelectedDecal();
		void deleteDecalDatablock( String lookupName );
		void setMode( String mode, bool sourceShortcut );
		
		void forceRedraw( DecalInstance * decalInstance );
		void setGizmoFocus( DecalInstance * decalInstance );
	public:
		String mMode;
		DecalInstance *mSELDecal;
		DecalInstance *mHLDecal;

   protected:
     
      DecalData *mCurrentDecalData;

      Vector<Point3F> mSELEdgeVerts;
      Vector<Point3F> mHLEdgeVerts;

      void _renderDecalEdge( const Vector<Point3F> &verts, const ColorI &color );
};

//Decal Instance Create Undo Actions
class DICreateUndoAction : public UndoAction
{
   typedef UndoAction Parent;

public:
	GuiDecalEditorCtrl *mEditor;	

protected:

      /// The captured object state.
      DecalInstance mDecalInstance;
		S32 mDatablockId;

public:

   DECLARE_CONOBJECT( DICreateUndoAction );
   static void initPersistFields();
   
   DICreateUndoAction( const UTF8* actionName = "Create Decal " );
   virtual ~DICreateUndoAction();

   void addDecal( DecalInstance decal );

   // UndoAction
   virtual void undo();
   virtual void redo();
};

//Decal Instance Delete Undo Actions
class DIDeleteUndoAction : public UndoAction
{
   typedef UndoAction Parent;

public:
	GuiDecalEditorCtrl *mEditor;	

protected:

   /// The captured object state.
   DecalInstance mDecalInstance;
	S32 mDatablockId;

public:

   DECLARE_CONOBJECT( DIDeleteUndoAction );
   static void initPersistFields();
   
   DIDeleteUndoAction( const UTF8* actionName = "Delete Decal" );
   virtual ~DIDeleteUndoAction();

   ///
   void deleteDecal( DecalInstance decal );

   // UndoAction
   virtual void undo();
   virtual void redo();
};

//Decal Datablock Delete Undo Actions
class DBDeleteUndoAction : public UndoAction
{
   typedef UndoAction Parent;

public:
	GuiDecalEditorCtrl *mEditor;	
	S32 mDatablockId;

protected:
	
	// The captured decalInstance states
	Vector<DecalInstance> mDecalInstanceVec;
	
public:

   DECLARE_CONOBJECT( DBDeleteUndoAction );
   static void initPersistFields();
   
   DBDeleteUndoAction( const UTF8* actionName = "Delete Decal Datablock" );
   virtual ~DBDeleteUndoAction();

   void deleteDecal( DecalInstance decal );

   // UndoAction
   virtual void undo();
   virtual void redo();
};

#endif // _GUIDECALEDITORCTRL_H_



