#include "guiRoleList.h"
#include "console/consoleTypes.h"
#include "gfx/gfxDrawUtil.h"

IMPLEMENT_CONOBJECT(guiRoleList);

guiRoleList::guiRoleList():
_mFirstRoleOffset(0,0),
_mRoleHeightOffset(0),
_mRoleSize(0,0),
_mRoleBitmapName(0),
_mTextureRoleBitmap(0),
_mRoleBitmapNameHL(0),
_mTextureRoleBitmapHL(0),
_mHeadOffset(0,0),
_mHeadSize(10,10),
_mNameOffset(0,0)
{

}

void guiRoleList::initPersistFields()
{
	Parent::initPersistFields();
	addGroup("guiRoleList");		

	addField("firstRoleOffset", TypePoint2I, Offset(_mFirstRoleOffset, guiRoleList));
	addField("roleSize", TypePoint2I, Offset(_mRoleSize, guiRoleList));
	addField("roleHeightOffset", TypeS32, Offset(_mRoleHeightOffset, guiRoleList));
	addProtectedField( "roleBitmap", TypeFilename, Offset( _mRoleBitmapName, guiRoleList ), &setRoleBitmap, &defaultProtectedGetFn, "" );
	addProtectedField( "roleBitmapHL", TypeFilename, Offset( _mRoleBitmapNameHL, guiRoleList ), &setRoleBitmapHL, &defaultProtectedGetFn, "" );
	addField("headOffset", TypePoint2I, Offset(_mHeadOffset, guiRoleList));
	addField("headSize", TypePoint2I, Offset(_mHeadSize, guiRoleList));
	addField("nameOffset", TypePoint2I, Offset(_mNameOffset, guiRoleList));

	endGroup("guiRoleList");		
}

bool guiRoleList::setRoleBitmap( void *obj, const char *data )
{
	static_cast<guiRoleList *>( obj )->setRoleBitmap( data );
	return false;
}

void guiRoleList::setRoleBitmap( const char * fileName )
{
	_mRoleBitmapName = StringTable->insert(fileName);
	if (*_mRoleBitmapName)
	{
		_mTextureRoleBitmap.set( _mRoleBitmapName, &GFXDefaultGUIProfile ,"role rect");
	}
	else
		_mTextureRoleBitmap = NULL;

	setUpdate();
}

bool guiRoleList::setRoleBitmapHL( void *obj, const char *data )
{
	static_cast<guiRoleList *>( obj )->setRoleBitmapHL( data );
	return false;
}

void guiRoleList::setRoleBitmapHL( const char * fileName )
{
	_mRoleBitmapNameHL = StringTable->insert(fileName);
	if (*_mRoleBitmapNameHL)
	{
		_mTextureRoleBitmapHL.set( _mRoleBitmapNameHL, &GFXDefaultGUIProfile ,"role rect hightlight");
	}
	else
		_mTextureRoleBitmapHL = NULL;

	setUpdate();
}

void guiRoleList::onRender( Point2I offset, const RectI &updateRect )
{
	if (mTextureObject)
	{
		GFX->getDrawUtil()->clearBitmapModulation();
		RectI rect(offset, getExtent());
		GFX->getDrawUtil()->drawBitmapStretch(mTextureObject, rect);
	}

	Point2I offsetRole(offset);
	RectI rectRole;
	offsetRole += _mFirstRoleOffset;
	rectRole.point = offsetRole;
	rectRole.extent = _mRoleSize;
	std::list<RoleInfo>::iterator it =	_mListRole.begin();
	GFont *font = getControlProfile()->mFont;
	for ( ; it != _mListRole.end() ; ++it)
	{
		//role bg
		if (_mTextureRoleBitmap && !(it->roleSelected))
		{
			GFX->getDrawUtil()->drawBitmapStretch(_mTextureRoleBitmap, rectRole);
		}
		else if (_mTextureRoleBitmapHL && it->roleSelected)
		{
			GFX->getDrawUtil()->drawBitmapStretch(_mTextureRoleBitmapHL, rectRole);
		}
		//role head
		rectRole.point += _mHeadOffset;
		rectRole.extent = _mHeadSize;
		if (it->textureRoleHead)
		{
			GFX->getDrawUtil()->drawBitmapStretch(it->textureRoleHead, rectRole);
		}
		rectRole.point -= _mHeadOffset;
		rectRole.extent = _mRoleSize;
		//role name
		GFX->getDrawUtil()->drawText( font, rectRole.point + _mNameOffset, it->roleNameUTF8, getControlProfile()->mFontColors );

		rectRole.point.y += _mRoleHeightOffset;
		rectRole.point.y += _mRoleSize.y;
	}

	renderChildControls(offset, updateRect);
}

