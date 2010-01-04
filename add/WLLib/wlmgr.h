#ifndef __WL_MGR__
#define __WL_MGR__
#include "D:\\My Documents\\Visual Studio 2008\\Projects\\WLLib\\WLLib\\WLLib.h"

class CWLMgr
{
	CWLMgr();
	~CWLMgr();
	static CWLMgr * _instance;
	WL::WINDOWS_DLL *			_dll;
	WL::CSpaceHashTable *		_spaceHashTable;
	WL::CLockFreeQueue *			_lockFreeQueue_RecvPkt;
	WL::CLockFreeQueue *			_lockFreeQueue_SendPkt;
	WL::CLockFreeQueue *			_lockFreeQueue_MonsterAction;
	WL::CThreadPool *				_threadPool;
	WL::CSpaceIndexedMesh *		_spaceIndexedMesh;
	WL::CStrOp *					_strOp;
public:
	static CWLMgr * getInstance();
	static void destroy();

	void	createSpaceHashTable(WL::Box world,int xBlock,int yBlock,int zBlock);
	void	createSpaceIndexedMesh(WL::Box world,int xBlock,int yBlock,int zBlock);
	void	createThreadPool(int nThreadCount, int nThreadCapability);
	void	createStrOp();
	void	createStack_RecvPkt();
	void	createStack_SendPkt();
	void	createStack_MonsterAction();
	WL::CSpaceHashTable * getSpaceTable();
	WL::CSpaceIndexedMesh * getSpaceMesh();
	WL::CLockFreeQueue * getStack_RecvPkt();
	WL::CLockFreeQueue * getStack_SendPkt();
	WL::CLockFreeQueue * getStack_MonsterAction();
	WL::CThreadPool * getThreadPool();
	WL::CStrOp * getStrOp();

	WL::CLockFreeQueue *	createLockFreeQueueInstance();
	void						destroyLockFreeQueueInstance(WL::CLockFreeQueue * pInstance);
};

#endif