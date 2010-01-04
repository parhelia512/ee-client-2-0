#ifndef __CS_LOCK__
#define __CS_LOCK__

#include <windows.h>

class CSLock
{
	CRITICAL_SECTION * _pCS;
public:
	CSLock(CRITICAL_SECTION * cs):_pCS(cs){EnterCriticalSection(_pCS);}
	~CSLock(){LeaveCriticalSection(_pCS);}
};

#endif