void guiRoleList::onMouseDown( const GuiEvent &event )
{
	Point2I localPt = globalToLocalCoord(event.mousePoint);
	RectI cellRect;
	cellRect.point = _mFirstRoleOffset;
	cellRect.extent = _mRoleSize;
	std::list<RoleInfo>::iterator it =	_mListRole.begin();
	for ( ; it != _mListRole.end() ; ++it)
	{
		if (cellRect.pointInRect(localPt))
		{
			it->roleSelected = true;
			break;
		}
		cellRect.point.y += _mRoleHeightOffset;
		cellRect.point.y += _mRoleSize.y;
	}
	if (it == _mListRole.end())
	{
		return;
	}

	cellRect.point = _mFirstRoleOffset;
	cellRect.extent = _mRoleSize;
	std::list<RoleInfo>::iterator it2 =	_mListRole.begin();
	for ( ; it2 != _mListRole.end() ; ++it2)
	{
		if (it2 != it)
		{
			it2->roleSelected = false;
		}
	}

	//fire the script
	Con::executef(this,"onSelected",it->roleNameUTF8,Con::getIntArg(it->roleJob),Con::getIntArg( it->roleIsMale));
}

S32 guiRoleList::getRoleCount()
{
	return _mListRole.size();
}

void guiRoleList::addRole( const char * roleName , bool isMale , int job )
{
	RoleInfo info(roleName,isMale,job);
	_mListRole.push_back(info);
}

void guiRoleList::clear()
{
	_mListRole.clear();
}

void guiRoleList::setSelect( int id )
{
	if (id < _mListRole.size())
	{
		std::list<RoleInfo>::iterator it =	_mListRole.begin();
		for (int i = 0 ; it != _mListRole.end() ; ++it,++i)
		{
			if (i == id)
			{
				it->roleSelected = true;
				Con::executef(this,"onSelected",it->roleNameUTF8,Con::getIntArg(it->roleJob),Con::getIntArg( it->roleIsMale));
			}
			else
			{
				it->roleSelected = false;
			}
		}
	}
}

guiRoleList::RoleInfo * guiRoleList::getSelected()
{
	std::list<RoleInfo>::iterator it =	_mListRole.begin();
	for (int i = 0 ; it != _mListRole.end() ; ++it,++i)
	{
		if (it->roleSelected)
		{
			return &(*it);
		}
	}
	return NULL;
}

void guiRoleList::clearSelected()
{
	std::list<RoleInfo>::iterator it =	_mListRole.begin();
	for (int i = 0 ; it != _mListRole.end() ; ++it,++i)
	{
		if (it->roleSelected)
		{
			_mListRole.erase(it);
			break;
		}
	}
	setSelect(0);
}

ConsoleMethod(guiRoleList,setSelect,void,3,3,"id")
{
	object->setSelect(dAtoi(argv[2]));
}

ConsoleMethod(guiRoleList,getSelectedName,const char *,2,2,"")
{
	guiRoleList::RoleInfo * pRole = object->getSelected();
	return pRole ? pRole->roleNameUTF8 : NULL;
}

ConsoleMethod(guiRoleList,addRole,void,5,5,"name,ismale,job")
{
	object->addRole(argv[2],dAtob(argv[3]),dAtoi(argv[4]));
}

ConsoleMethod(guiRoleList,getRoleCount,S32,2,2,"")
{
	return object->getRoleCount();
}

ConsoleMethod(guiRoleList,clear,void,2,2,"")
{
	object->clear();
}

ConsoleMethod(guiRoleList,clearSelected,void,2,2,"")
{
	object->clearSelected();
}