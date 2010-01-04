//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef TORQUE_TGB_ONLY

#include "guiDecalEditorCtrl.h"
#include "platform/platform.h"

#include "console/consoleTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "collision/collision.h"
#include "math/util/frustum.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/gfxTextureHandle.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxDrawUtil.h"
#include "gui/core/guiCanvas.h"
#include "gui/buttons/guiButtonCtrl.h"
#include "gui/worldEditor/gizmo.h"
#include "T3D/decal/decalManager.h"
#include "T3D/decal/decalInstance.h"
#include "gui/worldEditor/undoActions.h"

IMPLEMENT_CONOBJECT(GuiDecalEditorCtrl);

GuiDecalEditorCtrl::GuiDecalEditorCtrl()
{   
   mSELDecal = NULL;
   mHLDecal = NULL;
   mCurrentDecalData = NULL;
	mMode = "AddDecalMode";
}

GuiDecalEditorCtrl::~GuiDecalEditorCtrl()
{
   // nothing to do
}

bool GuiDecalEditorCtrl::onAdd()
{
   if( !Parent::onAdd() )
      return false;

   return true;
}

void GuiDecalEditorCtrl::initPersistFields()
{
   addField( "currentDecalData", TypeSimObjectPtr, Offset( mCurrentDecalData, GuiDecalEditorCtrl ) );

   Parent::initPersistFields();
}

void GuiDecalEditorCtrl::onEditorDisable()
{
   // Tools are not deleted/recreated between missions, but decals instances
   // ARE. So we must release any references.
   mSELDecal = NULL;
   mHLDecal = NULL;
}

bool GuiDecalEditorCtrl::onWake()
{
   if ( !Parent::onWake() )
      return false;
	
	

   return true;
}

void GuiDecalEditorCtrl::onSleep()
{
   Parent::onSleep();   
}

void GuiDecalEditorCtrl::get3DCursor( GuiCursor *&cursor, 
                                       bool &visible, 
                                       const Gui3DMouseEvent &event_ )
{
   cursor = NULL;
   visible = false;

   GuiCanvas *root = getRoot();
   if ( !root )
      return;

   S32 currCursor = PlatformCursorController::curArrow;

   if ( root->mCursorChanged == currCursor )
      return;

   PlatformWindow *window = root->getPlatformWindow();
   PlatformCursorController *controller = window->getCursorController();
   
   // We've already changed the cursor, 
   // so set it back before we change it again.
   if( root->mCursorChanged != -1)
      controller->popCursor();

   // Now change the cursor shape
   controller->pushCursor(currCursor);
   root->mCursorChanged = currCursor;   
}

