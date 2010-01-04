#include "wlmgr.h"
#include "core/util/safeDelete.h"
#include "../Global/GlobalStatic.h"

CWLMgr * CWLMgr::_instance = 0;
static const char * wldll = "WLLib.dll";

CWLMgr::CWLMgr():_dll(0),_spaceHashTable(0),_lockFreeQueue_RecvPkt(0),\
_threadPool(0),_spaceIndexedMesh(0),_strOp(0),_lockFreeQueue_SendPkt(NULL),\
_lockFreeQueue_MonsterAction(NULL)
{
	_dll  = new WL::WINDOWS_DLL(wldll);
	//启动线程池
	//createThreadPool(2,100);
	//_threadPool->setActiveThreadCount(2);
	//启动接收udp包的链表
	createStack_RecvPkt();
	createStack_SendPkt();
	//启动怪物action链表
	createStack_MonsterAction();
}

CWLMgr::~CWLMgr()
{
	SAFE_DELETE(_spaceHashTable);
	SAFE_DELETE(_spaceIndexedMesh);

	while(_lockFreeQueue_RecvPkt->size())
		CGlobalStatic::freePkt((PKT*)_lockFreeQueue_RecvPkt->pop());
	_lockFreeQueue_RecvPkt->destroy();

	while(_lockFreeQueue_SendPkt->size())
		CGlobalStatic::freePktSend((PKTSEND*)_lockFreeQueue_SendPkt->pop());
	_lockFreeQueue_SendPkt->destroy();

	while(_lockFreeQueue_MonsterAction->size())
		_lockFreeQueue_MonsterAction->pop();//CMonsterManager::freeParam((CActionParam*)_lockFreeQueue_MonsterAction->pop());
	_lockFreeQueue_MonsterAction->destroy();

	//SAFE_DELETE(_threadPool);
	//SAFE_DELETE(_strOp);这个不需要销毁
	SAFE_DELETE(_dll);
}

void CWLMgr::destroy()
{
	SAFE_DELETE(_instance);
}

CWLMgr * CWLMgr::getInstance()
{
	static CWLMgr instance;
	return & instance;
	// 	if (_instance == 0)
	// 	{
	// 		_instance = new CWLMgr();
	// 	}
	// 	return _instance;
}

void CWLMgr::createSpaceHashTable(WL::Box world,int xBlock,int yBlock,int zBlock)
{
	if (_dll && _spaceHashTable == 0)
	{
		typedef WL::CSpaceHashTable * (*CREATESPACEHASH) (WL::Box,int,int,int);
		CREATESPACEHASH fun = (CREATESPACEHASH)(_dll->getProcessInDll("CreateSpaceHashTable"));
		if (fun)
		{
			_spaceHashTable = fun(world,xBlock,yBlock,zBlock);
		}
	}
}


void CWLMgr::createStack_RecvPkt()
{
	if (_dll && _lockFreeQueue_RecvPkt == 0)
	{
		typedef WL::CLockFreeQueue * (*CREATESTACK) ();
		CREATESTACK fun = (CREATESTACK)(_dll->getProcessInDll("CreateLockFreeQueue"));
		if (fun)
		{
			_lockFreeQueue_RecvPkt = fun();
		}
	}
}

void CWLMgr::createStack_SendPkt()
{
	if (_dll && _lockFreeQueue_SendPkt == 0)
	{
		typedef WL::CLockFreeQueue * (*CREATESTACK) ();
		CREATESTACK fun = (CREATESTACK)(_dll->getProcessInDll("CreateLockFreeQueue"));
		if (fun)
		{
			_lockFreeQueue_SendPkt = fun();
		}
	}
}

void CWLMgr::createStrOp()
{
	if (_dll && _strOp == 0)
	{
		typedef WL::CStrOp * (*CREATESTROP) ();
		CREATESTROP fun = (CREATESTROP)(_dll->getProcessInDll("CreateStrOp"));
		if (fun)
		{
			_strOp = fun();
		}
	}
}

void CWLMgr::createSpaceIndexedMesh(WL::Box world,int xBlock,int yBlock,int zBlock)
{
	if (_dll && _spaceIndexedMesh == 0)
	{
		typedef WL::CSpaceIndexedMesh * (*CREATESPACEMESH) (WL::Box,int,int,int);
		CREATESPACEMESH fun = (CREATESPACEMESH)(_dll->getProcessInDll("CreateSpaceIndexedMesh"));
		if (fun)
		{
			_spaceIndexedMesh = fun(world,xBlock,yBlock,zBlock);
		}
	}
}

void CWLMgr::createThreadPool(int nThreadCount, int nThreadCapability)
{
	if (_dll && _threadPool == 0)
	{
		typedef WL::CThreadPool * (*CREATETHREADPOOL) (int,int);
		CREATETHREADPOOL fun = (CREATETHREADPOOL)(_dll->getProcessInDll("CreateThreadPool"));
		if(fun)
			_threadPool = fun(nThreadCount,nThreadCapability);
	}
}

WL::CSpaceHashTable * CWLMgr::getSpaceTable()
{
	return _spaceHashTable;
}

WL::CSpaceIndexedMesh * CWLMgr::getSpaceMesh()
{
	return _spaceIndexedMesh;
}

WL::CLockFreeQueue * CWLMgr::getStack_RecvPkt()
{
	return _lockFreeQueue_RecvPkt;
}

WL::CLockFreeQueue * CWLMgr::getStack_SendPkt()
{
	return _lockFreeQueue_SendPkt;
}

WL::CThreadPool * CWLMgr::getThreadPool()
{
	return _threadPool;
}

WL::CStrOp * CWLMgr::getStrOp()
{
	if (_strOp == NULL)
	{
		createStrOp();
	}
	return _strOp;
}

void CWLMgr::createStack_MonsterAction()
{
	if (_dll && _lockFreeQueue_MonsterAction == 0)
	{
		typedef WL::CLockFreeQueue * (*CREATESTACK) ();
		CREATESTACK fun = (CREATESTACK)(_dll->getProcessInDll("CreateLockFreeQueue"));
		if (fun)
		{
			_lockFreeQueue_MonsterAction = fun();
		}
	}
}

WL::CLockFreeQueue * CWLMgr::getStack_MonsterAction()
{
	return _lockFreeQueue_MonsterAction;
}

WL::CLockFreeQueue * CWLMgr::createLockFreeQueueInstance()
{
	WL::CLockFreeQueue * pInstance = NULL;
	if (_dll)
	{
		typedef WL::CLockFreeQueue * (*CREATESTACK) ();
		CREATESTACK fun = (CREATESTACK)(_dll->getProcessInDll("CreateLockFreeQueue"));
		if (fun)
		{
			pInstance = fun();
		}
	}
	return pInstance;
}

void CWLMgr::destroyLockFreeQueueInstance(WL::CLockFreeQueue * pInstance)
{
	while(pInstance->size())
		pInstance->pop();
	pInstance->destroy();
}