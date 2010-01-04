#ifndef __GLOBAL_STATIC__
#define __GLOBAL_STATIC__
#include "platform/platformNet.h"
#include "../Others/LockFreeChunker.h"
#include "math/mMath.h"
#include <vector>

#include <windows.h>
class NetConnection;
class Player;

struct PKT 
{
	char data[MAXPACKETSIZE];
	NetAddress address;
	int bytesRead;
	~PKT(){/*data.~RawData();*/}
};
struct PKTSEND
{
	char data[MAXPACKETSIZE];
	int		dataSize;
	NetAddress address;
	PKTSEND(const char * pData,int size):dataSize(size)
	{
		if (pData && size > 0)
		{
			dMemcpy(data,pData,size);
		}
	}
	~PKTSEND(){/*if(data)delete[] data;*/}
};
class CGlobalStatic
{
protected:
	static  std::vector<U32> * g_pActorsFounded;
	static NetConnection * g_pScopingConn;
	static WL::LockFreeChunker<PKT> g_ChunkListPkt;
	static WL::LockFreeChunker<PKTSEND> g_ChunkListPktSend;
	static HANDLE				g_hRcvThread;
	static HANDLE				g_hSendThread;
public:
	static bool					g_bShutDown;
	static HANDLE				g_hEventSend;

	static void init();
	static void shutdown();
	static void tick();
	static void scope(void * pContent);
	static void setScopingConnection(NetConnection * pConn);
	static PKT * allocPkt();
	static void freePkt(PKT* pkt);
	static PKTSEND * allocPktSend(const char * data,int size);
	static void freePktSend(PKTSEND* pkt);
	static F32 getMapHeight(const Point2F xy);
	static void getActorsSurrounded(Player * pSelf , std::vector<U32> * actorsID);//获得周围的player
	static void actorFounded(void * pContent);
};

#endif