void GuiDecalEditorCtrl::on3DMouseDown(const Gui3DMouseEvent & event)
{
   if ( !isFirstResponder() )
      setFirstResponder();
	
	bool dblClick = ( event.mouseClickCount > 1 );

	// Gather selected decal information 
   RayInfo ri;
   bool hit = getRayInfo( event, &ri );

   Point3F start = event.pos;
   Point3F end = start + event.vec * 300.0f; // use visible distance here??

   DecalInstance *pDecal = gDecalManager->raycast( start, end );
	
	if( mMode.compare("AddDecalMode") != 0 )
	{
		if ( mSELDecal )
		{
			// If our click hit the gizmo we are done.
			mGizmo->on3DMouseDown( event );
			if ( mGizmo->getSelection() != Gizmo::None )
			{
				char returnBuffer[256];
				dSprintf(returnBuffer, sizeof(returnBuffer), "%f %f %f %f %f %f %f", 
				mSELDecal->mPosition.x, mSELDecal->mPosition.y, mSELDecal->mPosition.z, 
				mSELDecal->mTangent.x, mSELDecal->mTangent.y, mSELDecal->mTangent.z,
				mSELDecal->mSize);

				Con::executef( this, "prepGizmoTransform", Con::getIntArg(mSELDecal->mId), returnBuffer );

				return;
			}
		}

		if ( mHLDecal && pDecal == mHLDecal )
		{
			mHLDecal = NULL;            
			selectDecal( pDecal );   

			if ( isMethod( "onSelectInstance" ) )
			{
				char idBuf[512];
				dSprintf(idBuf, 512, "%i", pDecal->mId);
				Con::executef( this, "onSelectInstance", String(idBuf).c_str(), pDecal->mDataBlock->lookupName.c_str() );
			}

			return;
		}
		else if ( hit && !pDecal)
		{
			if ( dblClick )
				setMode( String("AddDecalMode"), true );

			return;
		}
	}
	else
	{
		// If we accidently hit a decal, then bail(probably an accident). If the use hits the decal twice,
		// then boot them into selection mode and select the decal.
		if ( mHLDecal && pDecal == mHLDecal )
		{
			if ( dblClick )
			{
				mHLDecal = NULL;            
				selectDecal( pDecal );   

				if ( isMethod( "onSelectInstance" ) )
				{
					char idBuf[512];
					dSprintf(idBuf, 512, "%i", pDecal->mId);
					Con::executef( this, "onSelectInstance", String(idBuf).c_str(), pDecal->mDataBlock->lookupName.c_str() );
				}
				setMode( String("SelectDecalMode"), true );
			}
			return;	
		}

		if ( hit && mCurrentDecalData ) // Create a new decal...
		{
			U8 flags = PermanentDecal | SaveDecal;

			DecalInstance *decalInst = gDecalManager->addDecal( ri.point, ri.normal, 0.0f, mCurrentDecalData, 1.0f, -1, flags );      
	      
			if ( decalInst )  
			{
				// Give the decal an id
				decalInst->mId = gDecalManager->mDecalInstanceVec.size();
				gDecalManager->mDecalInstanceVec.push_back(decalInst);

				selectDecal( decalInst );
				
				// Grab the mission editor undo manager.
				UndoManager *undoMan = NULL;
				if ( !Sim::findObject( "EUndoManager", undoMan ) )
				{
					Con::errorf( "GuiMeshRoadEditorCtrl::on3DMouseDown() - EUndoManager not found!" );
					return;           
				}

				// Create the UndoAction.
				DICreateUndoAction *action = new DICreateUndoAction("Create Decal");
				action->addDecal( *decalInst );
				
				action->mEditor = this;
				// Submit it.               
				undoMan->addAction( action );

				if ( isMethod( "onCreateInstance" ) )
				{
					char buffer[512];
					dSprintf(buffer, 512, "%i", decalInst->mId);
					Con::executef( this, "onCreateInstance", buffer, decalInst->mDataBlock->lookupName.c_str());
				}
			}

			return;
		}
	}

   if ( !mSELDecal )
      return;
}

void GuiDecalEditorCtrl::on3DRightMouseDown(const Gui3DMouseEvent & event)
{
   //mIsPanning = true;
   //mGizmo->on3DRightMouseDown( event );
}

void GuiDecalEditorCtrl::on3DRightMouseUp(const Gui3DMouseEvent & event)
{
   //mIsPanning = false;
//   mGizmo->on3DRightMouseUp( event );
}

void GuiDecalEditorCtrl::on3DMouseUp(const Gui3DMouseEvent & event)
{
   if ( mSELDecal )
	{
		if ( mGizmo->isDirty() )
		{
			char returnBuffer[256];
			dSprintf(returnBuffer, sizeof(returnBuffer), "%f %f %f %f %f %f %f", 
			mSELDecal->mPosition.x, mSELDecal->mPosition.y, mSELDecal->mPosition.z, 
			mSELDecal->mTangent.x, mSELDecal->mTangent.y, mSELDecal->mTangent.z,
			mSELDecal->mSize);

			Con::executef( this, "completeGizmoTransform", Con::getIntArg(mSELDecal->mId), returnBuffer );

			mGizmo->markClean();
		}

      mGizmo->on3DMouseUp( event );
	}
}

