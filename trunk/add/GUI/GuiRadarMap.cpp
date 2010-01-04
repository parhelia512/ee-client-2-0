#include "GuiRadarMap.h"
#include "console/consoleTypes.h"
#include "T3D/gameConnection.h"
#include "math/mathUtils.h"
#include "terrain/terrData.h"
#include "gfx/gfxDrawUtil.h"
#include "T3D/fps/guiShapeNameHud.h"

IMPLEMENT_CONOBJECT(guiRadarMap);

guiRadarMap::guiRadarMap():_m_n_RadaRaius(20),_m_tex_PlayerBmp(NULL),_m_str_PlayerBmp(NULL)
{

}

void guiRadarMap::initPersistFields()
{
	Parent::initPersistFields();
	addGroup("guiRadarMap");		
	addProtectedField( "playerBmp", TypeFilename, Offset( _m_str_PlayerBmp, guiRadarMap ), &setPlayerBmpName, &defaultProtectedGetFn, "" );
	addField("radaRadius",   TypeS32,     Offset(_m_n_RadaRaius,       guiRadarMap));
	endGroup("guiRadarMap");	
}

void guiRadarMap::setPlayerBmp(const char *bmpName)
{
	_m_str_PlayerBmp = StringTable->insert(bmpName);
	if (*_m_str_PlayerBmp)
	{
		_m_tex_PlayerBmp.set( _m_str_PlayerBmp, &GFXDefaultGUIProfile ,"player arrow");
	}
	else
		_m_tex_PlayerBmp = NULL;

	setUpdate();
}

bool guiRadarMap::setPlayerBmpName( void *obj, const char *data )
{
	// Prior to this, you couldn't do bitmap.bitmap = "foo.jpg" and have it work.
	// With protected console types you can now call the setBitmap function and
	// make it load the image.
	static_cast<guiRadarMap *>( obj )->setPlayerBmp( data );

	// Return false because the setBitmap method will assign 'mBitmapName' to the
	// argument we are specifying in the call.
	return false;
}

void guiRadarMap::renderPlayer(Point2I offset,Point3F selfPos,Point3F otherPos,int type /* = 0 */)
{
	//to see if the distance is too large
	Point3F diff = otherPos - selfPos;
	F32 lenth = diff.len();
	if (lenth > _m_n_RadaRaius)
		return;

	S32 W = 4;
	S32 H = 4;
	F32 angle,pitch;
	MathUtils::getAnglesFromVector(diff,angle,pitch);
	F32 ratio = lenth / (F32)_m_n_RadaRaius;
	F32 screenLen = (((F32)(getExtent().x < getExtent().y ? getExtent().x : getExtent().y)) / 2) * ratio;
	F32 x = screenLen * mSin(angle);
	F32 y = screenLen * mCos(angle); 

	RectI rect(offset.x + getExtent().x / 2 - W / 2 + 0.5,
		offset.y + getExtent().y / 2  - H / 2  + 0.5,
		W,H);
	rect.point.x += x;
	rect.point.y -= y;

	ColorI color(255,0,0);
	GFX->getDrawUtil()->drawRectFill(rect,color);
}

RectI guiRadarMap::getMapRect(Point3F selfPos)
{
	RectI rect(0,0,0,0);
	TerrainBlock* pBlock = gClientSceneGraph->getCurrentTerrain();
	if(pBlock && mTextureObject)
	{
		F32 xRatio = (selfPos.x - pBlock->getPosition().x) / 2048;
		F32 yRatio = (2048 - selfPos.y + pBlock->getPosition().y) / 2048;
		F32 wRatio = (F32)(_m_n_RadaRaius) * 2  / 2048;
		Point2I pos,ext;
		ext.x = mTextureObject->getWidth() * wRatio + 0.5;
		ext.y = mTextureObject->getHeight() * wRatio + 0.5;
		rect.extent = ext;

		pos.x = mTextureObject->getWidth() * xRatio + 0.5;
		pos.y = mTextureObject->getHeight() * yRatio + 0.5;
		pos.x -= rect.extent.x / 2 + 0.5;
		pos.y -= rect.extent.y / 2 + 0.5;
		rect.point = pos;
	}
	return rect;
}

void guiRadarMap::onRender(Point2I offset, const RectI &updateRect)
{
	GameConnection * conn = GameConnection::getConnectionToServer();
	GameBase * pControled = conn ? conn->getControlObject() : NULL;
	F32 angle = 0;
	F32 pitch = 0;
	Point3F selfPos(0,0,0);
	Point3F otherPos(0,0,0);
	if (pControled)
	{
		Point3F face;
		pControled->getTransform().getColumn(1,&face);
		pControled->getTransform().getColumn(3,&selfPos);
		face.z = 0;
		face.normalize();
		MathUtils::getAnglesFromVector(face,angle,pitch);
	}

	if (mTextureObject)
	{
		RectI rect(offset, getExtent());
		RectI srcRect = getMapRect(selfPos);
		GFX->getDrawUtil()->drawBitmapStretchSRCircle(mTextureObject,rect,srcRect);
	}
	if (_m_tex_PlayerBmp)
	{
		RectI desRect(offset.x + getExtent().x / 2 - _m_tex_PlayerBmp.getWidth() / 2 + 0.5 , 
			offset.y + getExtent().y / 2  - _m_tex_PlayerBmp.getHeight() / 2  + 0.5,
			_m_tex_PlayerBmp.getWidth(),
			_m_tex_PlayerBmp.getHeight());
		GFX->getDrawUtil()->drawBitmapStretchRotate(_m_tex_PlayerBmp,desRect,angle);
	}
	if (!GuiShapeNameHud::PlayersInScene.empty())
	{
		for (int n = 0 ; n < GuiShapeNameHud::PlayersInScene.size() ; n++)
		{
			if (GuiShapeNameHud::PlayersInScene[n] != pControled)
			{
				GuiShapeNameHud::PlayersInScene[n] ->getTransform().getColumn(3,&otherPos);
				renderPlayer(offset,selfPos,otherPos,0);
			}
		}
	}
	renderChildControls(offset, updateRect);
}