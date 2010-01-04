#include "guiShapeNameHud.h"
#include "T3D/player.h"
#include "gfx/gfxDrawUtil.h"

IMPLEMENT_CONOBJECT(GuiShapeNameHud);

/// Default distance for object's information to be displayed.
static const F32 cDefaultVisibleDistance = 500.0f;
Vector<SceneObject*>	GuiShapeNameHud::PlayersInScene;

GuiShapeNameHud::GuiShapeNameHud()
{
	mFillColor.set( 0.25f, 0.25f, 0.25f, 0.25f );
	mFrameColor.set( 0, 1, 0, 1 );
	mTextColor.set( 0, 1, 0, 1 );
	mShowFrame = mShowFill = true;
	mVerticalOffset = 0.5f;
	mDistanceFade = 0.1f;

	_m_tex_MarkBmp2 = NULL;
	_m_tex_MarkBmp1 = NULL;
	_m_str_MarkBmp2 = NULL;
	_m_str_MarkBmp1 = NULL;
}

void GuiShapeNameHud::initPersistFields()
{
	Parent::initPersistFields();
	addGroup("Colors");     
	addField( "fillColor",  TypeColorF, Offset( mFillColor, GuiShapeNameHud ) );
	addField( "frameColor", TypeColorF, Offset( mFrameColor, GuiShapeNameHud ) );
	addField( "textColor",  TypeColorF, Offset( mTextColor, GuiShapeNameHud ) );
	endGroup("Colors");     

	addGroup("Misc");       
	addField( "showFill",   TypeBool, Offset( mShowFill, GuiShapeNameHud ) );
	addField( "showFrame",  TypeBool, Offset( mShowFrame, GuiShapeNameHud ) );
	addField( "verticalOffset", TypeF32, Offset( mVerticalOffset, GuiShapeNameHud ) );
	addField( "distanceFade", TypeF32, Offset( mDistanceFade, GuiShapeNameHud ) );
	endGroup("Misc");

	addProtectedField( "MarkMissionComplete", TypeFilename, Offset( _m_str_MarkBmp1, GuiShapeNameHud ), &setMarkBmp1Name, &defaultProtectedGetFn, "" );
	addProtectedField( "MarkMissionValide", TypeFilename, Offset( _m_str_MarkBmp2, GuiShapeNameHud ), &setMarkBmp2Name, &defaultProtectedGetFn, "" );
}


static void findObjectsCallback(SceneObject* obj, void * val)
{
	Vector<SceneObject*> * list = (Vector<SceneObject*>*)val;
	list->push_back(obj);
}

