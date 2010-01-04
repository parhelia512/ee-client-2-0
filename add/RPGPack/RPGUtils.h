#ifndef __RPGUTILS_H__
#define __RPGUTILS_H__

#include "core/stream/bitStream.h"
#include "console/simObject.h"
#include <vector>
#include <typeinfo>

#define CLIENTONLY(T) class T;\
	ClientOnlyNetObject gClinetOnly##T(&(typeid(T)))

class ClientOnlyNetObject
{
	static std::vector<const type_info *> _mClientOnlys;
public:
	ClientOnlyNetObject(const type_info *);
	static bool	isClientOnly(const type_info *);
};

class RPGUtils
{
public:
	static inline void writeDatablockID(BitStream* s, SimObject* simobj, bool packed=false)
	{
		if (s->writeFlag(simobj))
			s->writeRangedU32(packed ? SimObjectId(simobj) : simobj->getId(), 
			DataBlockObjectIdFirst, DataBlockObjectIdLast);
	}

	static inline S32 readDatablockID(BitStream* s)
	{
		return (!s->readFlag()) ? 0 : ((S32)s->readRangedU32(DataBlockObjectIdFirst, 
			DataBlockObjectIdLast));
	}

	static S32 rolloverRayCast(Point3F start, Point3F end, U32 mask);
};

#endif