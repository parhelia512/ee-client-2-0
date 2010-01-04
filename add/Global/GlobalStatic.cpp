#include "GlobalStatic.h"
#include "../wllib/wlmgr.h"
#include "T3D/gameConnection.h"
#include "T3D/player.h"
#include "terrain/terrData.h"
#include "sceneGraph/sceneGraph.h"
#include "../others/CSLock.h"

NetConnection * CGlobalStatic::g_pScopingConn = NULL;
std::vector<U32> * CGlobalStatic::g_pActorsFounded = NULL;
WL::LockFreeChunker<PKT> CGlobalStatic::g_ChunkListPkt;
WL::LockFreeChunker<PKTSEND> CGlobalStatic::g_ChunkListPktSend;
bool				CGlobalStatic::g_bShutDown = false;
HANDLE				CGlobalStatic::g_hRcvThread = NULL;
HANDLE				CGlobalStatic::g_hSendThread = NULL;
HANDLE				CGlobalStatic::g_hEventSend = NULL;

extern int udpSocket;
extern void IPSocketToNetAddress(const struct sockaddr_in *sockAddr,  NetAddress *address);
extern void netToIPSocketAddress(const NetAddress *address, struct sockaddr_in *sockAddr);

DWORD WINAPI ThreadSend( LPVOID lpParam )
{
	WL::CLockFreeQueue * pListSend = CWLMgr::getInstance()->getStack_SendPkt();
	PKTSEND * pkt = NULL;
	sockaddr_in ipAddr;
	while (!CGlobalStatic::g_bShutDown)
	{
		WaitForSingleObject(CGlobalStatic::g_hEventSend,2000); 
		//Platform::sleep(1);
		while(pListSend->size())
		{
			pkt = (PKTSEND*)(pListSend->pop());
			if (pkt == NULL)
				continue;
			netToIPSocketAddress(&(pkt->address), &ipAddr);
			::sendto(udpSocket,(const char *)pkt->data,pkt->dataSize,0,(sockaddr *) &ipAddr, sizeof(sockaddr_in));
			CGlobalStatic::freePktSend(pkt);
		}
	}
	return 0;
}

DWORD WINAPI ThreadRecv( LPVOID lpParam )
{
	sockaddr sa;
	sa.sa_family = AF_UNSPEC;
	PKT * tmpBuffer = NULL;
	int addrLen = sizeof(sa);
	S32 bytesRead = -1;	
	while (!CGlobalStatic::g_bShutDown)
	{
		addrLen = sizeof(sa);
		bytesRead = -1;
		tmpBuffer = CGlobalStatic::allocPkt();
		//tmpBuffer->data.alloc(MAXPACKETSIZE);

		if(udpSocket != InvalidSocket)
			bytesRead = recvfrom(udpSocket, (char *) tmpBuffer->data, MAXPACKETSIZE, 0, &sa, &addrLen);

		if(bytesRead == -1)
		{
			CGlobalStatic::freePkt(tmpBuffer);
			continue;
		}

		if(sa.sa_family == AF_INET)
			IPSocketToNetAddress((sockaddr_in *) &sa,  &(tmpBuffer->address));
		else
		{
			CGlobalStatic::freePkt(tmpBuffer);
			continue;
		}

		if(bytesRead <= 0)
		{
			CGlobalStatic::freePkt(tmpBuffer);
			continue;
		}

		if(tmpBuffer->address.type == NetAddress::IPAddress &&
			tmpBuffer->address.netNum[0] == 127 &&
			tmpBuffer->address.netNum[1] == 0 &&
			tmpBuffer->address.netNum[2] == 0 &&
			tmpBuffer->address.netNum[3] == 1 &&
			tmpBuffer->address.port == 0)
		{
			CGlobalStatic::freePkt(tmpBuffer);
			continue;
		}

		tmpBuffer->bytesRead = bytesRead;
		CWLMgr::getInstance()->getStack_RecvPkt()->push(tmpBuffer);
	}
	return 0;
}


void CGlobalStatic::init()
{
	CWLMgr::getInstance();
	g_hRcvThread = CreateThread(NULL,0,ThreadRecv,NULL,0,NULL);
	g_hSendThread = CreateThread(NULL,0,ThreadSend,NULL,0,NULL);
	g_hEventSend = CreateEvent( 
		NULL,               // default security attributes
		FALSE,               // auto reset event
		FALSE,               // initial state is nonsignaled
		NULL
		); 
}

