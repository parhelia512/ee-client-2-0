//-----------------------------------------------------------------------------
// Torque Game Engine Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SKY_H_
#define _SKY_H_

#ifndef _MMATH_H_
#include "math/mMath.h"
#endif
#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _SCENESTATE_H_
#include "sceneGraph/sceneState.h"
#endif
#ifndef _SCENEGRAPH_H_
#include "sceneGraph/sceneGraph.h"
#endif
#ifndef _MPOINT3_H_
#include "math/mPoint3.h"
#endif
#ifndef _MATERIALLIST_H_
#include "materials/materialList.h"
#endif
#ifndef _GAMEBASE_H_
//#include "T3D/gameBase.h"
#endif
#ifndef _RENDERPASSMANAGER_H_
#include "renderInstance/renderPassManager.h"
#endif

#include "gfx/gfxDevice.h"

#define MAX_NUM_LAYERS 3
#define MAX_BAN_POINTS 20

class SceneGraph;
class SceneState;

enum SkyState
{
   isDone = 0,
   comingIn = 1,
   goingOut = 2
};

typedef struct
{
   bool StormOn;
   bool FadeIn;
   bool FadeOut;
   S32 currentCloud;
   F32 stormSpeed;
   F32 stormDir;
   S32 numCloudLayers;
   F32 fadeSpeed;
   SkyState stormState;
} StormInfo;

typedef struct
{
   SkyState state;
   F32 speed;
   F32 time;
   F32 fadeSpeed;
} StormCloudData;

//---------------------------------------------------------------------------
class Cloud
{
  private:
   Point3F mPoints[25];
   Point2F mSpeed;
   F32 mCenterHeight, mInnerHeight, mEdgeHeight;
   F32 mAlpha[25];
   S32 mDown, mOver;
   static F32 mRadius;
   U32 mLastTime;
   F32 mOffset;
   Point2F mBaseOffset, mTexCoords[25], mTextureScale;
   GFXTexHandle mCloudHandle;

   Point2F alphaCenter;
   Point2F stormUpdate;
   F32 stormAlpha[25];
   F32 mAlphaSave[25];

   GFXStateBlockRef mCloudSB;

   static StormInfo mGStormData;
  public:
   Cloud();
   ~Cloud();
   void setPoints();
   void setHeights(F32 cHeight, F32 iHeight, F32 eHeight);
   void setTexture(GFXTexHandle);
   void setSpeed(const Point2F &speed);
   void setTextPer(F32 cloudTextPer);
   void updateCoord();
   void calcAlpha();
   void render(U32, U32, bool, S32, PlaneF*);
   void updateStorm();
   void calcStorm(F32 speed, F32 fadeSpeed);
   void calcStormAlpha();
   static void startStorm(SkyState);
   static void setRadius(F32 rad) {mRadius = rad;}
   void setRenderPoints(Point3F* renderPoints, Point2F* renderTexPoints, F32* renderAlpha, F32* renderSAlpha, S32 index);
   void clipToPlane(Point3F* points, Point2F* texPoints, F32* alphaPoints, F32* sAlphaPoints, U32& rNumPoints, const PlaneF& rPlane);
};

//--------------------------------------------------------------------------
class Sky : public SceneObject
{
   typedef SceneObject Parent;
  private:

    StormCloudData mStormCloudData;
    GFXTexHandle mSkyHandle[6];
    F32 mCloudHeight[MAX_NUM_LAYERS];
    F32 mCloudSpeed[MAX_NUM_LAYERS];
    Cloud mCloudLayer[MAX_NUM_LAYERS];
    F32 mRadius;
    Point3F mPoints[10];
    Point2F mTexCoord[4];
    FileName mMaterialListName;
    Point3F mSkyBoxPt;
    Point3F mTopCenterPt;
    Point3F mSpherePt;
    ColorI mRealSkyColor;

    MaterialList mMaterialList;
    bool mSkyTexturesOn;
    bool mRenderBoxBottom;
    ColorF mSolidFillColor;

    bool mNoRenderBans;
    F32 mBanOffsetHeight;

    S32 mNumCloudLayers;
    Point3F mWindVelocity;

    F32 mLastVisDisMod;

    GFXVertexBufferHandle<GFXVertexPCT> mSkyVB;

    static bool smCloudsOn;
    static bool smCloudOutlineOn;
    static bool smSkyOn;
    static S32  smNumCloudsOn;

    bool mStormCloudsOn;

    bool mSkyGlow;
    ColorF mSkyGlowColor;

    GFXStateBlockRef mClearSB;
    GFXStateBlockRef mSkyBoxSB;
    GFXStateBlockRef mRenderBansSB;

    void calcPoints();
    void loadVBPoints();
    void setupStateBlocks();
  protected:
    bool onAdd();
    void onRemove();

    bool prepRenderImage  ( SceneState *state, const U32 stateKey, const U32 startZone, const bool modifyBaseZoneState=false);
    void renderObject( ObjectRenderInst *ri, SceneState *state, BaseMatInstance* );
    void render(SceneState *state);
    void renderSkyBox(F32 lowerBanHeight, F32 alphaIn);
    void calcBans(F32 *banHeights, Point3F banPoints[][MAX_BAN_POINTS], Point3F *cornerPoints);
    void renderBans(const F32 *alphaBan, const F32 *banHeights, const Point3F banPoints[][MAX_BAN_POINTS], const Point3F *cornerPoints, const ColorI& fogColor);
    void inspectPostApply();
    void startStorm();
    void setSkyColor();
    void initSkyData();
    bool loadDml();
    void setRenderPoints(Point3F* renderPoints, S32 index);
    void calcTexCoords(Point2F* texCoords, Point3F* renderPoints, S32 index, F32 lowerBanHeight);
  public:
    Point2F mWindDir;
    enum NetMaskBits {
        InitMask = BIT(0),
        StormCloudMask = BIT(1),
        WindMask = BIT(2),
        StormCloudsOnMask = BIT(3),
        SkyGlowMask = BIT(4)
   };
   enum Constants {
        EnvMapMaterialOffset = 6,
        CloudMaterialOffset  = 7
   };

   Sky();
   ~Sky();

   /// @name Storm management.
   /// @{
   void stormCloudsShow(bool);
   void stormCloudsOn(S32 state, F32 time);
   /// @}

   /// @name Wind velocity
   /// @{

   void setWindVelocity(const Point3F &);
   const Point3F &getWindVelocity() const;
   /// @}

   /// @name Environment mapping
   /// @{

//   TextureHandle getEnvironmentMap() { return mMaterialList.getMaterial(EnvMapMaterialOffset); }
   /// @}

   /// Torque infrastructure
   DECLARE_CONOBJECT(Sky);
   static void initPersistFields();
   static void consoleInit();

   U32  packUpdate  (NetConnection *conn, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *conn,           BitStream *stream);

   void applySkyChanges()
   {
	   inspectPostApply();
   }
};




#endif
