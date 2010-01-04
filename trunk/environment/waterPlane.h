//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _WATERPLANE_H_
#define _WATERPLANE_H_

#ifndef _GAMEBASE_H_
#include "T3D/gameBase.h"
#endif
#ifndef _GFXDEVICE_H_
#include "gfx/gfxDevice.h"
#endif
#ifndef _SCENEDATA_H_
#include "materials/sceneData.h"
#endif
#ifndef _MATINSTANCE_H_
#include "materials/matInstance.h"
#endif
#ifndef _GFXPRIMITIVEBUFFER_H_
#include "gfx/gfxPrimitiveBuffer.h"
#endif
#ifndef _RENDERPASSMANAGER_H_
#include "renderInstance/renderPassManager.h"
#endif
#ifndef _MATHUTIL_FRUSTUM_H_
#include "math/util/frustum.h"
#endif
#ifndef _WATEROBJECT_H_
#include "environment/waterObject.h"
#endif

class AudioEnvironment;


//*****************************************************************************
// WaterBlock
//*****************************************************************************
class WaterPlane : public WaterObject
{
   typedef WaterObject Parent;

public:

   // LEGACY support
   enum EWaterType
   {
      eWater            = 0,
      eOceanWater       = 1,
      eRiverWater       = 2,
      eStagnantWater    = 3,
      eLava             = 4,
      eHotLava          = 5,
      eCrustyLava       = 6,
      eQuicksand        = 7,
   }; 

private:

   enum MaskBits {
      UpdateMask =   Parent::NextFreeMask,
      NextFreeMask = Parent::NextFreeMask << 1
   };
   
   // vertex / index buffers
   GFXVertexBufferHandle<GFXWaterVertex> mVertBuff;
   GFXPrimitiveBufferHandle mPrimBuff;

   // misc
   U32            mGridSize;
   U32            mGridSizeMinusOne;
   F32            mGridElementSize;
   U32            mVertCount;
   U32            mIndxCount;
   U32            mPrimCount;   
   Frustum        mFrustum;
   
   SceneGraphData setupSceneGraphInfo( SceneState *state );
   void setShaderParams( SceneState *state, BaseMatInstance* mat, const WaterMatParams& paramHandles );
   void setupVBIB( const Point3F &camPos );
   virtual bool prepRenderImage( SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseZoneState = false );
   virtual void innerRender( SceneState *state );
   void setMultiPassProjection();

protected:

   //-------------------------------------------------------
   // Standard engine functions
   //-------------------------------------------------------
   bool onAdd();
   void onRemove();   
   U32  packUpdate  (NetConnection *conn, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *conn,           BitStream *stream);
   bool castRay(const Point3F &start, const Point3F &end, RayInfo* info);

public:
   WaterPlane();
   virtual ~WaterPlane();

   DECLARE_CONOBJECT(WaterPlane);   

   static void initPersistFields();
   void onStaticModified( const char* slotName, const char*newValue = NULL );
   virtual void inspectPostApply();
   virtual void setTransform( const MatrixF & mat );

   // WaterObject
   virtual F32 getWaterCoverage( const Box3F &worldBox ) const;
   virtual F32 getSurfaceHeight( const Point2F &pos ) const;
   virtual void onReflectionInfoChanged();
   virtual bool isUnderwater( const Point3F &pnt );

   // WaterBlock   
   bool isPointSubmerged ( const Point3F &pos, bool worldSpace = true ) const{ return true; }
   AudioEnvironment * getAudioEnvironment(){ return NULL; }   

   // WaterPlane
   void setGridSize( U32 inSize );
   void setGridElementSize( F32 inSize );

   // Protected Set'ers
   static bool protectedSetGridSize( void *obj, const char *data );
   static bool protectedSetGridElementSize( void *obj, const char *data );

protected:

   // WaterObject
   virtual void _getWaterPlane( const Point3F &camPos, PlaneF &outPlane, Point3F &outPos );
};

#endif // _WATERPLANE_H_
