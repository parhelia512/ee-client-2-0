 //-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _DECALMANAGER_H_
#define _DECALMANAGER_H_

#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _MATHUTIL_FRUSTUM_H_
#include "math/util/frustum.h"
#endif
#ifndef _GFXPRIMITIVEBUFFER_H_
#include "gfx/gfxPrimitiveBuffer.h"
#endif
#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif
#ifndef _ITICKABLE_H_
#include "core/iTickable.h"
#endif
#ifndef _CLIPPEDPOLYLIST_H_
#include "collision/clippedPolyList.h"
#endif
#ifndef _DECALDATAFILE_H_
#include "decalDataFile.h"
#endif
#ifndef __RESOURCE_H__
#include "core/resource.h"
#endif
#ifndef _DECALINSTANCE_H_
#include "decalInstance.h"
#endif
#ifndef _TSIGNAL_H_
#include "core/util/tSignal.h"
#endif
#ifndef _DATACHUNKER_H_
#include "core/dataChunker.h"
#endif

//#define DECALMANAGER_DEBUG

struct ObjectRenderInst;
class Material;

enum DecalFlags 
{
   PermanentDecal = 1 << 0,
   SaveDecal      = 1 << 1,
   ClipDecal      = 1 << 2,
   CustomDecal    = 1 << 3 // DecalManager will not attempt to clip or remove this decal
                           // it is managed by someone else.
};


/// Manage decals in the scene.
class DecalManager : public SceneObject, public ITickable
{
   typedef SceneObject Parent;   

public:
   
   static bool smDecalsOn;
   static F32 smDecalLifeTimeScale;   
   static const U32 smMaxVerts;
   static const U32 smMaxIndices;

	Vector<DecalInstance *> mDecalInstanceVec;

#ifdef DECALMANAGER_DEBUG
   Vector<VectorF> mDebugVectors;
   Vector<Point3F> mDebugPoints;
   Vector<PlaneF> mDebugPlanes;
   Point3F mDebugVecPos;
#endif

public:

   DecalManager();
   ~DecalManager();

   DECLARE_CONOBJECT( DecalManager );
   static void consoleInit();

   // ITickable
   virtual void interpolateTick( F32 delta );
   virtual void processTick();
   virtual void advanceTime( F32 timeDelta );

   /// @name Decal Addition
   /// @{

   /// Adds a decal using a normal and a rotation.
   ///
   /// @param pos The 3d position for the decal center.
   /// @param normal The decal up vector.
   /// @param rotAroundNormal The decal rotation around the normal.
   /// @param decalData The datablock which defines this decal.
   /// @param decalScale A scalar for adjusting the default size of the decal.
   /// @param decalTexIndex   Selects the texture coord index within the decal
   ///                        data to use.  If it is less than zero then a random
   ///                        coord is selected.
   /// @param flags  The decal flags. @see DecalFlags
   ///
   DecalInstance* addDecal( const Point3F &pos,
                            const Point3F &normal,
                            F32 rotAroundNormal,
                            DecalData *decalData,
                            F32 decalScale = 1.0f,
                            S32 decalTexIndex = 0,
                            U8 flags = 0x000 );

   /// Adds a decal using a normal and a tangent.
   ///
   /// @param pos The 3d position for the decal center.
   /// @param normal The decal up vector.
   /// @param tanget The decal right vector.
   /// @param decalData The datablock which defines this decal.
   /// @param decalScale A scalar for adjusting the default size of the decal.
   /// @param decalTexIndex   Selects the texture coord index within the decal
   ///                        data to use.  If it is less than zero then a random
   ///                        coord is selected.
   /// @param flags  The decal flags. @see DecalFlags
   ///
   DecalInstance* addDecal( const Point3F &pos,
                            const Point3F &normal,
                            const Point3F &tangent,
                            DecalData *decalData,
                            F32 decalScale = 1.0f,
                            S32 decalTexIndex = 0,
                            U8 flags = 0 );

   /// @}

   /// @name Decal Removal
   /// @{

   void removeDecal( DecalInstance *inst );

   /// @}

   DecalInstance* getDecal( S32 id );

   DecalInstance* getClosestDecal( const Point3F &pos );

   /// Return the closest DecalInstance hit by a ray.
   DecalInstance* raycast( const Point3F &start, const Point3F &end, bool savedDecalsOnly = true );

   //void dataDeleted( DecalData *data );

   void saveDecals( const UTF8 *fileName );
   bool loadDecals( const UTF8 *fileName );
   void clearData();

   const Frustum& getFrustum() const { return mCuller; }   

   /// Returns true if changes have been made since the last load/save
   bool isDirty() const { return mDirty; }

   bool clipDecal( DecalInstance *decal, Vector<Point3F> *edgeVerts = NULL, const Point2F *clipDepth = NULL );

   void renderDecalSpheres();

   void notifyDecalModified( DecalInstance *inst );

   bool _createDataFile();
   
   Signal< void() >& getClearDataSignal() { return mClearDataSignal; }

	Resource<DecalDataFile> getDecalDataFile() { return mData; }

protected:
   
   // Assume that a class is already given for the object:
   //    Point with coordinates {float x, y;}
   //===================================================================

   // isLeft(): tests if a point is Left|On|Right of an infinite line.
   //    Input:  three points P0, P1, and P2
   //    Return: >0 for P2 left of the line through P0 and P1
   //            =0 for P2 on the line
   //            <0 for P2 right of the line
   //    See: the January 2001 Algorithm on Area of Triangles
   inline F32 isLeft( const Point3F &P0, const Point3F &P1, const Point3F &P2 )
   {
       return (P1.x - P0.x)*(P2.y - P0.y) - (P2.x - P0.x)*(P1.y - P0.y);
   }

   U32 _generateConvexHull( const Vector<Point3F> &points, Vector<Point3F> *outPoints );

   // Rendering
   bool prepRenderImage(SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseZoneState);
   
   void _generateWindingOrder( const Point3F &cornerPoint, Vector<Point3F> *sortPoints );

   // Helpers for creating and deleting the vert and index arrays
   // held by DecalInstance.
   void _allocBuffers( DecalInstance *inst );
   void _freeBuffers( DecalInstance *inst );

   /// Returns index used to index into the correct sized FreeListChunker for
   /// allocating vertex and index arrays.
   S32 _getSizeClass( DecalInstance *inst ) const;

protected:

   Frustum mCuller;

   Vector<DecalInstance*> mDecalQueue;

   StringTableEntry mDataFileName;
   Resource<DecalDataFile> mData;
   
   Signal< void() > mClearDataSignal;
   
   Vector< GFXVertexBufferHandle<DecalVertex> > mVBs;
   Vector<GFXPrimitiveBufferHandle> mPrimBuffs;

   FreeListChunkerUntyped *mChunkers[3];

   bool mDirty;

   struct DecalBatch
   {
      U32 startDecal;
      U32 decalCount;
      U32 iCount;
      U32 vCount;
      U8 priority;
      Material *mat;
      BaseMatInstance *matInst;
      bool dynamic;
   };
};

extern DecalManager* gDecalManager;

#endif // _DECALMANAGER_H_
