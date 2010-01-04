#include "RPGUtils.h"
#include "T3D/gameConnection.h"


S32 RPGUtils::rolloverRayCast(Point3F start, Point3F end, U32 mask)
{
	GameConnection* conn = GameConnection::getConnectionToServer();
	SceneObject* ctrl_obj = NULL;

	if (conn != NULL)
		ctrl_obj = conn->getControlObject();

	//if (ctrl_obj)
	//	ctrl_obj->disableCollision();

	static SceneObject* rollover_obj = 0;
	SceneObject* picked_obj = 0;

	RayInfo hit_info;
	if (gClientContainer.castRay(start, end, mask, &hit_info))
		picked_obj = dynamic_cast<SceneObject*>(hit_info.object);

	//if (ctrl_obj)
	//	ctrl_obj->enableCollision();

	if (picked_obj != rollover_obj)
	{
		if (rollover_obj)
			rollover_obj->setSelectionFlags(rollover_obj->getSelectionFlags() & ~SceneObject::PRE_SELECTED);
		if (picked_obj)
			picked_obj->setSelectionFlags(picked_obj->getSelectionFlags() | SceneObject::PRE_SELECTED);
		rollover_obj = picked_obj;

		if (conn)
			conn->setRolloverObj(rollover_obj);
	}

	return (picked_obj) ? picked_obj->getId() : -1;
}

//===========================================
std::vector<const type_info *> ClientOnlyNetObject::_mClientOnlys;

ClientOnlyNetObject::ClientOnlyNetObject( const type_info * typeinfo )
{
	_mClientOnlys.push_back(typeinfo);
}

bool ClientOnlyNetObject::isClientOnly( const type_info * typeinfo )
{
	for (S32 i = 0 ; i < _mClientOnlys.size() ; ++i)
	{
		if ((*_mClientOnlys[i]) == *typeinfo)
		{
			return true;
		}
	}
	return false;
}

//CLIENTONLY(TSStatic);


ConsoleMethod(NetConnection, GetGhostIndex, S32, 3, 3, "")
{
	NetObject* pObject;
	if (Sim::findObject(argv[2], pObject))
		return object->getGhostIndex(pObject);
	return 0;
}

ConsoleMethod(NetConnection, ResolveGhost, S32, 3, 3, "")
{
	S32 nIndex = dAtoi(argv[2]);
	if (nIndex != -1)
	{
		NetObject* pObject = NULL;
		if( object->isGhostingTo())
			pObject = object->resolveGhost(nIndex);
		else if( object->isGhostingFrom())
			pObject = object->resolveObjectFromGhostIndex(nIndex);
		if (pObject)
			return pObject->getId();
	} 
	return 0;
}