void GuiDecalEditorCtrl::on3DMouseMove(const Gui3DMouseEvent & event)
{
   if ( mSELDecal )
      mGizmo->on3DMouseMove( event );

   RayInfo ri;
   if ( !getRayInfo( event, &ri ) )
      return; 

   Point3F start = event.pos;
   Point3F end = start + event.vec * 300.0f; // use visible distance here??

   DecalInstance *pDecal = gDecalManager->raycast( start, end );

   if ( pDecal && pDecal != mSELDecal )
      mHLDecal = pDecal;   
   else if ( !pDecal )
      mHLDecal = NULL;
}

void GuiDecalEditorCtrl::on3DMouseDragged(const Gui3DMouseEvent & event)
{ 
   if ( !mSELDecal )
      return;

   // Update the Gizmo.
   mGizmo->on3DMouseDragged( event );

   // Pull out the Gizmo transform
   // and position.
   const MatrixF &gizmoMat = mGizmo->getTransform();
   const Point3F &gizmoPos = gizmoMat.getPosition();
   
   // Get the new projection vector.
   VectorF upVec, rightVec;
   gizmoMat.getColumn( 0, &rightVec );
   gizmoMat.getColumn( 2, &upVec );

   const Point3F &scale = mGizmo->getScale();

   // Set the new decal position and projection vector.
   mSELDecal->mSize = (scale.x + scale.y) * 0.5f;
   mSELDecal->mPosition = gizmoPos;
   mSELDecal->mNormal = upVec;
   mSELDecal->mTangent = rightVec;
   gDecalManager->notifyDecalModified( mSELDecal );

	Con::executef( this, "syncNodeDetails" );
}

void GuiDecalEditorCtrl::on3DMouseEnter(const Gui3DMouseEvent & event)
{
   // nothing to do
}

void GuiDecalEditorCtrl::on3DMouseLeave(const Gui3DMouseEvent & event)
{
   // nothing to do
}

void GuiDecalEditorCtrl::updateGuiInfo()
{
   // nothing to do
}
      
void GuiDecalEditorCtrl::onRender( Point2I offset, const RectI &updateRect )
{
   PROFILE_SCOPE( GuiDecalEditorCtrl_OnRender );

   Parent::onRender( offset, updateRect );
}

void GuiDecalEditorCtrl::forceRedraw( DecalInstance * decalInstance )
{ 
	if ( !decalInstance )
		return;

	GFXDrawUtil *drawUtil = GFX->getDrawUtil();  
	GFXStateBlockDesc desc;
   desc.setBlend( true );
   desc.setZReadWrite( true, false );

   Vector<Point3F> verts;
   verts.clear();
   if ( gDecalManager->clipDecal( decalInstance, &verts ) )
      _renderDecalEdge( verts, ColorI( 255, 255, 255, 255 ) );

   const F32 &decalSize = decalInstance->mSize;
   Point3F boxSize( decalSize, decalSize, decalSize );

   MatrixF worldMat( true );
   decalInstance->getWorldMatrix( &worldMat, true );   

   drawUtil->drawObjectBox( desc, boxSize, decalInstance->mPosition, worldMat, ColorI( 255, 255, 255, 255 ) );
}

void GuiDecalEditorCtrl::_renderDecalEdge( const Vector<Point3F> &verts, const ColorI &color )
{
   U32 vertCount = verts.size();

   GFXTransformSaver saver;

   PrimBuild::color( color );

   Point3F endPt( 0, 0, 0 );
   for ( U32 i = 0; i < vertCount; i++ )
   {
      const Point3F &vert = verts[i];
      if ( i + 1 < vertCount )
         endPt = verts[i + 1];
      else
         break;

      PrimBuild::begin( GFXLineList, 2 );

      PrimBuild::vertex3f( vert.x, vert.y, vert.z );
      PrimBuild::vertex3f( endPt.x, endPt.y, endPt.z );

      PrimBuild::end();
   }
}

