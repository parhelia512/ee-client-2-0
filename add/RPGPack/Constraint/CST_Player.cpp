#include "CST_Player.h"
#include "T3D/player.h"

IMPLEMENT_CONOBJECT(CST_Player);

CST_Player::CST_Player()
{

}

void CST_Player::setAnimClip( const char * clipName , bool locked /*= false*/ )
{
	Player * pPlayer = dynamic_cast<Player*>(getSceneObj());
	if (pPlayer)
	{
		pPlayer->setActionThread(clipName,false,true,false);
	}
}
//=========================DESC=====================
class CST_Player_Desc : public ConstraintDesc
{
public:
	bool							isMatchDesc(const char * szConstraint);
	Constraint *				createConstraint(const char * szConstraint);
};

bool CST_Player_Desc::isMatchDesc( const char * szConstraint )
{
	return dStrstr(szConstraint,"#player") != NULL;
}

Constraint * CST_Player_Desc::createConstraint( const char * szConstraint )
{
	CST_Player * pInstance = NULL;
	const char * pName = dStrchr(szConstraint,'.');
	SceneObject * pObj = dynamic_cast<Player *>( Sim::findObject(pName) );
	if (pName && pObj)
	{
		pInstance = new CST_Player();
		pInstance->setSceneObj(pObj);
	}
	return pInstance;
}
IMPLEMENT_CSTDESC(CST_Player_Desc);