//----------------------------------------------------------------------------
/// Core rendering method for this control.
///
/// This method scans through all the current client ShapeBase objects.
/// If one is named, it displays the name and damage information for it.
///
/// Information is offset from the center of the object's bounding box,
/// unless the object is a PlayerObjectType, in which case the eye point
/// is used.
///
/// @param   updateRect   Extents of control.
void GuiShapeNameHud::onRender( Point2I, const RectI &updateRect)
{
	// Background fill first
	if (mShowFill)
		GFX->getDrawUtil()->drawRectFill(updateRect, mFillColor);

	// Must be in a TS Control
	GuiTSCtrl *parent = dynamic_cast<GuiTSCtrl*>(getParent());
	if (!parent) return;

	// Must have a connection and control object
	GameConnection* conn = GameConnection::getConnectionToServer();
	if (!conn) return;
	GameBase * control = dynamic_cast<GameBase*>(conn->getControlObject());
	if (!control) return;

	// Get control camera info
	MatrixF cam;
	Point3F camPos;
	VectorF camDir;
	conn->getControlCameraTransform(0,&cam);
	cam.getColumn(3, &camPos);
	cam.getColumn(1, &camDir);

	F32 camFov;
	conn->getControlCameraFov(&camFov);
	camFov = mDegToRad(camFov) / 2;

	// Visible distance info & name fading
	F32 visDistance = 200;//gClientSceneGraph->getVisibleDistance();
	F32 visDistanceSqr = 40000;//visDistance * visDistance;
	F32 fadeDistance = visDistance * mDistanceFade;

	// Collision info. We're going to be running LOS tests and we
	// don't want to collide with the control object.
	static U32 losMask =  TerrainObjectType | InteriorObjectType | ShapeBaseObjectType;
	control->disableCollision();


	U32 mask = PlayerObjectType | VehicleObjectType ;
	PlayersInScene.clear();
	gClientContainer.findObjects(mask, findObjectsCallback, &PlayersInScene);

	// All ghosted objects are added to the server connection group,
	// so we can find all the shape base objects by iterating through
	// our current connection.
#if 1
	for (int n = 0 ; n < PlayersInScene.size() ; n++)
	{
		if (PlayersInScene[n]->getType() & ShapeBaseObjectType)
		{
			ShapeBase * shape = static_cast<ShapeBase*>(PlayersInScene[n]);
#else
	for (SimSetIterator itr(conn); *itr; ++itr) {
		if ((*itr)->getType() & ShapeBaseObjectType) {
			ShapeBase * shape = static_cast<ShapeBase*>(*itr);
#endif
			if (shape != control && shape->getShapeName()) 
			{

				// Target pos to test, if it's a player run the LOS to his eye
				// point, otherwise we'll grab the generic box center.
				Point3F shapePos;
				if (shape->getType() & PlayerObjectType) 
				{
					MatrixF eye;

					// Use the render eye transform, otherwise we'll see jittering
					shape->getRenderEyeTransform(&eye);
					eye.getColumn(3, &shapePos);
				} 
				else 
				{
					// Use the render transform instead of the box center
					// otherwise it'll jitter.
					MatrixF srtMat = shape->getRenderTransform();
					srtMat.getColumn(3, &shapePos);
				}
				VectorF shapeDir = shapePos - camPos;

				// Test to see if it's in range
				F32 shapeDist = shapeDir.lenSquared();
				if (shapeDist == 0 || shapeDist > visDistanceSqr)
					continue;
				shapeDist = mSqrt(shapeDist);

				// Test to see if it's within our viewcone, this test doesn't
				// actually match the viewport very well, should consider
				// projection and box test.
				shapeDir.normalize();
				F32 dot = mDot(shapeDir, camDir);
				if (dot < camFov)
					continue;

				// Test to see if it's behind something, and we want to
				// ignore anything it's mounted on when we run the LOS.
				RayInfo info;
				shape->disableCollision();
				ShapeBase *mount = dynamic_cast<ShapeBase*>(shape->getObjectMount());
				if (mount)
					mount->disableCollision();
				bool los = !gClientContainer.castRay(camPos, shapePos,losMask, &info);
				shape->enableCollision();
				if (mount)
					mount->enableCollision();
				if (!los)
					continue;

				// Project the shape pos into screen space and calculate
				// the distance opacity used to fade the labels into the
				// distance.
				Point3F projPnt;
				shapePos.z += mVerticalOffset;
				if (!parent->project(shapePos, &projPnt))
					continue;
				F32 opacity = (shapeDist < fadeDistance)? 1.0:
					1.0 - (shapeDist - fadeDistance) / (visDistance - fadeDistance);

				// Render the shape's name
				drawName(Point2I((S32)projPnt.x, (S32)projPnt.y),shape->getShapeName(),opacity);
				// Render the mark
				/*
				Player * pNPC = dynamic_cast<Player*>(shape);
				if (pNPC && pNPC->getMissionObjSimID())
				{
					projPnt.y -= 30;
					drawMark(Point2I((S32)projPnt.x, (S32)projPnt.y),MARK_CANRETURN,shapeDist);
				}
				*/
			}
		}
	}

	// Restore control object collision
	control->enableCollision();

	// Border last
	if (mShowFrame)
		GFX->getDrawUtil()->drawRect(updateRect, mFrameColor);
}


//----------------------------------------------------------------------------
/// Render object names.
///
/// Helper function for GuiShapeNameHud::onRender
///
/// @param   offset  Screen coordinates to render name label. (Text is centered
///                  horizontally about this location, with bottom of text at
///                  specified y position.)
/// @param   name    String name to display.
/// @param   opacity Opacity of name (a fraction).
void GuiShapeNameHud::drawName(Point2I offset, const char *name, F32 opacity)
{
	// Center the name
	offset.x -= mProfile->mFont->getStrWidth((const UTF8 *)name) / 2;
	offset.y -= mProfile->mFont->getHeight();

	// Deal with opacity and draw.
	mTextColor.alpha = opacity;
	GFX->getDrawUtil()->setBitmapModulation(mTextColor);
	GFX->getDrawUtil()->drawText(mProfile->mFont, offset, name);
	GFX->getDrawUtil()->clearBitmapModulation();
}

bool GuiShapeNameHud::setMarkBmp1Name( void *obj, const char *data )
{
	static_cast<GuiShapeNameHud *>( obj )->setMarkBmp1( data );
	return false;
}

void GuiShapeNameHud::setMarkBmp1( const char * bmpName )
{
	_m_str_MarkBmp1 = StringTable->insert(bmpName);
	if (*_m_str_MarkBmp1)
	{
		_m_tex_MarkBmp1.set( _m_str_MarkBmp1, &GFXDefaultGUIProfile ,"mark 1");
	}
	else
		_m_tex_MarkBmp1 = NULL;

	setUpdate();
}

bool GuiShapeNameHud::setMarkBmp2Name( void *obj, const char *data )
{
	static_cast<GuiShapeNameHud *>( obj )->setMarkBmp2( data );
	return false;
}

void GuiShapeNameHud::setMarkBmp2( const char * bmpName )
{
	_m_str_MarkBmp2 = StringTable->insert(bmpName);
	if (*_m_str_MarkBmp2)
	{
		_m_tex_MarkBmp2.set( _m_str_MarkBmp2, &GFXDefaultGUIProfile ,"mark 2");
	}
	else
		_m_tex_MarkBmp2 = NULL;

	setUpdate();
}

void GuiShapeNameHud::drawMark( Point2I offset , MARKS mark , F32 distance )
{
	GFXTexHandle handle = NULL;
	switch(mark)
	{
	case MARK_QUESTION:
		handle = _m_tex_MarkBmp1;
	case MARK_CANRETURN:
		handle = _m_tex_MarkBmp2;
	}
	if(handle)
	{
		F32 ratio = ( 1 - distance /  20.f );
		if (ratio > 0)
		{	
			U32 width = 70 * ratio;
			U32 height = 200 * ratio;
			offset.x -= width  / 2;
			offset.y -= height;
			RectI rect(offset.x,offset.y,width,height);
			GFX->getDrawUtil()->drawBitmapStretch(handle,rect);
		}
		handle = NULL;	
	}
}