void GuiDecalEditorCtrl::renderScene(const RectI & updateRect)
{
   PROFILE_SCOPE( GuiDecalEditorCtrl_renderScene );

   GFXTransformSaver saver;

   RectI bounds = getBounds();
   
   ColorI hlColor(0,255,0,255);
   ColorI regColor(255,0,0,255);
   ColorI selColor(0,0,255,255);
   ColorI color;
   
   GFXDrawUtil *drawUtil = GFX->getDrawUtil();   

   GFXStateBlockDesc desc;
   desc.setBlend( true );
   desc.setZReadWrite( true, false );

   // Draw 3D stuff here.   
   if ( mSELDecal )
   {
      mGizmo->renderGizmo(mLastCameraQuery.cameraMatrix);

      mSELEdgeVerts.clear();
      if ( gDecalManager->clipDecal( mSELDecal, &mSELEdgeVerts ) )
         _renderDecalEdge( mSELEdgeVerts, ColorI( 255, 255, 255, 255 ) );

      const F32 &decalSize = mSELDecal->mSize;
      Point3F boxSize( decalSize, decalSize, decalSize );

      MatrixF worldMat( true );
      mSELDecal->getWorldMatrix( &worldMat, true );   

      drawUtil->drawObjectBox( desc, boxSize, mSELDecal->mPosition, worldMat, ColorI( 255, 255, 255, 255 ) );
   }

   if ( mHLDecal )
   {
      mHLEdgeVerts.clear();
      if ( gDecalManager->clipDecal( mHLDecal, &mHLEdgeVerts ) )
         _renderDecalEdge( mHLEdgeVerts, ColorI( 255, 255, 255, 255 ) );

      const F32 &decalSize = mHLDecal->mSize;
      Point3F boxSize( decalSize, decalSize, decalSize );

      MatrixF worldMat( true );
      mHLDecal->getWorldMatrix( &worldMat, true );  

      drawUtil->drawObjectBox( desc, boxSize, mHLDecal->mPosition, worldMat, ColorI( 255, 255, 255, 255 ) );
   }

   gDecalManager->renderDecalSpheres();
} 

bool GuiDecalEditorCtrl::getRayInfo( const Gui3DMouseEvent & event, RayInfo *rInfo )
{       
   Point3F startPnt = event.pos;
   Point3F endPnt = event.pos + event.vec * 100.0f;

   bool hit;         
         
   hit = gServerContainer.castRayRendered( startPnt, endPnt, STATIC_COLLISION_MASK, rInfo );    
   
   return hit;
}

void GuiDecalEditorCtrl::selectDecal( DecalInstance *decalInst )
{
   // If id is zero or invalid we set the selected decal to null
   // which is correct.
   mSELDecal = decalInst;

   if ( decalInst )
      setGizmoFocus( decalInst );
}

void GuiDecalEditorCtrl::deleteSelectedDecal()
{
   if ( !mSELDecal )
      return;
	
	// Grab the mission editor undo manager.
	UndoManager *undoMan = NULL;
	if ( !Sim::findObject( "EUndoManager", undoMan ) )
	{
		Con::errorf( "GuiMeshRoadEditorCtrl::on3DMouseDown() - EUndoManager not found!" );
		return;           
	}

	// Create the UndoAction.
	DIDeleteUndoAction *action = new DIDeleteUndoAction("Delete Decal");
	action->deleteDecal( *mSELDecal );
	
	action->mEditor = this;
	// Submit it.               
	undoMan->addAction( action );
	
	if ( isMethod( "onDeleteInstance" ) )
	{
		char buffer[512];
		dSprintf(buffer, 512, "%i", mSELDecal->mId);
		Con::executef( this, "onDeleteInstance", String(buffer).c_str(), mSELDecal->mDataBlock->lookupName.c_str() );
	}

   gDecalManager->removeDecal( mSELDecal );
   mSELDecal = NULL;
}