void CGlobalStatic::shutdown()
{
	g_bShutDown = true;
	WaitForSingleObject(g_hRcvThread,2000);
	::TerminateThread(g_hRcvThread,0);
	::TerminateThread(g_hSendThread,0);
	CloseHandle(g_hRcvThread);
	CloseHandle(g_hSendThread);
	CloseHandle(g_hEventSend);
	CWLMgr::destroy();
}

void CGlobalStatic::scope(void * pContent)
{
	SceneObject * pObj = reinterpret_cast<SceneObject *>(pContent);	
	if(pObj->isScopeable())
		g_pScopingConn->objectInScope(pObj);
}

void CGlobalStatic::setScopingConnection(NetConnection * pConn)
{
	g_pScopingConn = pConn;
}

void CGlobalStatic::tick()
{
	static U32 time = Platform::getVirtualMilliseconds();
	static U32 timeDelta;
	timeDelta = Platform::getVirtualMilliseconds() - time;
	time = Platform::getVirtualMilliseconds();

	static int sendCount;
	static int recvCount;
	sendCount = CWLMgr::getInstance()->getStack_SendPkt()->size();
	recvCount = CWLMgr::getInstance()->getStack_RecvPkt()->size();
	if (sendCount > 10)
	{
		Con::printf("send list %d",sendCount);
	}
	if (recvCount > 10)
	{
		Con::printf("recv list %d",recvCount);
	}

	//monsters tick
	/*
	static CMonsterManager * pMonsterMgr = CMonsterManager::getSingleTon();
	if (pMonsterMgr)
		pMonsterMgr->onTick(timeDelta);
	else
		pMonsterMgr = CMonsterManager::getSingleTon();
		*/
}

void CGlobalStatic::freePkt(PKT* pkt)
{
	pkt->~PKT();
	g_ChunkListPkt.free(pkt);
}

PKT * CGlobalStatic::allocPkt()
{
	PKT * p = g_ChunkListPkt.alloc();
	if (p)
	{
		new(p) PKT();
	}
	return p;
}

void CGlobalStatic::freePktSend(PKTSEND* pkt)
{
	pkt->~PKTSEND();
	g_ChunkListPktSend.free(pkt);
}

PKTSEND * CGlobalStatic::allocPktSend(const char * data,int size)
{
	PKTSEND * p = g_ChunkListPktSend.alloc();
	if (p)
	{
		new(p) PKTSEND(data,size);
	}
	return p;
}

F32 CGlobalStatic::getMapHeight(const Point2F xy)
{
	TerrainBlock* pBlock = gServerSceneGraph->getCurrentTerrain();
	F32 height;
	Point2F mXY(xy);
	Point3F position = pBlock->getPosition();
	mXY.x -= position.x;
	mXY.y -= position.y;
	if (pBlock->getHeight(mXY,&height))
		return height;
	return -1;
}

void CGlobalStatic::getActorsSurrounded( Player * pSelf , std::vector<U32> * actorsID )
{
	Point3F pos;
	MatrixF transform = pSelf->getTransform();
	transform.getColumn(3,&pos);
	g_pActorsFounded = actorsID;
	CWLMgr::getInstance()->getSpaceMesh()->visitExceptSelf(pSelf,WL::Pt3F(pos.x,pos.y,pos.z),CGlobalStatic::actorFounded);
}

void CGlobalStatic::actorFounded( void * pContent )
{
	Player * pObj = reinterpret_cast<Player *>(pContent);	
	if(pObj)
		g_pActorsFounded->push_back(pObj->getId());
}

//×Ö·û´®hash
unsigned int hash(const char *str)
{ 
	register unsigned int h; 
	register unsigned char *p; 
	for(h=0, p = (unsigned char *)str; *p ; p++) 
		h = 31 * h + *p; 
	return h; 
}

//¿í×Ö·û´®hash
unsigned int hash(const wchar_t *key)
{
	register wchar_t *s;
	register int c, k;
	s = (wchar_t *)key;
	k = *s;
	while ((c = *++s))
		k = 31 * k + c;
	return k;
}