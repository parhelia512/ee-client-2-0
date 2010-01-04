#ifndef __GUIROLELIST__
#define __GUIROLELIST__

#include "gui/controls/guiBitmapCtrl.h"
#include <list>

class guiRoleList : public GuiBitmapCtrl
{
public:
	typedef GuiBitmapCtrl Parent;

	struct RoleInfo 
	{
		char	roleNameUTF8[128];
		bool	roleIsMale;
		int		roleJob;
		bool	roleSelected;
		GFXTexHandle	textureRoleHead;
		RoleInfo():textureRoleHead(0),roleSelected(false)
		{
			roleNameUTF8[0] = 0;
		}
		RoleInfo(const char * roleName , bool isMale  , int job):
		roleIsMale(isMale),roleJob(job),textureRoleHead(0)
		{
			dStrcpy(roleNameUTF8,roleName);
		}
	};
private:
	std::list<RoleInfo>	_mListRole;

	Point2I	_mFirstRoleOffset;
	U32		_mRoleHeightOffset;
	Point2I	_mRoleSize;
	StringTableEntry _mRoleBitmapName;
	GFXTexHandle	_mTextureRoleBitmap;
	StringTableEntry _mRoleBitmapNameHL;
	GFXTexHandle	_mTextureRoleBitmapHL;
	Point2I	_mHeadOffset;
	Point2I	_mHeadSize;
	Point2I	_mNameOffset;
public:
	DECLARE_CONOBJECT(guiRoleList);
	guiRoleList();
	static void initPersistFields();
	static bool setRoleBitmap( void *obj, const char *data );
	void	setRoleBitmap(const char * fileName);
	static bool setRoleBitmapHL( void *obj, const char *data );
	void	setRoleBitmapHL(const char * fileName);
	void onRender(Point2I offset, const RectI &updateRect);
	void onMouseDown(const GuiEvent &event);
	S32 getRoleCount();
	void setSelect(int id);
	RoleInfo * getSelected();
	void addRole(const char * roleName , bool isMale , int job); 
	void clear();
	void clearSelected();
};

#endif