void GuiDecalEditorCtrl::deleteDecalDatablock( String lookupName )
{
	DecalData * datablock = dynamic_cast<DecalData*> ( Sim::findObject(lookupName.c_str()) );
	if( !datablock )
		return;

	// Grab the mission editor undo manager.
	UndoManager *undoMan = NULL;
	if ( !Sim::findObject( "EUndoManager", undoMan ) )
	{
		Con::errorf( "GuiMeshRoadEditorCtrl::on3DMouseDown() - EUndoManager not found!" );
		return;           
	}

	// Create the UndoAction.
	DBDeleteUndoAction *action = new DBDeleteUndoAction("Delete Decal Datablock");
	action->mEditor = this;
	action->mDatablockId = datablock->getId();
	
	Vector<DecalInstance*> mDecalQueue;
	Vector<DecalInstance *>::iterator iter;
	mDecalQueue.clear();
	const Vector<DecalSphere*> &grid = gDecalManager->getDecalDataFile()->getGrid();

	for ( U32 i = 0; i < grid.size(); i++ )
	{
		const DecalSphere *decalSphere = grid[i];
		mDecalQueue.merge( decalSphere->mItems );
	}

	for ( iter = mDecalQueue.begin();iter != mDecalQueue.end();iter++ )
	{	
		if( !(*iter) )
			continue;

		if( (*iter)->mDataBlock->lookupName.compare( lookupName ) == 0 )
		{
			if( (*iter)->mId != -1 )
			{
				//make sure to call onDeleteInstance as well
				if ( isMethod( "onDeleteInstance" ) )
				{
					char buffer[512];
					dSprintf(buffer, 512, "%i", (*iter)->mId);
					Con::executef( this, "onDeleteInstance", String(buffer).c_str(), (*iter)->mDataBlock->lookupName.c_str() );
				}
				
				action->deleteDecal( *(*iter) );
				
				if( mSELDecal == (*iter) )
					mSELDecal = NULL;

				if( mHLDecal == (*iter) )
					mHLDecal = NULL;
			}
			gDecalManager->removeDecal( (*iter) );
		}
	}
	
	undoMan->addAction( action );

	mCurrentDecalData = NULL;
}

void GuiDecalEditorCtrl::setMode( String mode, bool sourceShortcut = false )
{
	if( mode.compare("SelectDecalMode") == 0)
		mGizmo->getProfile()->mode = NoneMode;
	else if( mode.compare("AddDecalMode") == 0)
		mGizmo->getProfile()->mode = NoneMode;
	else if( mode.compare("MoveDecalMode") == 0)
		mGizmo->getProfile()->mode = MoveMode;
	else if( mode.compare("RotateDecalMode") == 0)
		mGizmo->getProfile()->mode = RotateMode;
	else if( mode.compare("ScaleDecalMode") == 0)
		mGizmo->getProfile()->mode = ScaleMode;
	
	mMode = mode;

	if( sourceShortcut )
		Con::executef( this, "paletteSync", mMode );
}

ConsoleMethod( GuiDecalEditorCtrl, deleteSelectedDecal, void, 2, 2, "deleteSelectedDecal()" )
{
   object->deleteSelectedDecal();
}

ConsoleMethod( GuiDecalEditorCtrl, deleteDecalDatablock, void, 3, 3, "deleteSelectedDecalDatablock( String datablock )" )
{
	String lookupName( argv[2] );
	if( lookupName == String::EmptyString )
		return;
	
	object->deleteDecalDatablock( lookupName );
}

ConsoleMethod( GuiDecalEditorCtrl, setMode, void, 3, 3, "setMode( String mode )()" )
{
	String newMode = ( argv[2] );
	object->setMode( newMode );
}

ConsoleMethod( GuiDecalEditorCtrl, getMode, const char*, 2, 2, "getMode()" )
{
	return object->mMode;
}

ConsoleMethod( GuiDecalEditorCtrl, getDecalCount, S32, 2, 2, "getDecalCount()" )
{
	return gDecalManager->mDecalInstanceVec.size();
}

ConsoleMethod( GuiDecalEditorCtrl, getDecalTransform, const char*, 3, 3, "getDecalTransform()" )
{
	DecalInstance *decalInstance = gDecalManager->mDecalInstanceVec[dAtoi(argv[2])];
	if( decalInstance == NULL )
		return "";

	char* returnBuffer = Con::getReturnBuffer(256);
   returnBuffer[0] = 0;

   if ( decalInstance )
   {
	   dSprintf(returnBuffer, 256, "%f %f %f %f %f %f %f",
         decalInstance->mPosition.x, decalInstance->mPosition.y, decalInstance->mPosition.z, 
		   decalInstance->mTangent.x, decalInstance->mTangent.y, decalInstance->mTangent.z,
		   decalInstance->mSize);
   }

	return returnBuffer;
}

