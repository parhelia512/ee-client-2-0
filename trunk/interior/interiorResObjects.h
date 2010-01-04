//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _INTERIORRESOBJECTS_H_
#define _INTERIORRESOBJECTS_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif
#ifndef _MBOX_H_
#include "math/mBox.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _POLYHEDRON_H_
#include "collision/polyhedron.h"
#endif

class Stream;

struct InteriorDictEntry
{
   char name[256];
   char value[256];
};

class InteriorDict : public Vector<InteriorDictEntry>
{
public:
   void read(Stream& stream);
   void write(Stream& stream) const;
};

class InteriorResTrigger
{
  public:
   enum Constants {
      MaxNameChars = 255
   };

   char       mName[MaxNameChars+1];
   StringTableEntry mDataBlock;
   InteriorDict mDictionary;

   Point3F    mOffset;
   Polyhedron mPolyhedron;

  public:
   InteriorResTrigger() { }

   bool read(Stream& stream);
   bool write(Stream& stream) const;
};

class InteriorPathFollower
{
  public:
   struct WayPoint {
      Point3F  pos;
      QuatF    rot;
      U32      msToNext;
      U32      smoothingType;
   };
   StringTableEntry         mName;
   StringTableEntry         mDataBlock;
   U32                      mInteriorResIndex;
   U32                      mPathIndex;
   Point3F                  mOffset;
   Vector<U32>              mTriggerIds;
   Vector<WayPoint>         mWayPoints;
   U32                      mTotalMS;
   InteriorDict mDictionary;

  public:
   InteriorPathFollower();
   ~InteriorPathFollower();

   bool read(Stream& stream);
   bool write(Stream& stream) const;
};


class AISpecialNode
{
   public:
      enum
      {
         chute = 0,
      };

   public:
      StringTableEntry  mName;
      Point3F           mPos;
      //U32               mType;

  public:
   AISpecialNode();
   ~AISpecialNode();

   bool read(Stream& stream);
   bool write(Stream& stream) const;

};

class ItrGameEntity
{
   public:
      StringTableEntry  mDataBlock;
      StringTableEntry  mGameClass;
      Point3F           mPos;
      InteriorDict mDictionary;

  public:
   ItrGameEntity();
   ~ItrGameEntity();

   bool read(Stream& stream);
   bool write(Stream& stream) const;

};

#endif  // _H_INTERIORRESOBJECTS_