ConsoleMethod( GuiDecalEditorCtrl, getDecalLookupName, const char*, 3, 3, "getDecalLookupName( S32 )()" )
{
	DecalInstance *decalInstance = gDecalManager->mDecalInstanceVec[dAtoi(argv[2])];
	if( decalInstance == NULL )
		return "invalid";

	return decalInstance->mDataBlock->lookupName;
}

ConsoleMethod( GuiDecalEditorCtrl, selectDecal, void, 3, 3, "selectDecal( S32 )()" )
{
	DecalInstance *decalInstance = gDecalManager->mDecalInstanceVec[dAtoi(argv[2])];
	if( decalInstance == NULL )
		return;

	object->selectDecal( decalInstance );
}

ConsoleMethod( GuiDecalEditorCtrl, editDecalDetails, void, 4, 4, "editDecalDetails( S32 )()" )
{
	DecalInstance *decalInstance = gDecalManager->mDecalInstanceVec[ dAtoi(argv[2]) ];
	if( decalInstance == NULL )
		return;

	Point3F pos;
	Point3F tan;
	F32 size;

	S32 count = dSscanf( argv[3], "%f %f %f %f %f %f %f", 
		&pos.x, &pos.y, &pos.z, &tan.x, &tan.y, &tan.z, &size);
	
	if ( (count != 7) )
   {
		Con::printf("Failed to parse decal information \"px py pz tx ty tz s\" from '%s'", argv[3]);
      return;
   }

   decalInstance->mPosition = pos;
	decalInstance->mTangent = tan;
	decalInstance->mSize = size;
	
	if ( decalInstance == object->mSELDecal )
		object->setGizmoFocus( decalInstance );

	object->forceRedraw( decalInstance );

	gDecalManager->notifyDecalModified( decalInstance );
}

void GuiDecalEditorCtrl::setGizmoFocus( DecalInstance * decalInstance )
{
	const F32 &size = decalInstance->mSize;
   MatrixF worldMat( true );
   decalInstance->getWorldMatrix( &worldMat, true );
   worldMat.setPosition( Point3F( 0, 0, 0 ) );
   mGizmo->set( worldMat, decalInstance->mPosition, Point3F( size, size, size ) );
}

//Decal Instance Create Undo Actions
IMPLEMENT_CONOBJECT( DICreateUndoAction );

DICreateUndoAction::DICreateUndoAction( const UTF8* actionName )
   :  UndoAction( actionName )
{
}

DICreateUndoAction::~DICreateUndoAction()
{
}

void DICreateUndoAction::initPersistFields()
{
   Parent::initPersistFields();
}

void DICreateUndoAction::addDecal( DecalInstance decal )
{
	mDecalInstance = decal;
	mDatablockId = decal.mDataBlock->getId();
}

void DICreateUndoAction::undo()
{
	Vector<DecalInstance *>::iterator iter;
   for(iter = gDecalManager->mDecalInstanceVec.begin();iter != gDecalManager->mDecalInstanceVec.end();iter++)
	{
		if( !(*iter) )
			continue;
		
		if( (*iter)->mId != mDecalInstance.mId )
			continue;

		if ( mEditor->isMethod( "onDeleteInstance" ) )
		{
			char buffer[512];
			dSprintf(buffer, 512, "%i", (*iter)->mId);
			Con::executef( mEditor, "onDeleteInstance", String(buffer).c_str(), (*iter)->mDataBlock->lookupName.c_str() );
		}
		
		// Decal manager handles clearing the vector if the decal contains a valid id
		if( mEditor->mSELDecal == (*iter) )
			mEditor->mSELDecal = NULL;

		if( mEditor->mHLDecal == (*iter) )
			mEditor->mHLDecal = NULL;

		gDecalManager->removeDecal( (*iter) );
		break;
	}
}

void DICreateUndoAction::redo()
{
	//Reinstate the valid datablock pointer	
	mDecalInstance.mDataBlock = dynamic_cast<DecalData *>( Sim::findObject( mDatablockId ) );

	DecalInstance * decal = gDecalManager->addDecal( mDecalInstance.mPosition, 
		mDecalInstance.mNormal, 
		mDecalInstance.mTangent, 
		mDecalInstance.mDataBlock,
		( mDecalInstance.mSize / mDecalInstance.mDataBlock->size ), 
		mDecalInstance.mTextureRectIdx, 
		mDecalInstance.mFlags );
	
	decal->mId = mDecalInstance.mId;

	// Override the rectIdx regardless of random decision in addDecal
	decal->mTextureRectIdx = mDecalInstance.mTextureRectIdx;
	
	// We take care of filling in the vector space that was once there
	gDecalManager->mDecalInstanceVec[decal->mId] = decal;

	if ( mEditor->isMethod( "onCreateInstance" ) )
	{
		char buffer[512];
		dSprintf(buffer, 512, "%i", decal->mId);
		Con::executef( mEditor, "onCreateInstance", buffer, decal->mDataBlock->lookupName.c_str());
	}
	mEditor->selectDecal( decal );
}

//Decal Instance Delete Undo Actions
IMPLEMENT_CONOBJECT( DIDeleteUndoAction );

DIDeleteUndoAction::DIDeleteUndoAction( const UTF8 *actionName )
   :  UndoAction( actionName )
{
}

DIDeleteUndoAction::~DIDeleteUndoAction()
{
}

void DIDeleteUndoAction::initPersistFields()
{
   Parent::initPersistFields();
}

void DIDeleteUndoAction::deleteDecal( DecalInstance decal )
{
	mDecalInstance = decal;
	mDatablockId = decal.mDataBlock->getId();
}

void DIDeleteUndoAction::undo()
{
	//Reinstate the valid datablock pointer	
	mDecalInstance.mDataBlock = dynamic_cast<DecalData *>( Sim::findObject( mDatablockId ) );

	DecalInstance * decal = gDecalManager->addDecal( mDecalInstance.mPosition, 
		mDecalInstance.mNormal, 
		mDecalInstance.mTangent, 
		mDecalInstance.mDataBlock,
		( mDecalInstance.mSize / mDecalInstance.mDataBlock->size ), 
		mDecalInstance.mTextureRectIdx, 
		mDecalInstance.mFlags );
	
	decal->mId = mDecalInstance.mId;

	// Override the rectIdx regardless of random decision in addDecal
	decal->mTextureRectIdx = mDecalInstance.mTextureRectIdx;
	
	// We take care of filling in the vector space that was once there
	gDecalManager->mDecalInstanceVec[decal->mId] = decal;

	if ( mEditor->isMethod( "onCreateInstance" ) )
	{
		char buffer[512];
		dSprintf(buffer, 512, "%i", decal->mId);
		Con::executef( mEditor, "onCreateInstance", buffer, decal->mDataBlock->lookupName.c_str());
	}
	mEditor->selectDecal( decal );
}

void DIDeleteUndoAction::redo()
{
	Vector<DecalInstance *>::iterator iter;
   for(iter = gDecalManager->mDecalInstanceVec.begin();iter != gDecalManager->mDecalInstanceVec.end();iter++)
	{
		if( !(*iter) )
			continue;
		
		if( (*iter)->mId != mDecalInstance.mId )
			continue;

		if ( mEditor->isMethod( "onDeleteInstance" ) )
		{
			char buffer[512];
			dSprintf(buffer, 512, "%i", (*iter)->mId);
			Con::executef( mEditor, "onDeleteInstance", String(buffer).c_str(), (*iter)->mDataBlock->lookupName.c_str() );
		}
		
		// Decal manager handles clearing the vector if the decal contains a valid id
		if( mEditor->mSELDecal == (*iter) )
			mEditor->mSELDecal = NULL;

		if( mEditor->mHLDecal == (*iter) )
			mEditor->mHLDecal = NULL;

		gDecalManager->removeDecal( (*iter) );
		break;
	}
}

//Decal Datablock Delete Undo Actions
IMPLEMENT_CONOBJECT( DBDeleteUndoAction );

DBDeleteUndoAction::DBDeleteUndoAction( const UTF8 *actionName )
   :  UndoAction( actionName )
{
}

DBDeleteUndoAction::~DBDeleteUndoAction()
{
}

void DBDeleteUndoAction::initPersistFields()
{
   Parent::initPersistFields();
}

void DBDeleteUndoAction::deleteDecal( DecalInstance decal )
{
	mDecalInstanceVec.increment();
   mDecalInstanceVec.last() = decal;
}

void DBDeleteUndoAction::undo()
{
	DecalData * datablock = dynamic_cast<DecalData *>( Sim::findObject( mDatablockId ) );
	if ( mEditor->isMethod( "undoDeleteDecalDatablock" ) )
			Con::executef( mEditor, "undoDeleteDecalDatablock", datablock->lookupName.c_str());

	// Create and restore the decal instances
	for ( S32 i= mDecalInstanceVec.size()-1; i >= 0; i-- )
   {
		DecalInstance vecInstance = mDecalInstanceVec[i];

		//Reinstate the valid datablock pointer		
		vecInstance.mDataBlock = datablock;

		DecalInstance * decalInstance = gDecalManager->addDecal( vecInstance.mPosition, 
		vecInstance.mNormal, 
		vecInstance.mTangent, 
		vecInstance.mDataBlock,
		( vecInstance.mSize / vecInstance.mDataBlock->size ), 
		vecInstance.mTextureRectIdx, 
		vecInstance.mFlags );
	
		decalInstance->mId = vecInstance.mId;

		// Override the rectIdx regardless of random decision in addDecal
		decalInstance->mTextureRectIdx = vecInstance.mTextureRectIdx;
	
		// We take care of filling in the vector space that was once there
		gDecalManager->mDecalInstanceVec[decalInstance->mId] = decalInstance;

		if ( mEditor->isMethod( "onCreateInstance" ) )
		{
			char buffer[512];
			dSprintf(buffer, 512, "%i", decalInstance->mId);
			Con::executef( mEditor, "onCreateInstance", buffer, decalInstance->mDataBlock->lookupName.c_str());
		}
	}
	
}

void DBDeleteUndoAction::redo()
{
	for ( S32 i=0; i < mDecalInstanceVec.size(); i++ )
   {
		DecalInstance vecInstance = mDecalInstanceVec[i];

		Vector<DecalInstance *>::iterator iter;
		for(iter = gDecalManager->mDecalInstanceVec.begin();iter != gDecalManager->mDecalInstanceVec.end();iter++)
		{
			DecalInstance * decalInstance = (*iter);
			if( !decalInstance )
				continue;
			
			if( decalInstance->mId != vecInstance.mId )
				continue;

			if ( mEditor->isMethod( "onDeleteInstance" ) )
			{
				char buffer[512];
				dSprintf(buffer, 512, "%i", decalInstance->mId);
				Con::executef( mEditor, "onDeleteInstance", String(buffer).c_str(), decalInstance->mDataBlock->lookupName.c_str() );
			}
			
			// Decal manager handles clearing the vector if the decal contains a valid id
			if( mEditor->mSELDecal == decalInstance )
				mEditor->mSELDecal = NULL;

			if( mEditor->mHLDecal == decalInstance )
				mEditor->mHLDecal = NULL;

			gDecalManager->removeDecal( decalInstance );
			break;
		}
	}
	
	DecalData * datablock = dynamic_cast<DecalData *>( Sim::findObject( mDatablockId ) );
	if ( mEditor->isMethod( "redoDeleteDecalDatablock" ) )
		Con::executef( mEditor, "redoDeleteDecalDatablock", datablock->lookupName.c_str());
}
#endif